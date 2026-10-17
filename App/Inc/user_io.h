/* System console and debug logging routines */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stdint.h>

void DebugInit (int dbg_lvl);
void DebugPrint(int msg_level, bool print_header, bool print_trailer, char *format, ...);
void DebugDump (int msg_level, const char *format, const char *buffer, int len, ...);
void DebugDump2 (int msg_level, const char *format, const uint32_t *array, int len, ...);
void PrintDiagnostics (void);
int __io_putchar(int ch);

#endif /* DEBUG_H */
