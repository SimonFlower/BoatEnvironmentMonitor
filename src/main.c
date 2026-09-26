#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "stm32l432xx.h"
#include "stm32l4xx_hal.h"

#include "task_comms.h"
#include "debug.h"
#include "led.h"
#include "adc.h"
#include "dht22.h"
#include "modem.h"
#include "system_funcs.h"

// Definitions for the numbers of tasks
#define N_ADC_TASKS         4U
#define N_DHT22_TASKS       5U
#define N_TASKS             (N_ADC_TASKS + N_DHT22_TASKS)
#define ALL_TASKS_BITMASK   ((1U << N_TASKS) - 1U)
#if N_TASKS > 24
#error "Number of tasks must be 24 or less (because it is used with FreeRTOS event groups)"
#endif

// Timeout (in mS) for the tasks to complete
#define TASK_TIMEOUT        10000

// Time between collections (in mS)
#define COLLECTION_INTERVAL 60000

// Size of the FreeRTOS stack for data collection tasks, for the modem task
// and for the main task measured in words (not bytes).
// Floating point requires everything to be aligned on 8-byte boundaries, so
// these values must be integer multiples of 8
#define DC_TASK_STACK_SIZE      256
#define MODEM_TASK_STACK_SIZE   1024
#define MAIN_TASK_STACK_SIZE    1024

// FreeRTOS priority for data collections tasks, modem tasks and the main task
#define DC_TASK_PRIO            1
#define MODEM_TASK_PRIO         2
#define MAIN_TASK_PRIO          3

#define FPU_FPCCR_ASPEN (1UL << 31)
#define FPU_FPCCR_LSPEN (1UL << 30)

// a structure that allows multiple parameters to be passed into the main task
typedef struct {
    // An event group used to start a data collection operation
    // This is *written* by the main task
    EventGroupHandle_t EG_trigger;
    // An event group used to signal data collection completed
    // This is *read* by the main task
    EventGroupHandle_t EG_sync;
    // The queue used to pass data collection readings back to the main task
    QueueHandle_t results_queue;
    // The queue used to communicate with the modem
    QueueHandle_t modem_queue;
    // A list of the data collection tasks
    TaskHandle_t tasks [N_TASKS];
    // The modem task
    TaskHandle_t modem_task;
} MainTaskCfg_t;

// forward declarations
static BaseType_t AddTask (TaskType_t type, char *name, MainTaskCfg_t *main_task_cfg, TaskCfg_t *task_cfg, int task_count, BaseType_t status);
static void MainTask(void *pvParameters);
static void InitHardware (void);
static void ShutdownHardware (void);

