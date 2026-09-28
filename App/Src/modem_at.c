/** AT command level functions for managing the Clipper LGE 4G modem
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "debug.h"
#include "modem_ll.h"
#include "modem_at.h"

// private forward declarations
static char *str_upr (char *s);

/**
 * @brief initialise the modem and power it up
 * @retval true if the modem started OK
 */
void ModemStart (void) {
	ModemLLInit ();
	ModemLLPower (true);
}

/**
 * @brief check the modem is responding
 * @param n_retries number of times to retry the test
 * @param retry_callback a function that is called before each retry
 * @retval true if the modem responded OK
 */
bool ModemTest (int n_retries, void (*retry_callback)(void)) {
	bool found = false;
    for (int count=0; count<n_retries && ! found; count ++) {
#if DEBUG >= 2
		printf ("Modem test retry %d of %d\r\n", count +1, n_retries);
#endif
		// send "AT" and wait for "OK"
		found = ModemLLTransact ("AT", "OK", 3000, 100, 5000);
		
		// Call the retry callback
		if (retry_callback != NULL) {
            retry_callback();	
        }	
                
		// if the modem didn't respond, try resetting it
		if (! found)
			ModemLLReset ();
	}
	
#if DEBUG >= 2
		printf ("Modem test completed %s\r\n", found ? "succesfully" : "unsuccesfully");
#endif	
	return found;
}

/**
 * @brief make an IP data connection to the mobile network
 * @param apn the access point name
 * @retval true if the connection was made
 */
bool ModemIPConnect (char *apn) {
	char buffer [256];

#if DEBUG >= 2
		printf ("Modem connecting to internet...\r\n");
#endif

	// Check SIM state
	bool status = ModemLLTransact ("AT+CPIN?", "OK", 0, 100, 2000);
	
    // Check GPRS/LTE attachment state
    if (status)
		status = ModemLLTransact ("AT+CGATT?", "OK", 0, 100, 2000);

	// test whether there is an existing connection - if so, not need to connect
	if (! ModemIsIPActive ()) {
		// Define PDP context 1
		if (status) {
			snprintf(buffer, sizeof(buffer), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
			status = ModemLLTransact (buffer, "OK", 0, 100, 2000);
		}

		// 3GPP TS 27.007: Activate PDP Context 1 (State=1, CID=1)
		if (status) {
			 // 10s timeout for network activation
			status = ModemLLTransact ("AT+CGACT=1,1", "OK", 0, 100, 10000);
		}

		// 3GPP TS 27.007: Show PDP Address for Context 1
		if (status) {
			status = ModemLLSendCommand ("AT+CGPADDR=1", 100);
		}
		if (status) {
			bool ip_valid = false;
			while (ModemLLReceiveLine (buffer, sizeof(buffer), 2000)) {
				// Parse 3GPP response format: +CGPADDR: 1,"x.x.x.x"
				str_upr (buffer);
				if (strstr(buffer, "+CGPADDR: 1,\"") != NULL) {
					// Confirm non-empty IP string returned
					if (strlen(buffer) > 16) { 
						ip_valid = true;
					}
				}
				if (strstr(buffer, "OK") != NULL) {
					break;
				}
				if (strstr(buffer, "ERROR") != NULL) {
					status = false;
					break;
				}
			}
			if (! ip_valid)
				status = false;
		}
	}
	
#if DEBUG >= 2
		printf ("Modem connection to internet %s\r\n", status ? "succesfull" : "unsuccesfull");
#endif

	return status;
}

/**
 * @brief  Checks if PDP context 1 is already active.
 * @return true if active, false otherwise.
 */
bool ModemIsIPActive(void) {
    char buffer [256];
    
#if DEBUG >= 2
		printf ("Modem test connection to internet\r\n");
#endif

	bool status = ModemLLSendCommand ("AT+CGACT?", 100);
	if (status) {
		bool active = false;
		while (ModemLLReceiveLine (buffer, sizeof(buffer), 2000)) {
			// Look for +CGACT: 1,1 (CID=1, State=1)
			str_upr (buffer);
			if (strstr(buffer, "+CGACT: 1,1") != NULL) {
				active = true;
			}
			if (strstr(buffer, "OK") != NULL) {
				break;
			}
		}
		if (! active)
			status = false;
	}

#if DEBUG >= 2
		printf ("Modem connection to internet %s\r\n", status ? "active" : "inactive");
#endif

    return status;
}

/**
 * @brief  Deactivates PDP context 1 using standard 3GPP commands.
 * @return true on success, false otherwise.
 */
bool ModemIPDisconnect (void) {
#if DEBUG >= 2
		printf ("Modem disconnecting from internet...\r\n");
#endif

    // 3GPP TS 27.007: Deactivate PDP Context 1 (State=0, CID=1)
	bool status = ModemLLTransact ("AT+CGACT=0,1", "OK", 0, 100, 5000);

#if DEBUG >= 2
		printf ("Modem disconnection from internet %s\r\n", status ? "succesfull" : "unsuccesfull");
#endif

	return status;
}

static char *str_upr (char *s) {
	for (char *ptr = s; *ptr ; ptr += 1) {
		*ptr = toupper (*ptr);
	}
	return s;
}
