/* System console and debug logging routines */

#ifndef DEBUG_H
#define DEBUG_H

void DebugInit (int dbg_lvl);
void DebugPrint(int msg_level, char *format, ...);
void PrintDiagnostics (void);
int __io_putchar(int ch);

#endif /* DEBUG_H */
