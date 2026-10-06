/** AT command level functions for managing the Clipper LGE 4G modem
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "stm32l4xx_hal.h"

#include "debug.h"
#include "modem_ll.h"
#include "modem_at.h"

// network connection details
// channel identifier
#define NETWORK_CID		1
// number of HTP servers in modem HTP server pool
#define N_HTP_SERVERS 	16

// timeouts for interactions with the modem
#define ONE_SEC_TIMEOUT			    1000
#define TWO_SEC_TIMEOUT			    2000
#define FIVE_SEC_TIMEOUT			5000
#define TEN_SEC_TIMEOUT				10000
#define ONE_MIN_TIMEOUT				60000
#define TWO_MIN_TIMEOUT				120000
#define DEFAULT_DRAIN_TIMEOUT		0
#define DEFAULT_TX_TIMEOUT			100
#define DEFAULT_RX_TIMEOUT			TEN_SEC_TIMEOUT

// flag to show whether modem has been started
static bool modem_is_started = false;

/**
 * @brief initialise the modem and power it up
 * 
 * This function should be called before any other function in this
 * module to initialise the state of the module.
 * 
 * @param periodic_cb a callback that is called periodically during lengthy operations
 *        can be NULL
 */
void ModemStart (void (*periodic_cb)(void)) {
	if (! modem_is_started) {
		// initialise the low level functions
		ModemLLInit (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, DEFAULT_RX_TIMEOUT, periodic_cb);
		// soft power on
		ModemLLPower (true);
		
		modem_is_started = true;
	}
}

/**
 * @brief shutdown the modem
 * 
 * This function should be called after all work with the modem is finished
 */
void ModemStop (void) {
	if (modem_is_started) {
		// shutdown the low level functions
		ModemLLShutdown ();
		
		modem_is_started = false;
	}
}

/**
 * @brief check the modem is responding
 * @param n_retries number of times to retry the test
 * @retval true if the modem responded OK
 */
bool ModemTestComms (int n_retries) {
    bool status = false;
    for (int count=0; count<n_retries && ! status; count ++) {
		// sent "+++" in case the modem is in data mode - don't check for a succesful response
        ModemLLTransact (FIVE_SEC_TIMEOUT, ONE_SEC_TIMEOUT, ONE_SEC_TIMEOUT,
                         NULL,
                         RESPONSE_LIST( { "OK", false } ),
                         "+++");

        // send "AT" and wait for "OK"
        status = ModemLLTransact (FIVE_SEC_TIMEOUT, DEFAULT_RX_TIMEOUT, DEFAULT_RX_TIMEOUT,
                                  RESPONSE_LIST( { "ERROR", false } ),
                                  RESPONSE_LIST( { "OK", false } ),
                                  "AT");

		// send "ATE0" to turn off echoing
		if (status)
			status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
										RESPONSE_LIST( { "OK", false } ),
										"ATE0");
                                 
		// turn off unsolicited response codes
		if (status)
			status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
										RESPONSE_LIST( { "OK", false } ),
										"AT+CEREG=0");

        // send "ATI" to request modem information
        if (status)
            status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                       RESPONSE_LIST( { "OK", false } ),
                                       "ATI");
    }
    
#if DEBUG >= 2
        printf ("Modem test comms completed %s\r\n", status ? "successfully" : "unsuccessfully");
#endif	
    return status;
}

/**
 * @brief Get the modem's functionality mode
 * @retval FUNC_ERROR for a comms error, FUNC_LIMITED for anything other than
 *         full functionality, or FUNC_FULL for full functionality
 */
