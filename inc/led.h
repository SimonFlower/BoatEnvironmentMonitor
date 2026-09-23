/** Control the user LED */

#ifndef LED_H
#define LED_H

// Define symbolic values for the different patterns that the LED will display.
// The value of each symbolic constant defines the number of "flashes" the LED
// makes
typedef enum {
    LED_SUCCESS = 1,
    LED_INIT_ERR = 2, 
    LED_HARD_FAULT = 3 
} LEDPattern_t;

/* forward declarations */
void blinkLEDForever (LEDPattern_t pattern);
void blinkLED (LEDPattern_t pattern);

#endif /* LED_H */
