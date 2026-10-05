#ifndef MODEM_AT_H
#define MODEM_AT_H

// return values when checking the modem's functionality mode
typedef enum {FUNC_ERROR = -1, FUNC_FULL = 1, FUNC_LIMITED = 2} ModemFunctionality_t;

// return values when checking the modem's operator selection mode
typedef enum {OPER_SEL_ERROR = -1, OPER_SEL_AUTO = 1, OPER_SEL_NOT_AUTO = 2} ModemOperSelMode_t;

// return values when checking the modem's SIM status
typedef enum {SIM_ERROR = -1, SIM_READY = 1, SIM_NOT_READY = 2} ModemSIMStatus_t;

// return values when checking the modem's registration with the mobile network
typedef enum {REG_ERROR = -1, REG_REGISTERED = 1, REG_NOT_REGISTERED} ModemRegStatus_t;

// return values when checking that the modem is attached to the packet domain
typedef enum {PD_ERROR = -1, PD_ATTACHED = 1, PD_DETACHED = 2} ModemPacketDomain_t;

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

ModemPacketDomain_t ModemGetPDStatus (void);
bool ModemAttachToPD (const char *apn);
bool ModemDetachFromPD (void);
bool ModemCheckPDAttached (const char *apn, int n_retries);
bool ModemCheckPDADetached (int n_retries);

#if DEBUG > 0
char *ModemDecodeSIMStatus (ModemSIMStatus_t sim_status);
char *ModemDecodeOpSelMode (int op_sel_mode);
char *ModemDecodeRSSI (int rssi);
char *ModemDecodeBER (int ber);
char *ModemDecodeRegStat (int reg_stat);
char *ModemDecodePDAttachStat (int pd_attach_stat);
#endif

#endif /* MODEM_AT_H */