ModemFunctionality_t ModemGetFunctionality (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CFUN:", true }, { "OK", false } ),
                                     "AT+CFUN?");
    int func_code;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CFUN: %d", &func_code) != 1)
        status = false;
    ModemFunctionality_t ret_val;
    if (status) {
        if (func_code == 1) ret_val = FUNC_FULL;
        else ret_val = FUNC_LIMITED;
    } else {
        ret_val = FUNC_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem functionality: ");
    switch (ret_val) {
        case FUNC_FULL: printf ("full"); break;
        case FUNC_LIMITED: printf ("limited (%d)", func_code); break;
        default: printf ("*** error ***"); break;
    }
    printf ("\r\n");
#endif

    return ret_val;
}

/**
 * @brief set the modem into full functionality mode
 * @param reset true to reset the modem
 * @retval true if the modem was set into full functionality mode, false otherwise
 */
bool ModemSetFullFunctionality (bool reset) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "OK", false } ),
                                     "AT+CFUN=1%s", reset ? ",1" : "");
#if DEBUG >= 2
    printf ("Modem %s to full functionality\r\n", status ? "set" : "*not* set");
#endif
    return status;
}

/**
 * @brief check that the modem is in full functionality mode
 * @param n_retries the number of times to try putting the modem into full functionality mode
 * @retval true if the modem is in full functionality mode, false otherwise
 */
bool ModemCheckFullFunctionality (int n_retries) {
    ModemFunctionality_t mfunc = ModemGetFunctionality ();
    for (int count=0; count<n_retries && mfunc != FUNC_FULL; count ++) {
        ModemSetFullFunctionality (count > 0);
        mfunc = ModemGetFunctionality ();
    }
    return mfunc == FUNC_FULL;
}

/**
 * @brief Get the modem's operator selection mode
 * @retval OPER_SEL_ERROR for a comms error, OPER_SEL_NOT_AUTO for anything other than
 *         automatic mode, or OPER_SEL_AUTO for automatic
 */
ModemOperSelMode_t ModemGetOperSelMode (void) {
    bool status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, ONE_MIN_TIMEOUT,
                                   RESPONSE_LIST( { "ERROR", false } ),
                                   RESPONSE_LIST( { "+COPS:", true }, { "OK", false } ),
                                   "AT+COPS?");
    int op_sel_mode_code;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+COPS: %d", &op_sel_mode_code) != 1)
        status = false;	
    ModemOperSelMode_t ret_val;
    if (status) {
        if (op_sel_mode_code == 0) ret_val = OPER_SEL_AUTO;
        else ret_val = OPER_SEL_NOT_AUTO;
    } else {
        ret_val = OPER_SEL_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem operator selection mode: ");
    switch (ret_val) {
        case OPER_SEL_AUTO: printf ("automatic"); break;
        case OPER_SEL_NOT_AUTO: printf ("not automatic (%d)", op_sel_mode_code); break;
        default: printf ("*** error ***"); break;
    }
    printf ("\r\n");
#endif

    return ret_val;
}

/**
 * @brief set the modem into automatic operator selection mode
 * @retval true if the modem was set into full functionality mode, false otherwise
 */
bool ModemSetAutoOperSelMode (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "OK", false } ),
                                     "AT+COPS=0");
#if DEBUG >= 2
    printf ("Modem %s to automatic operator selection mode\r\n", status ? "set" : "*not* set");
#endif
    return status;
}

/**
 * @brief check that the modem is in automatic operator selection mode
 * @param n_retries the number of times to try putting the modem into automatic operator selection mode
 * @retval true if the modem is in automatic operator selection mode, false otherwise
 */
bool ModemCheckAutoOperSelMode (int n_retries) {
    ModemOperSelMode_t sel_mode = ModemGetOperSelMode ();
    for (int count=0; count<n_retries && sel_mode != OPER_SEL_AUTO; count ++) {
        ModemSetAutoOperSelMode ();
        sel_mode = ModemGetOperSelMode ();
    }
    return sel_mode == OPER_SEL_AUTO;
}

/**
 * @brief Get the status of the modem's SIM
 * @param modem_status the modem status
 * @retval SIM_ERROR for a comms error, SIM_NOT_READY for anything other than
 *         ready, or SIM_READY when the SIM is ready
 */
