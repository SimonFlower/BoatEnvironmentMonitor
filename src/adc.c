/* a data collection task to take readings from ADCs */

#include "task_comms.h"
#include "debug.h"
#include "adc.h"

/**
 * @brief task that reads Sensor data from an ADC
 * @param pvParameters configuration parameters for the task in the form of a TaskCfg_t structure
 */
void ADCTask(void *pvParameters) {
    configASSERT (pvParameters != NULL);

    TaskCfg_t *task_cfg = (TaskCfg_t *) pvParameters;
    configASSERT (task_cfg->EG_trigger != NULL);
    configASSERT (task_cfg->EG_sync != NULL);
    configASSERT (task_cfg->results_queue != NULL);
    configASSERT (task_cfg->EG_bitmask != 0);
    
    for (;;) {
        // Wait for trigger
        xEventGroupWaitBits (task_cfg->EG_trigger, task_cfg->EG_bitmask, pdTRUE, pdTRUE, portMAX_DELAY);    
    
        // TODO - need to reinitialise hardware after sleep?
    
        // TODO - get a sample from the ADC - need to work out whether multiple ADCs can convert simultaneously
        
        // Fill out and post the sensor data on the queue
        SensorData_t data;
        data.type = task_cfg->type;
        data.id = task_cfg->EG_bitmask;
        data.status = COMPLETED_OK;          // TODO
        data.value = (float) data.id;        // TODO
        data.value2 = (float) data.id * 2.0; // TODO
        xQueueSend(task_cfg->results_queue, &data, portMAX_DELAY);
        
        // Tell main task that we are done
        xEventGroupSetBits(task_cfg->EG_sync, task_cfg->EG_bitmask);        
    }
}


