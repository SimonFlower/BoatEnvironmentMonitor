#ifndef MODEM_AT_H
#define MODEM_AT_H

// a structure for gathering modem status information
typedef enum {SIM_READY,
			  SIM_WAIT_PIN,
			  SIM_WAIT_PUK,
			  SIM_WAIT_PASS,
		      SIM_WAIT_PIN2,
			  SIM_WAIT_PUK2,
			  SIM_WAIT_NET_PASS,
			  SIM_CRASH
} ModemSIMStatus_t;
typedef struct {
	ModemSIMStatus_t sim_status;
	int op_sel_mode;	// operator selection mode
	int rssi;			// received signal strength indication code for strength in dBm
	int ber;			// bit error rate code for error rate in %
	int reg_stat;		// registration status
	int pd_attach_stat;	// packet domain attached status
} ModemStatus_t;

void ModemStart (void (*periodic_cb)(void));
bool ModemTest (int n_retries);
bool ModemGetStatus (ModemStatus_t *modem_status);
bool ModemIPConnect (ModemStatus_t *modem_status, const char *apn);
bool ModemIsIPActive(void);
bool ModemIPDisconnect (void);

#if DEBUG > 0
char *ModemDecodeSIMStatus (ModemSIMStatus_t sim_status);
char *ModemDecodeOpSelMode (int op_sel_mode);
char *ModemDecodeRSSI (int rssi);
char *ModemDecodeBER (int ber);
char *ModemDecodeRegStat (int reg_stat);
char *ModemDecodePDAttachStat (int pd_attach_stat);
#endif

#endif /* MODEM_AT_H */