ModemSIMStatus_t ModemGetSIMStatus (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CPIN:", true }, { "OK", false } ),
                                     "AT+CPIN?");
    ModemSIMStatus_t sim_status;
    if (status) {
        if (strcasecmp (ModemLLGetRxStoredBuffer(), "+CPIN: READY") == 0)
            sim_status = SIM_READY;
        else
            sim_status = SIM_NOT_READY;
    } else {
        sim_status = SIM_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem SIM status: ");
    switch (sim_status) {
        case SIM_READY: printf ("ready"); break;
        case SIM_NOT_READY: printf ("not ready: \"%s\"", ModemLLGetRxStoredBuffer ()); break;
        default: printf ("*** error ***"); break;
    }
    printf ("\r\n");
#endif

    return sim_status;
}

/**
 * @brief wait for the modem's SIM to be ready
 * @param n_retries the number of times to retry the check
 * @retval true if the SIM is ready, false otherwise
 */
bool ModemCheckSIMReady (int n_retries) {
    ModemSIMStatus_t sim_status = SIM_ERROR;
    for (int count=0; count<n_retries && sim_status != SIM_READY; count ++) {
        sim_status = ModemGetSIMStatus ();
        if (sim_status != SIM_READY)
            HAL_Delay (TWO_SEC_TIMEOUT);
    }
    if (sim_status == SIM_READY) return true;
    return false;
}

/**
 * @brief get the status of the modem's registration with the LTE mobile network
 * 
 * The modem may be registered on 2G/3G networks for voice or data, but this
 * check is only for 4G LTE operation (which is required for internet
 * communications).
 * 
 * @retval REG_ERROR for a comms error, REG_NOT_REGISTERED for anything other than
 *         network connection, or SIM_REGISTERED when the modem is registered
 */
ModemRegStatus_t ModemGetRegistrationStatus (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CEREG:", true }, { "OK", false } ),
                                     "AT+CEREG?");
    int reg_code;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CEREG: %*d,%d", &reg_code) != 1)
        status = false;
    ModemRegStatus_t reg_status;
    if (status) {
        if (reg_code == 1 || reg_code == 5)
            reg_status = REG_REGISTERED;
        else
            reg_status = REG_NOT_REGISTERED;
    } else {
        reg_status = REG_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem network registration status: ");
    switch (reg_status) {
        case REG_REGISTERED: printf ("registered (%d)", reg_code); break;
        case REG_NOT_REGISTERED: printf ("not registered (%d)", reg_code); break;
        default: printf ("*** error ***"); break;
    }
    printf ("\r\n");
#endif

    return reg_status;
}

/**
 * @brief wait for the modem to be registered on the mobile network
 * @param n_retries the number of times to retry the check
 * @retval true if the modem is registered, false otherwise
 */
bool ModemCheckRegistered (int n_retries) {
    ModemRegStatus_t reg_status = REG_ERROR;
    for (int count=0; count<n_retries && reg_status != REG_REGISTERED; count ++) {
        reg_status = ModemGetRegistrationStatus ();
        if (reg_status != REG_REGISTERED)
            HAL_Delay (TWO_SEC_TIMEOUT);
    }
    if (reg_status == REG_REGISTERED) return true;
    return false;
}

/**
 * @brief get the signal strength of the mobile network
 * @retval the signal strength in dBm (which will be a negative number) or 999 for an error
 */
int ModemGetSignalStrength () {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CSQ:", true }, { "OK", false } ),
                                     "AT+CSQ");
    int rssi, rssi_code;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CSQ: %d", &rssi_code) == 1) {
        if (rssi_code == 99) rssi = 999;
        else if (rssi_code == 0) rssi = -113;
        else if (rssi_code >= 31) rssi = -51;
        else rssi = -113 + (2 * rssi_code);
    } else {
        rssi = 999;
    }
    
