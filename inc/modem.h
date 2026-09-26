#ifndef MODEM_H
#define MODEM_H

#include "queue.h"

// an enumeration for the type of message sent by the modem controller
typedef enum {MODEM_TIME, MODEM_FAIL } ModemMsgType_t;

// an enumeration for the possible modem failure modes
typedef enum {
    MODEM_FAIL_INIT, 
    MODEM_FAIL_BAD_RESPONSE,
    MODEM_FAIL_TIMEOUT
} ModemFailType_t;

// This structure is used to configure the modem task
typedef struct {
    // The queue used to pass status information from the modem back to the main task
    QueueHandle_t msg_queue;
} ModemTaskCfg_t;

// This structure is used to pass messages from the modem controller to the main task
typedef struct {
    ModemMsgType_t type;        // the type of message
    ModemFailType_t subtype;    // the type of failure (for a MODEM_FAIL message)
    // TODO
} ModemMsg_t;

void ModemTask(void *pvParameters);

#endif /* MODEM_H */