int main(void) {

    /* common hardware initialisation - this must be called early in program startup so that
     * hardware such as the user LED is available to fault handlers */ 
    InitHardware ();

#if DEBUG > 0
    /* create the mutex for the system _write() function so that it is thread safe */
    createUsartMutex();

    // Disable stdout buffering so characters print immediately without needing '\n'
    setvbuf(stdout, NULL, _IONBF, 0);
    
    printf ("Boat Environment Monitor starting\r\n");
#endif

    // The structure used to configure the main task
    static MainTaskCfg_t main_task_cfg;
    
    // The structures used to configure the data collection tasks
    static TaskCfg_t task_cfg [N_TASKS];

    // This variable is used to track correct initialization
    BaseType_t status = pdPASS;
    
    // Create the event groups used to trigger and synchronize data collection tasks.
    // Note that FreeRTOS creates the group with all bits cleared, so that waiting on
    // the event group will initially block.
    // Each task is assigned its own bit in both event groups - this bit is used to
    // trigger the task to start work (via the EG_trigger group), the same bit is also 
    // used by the task to signal that it has completed it's work (via the EG_sync group)
    main_task_cfg.EG_trigger = xEventGroupCreate();
    main_task_cfg.EG_sync = xEventGroupCreate();
    if (! main_task_cfg.EG_trigger || ! main_task_cfg.EG_sync)
        status = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        
    // Create the queue that data collection tasks will use to communicate results
    main_task_cfg.results_queue = xQueueCreate(N_TASKS, sizeof(SensorData_t));
    if (! main_task_cfg.results_queue)
        status = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

    // Create the queue that the modem task will use to communicate progress
    main_task_cfg.modem_queue = xQueueCreate(1, sizeof(ModemMsg_t));
    if (! main_task_cfg.modem_queue)
        status = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

    // Create the data collection tasks that will retrieve sensor data
    int task_count = 0;
    for (int count=0; count<3; count++) {
        char name [20];
        sprintf (name, "ADC-%d", count +1);
        status = AddTask (TASK_ADC, name, &main_task_cfg, task_cfg, task_count, status);
        task_count += 1;
    }
    status = AddTask (TASK_ADC, "ADC-MAINS", &main_task_cfg, task_cfg, task_count, status);
    task_count += 1;
    for (int count=0; count<5; count++) {
        char name [20];
        sprintf (name, "DHT22-%d", count +1);
        status = AddTask (TASK_DHT22, name, &main_task_cfg, task_cfg, task_count, status);
        task_count += 1;
    }

    // Create the task which manages the modem
    static ModemTaskCfg_t modem_task_cfg;
    modem_task_cfg.msg_queue = main_task_cfg.modem_queue;
    if (status == pdPASS)
        status = xTaskCreate (ModemTask, "Modem", MODEM_TASK_STACK_SIZE, (void *) &modem_task_cfg, 
                              MODEM_TASK_PRIO, &(main_task_cfg.modem_task));
    
    // Create the main task which controls the data collection tasks
    if (status == pdPASS)
        status = xTaskCreate (MainTask, "Main", MAIN_TASK_STACK_SIZE, (void *) &main_task_cfg, 
                              MAIN_TASK_PRIO, NULL);
    
    // Start FreeRTOS scheduler - this starts all the tasks
    if (status == pdPASS)
        vTaskStartScheduler();

    // Error handler - if all is working this code should never be reached
    BlinkLEDForever (LED_INIT_ERR, pdFALSE);
}

/**
 * @brief add a data collection task to the FreeRTOS scheduler
 * @param type the type of task
 * @param name a name for the task
 * @param main_task_cfg the main task configuration which contains the event groups and queues
 *        used by the sensor tasks - the contents are modified by this function
 * @param params the parameters used to configure the task - the contents are modified by this function
 * @param task_count a unique count value for this task
 * @param status the current status of the initialization - the contents may be modified by this function
 * @return the initialization status
 */
static BaseType_t AddTask (TaskType_t type, char *name, MainTaskCfg_t *main_task_cfg, TaskCfg_t *task_cfg, int task_count, BaseType_t status) {
    if (status == pdPASS) {
        TaskCfg_t *params = task_cfg + task_count;
        params->type = type;
        strncpy (params->name, name, sizeof(params->name) -1);
        params->name[sizeof (params->name) -1] = '\0';
        params->EG_trigger = main_task_cfg->EG_trigger;
        params->EG_sync = main_task_cfg->EG_sync;
        params->EG_bitmask = 1 << task_count;
        params->results_queue = main_task_cfg->results_queue;

        TaskFunction_t function;
        switch (type) {
            case TASK_ADC: function = ADCTask; break;
            case TASK_DHT22: function = DHT22Task; break;
            default: return pdFAIL;
        }
        status = xTaskCreate(function, params->name, DC_TASK_STACK_SIZE, (void *) params, DC_TASK_PRIO, &main_task_cfg->tasks[task_count]);
    }
    return status;
}

/**
 * @brief the FreeRTOS task that controls all other tasks
 * @param pvParameters parameters passed to the task in the form of a MainTaskCfg_t structure
 */
