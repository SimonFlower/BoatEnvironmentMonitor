/** Functions for managing the Clipper LGE 4G modem
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "debug.h"
#include "ring.h"
#include "modem.h"

// Defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart1;

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
void ModemInit(void) {
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
}

/** @brief use the modem's PWRKEY pin to request the modem powers on or off 
 * NOTE: The A7683E model requires at least 40mS after power on before the
 * PWRKEY pin can be used */
void ModemPower (bool on) {
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
void ModemReset (void) {
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
 * @brief check the modem is responding
 * @param n_retries number of times to retry the test
 * @param retry_callback a function that is called before each retry
 * @retval true if the modem responded OK
 */
bool ModemTest (int n_retries, void (*retry_callback)(void)) {
	bool found = false;
    for (int count=0; count<n_retries && ! found; count ++) {
#if DEBUG >= 2
		printf ("Modem test retry %d of %d\r\n", count +1, n_retries);
#endif
		// Send "AT"
		unsigned char cmd[] = "AT\r\n";
		if (HAL_UART_Transmit_DMA (&huart1, cmd, sizeof(cmd) - 1) == HAL_OK) {
			// Wait up to 5 seconds for "OK" response
			uint32_t start = HAL_GetTick();
			while ((HAL_GetTick() - start) < 5000 && ! found) {
				char line [64];
				if (RingBufferGetLine (&ring_buffer, line, sizeof (line), true) == RBGL_OK) {
#if DEBUG >= 3
					printf ("Modem received data: \"%s\"\r\n", line);
#endif
					if (strstr (line, "OK") != NULL) {
						found = true;
					}
				}
			}
		}
		
		// Call the retry callback
		if (retry_callback != NULL) {
            retry_callback();	
        }	
                
		// if the modem didn't respond, try resetting it
		if (! found)
			ModemReset ();
	}
	
#if DEBUG >= 2
		printf ("Modem test completed %s\r\n", found ? "succesfully" : "unsuccesfully");
#endif	
	return found;
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
