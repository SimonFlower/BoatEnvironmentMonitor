/* a place to provide system functions to implement or override FreeRTOS functions */

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>

#include "FreeRTOS.h"

#include "debug.h"

// Register definitions for USART2 (PA2 = TX, connected to ST-LINK)
#define RCC_BASE       0x40021000UL
#define GPIOA_BASE     0x48000000UL
#define USART2_BASE    0x40004400UL

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

#ifdef DEBUG
// Low-level syscall override to route stdout/stderr to USART2
int _write(int file, char *ptr, int len) {
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
    return len;
}

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
#endif /* DEBUG */
