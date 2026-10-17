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

#include "user_io.h"
#include "ring.h"
#include "utils.h"
#include "uart.h"
#include "modem_ll.h"

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

// Storage for the ring buffer used to receive data from the UART
static uint8_t rb_storage [1024];

// transmit and receive buffers for sending commands to the modem
// and receiving responses from it
static char tx_buffer [MODEM_TX_MAX_LEN];
static char rx_buffer [MODEM_RX_MAX_LEN];
static char rx_stored_buffer [MODEM_RX_MAX_LEN];

// default timeout values (in mS)
uint32_t default_wfi_timeout;
uint32_t default_tx_timeout;
uint32_t default_rx_timeout;

// A flag showing whether the Modem has been initialised
static volatile bool modem_initialised = false;

// a callback the is called periodically during lengthy operations
// could be NULL
static void (*periodic_cb)(void);

// private forward declarations
static bool ModemLLSendCommandV (uint32_t timeout, bool line_term, const char *fmt, va_list args);
static bool ModemLLTransactV(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
                             bool line_term,
		        			 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
		        			 const char *fmt, va_list args);
 
/**
 * @brief initialise communications with the modem
 * 
 * This function should be called once before any other function in this
 * module to initialise the state of the module.
 * 
 * @param dwfi_timeout the default timeout for ModemWaitForIdle()
 * @param dtx_timeout the default timeout for ModemSendCommand()
 * @param drx_timeout the default timeout for ModemReceiveLine()
 * @param pcb a callback that is called periodically during lengthy operations
 *        can be NULL
 * @retval true if the modem was initialised OK
 */
bool ModemLLInit(uint32_t dwfi_timeout, uint32_t dtx_timeout, uint32_t drx_timeout, void (*pcb)(void)) {
	bool status = true;
	
	// store parameters
	default_wfi_timeout = dwfi_timeout;
	default_tx_timeout = dtx_timeout;
	default_rx_timeout = drx_timeout;
	periodic_cb = pcb;

	// intialise the UART
	status = UARTInit (UART_MODEM, rb_storage, sizeof (rb_storage));

	DebugPrint (3, true, true, "Modem initialised %s", status ? "ok" : "*not* ok");

	return status;
}

/**
 * @brief start commincating with the modem
 * 
 * This function should be called before working with the modem
 *
 * @retval true if the modem was started OK
 */
bool ModemLLStart (void) {
	bool status = true;

	// empty the "history" of transmitted and received commands
	tx_buffer[0] = '\0';
	rx_buffer[0] = '\0';
	rx_stored_buffer[0] = '\0';
		
	if (! modem_initialised) {
		status = UARTStart (UART_MODEM);
		modem_initialised = true;
	}

	DebugPrint (3, true, true, "Modem %s", status ? "started ok" : "*not* started ok");
	return status;
}

 
/**
 * @brief close communications with the modem
 * 
 * This function should be called after all work with the modem is finished
 * 
 * @retval true if the modem was stopped OK
 */
bool ModemLLStop (void) {
	bool status = true;
	if (modem_initialised) {
		status = UARTStop (UART_MODEM);
		modem_initialised = false;
	}
	DebugPrint (3, true, true, "Modem %s", status ? "stopped ok" : "*not* stopped ok");

	return status;
}

/** @brief use the modem's PWRKEY pin to request the modem powers on or off 
 * 
 * NOTE: The A7683E model requires at least 40mS after power on before the
 * PWRKEY pin can be used
 * 
 * @retval true if the power change was completed OK
 */
bool ModemLLPower (bool on) {
	bool status = true;
	bool modem_was_initialised = modem_initialised;
	
	DebugPrint (3, true, true, "Modem power %s started...", on ? "on" : "off");

	// if the UART is initialised and we're turning the modem off then shut it down
	if ((! on) && modem_initialised) {
		if (! UARTStop (UART_MODEM))
			status = false;
		else
			modem_initialised = false;
	}

	// toggle the pwrkey pin
	if (status) {
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
	}

	// if the UART was initialised and we're turning the modem on then start it up again
	if (status && on && modem_was_initialised) {
		// restart the UART
		status = UARTStart (UART_MODEM);
		if (status) {
			modem_initialised = true;
			
			// remove any data sitting in receive buffer
			ModemLLWaitForIdle (PWRKEY_IDLE_CHECK);
			if (periodic_cb != NULL)
				periodic_cb ();
		}
	}

	DebugPrint (3, true, true, "Modem power %s completed %s", on ? "on" : "off", status ? "ok" : "*not* ok");
	
	return status;
}

