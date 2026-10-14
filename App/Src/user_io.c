/** User IO to make a "console" on the ST-LINK serial port + functions to manage debug logging */

#include <stdio.h>
#include <stdarg.h>

#include "stm32l4xx_hal.h"

#include "user_io.h"

// the level for debug printing (0 to 5)
static int debug_level;

// Handle for the UART connecting to ST-LINK
// This is defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart2;

/**
 * @brief initialise the debugging functions 
 * @param dbg_lvl the level for printing debug messages
 *        0 = none, 5 = maximum info
 */
void DebugInit (int dbg_lvl) {
	debug_level = dbg_lvl;
	setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering on stdout
}

/** 
 * @brief print debug messages to the "console"
 * @param level the level of this debug message (1 to 5)
 * @param format the printf style format string
 */
void DebugPrint(int msg_level, char *format, ...) {
	if (msg_level <= debug_level) {
		va_list args;

		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
} 

/** 
 * @brief print useful diagnostic information about the processor
 */
void PrintDiagnostics (void) {
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
