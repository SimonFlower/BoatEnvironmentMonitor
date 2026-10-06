/** Low level functions for managing the Clipper LGE 4G modem
 * 
 * Two sets of functions exist for each modem operation that sends and
 * receives data. Those with the suffix "DT" use the default timeout
 * values specified when calling ModemInit(). This without the suffix
 * must supply explicit timeout values
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "debug.h"
#include "ring.h"
#include "modem_ll.h"
#include "utils.h"

// some timing contants
#define PWRKEY_ON_PULSE_WIDTH	100
#define PWRKEY_OFF_PULSE_WIDTH	3000
#define PWRKEY_TIME_TILL_ON		6000
#define PWRKEY_TIME_TILL_OFF	4000
#define PWRKEY_IDLE_CHECK		500
#define RESET_PULSE_WIDTH		2500
#define RESET_TIME_TILL_READY	3000
#define RESET_IDLE_CHECK		500

// the maximum length of lines sent by ModemSendCmd() and received by ModemExpect()
#define MODEM_TX_MAX_LEN 		256
#define MODEM_RX_MAX_LEN		256

// transmit and receive buffers for sending commands to the modem
// and receiving responses from it
static char tx_buffer [MODEM_TX_MAX_LEN];
static char rx_buffer [MODEM_RX_MAX_LEN];
static char rx_stored_buffer [MODEM_RX_MAX_LEN];

// default timeout values (in mS)
uint32_t default_wfi_timeout;
uint32_t default_tx_timeout;
uint32_t default_rx_timeout;

// Defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart1;

// A flag showing whether the Modem has been initialised
static volatile bool modem_initialised = false;
static volatile bool dma_transfers_up = false;

// The ring buffer used to receive data from DMA
static RingBuffer_t ring_buffer;

// The DMA receive buffer for data sent from the modem
// This is *only* to be used by the DMA callback
#define DMA_BUFFER_SIZE	256
static uint8_t dma_rx_buffer[DMA_BUFFER_SIZE];
static volatile uint16_t old_dma_pos; 	// Tracks last processed DMA position

// a callback the is called periodically during lengthy operations
// could be NULL
static void (*periodic_cb)(void);

// private forward declarations
static bool ModemLLSendCommandV (uint32_t timeout, const char *fmt, va_list args);
static bool ModemLLTransactV(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
		        			 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
		        			 const char *fmt, va_list args);
static bool restartUART (UART_HandleTypeDef *huart);
 
/**
 * @brief set up DMA transfer with modem
 * 
 * This function should be called before any other function in this
 * module to initialise the state of the module.
 * 
 * @param dwfi_timeout the default timeout for ModemWaitForIdle()
 * @param dtx_timeout the default timeout for ModemSendCommand()
 * @param drx_timeout the default timeout for ModemReceiveLine()
 * @param pcb a callback that is called periodically during lengthy operations
 *        can be NULL
 */
void ModemLLInit(uint32_t dwfi_timeout, uint32_t dtx_timeout, uint32_t drx_timeout, void (*pcb)(void)) {
	// store parameters
	default_wfi_timeout = dwfi_timeout;
	default_tx_timeout = dtx_timeout;
	default_rx_timeout = drx_timeout;
	periodic_cb = pcb;

	// empty the "history" of transmitted and received commands
	tx_buffer[0] = '\0';
	rx_buffer[0] = '\0';
	rx_stored_buffer[0] = '\0';
		
	if (! modem_initialised) {
		// Initialise the DMA transfer ring buffer
		RingBufferInit (&ring_buffer);
		old_dma_pos = 0;

		// Start a DMA transfer - the UART's DMA must be configured in
		// "circular" mode so that it never stops receiving
		if (HAL_UARTEx_ReceiveToIdle_DMA (&huart1, dma_rx_buffer, sizeof(dma_rx_buffer)) != HAL_OK)
			Error_Handler ();
		__HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    
		modem_initialised = true;
		dma_transfers_up = true;
	}
#if DEBUG >= 3
		printf ("Modem initialised\r\n");
#endif
}

/**
 * @brief close communications with the modem
 * 
 * This function should be called after all work with the modem is finished
 */
void ModemLLShutdown (void) {
	if (modem_initialised && dma_transfers_up) {
		if (dma_transfers_up) {
			if (HAL_UART_AbortReceive (&huart1) != HAL_OK)
				Error_Handler ();
				
			dma_transfers_up = false;
		}
		modem_initialised = false;
	}
}

/** @brief use the modem's PWRKEY pin to request the modem powers on or off 
 * 
 * NOTE: The A7683E model requires at least 40mS after power on before the
 * PWRKEY pin can be used
 */