static void MainTask(void *pvParameters) {
    configASSERT (pvParameters != NULL);
    
    int n_results;
    SensorData_t data [N_TASKS];

    MainTaskCfg_t *main_task_cfg = (MainTaskCfg_t *) pvParameters;
    configASSERT (main_task_cfg->EG_sync != NULL);
    configASSERT (main_task_cfg->EG_trigger != NULL);
    configASSERT (main_task_cfg->modem_queue != NULL);
    configASSERT (main_task_cfg->modem_task != NULL);
    configASSERT (main_task_cfg->results_queue != NULL);
    configASSERT (main_task_cfg->tasks != NULL);

#if DEBUG >= 1
    printf ("Main task starting\r\n");
#endif

    for (;;) {
        // Clear previous completion bits
        xEventGroupClearBits(main_task_cfg->EG_sync, ALL_TASKS_BITMASK);

        // Trigger the modem task
        xTaskNotifyGive(main_task_cfg->modem_task);

        // Wait for the modem to be ready
        ModemMsg_t modem_msg;
        if (xQueueReceive(main_task_cfg->modem_queue, &modem_msg, pdMS_TO_TICKS(20000)) == pdPASS) {
            switch (modem_msg.type) {
                case MODEM_TIME:
#if DEBUG >= 2
                    printf ("Modem intialisation succeeded\r\n");
#endif
                    break;
                case MODEM_FAIL:
#if DEBUG >= 2
                    printf ("Modem intialisation failed: %d\r\n", modem_msg.subtype);
#endif
                    break;
                default:
#if DEBUG >= 2
                    printf ("Modem unxepected response: %d\r\n", modem_msg.type);
#endif
                    break;
            }
        } else {
#if DEBUG >= 2
            printf ("Modem initialisation timed out\r\n");
#endif
        }

        // Trigger all data collection tasks simultaneously
        xEventGroupSetBits(main_task_cfg->EG_trigger, ALL_TASKS_BITMASK);

        // Wait until all data collection tasks have completed or timed out
        EventBits_t uxBits = xEventGroupWaitBits(
            main_task_cfg->EG_sync,
            ALL_TASKS_BITMASK,
            pdTRUE,                     // Clear bits on exit
            pdTRUE,                     // Wait for ALL bits
            pdMS_TO_TICKS(TASK_TIMEOUT) // Timeout limit
        );

        // Reset trigger bits for next iteration
        xEventGroupClearBits(main_task_cfg->EG_trigger, ALL_TASKS_BITMASK);

        // Kill any tasks that have not completed
        if ((uxBits & ALL_TASKS_BITMASK) == ALL_TASKS_BITMASK) {
            // TODO
        }
        
        // Collect all data points from the queue
        n_results = 0;
        while (xQueueReceive(main_task_cfg->results_queue, &data[n_results], 0) == pdTRUE) {
#if DEBUG >= 1
            printf ("Result %d: %d %ld %d %f %f\r\n",
                    n_results,
                    data[n_results].type, data[n_results].id, data[n_results].status,
                    data[n_results].value, data[n_results].value2);
#endif
            n_results += 1;
        }
        if (n_results != N_TASKS) {
            // TODO - incorrect number of results
        }

        // Transmit via LTE Modem
        // TODO

        // show loop completed
#if DEBUG >= 1
        BlinkLED(LED_SUCCESS, pdTRUE);
#endif

        // shutdown hardware
        ShutdownHardware ();

        // Sleep until next scheduled sampling cycle
        // TODO - convert this call to code that puts the processor to sleep
        vTaskDelay(pdMS_TO_TICKS(COLLECTION_INTERVAL));
    }
}

/**
 * @brief Common hardware initialisation needed by all sub systems
 * 
 * Call this function at startup or after waking from sleep
 */
static void InitHardware (void) {
    /* initialise STM HAL library */
    HAL_Init ();

    /* Update SystemCoreClock variable according to RCC clock registers */
    SystemCoreClockUpdate();
    
    /* Enable CP10 and CP11 to ensure full access for hardware Floating Point Unit */
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));

    /* Enable FPU automatic state preservation and lazy stacking (ASPEN | LSPEN) */
    FPU->FPCCR |= (FPU_FPCCR_ASPEN | FPU_FPCCR_LSPEN);
    
    // Enable GPIOB peripheral clock (Bit 1 in RCC AHB2ENR)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    // Enable AHB2 clock access for GPIOA and GPIOB peripherals
    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);

    
    // TODO: Enable all GPIO registers
    // TODO: Enable ADCs
}

/**
 * @brief Common hardware shutdown needed by all sub systems
 * 
 * Call this function before sleeping
 */
static void ShutdownHardware (void) {
    // TODO
}


