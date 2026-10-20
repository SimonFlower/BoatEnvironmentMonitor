/** Manage transmitting and receiving UART data.
 * 
 * Transmission uses simple polling.
 * 
 * Receiving uses IRQ and DMA. If there is an error with HAL's UART handling, 
 * the uart's dma_transfer_up flag will be cleared. Users can check this flag 
 * via the UARTIsDMAUp() call.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "user_io.h"
#include "ring.h"
#include "uart.h"

#define DMA_BUFFER_SIZE	512

// A structure used to hold the current state of a UART
typedef struct { 
	const char *name;                     // A name for the UART
	UART_HandleTypeDef *huart;            // HAL's UART state
	volatile bool dma_transfers_up;       // A flag showing whether DMA transfers have been set up
    volatile bool huart_error_flag;       // A flag showing whether there has been a UART error
    volatile uint32_t huart_error_code;   // The corresponding error code
	uint8_t *dma_rx_buffer;	              // The buffer used to pull data out of DMA
    volatile uint16_t old_dma_pos;        // Tracks last processed DMA position
	RingBuffer_t ring_buffer;	          // The ring buffer used to transfer data to the user
	UARTDiagnostics_t diagnostics;		  // Diagnostic fields
} UARTState_t;

// A UART state structure for each UART that this module controls
static UARTState_t uart_state [N_UARTS] = { 0 };

// A DMA buffer for received data from each UART that this module controls
static uint8_t dma_buffer[N_UARTS][DMA_BUFFER_SIZE];

// private forward declarations
static UARTState_t *getUartPtr (UartID_t id, bool check_initialised);
static UART_HandleTypeDef *getHuart (UartID_t id);
static uint8_t *getDMARxBuffer (UartID_t id);
static const char *getUARTName (UartID_t id);
static UARTState_t *findUart (UART_HandleTypeDef *huart);

/**
 * @brief initialise a UART data structure
 * 
 * This function must be called once before any other function
 * in the module uses the UART
 * 
 * @param id the UART to initialise
 * @param rb_storage storage for the ring buffer
 * @param rb_size the length of the ring buffer storage
 * @retval true if the UART data structure was initialised (or had
 *         previously been initialised)
 */
bool UARTInit (UartID_t id, uint8_t *rb_storage, size_t rb_size) {
	bool status = true;

	// Gather information about the UART
	UARTState_t *uart_ptr = getUartPtr (id, false);
	UART_HandleTypeDef *huart = getHuart (id);
	uint8_t *dma_rx_buffer = getDMARxBuffer (id);
	const char *name = getUARTName (id);
	
	if (! uart_ptr || ! huart || ! dma_rx_buffer || ! name) {
		status = false;
	} else if (! uart_ptr->dma_transfers_up) {		// check the UART isn't being used
		// initialise the ring buffer
		RingBufferInit (&(uart_ptr->ring_buffer), rb_storage, rb_size);
		if (! RingBufferIsInit (&(uart_ptr->ring_buffer))) {
			status = false;
			memset (uart_ptr, 1, sizeof (UARTState_t));
		} else {
			uart_ptr->name = name;
			uart_ptr->huart = huart;
			uart_ptr->dma_rx_buffer = dma_rx_buffer;
			uart_ptr->dma_transfers_up = false;
			uart_ptr->huart_error_flag = false;
			uart_ptr->old_dma_pos = 0;
			uart_ptr->diagnostics.error_count = 0;
			uart_ptr->diagnostics.last_rx_size = 0;
			uart_ptr->diagnostics.rx_event_count = 0;
			uart_ptr->diagnostics.total_new_bytes = 0;
		}
	}
		
	return status;
} 

/**
 * @brief start up DMA transfer with the UART
 * 
 * @param id the UART to start
 * @retval true if communications opened OK
 */
bool UARTStart (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return false;

	bool status = true;
	uart_ptr->huart_error_flag = false;

	if (! uart_ptr->dma_transfers_up) {
		// Clear the DMA transfer ring buffer and the diagnostics
		RingBufferClear (&(uart_ptr->ring_buffer));
		uart_ptr->old_dma_pos = 0;
		uart_ptr->diagnostics.error_count = 0;
		uart_ptr->diagnostics.last_rx_size = 0;
		uart_ptr->diagnostics.rx_event_count = 0;
		uart_ptr->diagnostics.total_new_bytes = 0;

		// Initialize UART peripheral state then start DMA
		if (HAL_UART_DeInit(uart_ptr->huart) != HAL_OK) {
			status = false;
		} else if (HAL_UART_Init(uart_ptr->huart) != HAL_OK) {
			status = false;
		} else {
			uart_ptr->dma_transfers_up = true;
			if (HAL_UARTEx_ReceiveToIdle_DMA(uart_ptr->huart, uart_ptr->dma_rx_buffer, DMA_BUFFER_SIZE) != HAL_OK) {
				uart_ptr->dma_transfers_up = false;
				status = false;
			} else {
				// Check the actual DMA position
				uart_ptr->old_dma_pos = DMA_BUFFER_SIZE - (uint16_t) __HAL_DMA_GET_COUNTER (uart_ptr->huart->hdmarx);
				if (uart_ptr->old_dma_pos >= DMA_BUFFER_SIZE) uart_ptr->old_dma_pos = 0;
				// Re-clear the ring buffer
				RingBufferClear (&(uart_ptr->ring_buffer));
			}
		}
	}

	DebugPrint (4, true, true, "%s %s", uart_ptr->name, status ? "started" : "*not* started");

	return status;
}

