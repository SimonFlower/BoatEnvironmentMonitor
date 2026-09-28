/** Low level functions for managing the Clipper LGE 4G modem
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "debug.h"
#include "ring.h"
#include "modem_ll.h"

// the maximum length of a line received by the ModemExpect function
#define MODEM_EXPECT_LINE_LEN	256

// Defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart1;

// A flag showing whether the Modem has been initialise
static volatile bool modem_initialised = false;

// The ring buffer used to receive data from DMA
static RingBuffer_t ring_buffer;

// The DMA receive buffer for data sent from the modem
// This is *only* to be used by the DMA callback
#define DMA_BUFFER_SIZE	256
static uint8_t dma_rx_buffer[DMA_BUFFER_SIZE];
static uint16_t old_dma_pos; 	// Tracks last processed DMA position
 
/**
 * @brief set up DMA transfer with modem
 */
void ModemLLInit(void) {
	if (! modem_initialised) {
		// Initialise the DMA transfer ring buffer
		RingBufferInit (&ring_buffer);
		old_dma_pos = 0;

		// Start a DMA transfer - the UART's DMA must be configured in
		// "circular" mode so that it never stops receiving
		HAL_UARTEx_ReceiveToIdle_DMA (&huart1, dma_rx_buffer, sizeof(dma_rx_buffer));
		__HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    
#if DEBUG >= 3
		printf ("Modem initialised\r\n");
#endif
		modem_initialised = true;
	}
}

/** @brief use the modem's PWRKEY pin to request the modem powers on or off 
 * NOTE: The A7683E model requires at least 40mS after power on before the
 * PWRKEY pin can be used */
void ModemLLPower (bool on) {
#if DEBUG >= 2
	printf ("Modem power %s started...\r\n", on ? "on" : "off");
#endif
	HAL_GPIO_WritePin (MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_RESET);
	// The difference between an "on" request and an "off" request is the length
	// of time the PWRKEY pin is held low
	if (on)
		HAL_Delay (100);
	else
		HAL_Delay (3000);
	HAL_GPIO_WritePin (MODEM_PWRKEY_GPIO_Port, MODEM_PWRKEY_Pin, GPIO_PIN_SET);
	// If powering on it takes time before the module's UART is ready to respond
	// If powering off the power should not be removed for a period after the request
	if (on)
		HAL_Delay (6000);
	else
		HAL_Delay (4000);
#if DEBUG >= 2
	printf ("Modem power %s completed\r\n", on ? "on" : "off");
#endif
}

/** @brief use the modem's RESET pin to reset the modem */
void ModemLLReset (void) {
#if DEBUG >= 2
	printf ("Modem reset started...\r\n");
#endif
	HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_RESET);
	HAL_Delay (2500);
	HAL_GPIO_WritePin (MODEM_RESET_GPIO_Port, MODEM_RESET_Pin, GPIO_PIN_SET);
#if DEBUG >= 2
	printf ("Modem reset completed\r\n");
#endif
}

/**
 * @brief drain the modem receive buffer
 * @param delay the number of mS to wait before checking the buffer
 * @param timeout the number of mS to wait before failing
 * @retval true if the buffer is empty
 */
bool ModemLLDrainRx (uint32_t delay, uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	uint16_t n_bytes = 0;
	while ((HAL_GetTick() - start) < timeout) {
		HAL_Delay (delay);
		char byte;
		n_bytes = 0;
		while (RingBufferGet (&ring_buffer, &byte)) {
			n_bytes += 1;
		}
		if (n_bytes == 0) {
			break;
		}
	}
	if (n_bytes > 0) return false;
	return true;
}

/**
 * @brief send a command to the modem
 * @param cmd the command to send (line termination will be added)
 * @param timeout the time to wait for the transmission to complete, in mS
 * @retval true if the command was sent OK
 */
bool ModemLLSendCommand (char *cmd, uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	if (HAL_UART_Transmit (&huart1, (unsigned char *) cmd, strlen(cmd), timeout) == HAL_OK) {
		uint32_t elapsed = HAL_GetTick() - start;
		uint32_t remain = (timeout > elapsed) ? (timeout - elapsed) : 1;
		if (remain < 1) remain = 1;
		if (HAL_UART_Transmit (&huart1, (unsigned char *) "\r\n", 2, remain) == HAL_OK) {
#if DEBUG >= 3
			printf ("Modem Tx: \"%s\"\r\n", cmd);
#endif
			return true;
		}
	}
	return false;
}

/**
 * @brief receive a command response (or other line terminated string)
 *        from the modem
 * @param data where to put the received data
 * @param length length of the data buffer
 * @param timeout amount of time to wait, in mS
 * @retval the data on success, NULL on failure */
char *ModemLLReceiveLine (char *data, size_t length, uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < timeout) {
		if (RingBufferGetLine (&ring_buffer, data, length, true) == RBGL_OK) {
#if DEBUG >= 3
			printf ("Modem Rx: \"%s\"\r\n", data);
#endif
			return data;
		}
	}
	return NULL;
}

/**
 * @brief receive a command response (or other line terminated string)
 *        and compare it against an expected value, case independent
 * @param expect the string to expect
 * @param timeout amount of time to wait, in mS */
bool ModemLLExpect (char *expect, uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	char line [MODEM_EXPECT_LINE_LEN];
	while ((HAL_GetTick() - start) < timeout) {
		uint32_t remain = timeout - (HAL_GetTick() - start);
		if (ModemLLReceiveLine (line, MODEM_EXPECT_LINE_LEN, remain)) {
			if (strcasecmp (line, expect) == 0)
				return true;
		}
	}
	return false;
}

/**
 * @brief send a command and receive a response from the modem
 * @param cmd the command to send (line termination will be added)
 * @param expect the response to expect
 * @param drain_timeout the time to spend draining the Rx buffer, in mS before
 *        starting the transation - 0 means don't drain the queue
 * @param tx_tineout the time to wait for the command to be sent
 * @param rx_timeout the time to wait for the expeceted result in mS
 */
bool ModemLLTransact (char *cmd, char *expect, uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout) {
	bool status = true;

	// drain the receiver
	if (drain_timeout > 0) {
		uint32_t drain_tick = drain_timeout / 10;
		if (drain_tick <= 0) drain_tick = 1;
		status = ModemLLDrainRx (drain_tick, drain_timeout);
	}

	// send the command
	if (status) {
		status = ModemLLSendCommand (cmd, tx_timeout);
	}
	
	// wait for the response
	if (status) {
		status = ModemLLExpect (expect, rx_timeout);
	}
	
	return status;
}

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
            RingBufferPut(&ring_buffer, (char)dma_rx_buffer[old_dma_pos]);
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
        // Clear UART Error Flags (ORE, NE, FE, PE)
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);

        // Reset DMA position tracker
        old_dma_pos = 0;

        // Restart circular DMA receive
        if (modem_initialised) {
			HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buffer, sizeof(dma_rx_buffer));
			__HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
		}
    }
}
