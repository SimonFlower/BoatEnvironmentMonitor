.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global g_pfnVectors
.global Reset_Handler

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
/* Load top of stack and force 8-byte alignment */
  ldr   r0, =_estack
  mov   r1, #7
  bic   r0, r0, r1
  msr   msp, r0

  /* Enable 8-byte stack alignment in Configuration and Control Register (CCR) */
  ldr   r0, =0xE000ED14   /* SCB->CCR address */
  ldr   r1, [r0]
  orr   r1, r1, #(1 << 9) /* Set STKALIGN bit */
  str   r1, [r0]

  /* Copy .data from FLASH to RAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

  /* Zero fill .bss */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  /* Call SystemInit & main */
  bl SystemInit
  bl main

.size Reset_Handler, .-Reset_Handler

.section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .align 2
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word Default_Handler     /* NMI */
  .word HardFault_Handler
  .word Default_Handler     /* MemManage */
  .word Default_Handler     /* BusFault */
  .word Default_Handler     /* UsageFault */
  .word 0
  .word 0
  .word 0
  .word 0
  .word vPortSVCHandler     /* FreeRTOS SVC Handler */
  .word Default_Handler     /* DebugMon */
  .word 0
  .word xPortPendSVHandler  /* FreeRTOS PendSV Handler */
  .word xPortSysTickHandler /* FreeRTOS SysTick Handler */

  .weak vPortSVCHandler
  .thumb_set vPortSVCHandler, Default_Handler

  .weak xPortPendSVHandler
  .thumb_set xPortPendSVHandler, Default_Handler

  .weak xPortSysTickHandler
  .thumb_set xPortSysTickHandler, Default_Handler

  .section .text.Default_Handler,"ax",%progbits
  .global Default_Handler
  .type Default_Handler, %function
Default_Handler:
  /* Call C HardFault_Handler to display LED error pattern */
  bl HardFault_Handler
  .size Default_Handler, .-Default_Handler
  
