/** Control the user LED */

#ifndef LED_H
#define LED_H

// Define symbolic values for the different patterns that the LED will display.
// The value of each symbolic constant defines the number of "flashes" the LED
// makes
typedef enum {
    LED_SUCCESS = 1,
    LED_HAL_ERROR = 2,
    LED_IDLE = 3,
    LED_UNKNOWN = 4
} LEDPattern_t;

/* forward declarations */
__attribute__((noreturn))
void BlinkLEDForever (LEDPattern_t pattern, void (*blink_callback)(void));
void BlinkLED (LEDPattern_t pattern);

#endif /* LED_H */