#if DEBUG >= 2
    printf ("Modem received signal strength: %ddBm\r\n", rssi);
#endif

    return rssi;
}

/**
 * @brief check that the modem's internet stack is available
 * 
 * The packet domain service allows the modem to send and receive TCP traffic.
 * 
 * @retval PD_ERROR for a comms error, PD_DETACHED for anything other than
 *         packet domain connection, or PD_ATTACHED when the modem is registered
 */
ModemIPStatus_t ModemGetIPState (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+NETOPEN:", true }, { "OK", false } ),
                                     "AT+NETOPEN?");    
    int state;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+NETOPEN: %d", &state) != 1)
        status = false;
    ModemIPStatus_t ret_val;
    if (status) {
        if (state == 0)
            ret_val = IPSTAT_CLOSED;
        else
            ret_val = IPSTAT_OPEN;
    } else {
        ret_val = IPSTAT_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem IP status: ");
    switch (ret_val) {
        case IPSTAT_OPEN: printf ("open"); break;
        case IPSTAT_CLOSED: printf ("closed"); break;
        default: printf ("*** error ***"); break;
    }
    printf ("\r\n");
#endif

    return ret_val;
}

/**
 * @brief bring up the modem's internet stack
 * @param apn the access point name to use
 * @retval true if the modem was attached to the packet domain service
 */
bool ModemOpenIP (const char *apn) {
	// turn off PAP / CHAP authentication
	bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "OK", false } ),
                                     "AT+CGAUTH=%d,0", NETWORK_CID);
#if DEBUG >= 2
    printf ("Modem PAP/CHAP authentication on CID %d %s\r\n", NETWORK_CID, status ? "turned off" : "*not* turned off");
#endif

	
    // set the modem's access point name
    if (status)
        status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                    RESPONSE_LIST( { "OK", false } ),
                                    "AT+CGDCONT=%d,\"IP\",\"%s\"", NETWORK_CID, apn);
#if DEBUG >= 2
    printf ("Modem APN \"%s\" on CID %d %s\r\n", apn, NETWORK_CID, status ? "set" : "*not* set");
#endif

    // start network operations
    if (status) {
        status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TWO_MIN_TIMEOUT,
                                  RESPONSE_LIST( { "ERROR", false }, {"+IP ERROR", true} ),
                                  RESPONSE_LIST( { "OK", false }, { "+NETOPEN:", false } ),
                                  "AT+NETOPEN=%d", NETWORK_CID);
        if (! status) {
            // if the modem responds "+IP ERROR" the packet domain is already attached
            if (strcasecmp (ModemLLGetRxStoredBuffer (), "+IP ERROR: Network is already opened") == 0) {
                status = true;
                // there should still be an "ERROR" response from the modem - read it
                ModemLLReceiveLine (ONE_SEC_TIMEOUT);
            }
        }
#if DEBUG >= 2
    printf ("Modem packet domain %s\r\n", status ? "attached" : "*not* attached");
#endif
    }

    return status;
}

/**
 * @brief bring down the modem's internet stack
 * @retval true if the modem was detached from the packet domain service
 */
bool ModemCloseIP (void) {
    bool status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TWO_MIN_TIMEOUT,
                                   RESPONSE_LIST( { "ERROR", false } ),
                                   RESPONSE_LIST( { "+NETCLOSE:", true } ),
                                   "AT+NETCLOSE=%d", NETWORK_CID);
    // there may be an "OK" or "ERROR" still to be picked up
    ModemLLReceiveLine (ONE_SEC_TIMEOUT);

#if DEBUG >= 2
    printf ("Modem packet domain %s\r\n", status ? "detached" : "*not* detached");
#endif

    return status;
}

/**
 * @brief check that the modem's internet stack is up
 * @param n_retries the number of times to retry the check
 * @retval true if the packet domain is attached, false otherwise
 */
