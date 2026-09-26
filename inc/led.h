/** Control the user LED */

#ifndef LED_H
#define LED_H

// Define symbolic values for the different patterns that the LED will display.
// The value of each symbolic constant defines the number of "flashes" the LED
// makes
typedef enum {
    LED_SUCCESS = 1,
    LED_INIT_ERR = 2, 
    LED_HARD_FAULT = 3,
    LED_ASSERT = 4
} LEDPattern_t;

/* forward declarations */
__attribute__((noreturn))
void BlinkLEDForever (LEDPattern_t pattern, unsigned char use_free_rtos);
void BlinkLED (LEDPattern_t pattern, unsigned char use_free_rtos);

#endif /* LED_H */
