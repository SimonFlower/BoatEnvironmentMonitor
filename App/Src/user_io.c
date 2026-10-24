/** User IO to make a "console" on the ST-LINK serial port + functions to manage debug logging */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "stm32l4xx_hal.h"

#include "config.h"
#include "uart.h"
#include "utils.h"
#include "led.h"
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
 * @brief process user commands
 * 
 * This routine retrieves the program configuration and possibly changes it
 * Other places where the configuration has already been loaded will be
 * affected.
 * 
 * @param config the configuration
 * @param query_user true to ask the user whether to enter the command processer
 *        false to enter without asking
 * @param timeout the amount of time to wait for the user to respond to the queury. in mS
 * @param periodic_cb a callback that is called periodically during lengthy operations
 *        can be NULL
 */
void CommandProcesser (ConfigData_t * config, bool query_user, uint32_t timeout, void (*periodic_cb)(void)) {
	// if the UART isn't initialised, we can't do anything
	if (! uart_initialised)
		return;
	
	// Wait for the user to press a key before entering command mode
	bool done = false;
	if (query_user) {
		uint32_t start_time = HAL_GetTick();
		uint32_t elapse = 0;
		bool user_responded = false;
		do {
			elapse = HAL_GetTick() - start_time;
			printf ("Press any key to enter programming mode (%d)\r", ((int) timeout - (int) elapse) / 1000 );
			
			uint8_t byte;
			user_responded = UARTReceiveByte (UART_STLINK, &byte);
			
			if (! user_responded)
				HAL_Delay (200);
		} while ((elapse < timeout) && (! user_responded));
		printf ("\r\n");
		if (! user_responded)
			done = true;
	}

	// Tell the user how to get help
	if (! done)
		printf ("To get help type 'help'\r\n");

	// Get and process commands
	bool config_updated = false;
	while (! done) {
		// Print a prompt
		printf("> ");

		// Get the next line
		char line [100];
		while (! UserIOReadLine (line, sizeof (line))) {
			if (periodic_cb != NULL)
				periodic_cb ();
		}

		// The commands have at most 4 parameters. In some commands
		// the final parameter may include spaces. Split the line
		// into (up to) 4 tokens
		char *tokens [4];
		char *line_ptr = str_trim (line);
		for (int count=0; count<4; count ++) {
			size_t space_len;
			size_t token_len = get_next_token (line_ptr, &space_len);
			if (token_len == 0 && space_len == 0) {
				// there were no more tokens
				tokens [count] = "";
			} else {
				*(line_ptr + token_len) = '\0';
				tokens[count] = line_ptr;
				line_ptr += token_len + space_len;
			}
		}
		
		// Parse then run the command
		if (! strcmp (tokens[0], "end")) {
			bool do_it = true;
			if (config_updated)
				do_it = UserIOQuery ("Configuration has not been saved", periodic_cb);
			if (do_it) {
				printf ("Program terminating\r\n");
				BlinkLEDForever (LED_SUCCESS, periodic_cb);
			}
		} else if (! strcmp (tokens[0], "help")) {
			PrintHelp ();
		} else if (! strcmp (tokens[0], "run")) {
			bool do_it = true;
			if (config_updated)
				do_it = UserIOQuery ("Configuration has not been saved", periodic_cb);
			if (do_it)
				done = true;
		} else if (! strcmp (tokens[0], "restart")) {
			bool do_it = true;
			if (config_updated)
				do_it = UserIOQuery ("Configuration has not been saved", periodic_cb);
			if (do_it) {
				// Trigger a reboot
				NVIC_SystemReset();
			}
		} else if (! strcmp (tokens[0], "save")) {
			if (SaveConfig ()) {
				printf ("Configuration saved to flash memory\r\n");
				config_updated = false;
			} else {
				printf ("There was an error saving the configuration to flash memory\r\n");
			}
		} else if (! strcmp (tokens[0], "set")) {
			if (! strcmp (tokens[1], "apn")) {
				if (strlen (tokens [2]) == 0) {
					printf ("Missing access point name\r\n");
				} else if (strlen (tokens[3]) != 0) {
					printf ("Extra parameter to \"apn\" command: %s\r\n", tokens[3]);
				} else if (strlen (tokens[2]) >= CFG_APN_LEN) {
					printf ("Access point name must be %d characters or less\r\n", CFG_APN_LEN);
				} else {
					config_updated = true;
					strcpy (config->apn, tokens[2]);
				}				
			} else if (! strcmp (tokens[1], "battery")) {
				int battery_no;
				if (strlen (tokens[2]) == 0) {
					printf ("Missing battery number\r\n");
				} else if (! parse_int (tokens[2], &battery_no) || battery_no < 1 || battery_no > CFG_N_BATTERY_ADCS) {
					printf ("Battery number must be an integer between 1 and %d: %s\r\n", CFG_N_BATTERY_ADCS, tokens[2]);
				} else if (strlen (tokens[3]) == 0) {
					printf ("Missing battery name\r\n");
				} else if (strlen (tokens[3]) >= CFG_BATTERY_ADC_NAME_LEN) {
					printf ("Battery name must be %d characters or less\r\n", CFG_BATTERY_ADC_NAME_LEN);
				} else {
					config_updated = true;
					strcpy (config->battery_adc_names[battery_no -1], tokens[3]);
				}
			} else if (! strcmp (tokens[1], "debug")) {
				int debug_level;
				if (strlen (tokens [2]) == 0) {
					printf ("Missing debug level\r\n");
				} else if (strlen (tokens[3]) != 0) {
					printf ("Extra parameter to \"debug\" command: %s\r\n", tokens[3]);
				} else if (! parse_int (tokens[2], &debug_level) || debug_level < 0 || debug_level > 5) {
					printf ("Debug level must be an integer between 0 and 5: %s\r\n", tokens[2]);
				} else {
					config_updated = true;
					config->debug_level = debug_level;
				}				
			} else if (! strcmp (tokens[1], "defaults")) {
    			SetConfigDefaults ();
				config_updated = true;
				printf ("Configuration set to defaults\r\n");
			} else if (! strcmp (tokens[1], "htp")) {
				int htp_no;
				if (strlen (tokens[2]) == 0) {
					printf ("Missing HTP number\r\n");
				} else if (! parse_int (tokens[2], &htp_no) || htp_no < 1 || htp_no > CFG_N_HTP_HOSTS) {
					printf ("HTP number must be an integer between 1 and %d: %s\r\n", CFG_N_HTP_HOSTS, tokens[2]);
				} else if (strlen (tokens[3]) == 0) {
					printf ("Missing HTP host\r\n");
				} else if (strlen (tokens[3]) >= CFG_HTP_HOST_LEN) {
					printf ("HTP host name must be %d characters or less\r\n", CFG_HTP_HOST_LEN);
				} else {
					config_updated = true;
					strcpy (config->htp_hosts[htp_no -1], tokens[3]);
				}
			} else if (! strcmp (tokens[1], "temperature")) {
				int temperature_no;
				if (strlen (tokens[2]) == 0) {
					printf ("Missing temperature number\r\n");
				} else if (! parse_int (tokens[2], &temperature_no) || temperature_no < 1 || temperature_no > CFG_N_DHT22_SENSORS) {
					printf ("Temperature number must be an integer between 1 and %d: %s\r\n", CFG_N_DHT22_SENSORS, tokens[2]);
				} else if (strlen (tokens[3]) == 0) {
					printf ("Missing temperature name\r\n");
				} else if (strlen (tokens[3]) >= CFG_DHT22_NAME_LEN) {
					printf ("Temperature name must be %d characters or less\r\n", CFG_DHT22_NAME_LEN);
				} else {
					config_updated = true;
					strcpy (config->dht22_names[temperature_no -1], tokens[3]);
				}
			} else if (strlen (tokens[1]) == 0) {
				printf ("Missing 'set' option\r\n");
			} else {
				printf ("Unrecongnised 'set' option: %s\r\n", tokens[1]);
			}
		} else if (! strcmp (tokens[0], "show")) {
			if (! strcmp (tokens[1], "config")) {
				PrintConfig (config);
			} else if (! strcmp (tokens[1], "diagnostics")) {
				PrintDiagnostics ();
			} else if (strlen (tokens[1]) == 0) {
				printf ("Missing 'show' option\r\n");
			} else {
				printf ("Unrecongnised 'show' option: %s\r\n", tokens[1]);
			}
		} else if (strlen (tokens[0]) == 0) {
			// empty line - do nothing
		} else {
			printf ("Unrecongnised command: %s\r\n", tokens[0]);
		}
	}
}

