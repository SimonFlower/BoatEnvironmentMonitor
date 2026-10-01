/** AT command level functions for managing the Clipper LGE 4G modem
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "debug.h"
#include "modem_ll.h"
#include "modem_at.h"

// network connection details
// channel identifier
#define NETWORK_CID		1

// timeouts for interactions with the modem
#define DEFAULT_DRAIN_TIMEOUT		0
#define DEFAULT_TX_TIMEOUT			100
#define DEFAULT_RX_TIMEOUT			2000
#define TEST_DRAIN_TIMEOUT			3000
#define TEST_RX_TIMEOUT				5000
#define FIVE_SEC_TIMEOUT			5000
#define TEN_SEC_TIMEOUT				10000
#define ONE_MIN_TIMEOUT				60000
#define TWO_MIN_TIMEOUT				120000

/**
 * @brief initialise the modem and power it up
 * @param periodic_cb a callback that is called periodically during lengthy operations
 *        can be NULL
 */
void ModemStart (void (*periodic_cb)(void)) {
	ModemLLInit (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, DEFAULT_RX_TIMEOUT, periodic_cb);
	ModemLLPower (true);
}

/**
 * @brief check the modem is responding
 * @param n_retries number of times to retry the test
 * @retval true if the modem responded OK
 */
bool ModemTest (int n_retries) {
	bool found = false;
    for (int count=0; count<n_retries && ! found; count ++) {
#if DEBUG >= 2
		printf ("Modem test retry %d of %d\r\n", count +1, n_retries);
#endif
		// send "AT" and wait for "OK"
		found = ModemLLTransact (TEST_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEST_RX_TIMEOUT,
								 RESPONSE_LIST( { "ERROR", false } ),
								 RESPONSE_LIST( { "OK", false } ),
								 "AT");
								 
		if (found)
			found = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									   RESPONSE_LIST( { "OK", false } ),
									   "ATI");
                
		// if the modem didn't respond, try resetting it
		if (! found)
			ModemLLReset ();
	}
	
#if DEBUG >= 2
		printf ("Modem test completed %s\r\n", found ? "successfully" : "unsuccessfully");
#endif	
	return found;
}

/**
 * @brief Collect key status information about the modem and its network connection
 * @param modem_status the modem status
 * @retval true if status was gathered OK
 */
bool ModemGetStatus (ModemStatus_t *modem_status) {
#if DEBUG >= 2
	printf ("Modem collecting status...\r\n");
#endif

	bool status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								   RESPONSE_LIST( { "ERROR", false } ),
								   RESPONSE_LIST( { "+CPIN:", true }, { "OK", false } ),
								   "AT+CPIN?");
	if (status) {
		if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: READY") == 0)
			modem_status->sim_status = SIM_READY;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: SIM PIN") == 0)
			modem_status->sim_status = SIM_WAIT_PIN;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: SIM PUK") == 0)
			modem_status->sim_status = SIM_WAIT_PUK;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: PH-SIM PIN") == 0)
			modem_status->sim_status = SIM_WAIT_PASS;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: SIM PIN2") == 0)
			modem_status->sim_status = SIM_WAIT_PIN2;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: SIM PUK2") == 0)
			modem_status->sim_status = SIM_WAIT_PUK2;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: PH-NET PIN") == 0)
			modem_status->sim_status = SIM_WAIT_NET_PASS;
		else if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: SIM CRASH") == 0)
			modem_status->sim_status = SIM_CRASH;
		else
			status = false;
	}
	if (status)
		status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, ONE_MIN_TIMEOUT,
								  RESPONSE_LIST( { "ERROR", false } ),
								  RESPONSE_LIST( { "+COPS:", true }, { "OK", false } ),
                                  "AT+COPS?");
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+COPS: %d", &(modem_status->op_sel_mode)) != 1)
		status = false;	
	if (status)
		status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								  RESPONSE_LIST( { "ERROR", false } ),
								  RESPONSE_LIST( { "+CSQ:", true }, { "OK", false } ),
								  "AT+CSQ");
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CSQ: %d,%d", &(modem_status->rssi), &(modem_status->ber)) != 2)
		status = false;
	if (status)
	    status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								   RESPONSE_LIST( { "ERROR", false } ),
								   RESPONSE_LIST( { "+CREG:", true }, { "OK", false } ),
								   "AT+CREG?");
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CREG: %*d,%d", &(modem_status->reg_stat)) != 1)
		status = false;
	if (status)
	    status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								   RESPONSE_LIST( { "ERROR", false } ),
								   RESPONSE_LIST( { "+CGATT:", true }, { "OK", false } ),
								   "AT+CGATT?");
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CGATT: %d", &(modem_status->pd_attach_stat)) != 1)
		status = false;

