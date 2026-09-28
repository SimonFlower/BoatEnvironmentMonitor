/** The application code
 * This is called from the CubeMX generated main() function after all initialization has completed */

#include <stdio.h>
#include <stdbool.h>

#include "debug.h"
#include "modem.h"
#include "app_iwdg.h"
#include "led.h"
#include "app.h"

static void ProgressCallback (void);

void App (void) {
#if DEBUG >= 1
	printf ("Boat Environment Monitor starting\r\n");
#endif

	// Power the modem on
	ModemInit ();
	ModemPower (true);
	
	// Test the modem
	ModemTest (5, ProgressCallback);

	BlinkLEDForever (LED_SUCCESS);
	
}

void AppError (void) {
	BlinkLEDForever (LED_HAL_ERROR);
}

static void ProgressCallback (void) {
	BlinkLED (LED_SUCCESS);
	PingIWDG ();
}
