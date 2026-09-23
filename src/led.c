/** Control the user LED */

#include "FreeRTOS.h"
#include "task.h"

#include "stm32l432xx.h"

#include "debug.h"
#include "led.h"

// times for the blink and pause between blink, in mS
#define BLINK_TIME  200
#define PAUSE_TIME  2000

/**
 * @brief flash the user LED forever at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 * @return does not return
 */
void blinkLEDForever (LEDPattern_t pattern) {
    // Forever...
    for (;;) {
        blinkLED (pattern);
        vTaskDelay (pdMS_TO_TICKS(PAUSE_TIME));
    }
}

/**
 * @brief flash the user LED once at the specified rate
 * 
 * This is used by the program to indicate various conditions
 * 
 * @param delay the on/off time in mS
 */
void blinkLED (LEDPattern_t pattern) {
    static int first = pdTRUE;
    
    if (first) {
        // Configure PB3 as General Purpose Output (Bits 7:6 set to 01)
        GPIOB->MODER &= ~(3U << (3 * 2));
        GPIOB->MODER |=  (1U << (3 * 2));
        first = pdFALSE;
    }

    // Blink the LED
    for (int count = 0; count<pattern * 2; count ++) {
        // Toggle PB3 (Onboard LED)
        GPIOB->ODR ^= (1U << 3);

        // Delay task
        vTaskDelay(pdMS_TO_TICKS(BLINK_TIME));
    }        
}