void ModemLLPower (bool on) {
#if DEBUG >= 3
	printf ("Modem power %s started...\r\n", on ? "on" : "off");
#endif

	// if the modem is initialised and we're turning the modem off then turn off DMA transfers
	if ((! on) && modem_initialised && dma_transfers_up) {
		if (HAL_UART_AbortReceive (&huart1) != HAL_OK)
			Error_Handler ();
			
		dma_transfers_up = false;
	}

	// toggle the pwrkey pin
	HAL_GPIO_WritePin (MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_RESET);
	// The difference between an "on" request and an "off" request is the length
	// of time the PWRKEY pin is held low
	if (periodic_cb != NULL)
		periodic_cb ();
	if (on)
		HAL_Delay (PWRKEY_ON_PULSE_WIDTH);
	else
		HAL_Delay (PWRKEY_OFF_PULSE_WIDTH);
	HAL_GPIO_WritePin (MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_SET);
	if (periodic_cb != NULL)
		periodic_cb ();

	// If powering on it takes time before the module's UART is ready to respond
	// If powering off the supply should not be removed for a period after the request
	if (on)
		HAL_Delay (PWRKEY_TIME_TILL_ON);
	else
		HAL_Delay (PWRKEY_TIME_TILL_OFF);

	// if the modem was initialised and we're turning the modem on then turn DMA transfers back on
	if (on && modem_initialised && (! dma_transfers_up)) {
		old_dma_pos = 0;
		RingBufferClear (&ring_buffer);

		if (! restartUART (&huart1))
			Error_Handler ();

		// remove any data sitting in receive buffer
		ModemLLWaitForIdle (PWRKEY_IDLE_CHECK);
		if (periodic_cb != NULL)
			periodic_cb ();
			
		dma_transfers_up = true;
	}

#if DEBUG >= 3
	printf ("Modem power %s completed\r\n", on ? "on" : "off");
#endif

}

/** @brief use the modem's RESET pin to reset the modem */
void ModemLLReset (void) {
#if DEBUG >= 3
	printf ("Modem reset started...\r\n");
#endif

	bool was_dma_up = dma_transfers_up;

	// if DMA transfers are up, turn them off temporarily
	if (modem_initialised && was_dma_up) {
		if (HAL_UART_AbortReceive (&huart1) != HAL_OK)
			Error_Handler ();
		dma_transfers_up = false;
	}

	// toggle the modem's reset pin
	HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_RESET);
	HAL_Delay (RESET_PULSE_WIDTH);
	HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_SET);
	HAL_Delay (RESET_TIME_TILL_READY);
	if (periodic_cb != NULL)
		periodic_cb ();

	// if DMA transfers were up, turn them back on
	if (modem_initialised && was_dma_up) {
		old_dma_pos = 0;
		RingBufferClear (&ring_buffer);

		if (! restartUART (&huart1))
			Error_Handler ();
    
		dma_transfers_up = true;
	}

	// remove any data sitting in receive buffer
	ModemLLWaitForIdle (RESET_IDLE_CHECK);
	if (periodic_cb != NULL)
		periodic_cb ();

#if DEBUG >= 3
	printf ("Modem reset completed\r\n");
#endif
}

/**
 * @brief drain the modem receive buffer
 * @param timeout the number of mS to wait before failing
 * @retval true if the buffer is empty
 */
bool ModemLLWaitForIdle (uint32_t timeout) {
	bool status = false;

	// until timeout ...
	uint32_t start_time = HAL_GetTick();
	uint32_t last_byte_received_time = start_time;
	int n_bytes_discarded = 0;
	while ((HAL_GetTick() - start_time) < timeout && ! status) {
		// see if any data has arrived
		char byte;
		while (RingBufferGet (&ring_buffer, &byte)) {
			last_byte_received_time = HAL_GetTick();
			n_bytes_discarded += 1;
		}
		
		// if no data has been seen for 100mS, we're done
		if ((HAL_GetTick() - last_byte_received_time) >= 100)
			status = true;
		else
			HAL_Delay(1);

		if (periodic_cb != NULL)
			periodic_cb ();
	}

#if DEBUG >= 4
	printf ("Modem wait for idle, %d bytes discarded, modem %s idle\r\n", n_bytes_discarded, status ? "is" : "is *not*");
#endif

	return status;
}
bool ModemLLWaitForIdleDT (void) { return ModemLLWaitForIdle (default_wfi_timeout); }

/**
 * @brief Send a command to the modem.
 * @param timeout Time to wait for transmission completion (ms).
 * @param fmt printf-style format string. Line termination will be added.
 * @retval true if the command was sent successfully.
 */
