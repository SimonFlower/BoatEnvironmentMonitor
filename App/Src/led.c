/** Control the user LED - this module contains its own hardware initialisation so
 * that it will work at any time, even before the main program hardware initialisation has run */

#include <stdio.h>
#include <stdbool.h>

#include "stm32l432xx.h"

#include "debug.h"
#include "led.h"

// times for the blink and pause between blink, in mS
#define FOR_LOOPS_PER_MS	2500
#define BLINK_TIME  		200
#define PAUSE_TIME  		2000

// forward declarations
static void LEDDelay (int ms);

/**
 * @brief flash the user LED forever at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 * @param blink_callback a function that is called for each blink repeat
 * @return does not return
 */
__attribute__((noreturn))
void BlinkLEDForever (LEDPattern_t pattern, void (*blink_callback)(void)) {
    // Forever...
    for (;;) {
        BlinkLED (pattern);
        LEDDelay (PAUSE_TIME);
		if (blink_callback != NULL) {
            blink_callback();	
        }	
    }
}

/**
 * @brief flash the user LED once at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 */
void BlinkLED (LEDPattern_t pattern) {
    static bool first = true;
    
    if (first) {
        // Enable GPIOB's clock
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
        
        // Configure PB3 as General Purpose Output (Bits 7:6 set to 01)
        GPIOB->MODER &= ~(3U << (3 * 2));
        GPIOB->MODER |=  (1U << (3 * 2));
        first = false;
    }

    // pattern must not be 0, otherwise no blinking will occur
    if (pattern <= 0)
        pattern = LED_UNKNOWN;

    // Blink the LED
    for (int count = 0; count<pattern * 2; count ++) {
        // Toggle PB3 (Onboard LED)
        GPIOB->ODR ^= (1U << 3);

        // Delay task
        LEDDelay(BLINK_TIME);
    }        
}

static void LEDDelay (int ms) {
    // ms must be greater than 0
    if (ms <= 0) ms = 1;
        
	while (ms--) {
		for (volatile uint32_t i = 0; i < FOR_LOOPS_PER_MS; i++) {
			__NOP();
		}
	}
}
