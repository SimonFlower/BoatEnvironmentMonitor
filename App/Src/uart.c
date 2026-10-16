/** Manage transmitting and receiving UART data.
 * 
 * Transmision uses simple polling.
 * 
 * Receving uses IRQ and DMA. If there is an error with HAL's UART handling, 
 * the uart's dma_transfer_up flag will be cleared. Users can check this flag 
 * via the UARTIsDmaUp() call.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "user_io.h"
#include "ring.h"
#include "uart.h"

#define DMA_BUFFER_SIZE	256

// A structure used to hold the current state of a UART
typedef struct { 
	char *name;                           // A name for the UART
	UART_HandleTypeDef *huart;            // HALs UART state
	volatile bool dma_transfers_up;       // A flag showing whether DMA transfers have been set up
    volatile bool huart_error_flag;       // A flag showing whether there has been a UART error
    uint32_t huart_error_code;            // The corresponding error code
	uint8_t *dma_rx_buffer;	              // The buffer used to pull data out of DMA
    volatile uint16_t old_dma_pos;        // Tracks last processed DMA position
	RingBuffer_t ring_buffer;	          // The ring buffer used to transfer data to the user
} UARTState_t;

// A UART state structure for each UART that this module controls
static UARTState_t uart_state [N_UARTS] = { 0 };

// A DMA buffer for received data from each UART that this module controls
static uint8_t dma_buffer[N_UARTS][DMA_BUFFER_SIZE];

// private forward declarations
static UARTState_t *getUartPtr (UartID_t id);
static UART_HandleTypeDef *getHuart (UartID_t id);
static uint8_t *getDMARxBuffer (UartID_t id);
static char *getUARTName (UartID_t id);
static UARTState_t *findUart (UART_HandleTypeDef *huart);

/**
 * @brief initialise a UART data structure
 * 
 * This function must be called once before any other function
 * in the module uses the UART
 * 
 * @param id the UART to initialise
 * @param rb_storage storage for the riung buffer
 * @param size the length of the ring buffer storage
 * @retval true if the UART data structure was initialised
 */
bool UARTInit (UartID_t id, uint8_t *rb_storage, size_t rb_size) {
	bool status = true;
	
	UARTState_t *uart_ptr = getUartPtr (id);
	UART_HandleTypeDef *huart = getHuart (id);
	uint8_t *dma_rx_buffer = getDMARxBuffer (id);
	char *name = getUARTName (id);
	if (! uart_ptr || ! huart || ! dma_rx_buffer || ! name)
		status = false;
	
	if (status) {
		uart_ptr->name = name;
		uart_ptr->huart = huart;
		uart_ptr->dma_rx_buffer = dma_rx_buffer;
		uart_ptr->dma_transfers_up = false;
		uart_ptr->huart_error_flag = false;
		uart_ptr->old_dma_pos = 0;
		RingBufferInit (&(uart_ptr->ring_buffer), rb_storage, rb_size);
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
	UARTState_t *uart_ptr = getUartPtr (id);
	if (! uart_ptr)
		return false;

	bool status = true;
	uart_ptr->huart_error_flag = false;

	if (! uart_ptr->dma_transfers_up) {
		// Clear the DMA transfer ring buffer
		RingBufferClear (&(uart_ptr->ring_buffer));
		uart_ptr->old_dma_pos = 0;

		// Initialize UART peripheral state then start DMA
		if (HAL_UART_DeInit(uart_ptr->huart) != HAL_OK) {
			status = false;
		} else if (HAL_UART_Init(uart_ptr->huart) != HAL_OK) {
			status = false;
		} else if (HAL_UARTEx_ReceiveToIdle_DMA(uart_ptr->huart, uart_ptr->dma_rx_buffer, DMA_BUFFER_SIZE) != HAL_OK) {
			status = false;
		} else {
			__HAL_DMA_DISABLE_IT(uart_ptr->huart->hdmarx, DMA_IT_HT);
			uart_ptr->dma_transfers_up = true;
		}
	}

	DebugPrint (4, "DEBUG: %s %s\r\n", uart_ptr->name, status ? "initialised" : "*not* initialised");

	return status;
}

/**
 * @brief close communications with the UART
 * 
 * @param id the UART to stop
 * @retval true if communications closed OK
 */
bool UARTStop (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id);
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

	DebugPrint (4, "DEBUG: %s %s\r\n", uart_ptr->name, status ? "shutdown" : "*not* shutdown");

	return status;	
}

/**
 * @brief check whether DMA transfers are up
 * 
 * DMA transfers will be up after a succesful call to UARTStart, but
 * could be brought down by a later error. If this call returns
 * false, it's neccessary to call UARTStart again to restart DMA transers.
 * 
 * @param id the UART to check
 * @retval true if DMA transfers as running
 */
