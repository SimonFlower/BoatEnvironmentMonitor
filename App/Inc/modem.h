#ifndef MODEM_H
#define MODEM_H

void ModemInit(void);
void ModemPower (bool on);
void ModemReset (void);
bool ModemTest (int n_retries, void (*retry_callback)(void));

#endif /* MODEM_H */
