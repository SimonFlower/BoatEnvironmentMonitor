#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "stm32l432xx.h"

#include "task_comms.h"
#include "led.h"
#include "adc.h"
#include "dht22.h"
#include "debug.h"

// definitions for the numbers of tasks
#define N_ADC_TASKS         4U
#define N_DHT22_TASKS       5U
#define N_TASKS             (N_ADC_TASKS + N_DHT22_TASKS)
#define ALL_TASKS_BITMASK   ((1U << N_TASKS) - 1U)

// timeout (in mS) for the tasks to complete
#define TASK_TIMEOUT        10000

// time between collections (in mS)
#define COLLECTION_INTERVAL 60000

// a structure that allows multiple parameters to be passed to the main task
typedef struct {
    // An event group used to start a data collection operation
    // This is *written* by the main task
    EventGroupHandle_t EG_trigger;
    // An event group used to signal data collection completed
    // This is *read* by the main task
    EventGroupHandle_t EG_sync;
    // The queue used to pass sensor readings back to the main task
    QueueHandle_t results_queue;
    // A list of the data collection tasks
    TaskHandle_t tasks [N_TASKS];
} MainTaskCfg_t;

// forward declarations
BaseType_t addTask (TaskType_t type, char *name, MainTaskCfg_t *main_task_cfg, TaskCfg_t *task_cfg, int task_count, BaseType_t status);
void mainTask(void *pvParameters);
void initHardware (void);
void shutdownHardware (void);
void HardFault_Handler(void);

int main(void) {

#ifdef DEBUG
    // Disable stdout buffering so characters print immediately without needing '\n'
    setvbuf(stdout, NULL, _IONBF, 0);
    
    printf ("Boat Environment Monitor starting\n");
#endif

    // The structure used to configure the main task
    static MainTaskCfg_t main_task_cfg;
    
    // The structures used to configure the data collection tasks
    static TaskCfg_t task_cfg [N_TASKS];

    // This variable is used to track correct initialization
    BaseType_t status = pdPASS;
    
    // Create the event groups used to trigger and synchronize tasks
    // Note that FreeRTOS creates the group with all bits cleared
    main_task_cfg.EG_trigger = xEventGroupCreate();
    main_task_cfg.EG_sync = xEventGroupCreate();
    if (! main_task_cfg.EG_trigger || ! main_task_cfg.EG_sync)
        status = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        
    // Create the queue that sub systems will use to communicate results
    main_task_cfg.results_queue = xQueueCreate(N_TASKS, sizeof(SensorData_t));
    if (! main_task_cfg.results_queue)
        status = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

    // Create the tasks that will retrieve sensor data
    int task_count = 0;
    for (int count=0; count<3; count++) {
        char name [20];
        sprintf (name, "ADC-%d", count +1);
        status = addTask (TASK_ADC, name, &main_task_cfg, task_cfg, task_count, status);
        task_count += 1;
    }
    status = addTask (TASK_ADC, "ADC-MAINS", &main_task_cfg, task_cfg, task_count, status);
    task_count += 1;
    for (int count=0; count<5; count++) {
        char name [20];
        sprintf (name, "DHT22-%d", count +1);
        status = addTask (TASK_DHT22, name, &main_task_cfg, task_cfg, task_count, status);
        task_count += 1;
    }
    
    // Create the controlling task
    if (status == pdPASS)
        status = xTaskCreate(mainTask, "Main", configMINIMAL_STACK_SIZE, (void *) &main_task_cfg, 1, NULL);
    
    // Start FreeRTOS scheduler
    if (status == pdPASS)
        vTaskStartScheduler();

    // Error handler
    initHardware ();
    blinkLEDForever (LED_INIT_ERR);
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
BaseType_t addTask (TaskType_t type, char *name, MainTaskCfg_t *main_task_cfg, TaskCfg_t *task_cfg, int task_count, BaseType_t status) {
    if (status == pdPASS) {
        TaskCfg_t *params = task_cfg + task_count;
        params->type = type;
        strcpy (params->name, name);
        params->EG_trigger = main_task_cfg->EG_trigger;
        params->EG_sync = main_task_cfg->EG_sync;
        params->EG_bitmask = 1 << task_count;
        params->results_queue = main_task_cfg->results_queue;

        TaskFunction_t function;
        switch (type) {
            case TASK_ADC: function = adcTask; break;
            case TASK_DHT22: function = dht22Task; break;
            default: return pdFAIL;
        }
        status = xTaskCreate(function, params->name, configMINIMAL_STACK_SIZE, (void *) params, 1, &main_task_cfg->tasks[task_count]);
    }
    return status;
}

/**
 * @brief the FreeRTOS task that controls all other tasks
 * @param pvParameters parameters passed to the task in the form of a MainTaskCfg_t structure
 */
void mainTask(void *pvParameters) {

    int n_results;
    SensorData_t data [N_TASKS];

    MainTaskCfg_t *main_task_cfg = (MainTaskCfg_t *) pvParameters;

#ifdef DEBUG
    printf ("Boat Environment Monitor main task starting\n");
#endif

    for (;;) {
        // common hardware initialisation
        initHardware ();

        // Clear previous completion bits
        xEventGroupClearBits(main_task_cfg->EG_sync, ALL_TASKS_BITMASK);

        // Trigger all sensor tasks simultaneously
        xEventGroupSetBits(main_task_cfg->EG_trigger, ALL_TASKS_BITMASK);

        // Wait until ALL worker tasks have reported done or timeout
        EventBits_t uxBits = xEventGroupWaitBits(
            main_task_cfg->EG_sync,
            ALL_TASKS_BITMASK,
            pdTRUE,                     // Clear bits on exit
            pdTRUE,                     // Wait for ALL bits
            pdMS_TO_TICKS(TASK_TIMEOUT) // Timeout limit
        );

        // Reset trigger bit for next iteration
        xEventGroupClearBits(main_task_cfg->EG_trigger, ALL_TASKS_BITMASK);

        // Kill any tasks that have not completed
        if ((uxBits & ALL_TASKS_BITMASK) == ALL_TASKS_BITMASK) {
            // TODO
        }
        
        // Collect all data points from the queue
        n_results = 0;
        while (xQueueReceive(main_task_cfg->results_queue, &data[n_results], 0) == pdTRUE) {
#ifdef DEBUG
            printf ("Result %d: %d %ld %d %lu %f %f\n",
                    n_results,
                    data[n_results].type, data[n_results].id, data[n_results].status,
                    data[n_results].timestamp, data[n_results].value, data[n_results].value2);
#endif
            n_results += 1;
        }
        if (n_results != N_TASKS) {
            // TODO - incorrect number of results
        }

        // Transmit via LTE Modem
        // TODO

        // show loop completed
#ifdef DEBUG
        blinkLED(LED_SUCCESS);
#endif

        // shutdown hardware
        shutdownHardware ();

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
void initHardware (void) {
    // Enable GPIOB peripheral clock (Bit 1 in RCC AHB2ENR)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    
    // TODO: Enable all GPIO registers
    // TODO: Enable ADCs
}

/**
 * @brief Common hardware shutdown needed by all sub systems
 * 
 * Call this function before sleeping
 */
void shutdownHardware (void) {
    // TODO
}

/**
 * @brief Hard fault handler for debugging */
void HardFault_Handler(void) {
    initHardware ();
    blinkLEDForever (LED_HARD_FAULT);
}