bool ModemLLSendCommand(uint32_t timeout, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
	bool status = ModemLLSendCommandV(timeout, fmt, args);
    va_end(args);

	return status;
}
bool ModemLLSendCommandDT(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
	bool status = ModemLLSendCommandV(default_tx_timeout, fmt, args);
    va_end(args);

	return status;
}
static bool ModemLLSendCommandV(uint32_t timeout, const char *fmt, va_list args) {
    int len = vsnprintf(tx_buffer, sizeof(tx_buffer), fmt, args);

    /* Formatting error or truncation - in the check leave room for the
     * terminating "\r\n\0" */
    if ((len < 0) || (len >= (int) (sizeof(tx_buffer) -3))) {
        return false;
    }
	tx_buffer[len++] = '\r';
	tx_buffer[len++] = '\n';
	tx_buffer[len] = '\0';

    if (HAL_UART_Transmit(&huart1, (uint8_t *) tx_buffer, (uint16_t) len, timeout) == HAL_OK) {
#if DEBUG >= 4
        printf("Modem Tx: \"%.*s\"\r\n", len -2, tx_buffer);
#endif
        return true;
    }

    return false;
}

/**
 * @brief receive a command response (or other line terminated string)
 *        from the modem
 * 
 * The received data is put into the rx_buffer, where it can be
 * interrogated using ModemGetLastRx()
 * 
 * @param timeout amount of time to wait, in mS
 * @retval the data on success, NULL on failure */
bool ModemLLReceiveLine (uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < timeout) {
		if (RingBufferGetLine (&ring_buffer, rx_buffer, sizeof (rx_buffer), true) == RBGL_OK) {
#if DEBUG >= 4
			printf ("Modem Rx: \"%s\"\r\n", rx_buffer);
#endif
			return true;
		}
		HAL_Delay (1);
		if (periodic_cb != NULL)
			periodic_cb ();
	}
	return false;
}
bool ModemLLReceiveLineDT (void) { return ModemLLReceiveLine (default_rx_timeout); }

/**
 * @brief receive a command response (or other line terminated string)
 *        and compare it against a list of expected values
 * 
 * Modem responses are converted to upper case before comparison.
 * Expect strings must therefore be supplied in upper case.
 * 
 * Each received line of data is put into the rx_buffer, where the most recently
 * retrieved line of data can be interrogated using ModemGetLastRxBuffer()
 *  
 * If the "store" field is set in the most recently matched suceed_list or
 * fail_list item, the received data is copied into the stored_rx_buffer
 * and is available from ModemGetStoredRxBuffer().
 * 
 * @param timeout amount of time to wait, in mS
 * @param fail_list a list of responses that, if seen, causes the function to fail -
 *        all responses in the list are checked for each line received - any
 * 		  single match causes failure - may be NULL
 * @param succeed_list a list of responses that are taken in order and matched
 *        against successive lines read from the modem - in order to succeed all
 * 		  reponses in the list must be matched
 */
bool ModemLLExpect (uint32_t timeout, const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list) {
	if (succeed_list == NULL || succeed_list->count <= 0) return false;

	// clear any previously stored response
	rx_stored_buffer[0] = '\0';

#if DEBUG >= 3
	printf ("Modem expecting %s:", succeed_list->count > 1 ? "responses" : "response");
	for (size_t count=0; count<succeed_list->count; count++) {
		const ModemResponse_t *succeed = &(succeed_list->items[count]);
		printf (" \"%s\"", succeed->expect);
	}
	printf ("\r\n");
#endif
	
	bool status = false;
	bool done = false;
	size_t n_found = 0;	
	const ModemResponse_t *succeed = &(succeed_list->items[n_found]);
	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < timeout && ! done) {
		uint32_t remain = timeout - (HAL_GetTick() - start);
		if (ModemLLReceiveLine (remain)) {
			str_upr (rx_buffer);

			// check for failure responses
			if (fail_list != NULL) {
				for (size_t count=0; count<fail_list->count; count++) {
					const ModemResponse_t *fail = &(fail_list->items[count]);
					if (strstr (rx_buffer, fail->expect) != NULL) {
						if (fail->store) {
							strncpy(rx_stored_buffer, rx_buffer, sizeof(rx_stored_buffer) - 1);
							rx_stored_buffer[sizeof(rx_stored_buffer) - 1] = '\0';
						}
						done = true;
					}
				}
			}
			
			// check for successful responses
			if (strstr (rx_buffer, succeed->expect) != NULL) {
				if (succeed->store) {
					strncpy(rx_stored_buffer, rx_buffer, sizeof(rx_stored_buffer) - 1);
					rx_stored_buffer[sizeof(rx_stored_buffer) - 1] = '\0';
				}
				n_found += 1;
				if (n_found >= succeed_list->count) {
					status = true;
					done = true;
				} else {
					succeed = &(succeed_list->items[n_found]);
				}
			}
		}
		if (periodic_cb != NULL)
			periodic_cb ();
	}
	return status;
}
bool ModemLLExpectDT (const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list) {
	return ModemLLExpect (default_rx_timeout, fail_list, succeed_list);
}

