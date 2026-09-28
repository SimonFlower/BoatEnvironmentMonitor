/** debugging assistance to allow debug output to be sent to the ST-LINK serial port */

#include <stdio.h>

#include "stm32l4xx_hal.h"

#include "debug.h"

// Handle for the UART connecting to ST-LINK
// This is defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart2;

#if DEBUG > 0
/**
 * @brief initialise the debugging functions 
 */
void debug_init (void) {
	setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering on stdout
}

/**
 * @brief an implementation of __io_putchar that send characters to the serial port
 * @param ch the character to send
 * @retval the character
 */
int __io_putchar(int ch) {
    // Transmit 1 character via HAL UART with a brief timeout
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif /* DEBUG > 0 */
