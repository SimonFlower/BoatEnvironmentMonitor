.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb

.global g_pfnVectors
.global Reset_Handler

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  msr   msp, r0

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
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word 0               /* NMI */
  .word HardFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler     /* FreeRTOS SVC */
  .word 0
  .word 0
  .word PendSV_Handler  /* FreeRTOS PendSV */
  .word SysTick_Handler /* FreeRTOS SysTick */
  