/**
 * @brief send a command and receive a response from the modem
 * 
 * Modem responses are converted to upper case before comparison.
 * Expect strings must therefore be supplied in upper case.
 *
 * Each received line of data is put into the rx_buffer, where the most recently
 * retrieved line of data can be interrogated using ModemGetLastRxBuffer()
 *  
 * If the "store" field is set in the most recently matched suceed_list or
 * fail_list item, the received data is copied into the stored_rx_buffer
 * and is available from ModemGetStoredRxBuffer().
 * 
 * @param drain_timeout the time to spend draining the Rx buffer, in mS before
 *        starting the transation - 0 means don't drain the queue
 * @param tx_timeout the time to wait for the command to be sent
 * @param rx_timeout the time to wait for the expeceted result in mS
 * @param fail_list a list of responses that, if seen, causes the function to fail -
 *        all responses in the list are checked for each line received - any
 * 		  single match causes failure - may be NULL
 * @param succeed_list a list of responses that are taken in order and matched
 *        against successive lines read from the modem - in order to succeed all
 * 		  reponses in the list must be matched
 */
bool ModemLLTransact(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
					 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					 const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool status = ModemLLTransactV (drain_timeout, tx_timeout, rx_timeout,
	                                fail_list, succeed_list, fmt, args);
	va_end (args);
	
	return status;
}
bool ModemLLTransactDT(const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					   const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool status = ModemLLTransactV (default_wfi_timeout, default_tx_timeout, default_rx_timeout,
	                                fail_list, succeed_list, fmt, args);
	va_end (args);
	
	return status;
}
static bool ModemLLTransactV(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
		        			 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
		        			 const char *fmt, va_list args) {
	bool status = true;

	// drain the receiver
	if (drain_timeout > 0) {
		status = ModemLLWaitForIdle (drain_timeout);
	}

	// send the command
	if (status) {
		status = ModemLLSendCommandV(tx_timeout, fmt, args);
	}
	
	// wait for the response
	if (status) {
		status = ModemLLExpect (rx_timeout, fail_list, succeed_list);
	}
	
	return status;
}

/**
 * @brief get the most recently transmitted line of data
 * @retval the line
 */
char *ModemLLGetTxBuffer (void) { return tx_buffer; }

/**
 * @brief get the most recently received line of data
 * @retval the line
 */
char *ModemLLGetRxBuffer (void) { return rx_buffer; }

/**
 * @brief get the most recently stored line of data
 * @retval the line
 */
char *ModemLLGetRxStoredBuffer (void) { return rx_stored_buffer; }

/**
 * @brief HAL DMA callback for UART1 data received via DMA
 * 
 * Received data is added to the modem ring buffer.
 * 
 * @param huart the UART being used
 * @param n_bytes_rx the number of bytes received
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t n_bytes_rx) {
    if (huart == &huart1) {
        uint16_t new_bytes = 0;

        // Calculate how many NEW bytes arrived since the last callback
        if (n_bytes_rx >= old_dma_pos) {
            new_bytes = n_bytes_rx - old_dma_pos;
        } else {
            // DMA wrapped around to 0
            new_bytes = (DMA_BUFFER_SIZE - old_dma_pos) + n_bytes_rx;
        }

        // Copy new bytes into ring buffer
        for (uint16_t i = 0; i < new_bytes; i++) {
            if (! RingBufferPut(&ring_buffer, (char)dma_rx_buffer[old_dma_pos]))
				Error_Handler ();
            old_dma_pos = (old_dma_pos + 1) % DMA_BUFFER_SIZE;
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
    if (huart == &huart1) {
#if DEBUG >= 1
		printf("UART error 0x%08lx\r\n", huart->ErrorCode);
#endif

		// Clear all UART Error Flags in ICR register
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
        
        // Reset DMA position tracker
        old_dma_pos = 0;

		// Clear the ring buffer
		RingBufferClear(&ring_buffer);

        // Restart circular DMA receive
        if (modem_initialised && dma_transfers_up) {
			dma_transfers_up = false;		// reflects actual state, since HAL has turned DMA off
			if (restartUART (huart))
				dma_transfers_up = true;
		}
    }
}

/**
 * @brief restart DMA reception from the UART after an error
 * @param huart the UART
 * @retval true if DMA reception was restarted
 */
static bool restartUART (UART_HandleTypeDef *huart) {
	bool status = true;

	// Re-initialize UART peripheral state then restart DMA
    if (HAL_UART_DeInit(huart) != HAL_OK) {
		status = false;
    } else if (HAL_UART_Init(huart) != HAL_OK) {
		status = false;
	} else if (HAL_UARTEx_ReceiveToIdle_DMA(huart, dma_rx_buffer, sizeof(dma_rx_buffer)) != HAL_OK) {
		status = false;
	} else {
		__HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
	}

	return status;
}
