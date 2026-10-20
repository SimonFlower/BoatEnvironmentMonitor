/* System console and debug logging routines */

#ifndef USER_IO_H
#define USER_IO_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

bool UserIOInit (uint8_t dbg_lvl);
void DebugPrint(uint8_t msg_level, bool print_header, bool print_trailer, char *format, ...);
void DebugDump (uint8_t msg_level, const char *format, const char *buffer, int len, ...);
void DebugDump2 (uint8_t msg_level, const char *format, const uint32_t *array, int len, ...);
void PrintDiagnostics (void);
void PrintConfig (ConfigData_t *config);
bool UserIOReadLine (char *buf, size_t size);
int __io_putchar(int ch);

#endif /* USER_IO_H */
