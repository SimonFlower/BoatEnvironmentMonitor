/** The application code
 * This is called from the CubeMX generated main() function after all initialization has completed */

#include <stdio.h>

#include "debug.h"
#include "app_iwdg.h"
#include "led.h"
#include "app.h"

void App (void) {
#if DEBUG >= 1
	printf ("Boat Environment Monitor starting\r\n");
#endif

	BlinkLEDForever (LED_SUCCESS);
	
}

void AppError (void) {
	BlinkLEDForever (LED_HAL_ERROR);
}
