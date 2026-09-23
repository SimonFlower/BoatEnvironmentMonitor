/* Structures used for communication between FreeRTOS tasks */
#ifndef TASK_COMMS_H
#define TASK_COMMS_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"

// An identifier for the type of data collection task
typedef enum {TASK_ADC, TASK_DHT22} TaskType_t;

// A code for the completion status of a data collection task
typedef enum {COMPLETED_OK, COMPLETED_FAIL, COMPLETED_INTERRUPTED} TaskCompletion_t;

// This structure is used to configure data collection tasks
typedef struct {
    // The type of task (ADC or DHT22)
    TaskType_t type;
    // A name for the task
    char name [20];
    // An event group used to start a data collection operation
    // This is *read* by the task
    EventGroupHandle_t EG_trigger;
    // An event group used to signal data collection completed
    // This is *written* by the task
    EventGroupHandle_t EG_sync;
    // The bit used for this task in the two event groups
    // This also servers as an identifier for the task
    EventBits_t EG_bitmask;
    // The queue used to pass sensor readings back to the main task
    QueueHandle_t results_queue;
    // Task type specific configuration...
    union {
        struct {
            // TODO: parameters that specify which ADC to use and possibly metadata for the data as well
        } ADC_params;
        struct {
            // TODO: parameters that specify which DHT to use and possibly metadata for the data as well
        } DHT22_params;
    } task_specific;
} TaskCfg_t;

// A structure that is filled by data collection tasks to pass a reading back to the main task
typedef struct {
    // These members allow the task to be identified
    TaskType_t type;
    EventBits_t id;
    // The completion status
    TaskCompletion_t status;
    // The value(s) for the reading - only valid when status == COMPLETED_OK
    float value, value2;
} SensorData_t;

#endif /* TASK_COMMS_H */
