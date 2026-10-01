#ifndef MODEM_LL_H
#define MODEM_LL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// structures used to pass expected response strings into modem functions */
typedef struct {
	const char *expect;
	bool store;
} ModemResponse_t;
typedef struct {
    const ModemResponse_t *items;
    size_t count;
} ModemResponseList_t;

// a macro to help build response lists
#define RESPONSE_LIST(...)                                   \
    &(const ModemResponseList_t) {                           \
        .items = (const ModemResponse_t[]) { __VA_ARGS__ },  \
        .count = sizeof((const ModemResponse_t[])            \
                 { __VA_ARGS__ }) / sizeof(ModemResponse_t)  \
    }
    
void ModemLLInit(uint32_t dwfi_timeout, uint32_t dtx_timeout, uint32_t drx_timeout, void (*pcb)(void));
void ModemLLPower (bool on);
void ModemLLReset (void);

bool ModemLLWaitForIdle (uint32_t timeout);
bool ModemLLWaitForIdleDT (void);
bool ModemLLSendCommand(uint32_t timeout, const char *fmt, ...);
bool ModemLLSendCommandDT(const char *fmt, ...);
bool ModemLLReceiveLine (uint32_t timeout);
bool ModemLLReceiveLineDT (void);
bool ModemLLExpect (uint32_t timeout, const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list);
bool ModemLLExpectDT (const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list);
bool ModemLLTransact(uint32_t drain_timeout, uint32_t tx_timeout, uint32_t rx_timeout,
					 const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					 const char *fmt, ...);
bool ModemLLTransactDT(const ModemResponseList_t *fail_list, const ModemResponseList_t *succeed_list,
					   const char *fmt, ...);

char *ModemLLGetTxBuffer (void);
char *ModemLLGetRxBuffer (void);
char *ModemLLGetRxStoredBuffer (void);

#endif /* MODEM_LL_H */