bool ModemCheckIPOpened (const char *apn, int n_retries) {
    ModemIPStatus_t ip_status = ModemGetIPState ();
    for (int count=0; count<n_retries && ip_status != IPSTAT_OPEN; count ++) {
        ModemOpenIP (apn);
        ip_status = ModemGetIPState ();
    }
    return ip_status == IPSTAT_OPEN;
}

/**
 * @brief check that the modem's internet stack is down
 * @param n_retries the number of times to retry the check
 * @retval true if the packet domain is detached, false otherwise
 */
bool ModemCheckIPClosed (int n_retries) {
    ModemIPStatus_t ip_status = ModemGetIPState ();
    for (int count=0; count<n_retries && ip_status != IPSTAT_CLOSED; count ++) {
        ModemCloseIP ();
        ip_status = ModemGetIPState ();
    }
    return ip_status == IPSTAT_CLOSED;
}

/** 
 * @brief update the modem's real-time-clock from one of a list of
 *        http servers
 * 
 * The modem's internet stack must be up before calling this function
 * 
 * @param htp_hosts array of strings listing hosts to be used for HTP time query
 * @param n_htp_hosts length of the list
 * @param n_retries number of times to retry
 * @retval true if the time was received OK, false for an error
 */
bool ModemSyncToHTPTime (const char *htp_hosts[], int n_htp_hosts, int n_retries) {

#if DEBUG >= 2
	printf ("Modem syncing to network time\r\n");
#endif

	bool status = false;
	for (int retry=0; (retry<n_retries) && (! status); retry ++) {
		// bind the HTP protocol to the CID we are using
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
								    RESPONSE_LIST( { "OK", false } ),
								    "AT+CHTPCFG=\"CID\",%d", NETWORK_CID);   
                                     
		// remove all HTP servers from the modem's pool - ignore any errors
		for (int count=0; count<N_HTP_SERVERS && status; count ++) {
			ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
							   RESPONSE_LIST( { "OK", false } ),
							   "AT+CHTPSERV=\"DEL\",%d", count);   
		} 

		// add given HTP hosts to the modem's pool
		for (int count=0; count<n_htp_hosts && status; count ++) {
			status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
										RESPONSE_LIST( { "OK", false } ),
										"AT+CHTPSERV=\"ADD\",\"%s\",80,1", htp_hosts[count]);
		}
										
		// update the modem's real-time-clock using HTP
		if (status)
			status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, ONE_MIN_TIMEOUT,
									  RESPONSE_LIST( { "ERROR", false } ),
									  RESPONSE_LIST( { "OK", false } ),
									  "AT+CHTPUPDATE");
	}

#if DEBUG >= 2
	printf ("Modem %s to network time\r\n", status ? "synced" : "*not* synced");
#endif
	return status;
}

/**
 * @brief get the modems real-time-clock time
 * 
 * For a useful time value, the modem must have been synchronised to a network
 * time source before calling this function.
 * 
 * @param rtc_time the returned time as a UNIX epoch time
 * @param n_retries the number of times to retry the command
 * @retval true if time retrieved OK, false otherwise
 */
bool ModemGetRTCTime (time_t *rtc_time, int n_retries) {

	bool status = false;
	for (int retry=0; (retry<n_retries) && (! status); retry ++) {
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                    RESPONSE_LIST( { "+CCLK:", true }, { "OK", false} ),
                                    "AT+CCLK?");
             
		if (status) {
			*rtc_time = ModemParseCCLKToUTC (ModemLLGetRxStoredBuffer ());
			if (*rtc_time == (time_t) -1) {
				// there was an error parsing the time - assume the response
				// was corrupt and retry
				status = false;
				*rtc_time = 0;
			} else if (*rtc_time < (86400 * 365)) {
				// the time is within one year of the start epoch
				// the real-time clock has not yet been updated
				// (the HTP synchronisation takes place in the
				// background after the AT+CHTPUPDATE command has returned
				HAL_Delay (ONE_SEC_TIMEOUT);
				status = false;
			}
		}
	}