#if DEBUG >= 2
	if (status) {
		printf ("Modem status:\r\n");
		printf ("  SIM status: %s\r\n", ModemDecodeSIMStatus (modem_status->sim_status));
		printf ("  Operator selection mode: %s (%d)\r\n", ModemDecodeOpSelMode (modem_status->op_sel_mode), modem_status->op_sel_mode);
		printf ("  RSSI: %s (%d)\r\n", ModemDecodeRSSI (modem_status->rssi), modem_status->rssi);
		printf ("  BER: %s (%d)\r\n", ModemDecodeBER (modem_status->ber), modem_status->ber);
		printf ("  Registration status: %s (%d)\r\n", ModemDecodeRegStat (modem_status->reg_stat), modem_status->reg_stat);
		printf ("  Packet domain attach status: %s (%d)\r\n", ModemDecodePDAttachStat (modem_status->pd_attach_stat), modem_status->pd_attach_stat);
	} else {
		printf ("Modem could not get status information\r\n");
	}
#endif

	return status;
}


/**
 * @brief make an IP data connection to the mobile network
 * @param modem_status the modem status as retrieved by ModemGetStatus()
 * @param apn the access point name
 * @retval true if the connection was made
 */
bool ModemIPConnect (ModemStatus_t *modem_status, const char *apn) {
	(void) modem_status;
#if DEBUG >= 2
	printf ("Modem connecting to internet...\r\n");
#endif

	bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
								     RESPONSE_LIST( { "OK", false } ),
									 "AT+GSN");
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
									"AT+CPMUTEMP");
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
	                                "AT+CIMI");
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
                                    "AT+CICCID");
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
                                    "AT+CGATT?");

	// diagnostics
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
                                    "AT+CEREG?");
	if (status)
		status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
                                    "AT+CFUN?");
	if (status)
	    status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
									RESPONSE_LIST( { "OK", false } ),
                                    "AT+CSQ");

	// set APN
	if (status)
		status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								  RESPONSE_LIST( { "ERROR", false } ),
  	                              RESPONSE_LIST( { "OK", false } ),
								  "AT+CGDCONT=%d,\"IP\",\"%s\"", NETWORK_CID, apn);

	// force network attachment if needed
	if (status && ! ModemIsIPActive())
		status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
								  RESPONSE_LIST( { "ERROR", false } ),
								  RESPONSE_LIST( { "+CGATT: 1", false }, { "OK", false } ),
								  "AT+CGATT=1");

	// start packet network operations
	if (status) {
		for (int retries=0; retries<2; retries ++) {
			status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TWO_MIN_TIMEOUT,
		    						  RESPONSE_LIST( { "ERROR", false } ),
								      RESPONSE_LIST( { "OK", false } ),
									  "AT+NETOPEN");
			if (status) break;
		}
	}

#if DEBUG >= 2
	printf ("Modem connection to internet %s\r\n", status ? "successful" : "unsuccessful");
#endif

	return status;
}


/**
 * @brief  Disconnects internet connection
 * @return true on success, false otherwise.
 */
bool ModemIPDisconnect (void) {
#if DEBUG >= 2
	printf ("Modem disconnecting from internet...\r\n");
#endif

	bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
								     RESPONSE_LIST( { "OK", false } ),
                                     "AT+NETCLOSE");

#if DEBUG >= 2
	printf ("Modem disconnection from internet %s\r\n", status ? "successful" : "unsuccessful");
#endif

	return status;
}

/**
 * @brief make an IP data connection to the mobile network
 * @param apn the access point name
 * @retval true if the connection was made
 */
//bool ModemIPConnect (char *apn) {
//#if DEBUG >= 2
	//printf ("Modem connecting to internet...\r\n");
//#endif

	//// Check SIM state
	//bool status = ModemLLSendCommandDT ("AT+CPIN?");
	//if (status)
		//status = ModemLLExpectDT ("ERROR", 2, "+CPIN: READY", "OK");
	
    //// Check GPRS/LTE attachment state
    //bool attached = false;
    //if (status)
		//status = ModemLLSendCommandDT ("AT+CGATT?");
	//if (status) {
//#if DEBUG >= 2
		//printf ("Modem searching for attachment state\r\n");
//#endif
		//status = ModemLLReceiveLineDT ();
	//}
	//if (status) {
		//int attach_state;
		//char *rx_buffer = ModemLLGetRxBuffer();
		//if (sscanf (rx_buffer, "+CGATT: %d", &attach_state) != 1)
			//status = false;
		//else if (attach_state == 1)
			//attached = true;
		//// clear the OK that's waiting in the receive buffer
		//status = ModemLLExpectDT ("ERROR", 1, "OK");
	//}

	//// if not attached try to force attachment
	//if (status && ! attached)
		//status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
		                          //"ERROR", "OK", "AT+CGATT=1");

	//// test whether there is an existing connection - if so, not need to connect
	//if (status && ! ModemIsIPActive ()) {
		//if (status)
			//status = ModemLLTransactDT ("ERROR", "OK", "AT+CGDCONT=%d,\"IP\",\"%s\"", NETWORK_CID, apn);
		
		//// 3GPP TS 27.007: Activate PDP Context 1 (State=1, CID=1)
		//if (status)
			//status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
			                          //"ERROR", "OK", "AT+CGACT=1,%d", NETWORK_CID);

		//// 3GPP TS 27.007: Show PDP Address for Context 1
		//if (status) {
			//status = ModemLLSendCommandDT ("AT+CGPADDR=%d", NETWORK_CID);
		//}
		//if (status) {
			//// Parse 3GPP response format: +CGPADDR: 1,"x.x.x.x"
			//status = ModemLLExpectDT ("ERROR", 2, "+CGPADDR: 1,\"", "OK");
		//}
	//}
	
