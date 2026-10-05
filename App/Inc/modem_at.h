#ifndef MODEM_AT_H
#define MODEM_AT_H

#include <time.h>

// return values when checking the modem's functionality mode
typedef enum {FUNC_ERROR = -1, FUNC_FULL = 1, FUNC_LIMITED = 2} ModemFunctionality_t;

// return values when checking the modem's operator selection mode
typedef enum {OPER_SEL_ERROR = -1, OPER_SEL_AUTO = 1, OPER_SEL_NOT_AUTO = 2} ModemOperSelMode_t;

// return values when checking the modem's SIM status
typedef enum {SIM_ERROR = -1, SIM_READY = 1, SIM_NOT_READY = 2} ModemSIMStatus_t;

// return values when checking the modem's registration with the mobile network
typedef enum {REG_ERROR = -1, REG_REGISTERED = 1, REG_NOT_REGISTERED} ModemRegStatus_t;

// return values when checking the modem's Internet (IP) status
typedef enum {IPSTAT_ERROR = -1, IPSTAT_OPEN = 1, IPSTAT_CLOSED = 2} ModemIPStatus_t;

void ModemStart (void (*periodic_cb)(void));
bool ModemTestComms (int n_retries);

ModemFunctionality_t ModemGetFunctionality (void);
bool ModemSetFullFunctionality (bool reset);
bool ModemCheckFullFunctionality (int n_retries);

ModemOperSelMode_t ModemGetOperSelMode (void);
bool ModemSetAutoOperSelMode (void);
bool ModemCheckAutoOperSelMode (int n_retries);

ModemSIMStatus_t ModemGetSIMStatus (void);
bool ModemCheckSIMReady (int n_retries);

ModemRegStatus_t ModemGetRegistrationStatus (void);
bool ModemCheckRegistered (int n_retries);

int ModemGetSignalStrength ();

ModemIPStatus_t ModemGetIPState (void);
bool ModemOpenIP (const char *apn);
bool ModemCloseIP (void);
bool ModemCheckIPOpened (const char *apn, int n_retries);
bool ModemCheckIPClosed (int n_retries);

bool ModemSyncToHTPTime (const char *htp_hosts[], int n_htp_hosts, int n_retries);
bool ModemGetRTCTime (time_t *rtc_time, int n_retries);
time_t ModemParseCCLKToUTC (const char *cclk_str);

#endif /* MODEM_AT_H */