bool UARTIsDMAUp (UartID_t id) {
	UARTState_t *uart_ptr = getUartPtr (id);
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
 * @brief send data to the UART
 * @param id the UART to send data to
 * @param buffer the data to send
 * @param len the unumber of bytes to send
 * @param tineout the amount of time to wait for tranmission to complete (in mS)
 * @retval true if the data was transmitted OK
 */
bool UARTTransmit (UartID_t id, const char *buffer, int len, int timeout) {
	UARTState_t *uart_ptr = getUartPtr (id);
	if (! uart_ptr)
		return false;

	bool status = true;
	
	if (len > 0) {
		if (buffer == NULL || len > UINT16_MAX) {
			status = false;
		} else {
			if (HAL_UART_Transmit(uart_ptr->huart, (uint8_t *) buffer, (uint16_t) len, timeout) == HAL_OK) {
				// don't dump the STLINK UART or it will recurse infintely
				if (id != UART_STLINK)
					DebugDump (4, "DEBUG: %s Tx", buffer, len, uart_ptr->name);
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
	UARTState_t *uart_ptr = getUartPtr (id);
	if (! uart_ptr)
		return false;
		
	if (RingBufferGet (&(uart_ptr->ring_buffer), byte)) {
		// don't dump the STLINK UART
		if (id != UART_STLINK)
			DebugDump (4, "DEBUG: %s Rx", (char *) &byte, 1, uart_ptr->name);
	}
	
	return false;
}


/**
 * @brief receive a newline terminated string from the UART
 *
 * The string is always null terminated
 * 
 * @param id the UART to receive data from
 * @param line the line, including its terminating newline
 * @param size the size of the line which must be at least 2 bytes
 * @retval true if a line was received
 */
bool UARTReceiveLine (UartID_t id, char *line, size_t size) {
	UARTState_t *uart_ptr = getUartPtr (id);
	if (! uart_ptr)
		return false;
		
	if (RingBufferGetLine (&(uart_ptr->ring_buffer), line, size, true) == RBGL_OK) {
		// don't dump the STLINK UART
		if (id != UART_STLINK)
			DebugDump (4, "DEBUG: %s Rx", line, size, uart_ptr->name);
		return true;
	}

	return false;
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
        uint16_t new_bytes = 0;

        // Calculate how many NEW bytes arrived since the last callback
        if (n_bytes_rx >= uart_ptr->old_dma_pos) {
            new_bytes = n_bytes_rx - uart_ptr->old_dma_pos;
        } else {
            // DMA wrapped around to 0
            new_bytes = (DMA_BUFFER_SIZE - uart_ptr->old_dma_pos) + n_bytes_rx;
        }

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
 * Clears error flags and restarts circular DMA reception for the modem.
 *
 * @param huart Pointer to the UART handle triggering the error
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	UARTState_t *uart_ptr = findUart (huart);
    if (uart_ptr) {
		uart_ptr->huart_error_flag = true;
		uart_ptr->huart_error_code = huart->ErrorCode;

		// Clear all UART Error Flags in ICR register
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
        
        // Reset DMA position tracker
        uart_ptr->old_dma_pos = 0;

		// Clear the ring buffer
		RingBufferClear(&(uart_ptr->ring_buffer));

        // Flag the DMA transfers are down
		uart_ptr->dma_transfers_up = false;
    }
}

static UARTState_t *getUartPtr (UartID_t id) {
	switch (id) {
		case UART_STLINK:
			return &uart_state[0];
		case UART_MODEM:
			return &uart_state[1];
		default:
			return (UARTState_t *) 0;
	}
}
static UART_HandleTypeDef *getHuart (UartID_t id) {
	// UART handles are defined in CubeMX/Core/Src/usart.c
	// CubeMX sets up the UART configuration
	// Handle for the UART connecting to the modem
	extern UART_HandleTypeDef huart1;
	// Handle for the UART connecting to ST-LINK
	extern UART_HandleTypeDef huart2;

	switch (id) {
		case UART_STLINK:
			return &huart1;
		case UART_MODEM:
			return &huart2;
		default:
			return (UART_HandleTypeDef *) 0;
	}
}
static uint8_t *getDMARxBuffer (UartID_t id) {
	switch (id) {
		case UART_STLINK:
			return dma_buffer[0];
		case UART_MODEM:
			return dma_buffer[1];
		default:
			return (uint8_t *) 0;
	}
}
static char *getUARTName (UartID_t id) {
	switch (id) {
		case UART_STLINK:
			return "STLINK-UART";
		case UART_MODEM:
			return "MODEM-UART";
		default:
			return (char *) 0;
	}
}
static UARTState_t *findUart (UART_HandleTypeDef *huart) {
	for (int count=0; count<N_UARTS; count ++) {
		if (huart == uart_state[count].huart)
			return &(uart_state[count]);
	}
	return (UARTState_t *) 0;
}