/** @brief use the modem's RESET pin to reset the modem */
bool ModemLLReset (void) {
	bool status = true;
	bool modem_was_initialised = modem_initialised;

	DebugPrint (3, true, true, "Modem reset started...");

	// if the modem is initialised then shut it down
	if (modem_initialised) {
		if (! UARTStop (UART_MODEM))
			status = false;
		else
			modem_initialised = false;
	}

	// toggle the modem's reset pin
	if (status) {
		HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_RESET);
		HAL_Delay (RESET_PULSE_WIDTH);
		HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_SET);
		HAL_Delay (RESET_TIME_TILL_READY);
		if (periodic_cb != NULL)
			periodic_cb ();
	}

	// if the modem was initialised then start it up again
	if (status && modem_was_initialised) {
		// restart the UART
		status = UARTStart (UART_MODEM);
		if (status) {
			modem_initialised = true;			

			// remove any data sitting in receive buffer
			ModemLLWaitForIdle (PWRKEY_IDLE_CHECK);
			if (periodic_cb != NULL)
				periodic_cb ();
		}
	}

	DebugPrint (3, true, true, "Modem reset completed &s", status ? "ok" : "*not* ok");

	return status;
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
		// check that DMA transfers are running
		if (! UARTIsDMAUp (UART_MODEM)) {
			if (! UARTStart (UART_MODEM))
				// bail out (status will be false) as we can't receive anything
				break;
		}
		
		// see if any data has arrived
		uint8_t byte;
		while (UARTReceiveByte (UART_MODEM, &byte)) {
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

	DebugPrint (3, true, true, "Modem wait for idle, %d bytes discarded, modem %s idle",
	            n_bytes_discarded, status ? "is" : "is *not*");

	return status;
}
bool ModemLLWaitForIdleDT (void) { return ModemLLWaitForIdle (default_wfi_timeout); }

/**
 * @brief Send a command to the modem.
 * @param timeout Time to wait for transmission completion (ms).
 * @param line_term true to send line termination, false for no termination
 * @param fmt printf-style format string. Line termination will be added.
 * @retval true if the command was sent successfully.
 */
bool ModemLLSendCommand(uint32_t timeout, bool line_term, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
	bool status = ModemLLSendCommandV(timeout, line_term, fmt, args);
    va_end(args);

	return status;
}
bool ModemLLSendCommandDT(bool line_term, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
	bool status = ModemLLSendCommandV(default_tx_timeout, line_term, fmt, args);
    va_end(args);

	return status;
}
static bool ModemLLSendCommandV(uint32_t timeout, bool line_term, const char *fmt, va_list args) {
    int len = vsnprintf(tx_buffer, sizeof(tx_buffer), fmt, args);

    /* Formatting error or truncation - in the check leave room for the
     * terminating "\r\n\0" */
    if ((len < 0) || (len >= (int) (sizeof(tx_buffer) -3)))
        return false;
    if (line_term) {
		tx_buffer[len++] = '\r';
		tx_buffer[len++] = '\n';
	}
	tx_buffer[len] = '\0';

    return UARTTransmit(UART_MODEM, tx_buffer, len, timeout);
}

/**
 * @brief receive a command response (or other line terminated string)
 *        from the modem
 * 
 * The received data is put into the rx_buffer, where it can be
 * interrogated using ModemGetLastRx(). An empty line from the
 * modem will be returned (not ignored).
 * 
 * @param timeout amount of time to wait, in mS
 * @retval true if a line was received OK */
bool ModemLLReceiveLine (uint32_t timeout) {

	// While not timed out...
	uint32_t start = HAL_GetTick();
	rx_buffer [0] = '\0';
	while ((HAL_GetTick() - start) < timeout) {
		// check that DMA transfers are running
		if (! UARTIsDMAUp (UART_MODEM)) {
			if (! UARTStart (UART_MODEM))
				// bail out as we can't receive anything
				return false;
		}
		
		// Try to get some (more) data
		size_t len = strlen (rx_buffer);
		LineRetrievalType_t line_status = UARTReceiveLine (UART_MODEM, 
														   rx_buffer + len,
														   sizeof (rx_buffer) - len);
		switch (line_status) {
			case LINE_OK:
				// remove the trailing CRLF
				str_trim (rx_buffer);
				return true;
			case LINE_OVERFLOW:
				return false;
			default:
				// If a line was partially received or no data was received at all
				// then go round again
				break;
		}

		// Wait before trying again
		HAL_Delay (20);
		if (periodic_cb != NULL)
			periodic_cb ();
	}
	
	// Timeout
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

	DebugPrint (3, true, false, "Modem expecting %s:", succeed_list->count > 1 ? "responses" : "response");
	for (size_t count=0; count<succeed_list->count; count++) {
		const ModemResponse_t *succeed = &(succeed_list->items[count]);
		DebugPrint (3, false, false, " \"%s\"", succeed->expect);
	}
	DebugPrint (3, false, true, "");
	
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
 * @param line_term true to send line termination, false for no termination
 * @param fail_list a list of responses that, if seen, causes the function to fail -
 *        all responses in the list are checked for each line received - any
 * 		  single match causes failure - may be NULL
 * @param succeed_list a list of responses that are taken in order and matched
 *        against successive lines read from the modem - in order to succeed all
 * 		  reponses in the list must be matched
 */
bool ModemLLTransact(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
					 bool line_term, 
					 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					 const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool status = ModemLLTransactV (drain_timeout, tx_timeout, rx_timeout, line_term,
	                                fail_list, succeed_list, fmt, args);
	va_end (args);
	
	return status;
}
bool ModemLLTransactDT(bool line_term,
                       const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					   const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool status = ModemLLTransactV (default_wfi_timeout, default_tx_timeout, default_rx_timeout,
	                                line_term, fail_list, succeed_list, fmt, args);
	va_end (args);
	
	return status;
}
static bool ModemLLTransactV(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
							 bool line_term,
		        			 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
		        			 const char *fmt, va_list args) {
	bool status = true;

	// drain the receiver
	if (drain_timeout > 0)
		status = ModemLLWaitForIdle (drain_timeout);

	// send the command
	if (status)
		status = ModemLLSendCommandV(tx_timeout, line_term, fmt, args);
	
	// wait for the response
	if (status)
		status = ModemLLExpect (rx_timeout, fail_list, succeed_list);
	
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

