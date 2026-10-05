/** The application code
 * This is called from the CubeMX generated main() function after all initialization has completed */

#include <stdio.h>
#include <stdbool.h>

#include "stm32l4xx_hal.h"

#include "debug.h"
#include "modem_at.h"
#include "modem_ll.h"
#include "app_iwdg.h"
#include "led.h"
#include "app.h"

// TODO: configuration data
static const char *APN = "TM";
static const char *HTP_HOSTS[] = {"www.google.com", "www.cloudflare.com", "aws.amazon.com", "www.microsoft.com"};

static void WatchdogCallback (void);

void App (void) {
#if DEBUG >= 1
	printf ("\r\nBoat Environment Monitor starting\r\n");
#endif

	// Setup modem comms
	ModemStart (WatchdogCallback);

	// Modem setup...
#if DEBUG >= 1
	printf ("App: Setting up modem\r\n");
#endif
	bool modem_status = false;
	time_t rtc_time;
	for (int n_retries = 0; (n_retries < 3) && (! modem_status); n_retries ++) {
		// reset after previous attempts
		if (n_retries > 0) {
            ModemLLReset ();
			HAL_Delay(2000); // Allow power rails & UART to stabilize
		}

		// check comms to the modem
		modem_status = ModemTestComms (3);
	
		// Check the modem is in full functionality mode
		if (modem_status)
			modem_status = ModemCheckFullFunctionality (3);

		// check the SIM is ready
		if (modem_status)
			modem_status = ModemCheckSIMReady (5);

		// check that the modem is in automatic operator selection mode
		if (modem_status)
			modem_status = ModemCheckAutoOperSelMode (3);

		// check that the modem is registered with the mobile network
		if (modem_status)
			modem_status = ModemCheckRegistered (10);

		// check the received signal strength (mainly of interest for debugging, but would
		// be useful if it could be communicated in normal use, as it would show when the
		// modem is out of range from a cell tower)
		int rssi = 999;
		if (modem_status)
			rssi = ModemGetSignalStrength ();
		if (rssi == 999)
			modem_status = false;

		// bring up the modem's internet stack
		if (modem_status)
			modem_status = ModemCheckIPOpened (APN, 3);
			
		// update the modem's real-time-clock from the network
		if (modem_status)
			modem_status = ModemSyncToHTPTime (HTP_HOSTS, sizeof(HTP_HOSTS) / sizeof(HTP_HOSTS[0]), 3);
			
		if (modem_status)
			modem_status = ModemGetRTCTime (&rtc_time, 3);
	}
	if (! modem_status)
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	


	// bring down modem ...
#if DEBUG >= 1
	printf ("App: Bringing down modem\r\n");
#endif
	modem_status = false;
	for (int n_retries = 0; (n_retries < 3) && (! modem_status); n_retries ++) {
		// bring down the modem's internet stack
		modem_status = ModemCheckIPClosed (3);
	}
	if (! modem_status)
		BlinkLEDForever (LED_MODEM_ERR, WatchdogCallback);	

#if DEBUG >= 1
	printf ("App: Program finished, idling\r\n");
#endif
	BlinkLEDForever (LED_IDLE, WatchdogCallback);	
}

void AppError (void) {
	BlinkLEDForever (LED_HAL_ERROR, WatchdogCallback);
}

static void WatchdogCallback (void) {
	PingIWDG ();
}
	
