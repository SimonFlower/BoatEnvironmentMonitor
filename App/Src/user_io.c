/** User IO to make a "console" on the ST-LINK serial port + functions to manage debug logging */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>

#include "stm32l4xx_hal.h"

#include "config.h"
#include "uart.h"
#include "user_io.h"

// Storage for the ring buffer used to receive data from the UART
static uint8_t rb_storage [256];

// the level for debug printing (0 to 5)
static uint8_t debug_level;

// Handle for the UART connecting to ST-LINK
// This is defined in CubeMX/Core/Src/usart.c
extern UART_HandleTypeDef huart2;

// Flag to show whether UART is initialised
bool uart_initialised = false;

/**
 * @brief initialise the user IO functions 
 * 
 * This function sets up the STLINK UART with the uart.c module
 * Writing to the UART through __io_putchar() does not use the
 * uart.c module, because the debugging code needs __io_putchar()
 * to work before UserIOInit is called.
 * 
 * @param dbg_lvl the level for printing debug messages
 *        0 = none, 5 = maximum info
 * @retval true if the STLINK UART was initialised OK
 */
bool UserIOInit (uint8_t dbg_lvl) {
	bool status = true;
	
	debug_level = dbg_lvl;
	
	// Disable buffering on stdout, stderr and stdin
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdin, NULL, _IONBF, 0);
	
	if (! uart_initialised) {
		status = UARTInit (UART_STLINK, rb_storage, sizeof (rb_storage));
		if (status)
			status = UARTStart (UART_STLINK);
		if (status)
			uart_initialised = true;
	}

	return status;
}

/** 
 * @brief print debug messages to the "console"
 * @param msg_level the level of this debug message (1 to 5)
 * @param print_header if true print the "DEBUG" header start of the message
 * @param print_trailer if true finalise the message
 * @param format the printf style format string
 */
void DebugPrint(uint8_t msg_level, bool print_header, bool print_trailer, char *format, ...) {
	if (msg_level <= debug_level) {
		if (print_header)
			printf ("DEBUG (%d): ", (int) msg_level);
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		if (print_trailer)
			printf ("\r\n");
	}
} 

/**
 * @brief dump a (possibly binar) buffer
 * @param msg_level the level of this debug message (1 to 5)
 * @param format the printf style format string
 * @param buffer the buffer to dump
 * @param len the length of the buffer
 */
void DebugDump (uint8_t msg_level, const char *format, const char *buffer, int len, ...) {
	if (msg_level <= debug_level) {
		printf ("DEBUG (%d): ", (int) msg_level);

		va_list args;
		va_start(args, len);
		vprintf(format, args);
		va_end(args);

		printf (" \"");
		const char *ptr = buffer;
		for (int count=0; count<len; count++) {
			unsigned char c = (unsigned char) *ptr;
			if (isprint (c))
				printf ("%c", c);
			else
				printf ("\\x%02x", c);
			ptr ++;
		}
		printf ("\"\r\n");
	}
}

/**
 * @brief dump an integer array
 * @param msg_level the level of this debug message (1 to 5)
 * @param format the printf style format string
 * @param array the array to dump
 * @param len the length of the buffer
 */
void DebugDump2 (uint8_t msg_level, const char *format, const uint32_t *array, int len, ...) {
	if (msg_level <= debug_level) {
		printf ("DEBUG (%d): ", (int) msg_level);

		va_list args;
		va_start(args, len);
		vprintf(format, args);
		va_end(args);

		printf (" [");
		for (int count=0; count<len; count++) {
			if (count > 0) printf (", ");
			printf ("%lu", *(array + count));
		}
		printf ("]\r\n");
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
	printf ("  Configuration memory start / length): %p / %u\r\n",
	        GetConfigMemAddr (), GetConfigMemSize ());
}

void PrintConfig (ConfigData_t *config) {
	printf ("Configuration (%s):\r\n", config->loaded_from_flash ? "loaded from flash" : "using defaults");
	printf ("  Debug level: %d\r\n", (int) config->debug_level);
	printf ("  Access Point Name: %s\r\n", config->apn);
	for (int count=0; count<CFG_N_HTP_HOSTS; count++)
		printf ("  HTP host %d: %s\r\n", count +1, config->htp_hosts[count]);
	for (int count=0; count<CFG_N_DHT22_SENSORS; count++)
		printf ("  Temperature / humidity sensor %d name: %s\r\n", count +1, config->dht22_names[count]);
	for (int count=0; count<CFG_N_BATTERY_ADCS; count++)
		printf ("  Battery %d name: %s\r\n", count +1, config->battery_adc_names[count]);
}

/**
 * @brief Get a line from the STLINK uart
 * 
 * This call is non-blocking and can be called in a while look until it
 * returns true, at which point the line will be available. Received
 * characters are echoed back.
 * 
 * @param buf the line
 * @param size the maximum size of the line
 * @retval true when a full line is in buf (null terminated, no newline)
 */
bool UserIOReadLine (char *buf, size_t size) {
    static size_t len = 0;
    uint8_t byte;

    while (UARTReceiveByte (UART_STLINK, &byte)) {
        if (byte == '\r' || byte == '\n') {
            __io_putchar ('\r'); __io_putchar ('\n');
            buf[len] = '\0';
            len = 0;
            return true;
        } else if ((byte == '\b' || byte == 0x7f) && len > 0) {
            len--;
            printf ("\b \b");                       // erase on screen
        } else if (isprint (byte) && len < size - 1) {
            buf[len++] = (char) byte;
            __io_putchar (byte);
        }
    }
    return false;
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
