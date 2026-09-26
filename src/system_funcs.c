/* a place to provide system functions to implement or override FreeRTOS functions */

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "stm32l4xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "debug.h"
#include "led.h"
#include "system_funcs.h"

#define RCC_AHB2ENR    (*((volatile uint32_t *)(RCC_BASE + 0x4C)))
#define RCC_APB1ENR1   (*((volatile uint32_t *)(RCC_BASE + 0x58)))
#define GPIOA_MODER    (*((volatile uint32_t *)(GPIOA_BASE + 0x00)))
#define GPIOA_AFRL     (*((volatile uint32_t *)(GPIOA_BASE + 0x20)))
#define USART2_CR1     (*((volatile uint32_t *)(USART2_BASE + 0x00)))
#define USART2_BRR     (*((volatile uint32_t *)(USART2_BASE + 0x0C)))
#define USART2_ISR     (*((volatile uint32_t *)(USART2_BASE + 0x1C)))
#define USART2_TDR     (*((volatile uint32_t *)(USART2_BASE + 0x28)))

extern char _end;          /* Defined by linker */
extern char _estack;       /* Defined by linker */

// forward declarations
static void usart2_init(void);

/** 
 * @brief implement the _sbrk system function to fix link errors when using the printf family of functions
 */
caddr_t _sbrk(int incr)
{
    static char *heap_end;

    if (heap_end == NULL)
    {
        heap_end = &_end;
    }

    char *prev_heap_end = heap_end;

    /* Simple heap/stack collision check */
    uintptr_t new_heap = (uintptr_t)heap_end + incr;
    uintptr_t stack_limit = (uintptr_t)&_estack - 1024;
    if (new_heap > stack_limit)
    {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end = (char *)new_heap;

    return (caddr_t)prev_heap_end;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _close(int file) {
    (void)file;
    return -1;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _isatty(int file) {
    (void)file;
    return 1;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _getpid(void) {
    return 1;
}

/**
 * @brief add a dummy system function to prevent linker errors when using the printf family of functions
 */
int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

#if DEBUG > 0
// Mutex handle for UART access
static SemaphoreHandle_t usart_mutex = NULL;

/**
 * @brief create usart mutex
 * 
 * Call this function before the FreeRTOS scheduler is started if you want
 * writes through printf functions to be thread safe - if this function isn't
 * called, you should only call printf functions from a single thread
 */
void createUsartMutex (void) {
    usart_mutex = xSemaphoreCreateMutex();
}
#endif

/**
 * @brief Low-level syscall override to route stdout/stderr to USART2
 * 
 * A mutex ensures that this call (and hence printf calls) are thread safe.
 * 
 * If DEBUG is zero the code compiles to a dummy function that does nothing
 */
int _write(int file, char *ptr, int len) {
#if DEBUG > 0
    // Check if the RTOS scheduler is running
    BaseType_t scheduler_running = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);

    // Acquire mutex if scheduler is active and mutex is valid
    if (scheduler_running && usart_mutex != NULL) {
        xSemaphoreTake(usart_mutex, portMAX_DELAY);
    }

    // send the data
    usart2_send (ptr, len);
    
    // Release mutex if acquired
    if (scheduler_running && usart_mutex != NULL) {
        xSemaphoreGive(usart_mutex);
    }
    
    return len;
#else
    (void)file;
    (void)ptr;
    // Silently discard output in release builds
    return len;
#endif
}

#if DEBUG > 0
static void usart2_init(void) {
    // 1. Enable GPIOA and USART2 clocks
    RCC_AHB2ENR |= (1 << 0);       // GPIOA clock
    RCC_APB1ENR1 |= (1 << 17);     // USART2 clock

    // 2. Configure PA2 (TX) as Alternate Function 7 (AF7 = USART2)
    GPIOA_MODER &= ~(3 << (2 * 2));
    GPIOA_MODER |=  (2 << (2 * 2)); // Alternate function mode
    GPIOA_AFRL  &= ~(0xF << (2 * 4));
    GPIOA_AFRL  |=  (7 << (2 * 4));  // AF7

    // 3. Set baud rate (assuming default 4 MHz MSI clock -> 4000000 / 115200 = 35)
    USART2_BRR = 35;

    // 4. Enable USART2 and Transmitter (UE bit 0, TE bit 3)
    USART2_CR1 |= (1 << 0) | (1 << 3);
}

/** 
 * @brief direct access to the serial port that connects with the host
 * 
 * Only use this in a situation where FreeRTOS is not available or is
 * compromised - this function doesn't use any FreeRTOS resources, but
 * is not thread safe.
 */
void usart2_send(const char *ptr, int len) {
    static int first = pdTRUE;
    if (first) {
        usart2_init ();
        first = pdFALSE;
    }

    for (int i = 0; i < len; i++) {
        // Wait until Transmit Data Register is empty (TXE bit 7)
        while (!(USART2_ISR & (1 << 7)));
        // Write byte to UART
        USART2_TDR = (uint8_t)ptr[i];
    }
}

/**
 * @brief send a null terminated string to the debugger USART
 * 
 * Only use this in a situation where FreeRTOS is not available or is
 * compromised - this function doesn't use any FreeRTOS resources, but
 * is not thread safe.
 */
void usart2_send2(const char *ptr) {
    usart2_send (ptr, strlen (ptr));
}

/**
 * @brief print a label and a 32-bit value as 8 hex digits, over the debug USART
 */
void PrintHex32 (const char *label, uint32_t value) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char string[13];
    int pos = 0;
    string[pos++] = '0';
    string[pos++] = 'x';
    for (int i = 0; i < 8; i++) {
        string[pos++] = hex_digits[(value >> (28 - i * 4)) & 0xF];
    }
    string[pos] = '\0';

    usart2_send2 (label);
    usart2_send2 (" = ");
    usart2_send2 (string);
    usart2_send2 ("\r\n");
}

#endif /* DEBUG */

/**
 * @brief Update HAL's clock from FreeRTOS
 */
extern void xPortSysTickHandler (void);
void SysTick_Handler (void) {
    HAL_IncTick();   // always safe - just increments a counter

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();   // only touch FreeRTOS once it's actually running
    }
}

