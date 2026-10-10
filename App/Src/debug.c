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

/** @ brief some useful diagnostic information about the processor
 */
void diagnostics (void) {
	uint32_t sysclk = HAL_RCC_GetSysClockFreq();  // SYSCLK
	uint32_t hclk   = HAL_RCC_GetHCLKFreq();      // CPU/AHB clock
	uint32_t pclk1  = HAL_RCC_GetPCLK1Freq();     // APB1 (TIM2's bus)
	
	// Defined in ./CubeMX/Core/Src/tim.c
	extern TIM_HandleTypeDef htim2;

	printf ("Diagnostics: \r\n");
	printf ("  SYSCLK %lu, HCLK %lu, PCLK1 %lu\r\n", sysclk, hclk, pclk1);
	printf ("  TIM2 prescaler init %lu, current %lu\r\n", htim2.Init.Prescaler, TIM2->PSC);
}

/**
 * @brief an implementation of __io_putchar that send characters to the serial port
 * @param ch the character to send
 * @retval the character
 * 
 * @warning Thread-Safety & Execution Context:
 *          This function uses blocking HAL_UART_Transmit calls with HAL_MAX_DELAY.
 *          DO NOT call printf(), __io_putchar(), or any debug logging routines 
 *          from within Interrupt Service Routines (ISRs) or high-priority callbacks.
 *          Doing so can stall interrupt handling, trigger watchdog resets, or 
 *          cause stack overflow. 
 */
int __io_putchar(int ch) {
    // Transmit 1 character via HAL UART with a brief timeout
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif /* DEBUG > 0 */
