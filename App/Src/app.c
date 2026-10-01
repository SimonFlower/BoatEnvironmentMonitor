/** The application code
 * This is called from the CubeMX generated main() function after all initialization has completed */

#include <stdio.h>
#include <stdbool.h>

#include "debug.h"
#include "modem_at.h"
#include "app_iwdg.h"
#include "led.h"
#include "app.h"

// TODO: configuration data
#define APN "TM"

static void WatchdogCallback (void);

void App (void) {
#if DEBUG >= 1
	printf ("\r\nBoat Environment Monitor starting\r\n");
#endif

	// Start the modem
	ModemStatus_t modem_status;
	ModemStart (WatchdogCallback);
	if (! ModemTest (5))
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	
	if (! ModemGetStatus (&modem_status))
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	
	if (ModemIPConnect (&modem_status, APN)) {
		ModemIPDisconnect ();
	}

#if DEBUG >= 2
	printf ("Program finished, idling\r\n");
#endif
	BlinkLEDForever (LED_IDLE, WatchdogCallback);	
}

void AppError (void) {
	BlinkLEDForever (LED_HAL_ERROR, WatchdogCallback);
}

static void WatchdogCallback (void) {
	PingIWDG ();
}
	
