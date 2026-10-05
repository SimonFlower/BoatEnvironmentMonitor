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
#define ONE_SEC_TIMEOUT			    1000
#define TWO_SEC_TIMEOUT			    2000
#define FIVE_SEC_TIMEOUT			5000
#define TEN_SEC_TIMEOUT				10000
#define ONE_MIN_TIMEOUT				60000
#define TWO_MIN_TIMEOUT				120000
#define DEFAULT_DRAIN_TIMEOUT		0
#define DEFAULT_TX_TIMEOUT			100
#define DEFAULT_RX_TIMEOUT			TEN_SEC_TIMEOUT

/**
 * @brief initialise the modem and power it up
 * @param periodic_cb a callback that is called periodically during lengthy operations
 *        can be NULL
 */
void ModemStart (void (*periodic_cb)(void)) {
    // initialise the low level functions
    ModemLLInit (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, DEFAULT_RX_TIMEOUT, periodic_cb);
    // soft power on
    ModemLLPower (true);
}

/**
 * @brief check the modem is responding
 * @param n_retries number of times to retry the test
 * @retval true if the modem responded OK
 */
bool ModemTestComms (int n_retries) {
    bool status = false;
    for (int count=0; count<n_retries && ! found; count ++) {
        // send "AT" and wait for "OK"
        status = ModemLLTransact (FIVE_SEC_TIMEOUT, DEFAULT_TX_TIMEOUT, TEN_SEC_TIMEOUT,
                                 RESPONSE_LIST( { "ERROR", false } ),
                                 RESPONSE_LIST( { "OK", false } ),
                                 "AT");
                                 
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
        default: printf ("*** error ***");
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
    switch (func_code) {
        case FUNC_FULL: printf ("automatic"); break;
        case FUNC_LIMITED: printf ("not automatic (%d)", op_sel_mode_code); break;
        default: printf ("*** error ***");
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
                                     "AT+COPS=1");
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
        if (strcmp (ModemLLGetRxStoredBuffer(), "+CPIN: READY") == 0)
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
        default: printf ("*** error ***");
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
 * @brief get the status of the modem's registration with the mobile network
 * @retval REG_ERROR for a comms error, REG_NOT_REGISTERED for anything other than
 *         network connection, or SIM_REGISTERED when the modem is registered
 */
ModemRegStatus_t ModemGetRegistrationStatus (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CREG:", true }, { "OK", false } ),
                                     "AT+CREG?");
    int reg_code;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CREG: %*d,%d", &reg_code) != 1)
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
        case SIM_NOT_REGISTERED: printf ("not registered (%d)", reg_code); break;
        default: printf ("*** error ***");
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
        reg_status = ModemGetRegStatus ();
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
    if (status)
        status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                    RESPONSE_LIST( { "+CSQ:", true }, { "OK", false } ),
                                    "AT+CSQ");
    int rssi, rssi_code
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CSQ: %d", &rssi_code) == 1) {
        if (rssi_code == 99) rssi = 999;
        else if (rssi_code == 0) rssi -113;
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
 * @brief check that the modem is attached to the packet domain
 * 
 * The packet domain service allows the modem to send and receive TCP traffic.
 * 
 * @retval PD_ERROR for a comms error, PD_DETACHED for anything other than
 *         packet domain connection, or PD_ATTACHED when the modem is registered
 */
ModemPacketDomain_t ModemGetPDStatus (void) {
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "+CGATT:", true }, { "OK", false } ),
                                     "AT+CGATT?");    
    int state;
    if (status && sscanf (ModemLLGetRxStoredBuffer(), "+CGATT: %d", &state) != 1)
        status = false;
    ModemPacketDomain_t ret_val;
    if (status) {
        if (state == 0)
            ret_val = PD_DETACHED;
        else
            ret_val = PD_ATTACHED;
    } else {
        ret_val = PD_ERROR;
    }

#if DEBUG >= 2
    printf ("Modem packet domain status: ");
    switch (ret_val) {
        case PD_ATTACHED: printf ("attached");
        case PD_DETACHED: printf ("detached");
        default: printf ("*** error ***");
    }
    printf ("r\n");
#endif

    return ret_val;
}

/**
 * @brief attach to the packet domain service
 * @param apn the access point name to use
 * @retval true if the modem was attached to the packet domain service
 */
bool ModemAttachToPD (const char *apn) {
    // set the modem's access point name
    bool status = ModemLLTransactDT (RESPONSE_LIST( { "ERROR", false } ),
                                     RESPONSE_LIST( { "OK", false } ),
                                     "AT+CGDCONT=%d,\"IP\",\"%s\"", NETWORK_CID, apn);
#if DEBUG >= 2
    printf ("Modem APN %s on CID %d %s\r\n", apn, NETWORK_CID, status ? "set" : "*not* set");
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
 * @brief detach from the packet domain service
 * @retval true if the modem was detached from the packet domain service
 */
bool ModemDetachFromPD (void) {
    bool status = ModemLLTransact (DEFAULT_DRAIN_TIMEOUT, DEFAULT_TX_TIMEOUT, TWO_MIN_TIMEOUT,
                                   RESPONSE_LIST( { "ERROR", false } ),
                                   RESPONSE_LIST( {"+NETCLOSE:", true} ),
                                   "AT+NETCLOSE=%d", NETWORK_CID);
    // a response of "+NETCLOSE..." then "ERROR" indicates that the packet domain service
    // wasn't active - if this is the case, the success response list has terminated the
    // transaction, but there will still be an "Error" response waiting
    if (status)
        ModemLLReceiveLine (ONE_SEC_TIMEOUT);

#if DEBUG >= 2
    printf ("Modem packet domain %s\r\n", status ? "detached" : "*not* detached");
#endif

    return status;
}

/**
 * @brief check that the modem's packet domain service is attached
 * @param n_retries the number of times to retry the check
 * @retval true if the packet domain is attached, false otherwise
 */
bool ModemCheckPDAttached (const char *apn, int n_retries) {
    ModemPacketDomain_t pd_status = ModemGetPDStatus ();
    for (int count=0; count<n_retries && pd_status != PD_ATTACHED; count ++) {
        ModemAttachToPD ();
        pd_status = ModemGetPDStatus ();
    }
    return pd_status == PD_ATTACHED;
}

/**
 * @brief check that the modem's packet domain service is detached
 * @param n_retries the number of times to retry the check
 * @retval true if the packet domain is detached, false otherwise
 */
bool ModemCheckPDADetached (int n_retries) {
    ModemPacketDomain_t pd_status = ModemGetPDStatus ();
    for (int count=0; count<n_retries && pd_status != PD_DETACHED; count ++) {
        ModemDetachFromPD ();
        pd_status = ModemGetPDStatus ();
    }
    return pd_status == PD_DETACHED;
}

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
