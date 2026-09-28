#ifndef MODEM_LL_H
#define MODEM_LL_H

#include <stdint.h>

void ModemLLInit(void);
void ModemLLPower (bool on);
void ModemLLReset (void);

bool ModemLLDrainRx (uint32_t delay, uint32_t timeout);
bool ModemLLSendCommand (char *cmd, uint32_t timeout);
char *ModemLLReceiveLine (char *data, size_t length, uint32_t timeout);
bool ModemLLExpect (char *expect, uint32_t timeout);
bool ModemLLTransact (char *cmd, char *expect, uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout);

#endif /* MODEM_LL_H */
