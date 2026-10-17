/** Manage transmitting and receiving UART data. */

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ring.h"

// enumerate the UARTs that this module controls
typedef enum {UART_MODEM, UART_STLINK, N_UARTS} UartID_t;

// forward declarations
bool UARTInit (UartID_t id, uint8_t *rb_storage, size_t rb_size);
bool UARTStart (UartID_t id);
bool UARTStop (UartID_t id);
bool UARTIsDMAUp (UartID_t id);
uint32_t UARTGetOverflowCount(UartID_t id);
bool UARTTransmit (UartID_t id, const char *buffer, int len, int timeout);
bool UARTReceiveByte (UartID_t id, uint8_t *byte);
LineRetrievalType_t UARTReceiveLine (UartID_t id, char *line, size_t size);

#endif /* UART_H */