#if DEBUG >= 2
	char *time_str = asctime(gmtime(rtc_time));
	printf ("Modem RTC time %s: %.*s\r\n", status ? "retrieved ok" : "*not* retrieved ok", strlen (time_str) -1, time_str);
#endif

    return status;
}

/**
 * @brief Parses a SIMCom AT+CCLK response string and returns a pure UTC Unix Epoch.
 * 
 * Target format: "+CCLK: \"yy/MM/dd,hh:mm:ss±tz\""
 * Example:       "+CCLK: \"26/10/05,21:46:05+00\""
 *
 * @param cclk_str Raw modem response string buffer.
 * @return time_t  Pure UTC Unix epoch timestamp, or (time_t)-1 on parse error.
 */
time_t ModemParseCCLKToUTC (const char *cclk_str) {
    int year, month, day, hour, min, sec, tz_quarters;
    char tz_sign;

    // Locate starting quote of the CCLK string
    const char *start = strchr(cclk_str, '"');
    if (!start) {
        return (time_t)-1;
    }
    start++; // Advance past the quote

    // Match all 8 fields: yy, MM, dd, hh, mm, ss, sign (+/-), and quarter-hour tz offset
    int matched = sscanf(start, "%2d/%2d/%2d,%2d:%2d:%2d%c%2d",
                         &year, &month, &day,
                         &hour, &min, &sec,
                         &tz_sign, &tz_quarters);

    if (matched < 8) {
        return (time_t)-1;
    }

    struct tm tm_time;
    memset(&tm_time, 0, sizeof(struct tm));

    // Convert to standard C tm struct ranges
    tm_time.tm_year = (year >= 70) ? year : (100 + year); // 26 -> 126 (Year 2026)
    tm_time.tm_mon  = month - 1;                           // struct tm months: 0-11
    tm_time.tm_mday = day;                                 // 1-31
    tm_time.tm_hour = hour;                                // 0-23
    tm_time.tm_min  = min;                                 // 0-59
    tm_time.tm_sec  = sec;                                 // 0-59
    tm_time.tm_isdst = 0;                                  // Explicitly disable DST

    // Convert broken-down UTC time struct to epoch
    time_t utc_epoch = (time_t)-1;

#if defined(_GNU_SOURCE) || defined(__USE_MISC) || defined(_BSD_SOURCE)
    // Direct POSIX UTC conversion (Available in GNU/glibc, Newlib, and FreeRTOS GCC toolchains)
    utc_epoch = timegm(&tm_time);
#else
    // Custom lightweight math for embedded platforms lacking timegm()
    // Calculates UTC epoch directly from struct tm components without touching local environment
    static const int days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    int y = tm_time.tm_year + 1900;
    int m = tm_time.tm_mon;
    int d = tm_time.tm_mday - 1;

    // Count leap years since 1970
    int leap_years = (y - 1969) / 4 - (y - 1901) / 100 + (y - 1601) / 400;
    long days_since_epoch = (y - 1970) * 365 + leap_years + days_before_month[m] + d;

    // Add leap day if current year is a leap year and past February
    if (m > 1 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) {
        days_since_epoch++;
    }

    utc_epoch = (time_t)(days_since_epoch * 86400 + 
                         tm_time.tm_hour * 3600 + 
                         tm_time.tm_min * 60 + 
                         tm_time.tm_sec);
#endif

    if (utc_epoch == (time_t)-1) {
        return (time_t)-1;
    }

    // Convert reported timezone quarter-hours to seconds
    int tz_offset_seconds = tz_quarters * 15 * 60;
    if (tz_sign == '-') {
        tz_offset_seconds = -tz_offset_seconds;
    }

    // Subtract reported timezone offset to ensure returned epoch is strictly UTC ground truth
    return (utc_epoch - tz_offset_seconds);
}