/**
 * @brief Ask they user whether to continue or not
 * @retval true to continue, false to cancel
 */
bool UserIOQuery (char *msg, void (*periodic_cb)(void)) {
	printf ("%s - do you want to continue? Press \"y\" or \"n\" and ENTER: ", msg);
	bool done = false;
	bool answer;
	while (! done) {
		char line [20];
		while (! UserIOReadLine (line, sizeof (line))) {
			if (periodic_cb != NULL)
				periodic_cb ();
		}
		if (strcmp (line, "y") == 0) {
			done = true;
			answer = true;
		} else if (strcmp (line, "n") == 0) {
			done = true;
			answer = false;
		} else {
			printf ("Please type \"y\" or \"n\" and ENTER: ");
		}
	} 
	
	return answer;
}

/** 
 * @brief Display a help screen
 */
void PrintHelp (void) {
	printf ("Commands:\r\n");
	printf ("  end                               : terminate the program\r\n");
	printf ("  help                              : show this help information\r\n");
	printf ("  run                               : run the program\r\n");
	printf ("  restart                           : reboot the system\r\n");
	printf ("  save                              : save the configuration\r\n");
	printf ("  set apn <string>                  : set the access point name for the mobile network\r\n");
	printf ("  set battery <number> <string>     : set the name for battery 1-3\r\n");
	printf ("  set debug <number>                : set the program's debug level (0-5, 0 = off)\r\n");
	printf ("  set defaults                      : set the configuration back to default values\r\n");
	printf ("  set htp <number> <string>         : set HTP host 1-4\r\n");
	printf ("  set temperature <number> <string> : set the name for temperature / humidity sensor 1-4\r\n");
	printf ("  show config                       : show the current configuration\r\n");
	printf ("  show diagnostics                  : show processor diagnostics\r\n");
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

/**********************************************************************************
 ************************ Functions for debugging *********************************
 **********************************************************************************/

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

/**********************************************************************************
 ********************** Functions to handle UART IO *******************************
 **********************************************************************************/

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
	// if the UART isn't initialised, we can't do anything
	if (! uart_initialised)
		return false;

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
