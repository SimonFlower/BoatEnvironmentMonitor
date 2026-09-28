#ifndef MODEM_AT_H
#define MODEM_AT_H

void ModemStart (void);
bool ModemTest (int n_retries, void (*retry_callback)(void));
bool ModemIPConnect (char *apn);
bool ModemIsIPActive(void);
bool ModemIPDisconnect (void);

#endif /* MODEM_AT_H */
