/** Control the user LED - this module contains its own hardware initialisation so
 * that it will work at any time, even before the main program hardware initialisation has run */

#include "FreeRTOS.h"
#include "task.h"

#include "stm32l432xx.h"

#include "debug.h"
#include "system_funcs.h"
#include "led.h"

// times for the blink and pause between blink, in mS
#define BLINK_TIME  200
#define PAUSE_TIME  2000

// forward declarations
static void LEDDelay (int ms, unsigned char use_free_rtos);

/**
 * @brief flash the user LED forever at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 * @param use_free_rtos set to TRUE to use FreeRTOS timer, otherwise
 *        use a simple delay loop
 * @return does not return
 */
__attribute__((noreturn))
void BlinkLEDForever (LEDPattern_t pattern, unsigned char use_free_rtos) {
    // Forever...
    for (;;) {
        BlinkLED (pattern, use_free_rtos);
        LEDDelay (PAUSE_TIME, use_free_rtos);
    }
}

/**
 * @brief flash the user LED once at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 * @param use_free_rtos set to TRUE to use FreeRTOS timer, otherwise
 *        use a simple delay loop
 */
void BlinkLED (LEDPattern_t pattern, unsigned char use_free_rtos) {
    static unsigned char first = pdTRUE;
    
    if (first) {
        // Enable GPIOB's clock
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
        
        // Configure PB3 as General Purpose Output (Bits 7:6 set to 01)
        GPIOB->MODER &= ~(3U << (3 * 2));
        GPIOB->MODER |=  (1U << (3 * 2));
        first = pdFALSE;
    }

    // pattern must not be 0, otherwise no blinking will occur
    if (pattern <= 0)
        pattern = LED_ASSERT;

    // Blink the LED
    for (int count = 0; count<pattern * 2; count ++) {
        // Toggle PB3 (Onboard LED)
        GPIOB->ODR ^= (1U << 3);

        // Delay task
        LEDDelay(BLINK_TIME, use_free_rtos);
    }        
}

static void LEDDelay (int ms, unsigned char use_free_rtos) {
    // ms must be greater than 0
    if (ms <= 0)
        ms = 1;
        
    if (use_free_rtos) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        while (ms--) {
            for (volatile uint32_t i = 0; i < 750; i++) {
                __NOP();
            }
        }
    }
}
