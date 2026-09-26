#ifndef SYSTEM_FUNCS_H
#define SYSTEM_FUNCS_H

#include <sys/types.h>
#include <sys/stat.h>

#include "debug.h"

// forward declarations
void createUsartMutex (void);
caddr_t _sbrk(int incr);
int _close(int file);
int _fstat(int file, struct stat *st);
int _isatty(int file);
int _lseek(int file, int ptr, int dir);
int _read(int file, char *ptr, int len);
int _getpid(void);
int _kill(int pid, int sig);
int _write(int file, char *ptr, int len);
#if DEBUG > 0
void createUsartMutex (void);
void usart2_send (const char *ptr, int len);
void usart2_send2 (const char *ptr);
void PrintHex32 (const char *label, uint32_t value);
#endif

#endif /* SYSTEM_FUNCS_H */