/**
 * @brief close communications with the UART
 * 
 * @param id the UART to stop
 * @retval true if communications closed OK
 */
bool UARTStop (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return false;

	bool status = true;
	
	if (uart_ptr->dma_transfers_up) {
		if (HAL_UART_AbortReceive (uart_ptr->huart) != HAL_OK)
			status = false;
		else
			uart_ptr->dma_transfers_up = false;
	}
	if (status)
		status = HAL_UART_DeInit(uart_ptr->huart) == HAL_OK;

	DebugPrint (4, true, true, "%s %s", uart_ptr->name, status ? "stopped" : "*not* stopped");

	return status;	
}

/**
 * @brief check whether DMA transfers are up
 * 
 * DMA transfers will be up after a successful call to UARTStart, but
 * could be brought down by a later error. If this call returns
 * false, it's necessary to call UARTStart again to restart DMA transfers.
 * 
 * If the UART error flag is set, calling this routine clears it.
 * 
 * @param id the UART to check
 * @retval true if DMA transfers are running
 */
bool UARTIsDMAUp (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return false;

	// Check whether there has been an error and report it if there has
	if (uart_ptr->huart_error_flag) {
		printf("ERROR: UART %s error 0x%08lx\r\n", uart_ptr->name, uart_ptr->huart_error_code);
		uart_ptr->huart_error_flag = false;
	}
	return uart_ptr->dma_transfers_up;
}

/**
 * @brief get the number of overflows on the UART
 * @param id the UART to retrive the information for
 * @retval the number of overflows
 */
uint32_t UARTGetOverflowCount(UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return 0;
	return uart_ptr->ring_buffer.overflow_count;
}

/**
 * @brief send data to the UART
 * @param id the UART to send data to
 * @param buffer the data to send
 * @param len the number of bytes to send
 * @param timeout the amount of time to wait for transmission to complete (in mS)
 * @retval true if the data was transmitted OK
 */
bool UARTTransmit (UartID_t id, const char *buffer, int len, int timeout) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return false;

	bool status = true;
	if (len <= 0) {
		status = false;
	} else {
		if (buffer == NULL || len > UINT16_MAX) {
			status = false;
		} else {
			if (HAL_UART_Transmit(uart_ptr->huart, (uint8_t *) buffer, (uint16_t) len, timeout) == HAL_OK) {
				// don't dump the STLINK UART or it will recurse infinitely
				if (id != UART_STLINK)
					DebugDump (4, "%s Tx", buffer, len, uart_ptr->name);
			} else {
				status = false;
			}
		}
	}

	return status;
}

/**
 * @brief receive a byte from the UART
 * @param id the UART to receive data from
 * @param byte the received byte
 * @retval true if a byte was received
 */
bool UARTReceiveByte (UartID_t id, uint8_t *byte) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return false;
		
	if (RingBufferGet (&(uart_ptr->ring_buffer), byte)) {
		return true;
	}
	
	return false;
}


/**
 * @brief receive a newline terminated string from the UART
 *
 * The string is always null terminated, provided the ring buffer has
 * been initialised and the size of the supplied line buffer is at
 * least 1. If any data is available it will be returned. Data is 
 * never discarded. 
 * 
 * This function does not block - it returns whatever is available 
 * at the time of the call. More data may arrive after the call.
 * This can be retrieved using a while loop:
 * 
 *   buffer [0] = '\0';
 *   while (UARTReceiveLine(rb, 
 *                          buffer + strlen(buffer),
 *                          sizeof(buffer) - strlen(buffer)) == LINE_PARTIAL)
 *     ...
 * 
 * @param id the UART to receive data from
 * @param line the line, including its terminating newline - on repeated
 *        calls (as described in the example above) line will only be
 *        filled with new data
 * @param size the size of the line which must be at least 2 bytes
 * @retval LINE_OK a line, including newline terminator, was retrieved
 *         LINE_PARTIAL data was retrieved but no newline was found
 *         LINE_OVERFLOW data was retrieved but there is not enough
 *                       space to store it all - as much data as
 *                       possible has been retrieved - more will
 *                       be available on a subsequent call - the caller
 *                       may choose to retrieve more data or abandon the
 *                       attempt and clear the buffer
 *         LINE_NONE there was not data available
 */
LineRetrievalType_t UARTReceiveLine (UartID_t id, char *line, size_t size) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return LINE_NONE;

	// Get data from the ring buffer
	LineRetrievalType_t ret_val = RingBufferGetLine (&(uart_ptr->ring_buffer), line, size);
	// don't dump the STLINK UART
	if (ret_val != LINE_NONE && id != UART_STLINK)
		DebugDump (4, "%s Rx", line, strlen (line), uart_ptr->name);

	return ret_val;
}

