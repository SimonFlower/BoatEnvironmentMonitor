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

	// Setup modem comms
	ModemStart (WatchdogCallback);

	// Modem setup...
#if DEBUG >= 1
	printf ("Setting up modem\r\n");
#endif
	bool modem_status = false;
	for (int n_retries = 0; (n_retries < 3) && (! modem_status); n_retries ++) {
		// reset after previous attempts
		if (n_retries > 0)
            ModemLLReset ();

		// check comms to the modem
		modem_status = ModemTestComms (3);
	
		// Check the modem is in full functionality mode
		if (modem_status)
			modem_status = ModemCheckFullFunctionality (3);

		// check the SIM is ready
		if (modem_status)
			modem_status = ModemCheckSIMReady (3);

		// check that the modem is in automatic operator selection mode
		if (modem_status)
			modem_status = ModemCheckAutoOperSelMode ();

		// check that the modem is registered with the mobile network
		if (modem_status)
			modem_status = ModemCheckRegistered (3);

		// check the received signal strength (mainly of interest for debugging, but would
		// be useful if it could be communicated in normal use, as it would show when the
		// modem is out of range from a cell tower)
		int rssi = 999;
		if (modem_status)
			rssi = ModemGetSignalStrength ();
		if (rssi > 0)
			modem_status = false;

		// attach the modem to the packet domain network
		if (modem_status)
			modem_status = ModemCheckPDAttached (APN, 3);
	}
	if (! modem_status)
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	


	// bring down modem ...
#if DEBUG >= 1
	printf ("Bringing down modem\r\n");
#endif
	modem_status = false;
	for (int n_retries = 0; (n_retries < 3) && (! modem_status); n_retries ++) {
		// detach the modem from the packet domain network
		modem_status = ModemCheckPDDetached (3);
	}
	if (! modem_status)
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	

#if DEBUG >= 1
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
	