//#if DEBUG >= 2
	//printf ("Modem connection to internet %s\r\n", status ? "successful" : "unsuccessful");
//#endif

	//return status;
//}

/**
 * @brief  Checks if PDP context 1 is already active.
 * @return true if active, false otherwise.
 */
bool ModemIsIPActive(void) {    
#if DEBUG >= 2
		printf ("Modem test connection to internet\r\n");
#endif

	char cgact_response [20];
	snprintf (cgact_response, sizeof (cgact_response), "+CGACT: %d", NETWORK_CID);
	bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
								     RESPONSE_LIST( { cgact_response, true }, { "OK", false } ),
								     "AT+CGACT?");
	int cid = 0;
	int attach_state = 0;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CGACT: %d,%d", &cid, &attach_state) != 1)
		status = false;
	else if (cid != NETWORK_CID)
		status = false;
	else if (attach_state <= 0)
		status = false;
	else
		status = true;

#if DEBUG >= 2
		printf ("Modem connection to internet on CID %d %s (attach state %d)\r\n", 
		        NETWORK_CID, status ? "active" : "inactive", attach_state);
#endif

    return status;
}

/**
 * @brief  Deactivates PDP context 1 using standard 3GPP commands.
 * @return true on success, false otherwise.
 */
//bool ModemIPDisconnect (void) {
//#if DEBUG >= 2
	//printf ("Modem disconnecting from internet...\r\n");
//#endif

    //// 3GPP TS 27.007: Deactivate PDP Context 1 (State=0, CID=1)
	//bool status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, FIVE_SEC_TIMEOUT,
	                               //"ERROR", "OK", "AT+CGACT=0,%d", NETWORK_CID);

//#if DEBUG >= 2
	//printf ("Modem disconnection from internet %s\r\n", status ? "successful" : "unsuccessful");
//#endif

	//return status;
//}

#if DEBUG > 0
char *ModemDecodeSIMStatus (ModemSIMStatus_t sim_status) {
	switch (sim_status) {
		case SIM_READY: return "READY, SIM is not pending for any password";
		case SIM_WAIT_PIN: return "SIM PIN, SIM is waiting SIM PIN to be given";
		case SIM_WAIT_PUK: return "SIM PUK, SIM is waiting SIM PUK to be given";
		case SIM_WAIT_PASS: return "PH-SIM PIN, SIM is waiting phone-to-SIM card password to be given";
		case SIM_WAIT_PIN2: return "SIM PIN2, SIM is waiting SIM PIN2 to be given";
		case SIM_WAIT_PUK2: return "SIM PUK2, SIM is waiting SIM PUK2 to be given";
		case SIM_WAIT_NET_PASS: return "PH-NET PIN, SIM is waiting network personalization password to be given";
		case SIM_CRASH: return "SIM CRASH, SIM initialization failed or SIM access encountered a serious error";
	}
	return "unknown code";
}

char *ModemDecodeOpSelMode (int op_sel_mode) {
	switch (op_sel_mode) {
		case 0: return "automatic";
		case 1: return "manual";
		case 2: return "force deregister";
		case 3: return "set only <format>";
		case 4: return "manual/automatic";
	}
	return "unknown code";
}

char *ModemDecodeRSSI (int rssi) {
	switch (rssi) {
		case 0: return "-113 dBm or less";
		case 1: return "-111 dBm";
		case 31: return "-51 dBm or greater";
		case 99: return "not known or not detectable";
		default:
			if (rssi >= 2 && rssi <= 30)
				return "-109 - -53 dBm";
			break;
	}
	return "unknown code";
}

char *ModemDecodeBER (int ber) {
	switch (ber) {
		case 0: return "< 0.01%";
		case 1: return "0.01% - 0.1%";
		case 2: return "0.1% - 0.5%";
		case 3: return "0.5% - 1.0%";
		case 4: return "1.0% - 2.0%";
		case 5: return "2.0% - 4.0%";
		case 6: return "4.0% - 8.0%";
		case 7: return ">= 8.0%";
		case 99: return "not known or not detectable";
	}
	return "unknown code";
}

char *ModemDecodeRegStat (int reg_stat) {
	switch (reg_stat) {
		case 0: return "not registered, not searching";
		case 1: return "registered, home network";
		case 2: return "not registered, but searching";
		case 3: return "registration denied";
		case 4: return "unknown";
		case 5: return "registered, roaming";
		case 6: return "registered for \"SMS only\", home network";	
	}
	return "unknown code";
}

char *ModemDecodePDAttachStat (int pd_attach_stat) {
	switch (pd_attach_stat) {
		case 0: return "detached";
		case 1: return "attached";
	}
	return "unknown code";
}
#endif