/**
 * @brief Get the UART diagnotics
 * @param id the UART to get diagnostics on
 * @retval The diagnsotics OR NULL if there was an error
 */
UARTDiagnostics_t *UARTGetDiagnostics (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id, true);
	if (! uart_ptr)
		return NULL;
	return &(uart_ptr->diagnostics);
}

/**
 * @brief HAL DMA callback for uart data received via DMA
 * 
 * Received data is added to the uart's ring buffer.
 * 
 * @param huart the UART being used
 * @param n_bytes_rx the number of bytes received
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t n_bytes_rx) {
	UARTState_t *uart_ptr = findUart (huart);
    if (uart_ptr) {
		// Where has the DMA actually written up to? Recalculate n_bytes_rx
		// since it is not trustworthy on TC events; use the DMA counter instead
		uint16_t n_bytes_rx = DMA_BUFFER_SIZE - (uint16_t) __HAL_DMA_GET_COUNTER (huart->hdmarx);
		if (n_bytes_rx >= DMA_BUFFER_SIZE)
			n_bytes_rx = 0;
		
        // Calculate how many NEW bytes arrived since the last callback
        uint16_t new_bytes = 0;
        if (n_bytes_rx >= uart_ptr->old_dma_pos) {
            new_bytes = n_bytes_rx - uart_ptr->old_dma_pos;
        } else {
            // DMA wrapped around to 0
            new_bytes = (DMA_BUFFER_SIZE - uart_ptr->old_dma_pos) + n_bytes_rx;
        }
        
        // Update diagnostics
        uart_ptr->diagnostics.rx_event_count += 1;
        uart_ptr->diagnostics.last_rx_size = n_bytes_rx;
        uart_ptr->diagnostics.total_new_bytes += new_bytes;

        // Copy new bytes into ring buffer
        for (uint16_t i = 0; i < new_bytes; i++) {
			// Ignore the return value from RingBufferPut - if there's
			// a problem, the ring buffer's overflow counter will be
			// incremented
			RingBufferPut(&(uart_ptr->ring_buffer), (char) *(uart_ptr->dma_rx_buffer + uart_ptr->old_dma_pos));
            uart_ptr->old_dma_pos = (uart_ptr->old_dma_pos + 1) % DMA_BUFFER_SIZE;
        }
    }
}

/**
 * @brief HAL UART Error Callback
 * 
 * Called by the HAL when a UART transmission/reception error occurs.
 * Clears error flags and restarts circular DMA reception for the UART.
 *
 * @param huart Pointer to the UART handle triggering the error
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	UARTState_t *uart_ptr = findUart (huart);
    if (uart_ptr) {
		uart_ptr->diagnostics.error_count += 1;
		uart_ptr->huart_error_flag = true;
		uart_ptr->huart_error_code = huart->ErrorCode;

		// Clear all UART Error Flags in ICR register
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
        
        // Flag that DMA transfers are down - the DMA and ring buffers will
        // be reset if/when the UART is restarted
		uart_ptr->dma_transfers_up = false;
    }
}

// routines to access the uart_state "database"
static UARTState_t *getUartPtr (UartID_t id, bool check_initialised) {
	int idx;

	// find UART
	switch (id) {
		case UART_MODEM: idx = 0; break;
		case UART_STLINK: idx = 1; break;
		default: idx = -1; break;
	}

	// check UART has been initialised
	if (idx >= 0 && check_initialised && (! uart_state[idx].huart))
		idx = -1;

	if (idx < 0)
		return (UARTState_t *) 0;
	return &uart_state[idx];
}
static UART_HandleTypeDef *getHuart (UartID_t id) {
	// UART handles are defined in CubeMX/Core/Src/usart.c
	// CubeMX sets up the UART configuration
	// Handle for the UART connecting to the modem
	extern UART_HandleTypeDef huart1;
	// Handle for the UART connecting to ST-LINK
	extern UART_HandleTypeDef huart2;

	switch (id) {
		case UART_MODEM:
			return &huart1;
		case UART_STLINK:
			return &huart2;
		default:
			return (UART_HandleTypeDef *) 0;
	}
}
static uint8_t *getDMARxBuffer (UartID_t id) {
	switch (id) {
		case UART_MODEM:
			return dma_buffer[0];
		case UART_STLINK:
			return dma_buffer[1];
		default:
			return (uint8_t *) 0;
	}
}
static const char *getUARTName (UartID_t id) {
	switch (id) {
		case UART_MODEM:
			return "MODEM-UART";
		case UART_STLINK:
			return "STLINK-UART";
		default:
			return (const char *) 0;
	}
}
static UARTState_t *findUart (UART_HandleTypeDef *huart) {
	for (int count=0; count<N_UARTS; count ++) {
		if (huart == uart_state[count].huart) {
			return &(uart_state[count]);
		}
	}
	return (UARTState_t *) 0;
}
