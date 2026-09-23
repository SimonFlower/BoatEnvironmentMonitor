/* take readings from ADCs */

#include "task_comms.h"
#include "debug.h"
#include "adc.h"

/**
 * @brief task that reads Sensor data from an ADC
 * @param pvParameters configuration parameters for the task in the form of a TaskCfg_t structure
 */
void adcTask(void *pvParameters) {

    TaskCfg_t *task_cfg = (TaskCfg_t *) pvParameters;
    
    for (;;) {
        // Wait for trigger
        xEventGroupWaitBits (task_cfg->EG_trigger, task_cfg->EG_bitmask, pdTRUE, pdTRUE, portMAX_DELAY);    
    
        // TODO - need to reinitialise hardware after sleep?
    
        // TODO - get a sample from the ADC - need to work out whether multiple ADCs can convert simultaneously
        
        // Fill out and post the sensor data on the queue
        SensorData_t data;
        data.type = task_cfg->type;
        data.id = task_cfg->EG_bitmask;
        data.status = COMPLETED_OK;         // TODO
        data.timestamp = 0;                 // TODO
        data.value = 0.0;                   // TODO
        data.value2 = 0.0;                  // TODO
        xQueueSend(task_cfg->results_queue, &data, portMAX_DELAY);
        
        // Tell main task that we are done
        xEventGroupSetBits(task_cfg->EG_sync, task_cfg->EG_bitmask);        
    }
}


