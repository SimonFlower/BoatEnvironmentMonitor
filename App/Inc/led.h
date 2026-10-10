/** Control the user LED */

#ifndef LED_H
#define LED_H

// Define symbolic values for the different patterns that the LED will display.
// The value of each symbolic constant defines the number of "flashes" the LED
// makes
typedef enum {
    LED_SUCCESS = 1,
    LED_CLOCK_ERROR = 2,
    LED_MODEM_ERROR = 3,
    LED_DHT22_ERROR = 4,
    LED_SYSTEM_ERROR = 5
} LEDPattern_t;

/* forward declarations */
__attribute__((noreturn))
void BlinkLEDForever (LEDPattern_t pattern, void (*blink_cb)(void));
void BlinkLED (LEDPattern_t pattern);
void LEDOn (void);
void LEDOff (void);
void LEDToggle (void);

#endif /* LED_H */
