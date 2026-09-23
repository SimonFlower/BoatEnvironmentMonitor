#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
/* Ensure configCPU_CLOCK_HZ is non-zero */
#define configCPU_CLOCK_HZ ( ( SystemCoreClock != 0 ) ? SystemCoreClock : 4000000UL )
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
/* Increase heap size to accommodate Newlib reentrancy structures */
#define configTOTAL_HEAP_SIZE                   ((size_t)(20 * 1024)) // 20 KB Heap
// #define configTOTAL_HEAP_SIZE                   ((size_t)(16 * 1024)) // 16 KB Heap
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
  #define configPRIO_BITS                   __NVIC_PRIO_BITS
#else
  #define configPRIO_BITS                   4        /* 15 priority levels on STM32L4 */
#endif

/* Enable FPU support in the ARM_CM4F port */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0

/* Ensure 8-byte stack alignment for ARM AAPCS / FPU compliance */
#define configSTACK_ALLOCATION_TYPE             uint64_t
#define portBYTE_ALIGNMENT                      8

/* The lowest interrupt priority that can be used in a call to a "set priority" function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15

/* The highest interrupt priority from which FreeRTOS API calls can be made.
   Interrupts with priority levels 0 to 4 will NOT be masked by FreeRTOS critical sections. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt priorities used by the kernel port itself (shifted for NVIC hardware registers) */
#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Set the following definitions to 1 to include the API function, or 0 to exclude */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetSchedulerState          1

/* Define configASSERT for debug builds */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */
