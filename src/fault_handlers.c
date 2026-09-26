/* various handlers for FreeRTOS fault conditions */

#include <stdlib.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32l432xx.h"

#include "led.h"
#include "system_funcs.h"

/**
 * @brief a FreeRTOS configASSERT handler
 * @return does not return
 */
__attribute__((noreturn))
void AssertFailed(const char *file, uint32_t line) {
#if DEBUG > 0
    usart2_send2 ("\r\n*** ASSERT: ");
    usart2_send2 (file);
    usart2_send2 (":");
    char string [15];
    usart2_send2 (itoa (line, string, 10));
    usart2_send2 (" ***\r\n");
#endif

    taskDISABLE_INTERRUPTS();
    BlinkLEDForever (LED_ASSERT, pdFALSE);
}

/**
 * @brief C-level hard fault handler
 *
 * Called by the HardFault_Handler trampoline below with a pointer to the
 * stack frame that the CPU automatically pushed when it took the fault:
 * {R0, R1, R2, R3, R12, LR, PC, xPSR}. stacked_regs[6] (the stacked PC) is
 * the address of the instruction that actually faulted, and is the single
 * most useful piece of information here - look it up with
 * "arm-none-eabi-addr2line -e build/app.elf -f -C <address>" to find the
 * exact source line.
 *
 * @param stacked_regs pointer to the automatically-stacked exception frame
 */
__attribute__((noreturn))
void HardFault_HandlerC(uint32_t *stacked_regs) {
#if DEBUG > 0
    usart2_send2 ("\r\n*** HARD FAULT ***\r\n");

    // Verify pointer lies within STM32L432 64KB RAM boundaries
    uint32_t addr = (uint32_t)stacked_regs;
    if (addr >= 0x20000000 && addr <= (0x20000000 + (64 * 1024) - 32)) {
        PrintHex32 ("R0  ", stacked_regs[0]);
        PrintHex32 ("R1  ", stacked_regs[1]);
        PrintHex32 ("R2  ", stacked_regs[2]);
        PrintHex32 ("R3  ", stacked_regs[3]);
        PrintHex32 ("R12 ", stacked_regs[4]);
        PrintHex32 ("LR  ", stacked_regs[5]);
        PrintHex32 ("PC  ", stacked_regs[6]); // Faulting instruction
        PrintHex32 ("PSR ", stacked_regs[7]);

        // Print caller stack history (words 8 to 20 above exception frame)
        usart2_send2 ("-- Stack Dump --\r\n");
        for (int i = 8; i < 20; i++)
            PrintHex32 ("SP+", stacked_regs[i]);
    } else {
        usart2_send2 ("Stacked regs pointer invalid (Stack Corrupted!)\r\n");
    }

    // SCB System Control Registers are always safe to read from Flash/Internal space
    PrintHex32 ("CFSR", SCB->CFSR);
    PrintHex32 ("HFSR", SCB->HFSR);
    PrintHex32 ("MMAR", SCB->MMFAR);
    PrintHex32 ("BFAR", SCB->BFAR);
#endif

    // Disable interrupts and signal fault via LED pattern
    taskDISABLE_INTERRUPTS();
    BlinkLEDForever (LED_HARD_FAULT, pdFALSE);
}

/**
 * @brief Hard fault handler entry point - referenced directly from the
 *        vector table.
 *
 * This has to be a tiny "naked" trampoline rather than an ordinary C
 * function: by the time a normal function's prologue has run, we've
 * already lost access to the exception stack frame that tells us *why*
 * we faulted. All this does is work out whether the Main stack (MSP) or
 * Process stack (PSP) was active when the fault happened (bit 2 of the
 * EXC_RETURN value that lands in LR on exception entry tells us that),
 * and tail-calls into HardFault_HandlerC() with that stack pointer as
 * its argument (r0, per the standard calling convention).
 */
__attribute__((naked))
void HardFault_Handler(void) {
    __asm volatile
    (
        " tst lr, #4                \n"
        " ite eq                    \n"
        " mrseq r0, msp             \n"
        " mrsne r0, psp             \n"
        " ldr r1, =HardFault_HandlerC \n"
        " bx r1                     \n"
    );
// Old Claude code
//    (
//        " tst lr, #4                \n"
//        " ite eq                    \n"
//        " mrseq r0, msp             \n"
//        " mrsne r0, psp             \n"
//        " b HardFault_HandlerC      \n"
//    );
}

/**
 * @brief Called by FreeRTOS when pvPortMalloc fails to allocate memory 
 */
__attribute__((noreturn))
void vApplicationMallocFailedHook(void) {
#if DEBUG >= 1
    usart2_send2 ("\r\n*** Malloc failure handler called ***\r\n");
#endif

    // Disable interrupts and signal fault via LED pattern
    taskDISABLE_INTERRUPTS();
    BlinkLEDForever(LED_INIT_ERR, pdFALSE);
}

/* Called by FreeRTOS when a task overflows its stack */
__attribute__((noreturn))
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    
#if DEBUG >= 1
    usart2_send2 ("\r\n*** STACK OVERFLOW: ");
    usart2_send2 (pcTaskName);
    usart2_send2 (" ***\r\n");
#endif
    
    // Disable interrupts and signal fault via LED pattern
    taskDISABLE_INTERRUPTS();
    BlinkLEDForever(LED_HARD_FAULT, pdFALSE);
}
