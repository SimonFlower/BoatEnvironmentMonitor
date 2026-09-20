#include "stm32l432xx.h"
#include "FreeRTOS.h"
#include "task.h"

/* Hard fault handler for debugging */
void HardFault_Handler(void) {
    while (1);
}

/* FreeRTOS "Hello World" Task */
void vBlinkTask(void *pvParameters) {
    // Enable GPIOB peripheral clock (Bit 1 in RCC AHB2ENR)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    // Configure PB3 as General Purpose Output (Bits 7:6 set to 01)
    GPIOB->MODER &= ~(3U << (3 * 2));
    GPIOB->MODER |=  (1U << (3 * 2));

    for (;;) {
        // Toggle PB3 (Onboard LED)
        GPIOB->ODR ^= (1U << 3);

        // Delay task for 500ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void) {
    // Create the Blink task
    xTaskCreate(vBlinkTask, "Blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    // Should never reach here
    while (1);
}
