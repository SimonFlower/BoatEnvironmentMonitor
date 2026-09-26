/* functions to use a Clipper LTE 4G modem to pass data via MQTT */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "stm32l4xx_hal.h"

#include "debug.h"
#include "modem.h"
#include "led.h"

#define N_MODEM_TEST_RETRIES 20

// codes for the return value from ModemReadLine()
typedef enum {MRL_OK, MRL_TIMEOUT, MRL_DISCARDED, MRL_DISCARDED_AND_TIMEOUT, MRL_UNEXPECTED} ModemReadLineReturn_t;

// forward declarations
static void ModemReset (void);
static void ModemPowerRequest (void);
static ModemReadLineReturn_t ModemTest (UART_HandleTypeDef *huart, int n_retries);
static ModemReadLineReturn_t ModemExpect (UART_HandleTypeDef *huart, const char *expected, int max_len,
                                          int timeout, unsigned char *rx_buffer);
static ModemReadLineReturn_t ModemReadLine (UART_HandleTypeDef *huart, int max_len, int timeout, unsigned char *rx_buffer);

/**
  * @brief  Configure PA8 as output using direct CMSIS register manipulation.
  *         and set up UART for communication with the modem
  * @param huart the huart structure to configure
  * @retval TRUE if initialisation succeded 
  */
static int ModemInit (UART_HandleTypeDef *huart) {
    // Enable Clocks for GPIOA and USART1
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR  |= RCC_APB2ENR_USART1EN;
    
    // Configure PA8 (PWRKEY) as General Purpose Output (01)
    GPIOA->MODER &= ~(3U << (8 * 2)); // Clear bits [17:16]
    GPIOA->MODER |=  (1U << (8 * 2)); // Set bit 16 -> 01 (Output)
    /* Push-pull output */
    GPIOA->OTYPER &= ~(1U << 8);
    /* Low speed is sufficient */
    GPIOA->OSPEEDR &= ~(3U << (8 * 2));
    /* No pull-up/pull-down */
    GPIOA->PUPDR &= ~(3U << (8 * 2));
    // Set PA8 HIGH default state via atomic BSRR register
    GPIOA->BSRR = (1U << 8);

    /* PA11 = General purpose output */
    GPIOA->MODER &= ~(3U << (11 * 2));
    GPIOA->MODER |= (1U << (11 * 2));
    /* Push-pull output */
    GPIOA->OTYPER &= ~(1U << 11);
    /* Low speed is sufficient */
    GPIOA->OSPEEDR &= ~(3U << (11 * 2));
    /* No pull-up/pull-down */
    GPIOA->PUPDR &= ~(3U << (11 * 2));
    // Set PA11 HIGH default state via atomic BSRR register
    GPIOA->BSRR = (1U << 11); // PA11 HIGH

    // Configure PA9 (TX) and PA10 (RX) as Alternate Function 7 (AF7)
    // Clear MODER9 & MODER10, set to 10 (Alternate Function)
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->MODER |=  ((2U << (9 * 2)) | (2U << (10 * 2)));

    // Set AF7 for PA9 (AFRH) and PA10 (AFRH)
    GPIOA->AFR[1] &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((7U    << ((9 - 8) * 4)) | (7U    << ((10 - 8) * 4)));
    GPIOA->OTYPER &= ~((1U << 9) | (1U << 10));
    GPIOA->OSPEEDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));

    // configure the serial port on the Nucleo that communciates with the Clipper modem
    huart->Instance = USART1;
    huart->Init.BaudRate = 115200;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

#if DEBUG >=5
    printf("USART1 BRR=%08lx\r\n", USART1->BRR);
#endif

    int status = HAL_UART_Init(huart);

#if DEBUG >=5
    printf("HAL_UART_Init status: %d\r\n", status);
    printf("CR1=%08lx\r\n", USART1->CR1);
    printf("CR2=%08lx\r\n", USART1->CR2);
    printf("CR3=%08lx\r\n", USART1->CR3);
    printf("BRR=%08lx\r\n", USART1->BRR);
#endif
    
    if (status != HAL_OK) return pdFALSE;
    return pdTRUE;
}

/**
  * @brief  FreeRTOS task to manage the Clipper LTE modem
  * 
  * Interactions between the main task and the modem task work like this:
  * 
  * 1. Modem task started: waits for xTaskNotifyGive from main task
  * 2. First part of task: modem initialised;
  *                        check made that modem board is working;
  *                        time retrieved from network
  *    Modem task sends message to main task  via a queue, sending the 
  *    time retrieved from the network (or indicating failure)
  * 2. TODO
  * 
 * @param pvParameters configuration parameters for the task in the form of a ModemTaskCfg_t structure
  */
void ModemTask(void *pvParameters) {
    configASSERT (pvParameters != NULL);
    
    ModemTaskCfg_t *task_cfg = (ModemTaskCfg_t *) pvParameters;
    configASSERT (task_cfg->msg_queue != NULL);

    // initialise access to the modem
    UART_HandleTypeDef huart;
    unsigned char modem_init_ok = ModemInit(&huart);

    for (;;) {
        ModemReadLineReturn_t status;

        // wait until triggered
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ModemMsg_t msg;
        if (! modem_init_ok) {
            msg.type = MODEM_FAIL;
            msg.subtype = MODEM_FAIL_INIT;
        } else {
            // test whether the modem is already up
            status = ModemTest (&huart, N_MODEM_TEST_RETRIES);
            if (status != MRL_OK) {
                // reset the modem and re-test
                ModemReset();
                status = ModemTest (&huart, N_MODEM_TEST_RETRIES);
            }
            if (status != MRL_OK) {
                // toggle the power key, reset and re-test
                ModemPowerRequest();
                status = ModemTest (&huart, N_MODEM_TEST_RETRIES);
            }
            
            // Prepare a response for the main task
            switch (status) {
                case MRL_OK:
                    msg.type = MODEM_TIME;
                    break;
                case MRL_DISCARDED_AND_TIMEOUT:
                case MRL_TIMEOUT:
                    msg.type = MODEM_FAIL;
                    msg.subtype = MODEM_FAIL_TIMEOUT;
                    break;
                default:
                    msg.type = MODEM_FAIL;
                    msg.subtype = MODEM_FAIL_BAD_RESPONSE;
                    break;
            }
        }
        
        // send the message to the main task
        xQueueSend(task_cfg->msg_queue, &msg, portMAX_DELAY);        
    }
}

static void ModemReset (void) {
    // Hold modem in reset
    GPIOA->BSRR = (1U << (11 + 16)); // PA11 LOW (assuming RESET active low)
    vTaskDelay(pdMS_TO_TICKS(200));

    // Release reset
    GPIOA->BSRR = (1U << 11); // PA11 HIGH

    // pause
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void ModemPowerRequest (void) {
    // Pulse PWRKEY (PA8) LOW for 1.2s using BSRR register writes
    GPIOA->BSRR = (1U << 8);            // Set PA8 HIGH
    vTaskDelay(pdMS_TO_TICKS(100));

    GPIOA->BSRR = (1U << (8 + 16));     // Set PA8 LOW (Active LOW pulse)
    vTaskDelay(pdMS_TO_TICKS(1200));    // Yield CPU during power pulse

    GPIOA->BSRR = (1U << 8);            // Set PA8 HIGH (Release pulse)

    // pause
    vTaskDelay(pdMS_TO_TICKS(200));
}

static ModemReadLineReturn_t ModemTest (UART_HandleTypeDef *huart, int n_retries) {
    configASSERT (n_retries > 0);

    ModemReadLineReturn_t status = MRL_TIMEOUT;

    // drain the UART's FIFO buffer
    unsigned char byte;
    while (HAL_UART_Receive(huart, &byte, 1, 0) == HAL_OK) {
    }

    for (int count=0; count<n_retries; count ++) {
        // Transmit AT test command over UART (PA9 = TX, PA10 = TX)
        unsigned char at_cmd [] = "AT\r\n";
#if DEBUG >= 4
        for (int count=0; count<strlen((char *) at_cmd); count++)
            printf("Modem TX: %02X\r\n", at_cmd[count]);
#endif
        if (HAL_UART_Transmit(huart, at_cmd, sizeof(at_cmd) - 1, 200) != HAL_OK)
            return MRL_TIMEOUT;

        // read multiple lines, looking for "OK", to skip past anything
        // else (e.g. an echoed "AT" or a blank line)
        for (int line = 0; line < 4; line++) {
            unsigned char rx_buffer [32];
            status = ModemReadLine(huart, sizeof(rx_buffer), 500, rx_buffer);
#if DEBUG >= 3
            printf("Modem line: \"%s\", status=%d\r\n", rx_buffer, status);
#endif
            if (status == MRL_OK && strcmp ((char *)rx_buffer, "OK") == 0)
                return MRL_OK;
                
            // exit if there was a timeout
            if (status == MRL_TIMEOUT)
                break;
        }
        
        // delay between retries
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    
    return status;
}

/** 
 * @brief Test for an expected response from the modem
 * 
 * @param huart the UART description
 * @param expected the string to expect
 * @param max_len the size of the receive buffer
 * @param timeout the time to wait for a response, in mS
 * @param rx_buffer a buffer to receive data into. If the return value
 *        if MRL_UNEXPECTED then the rx_buffer contains the string that
 *        was received
 * @retval a ModemReadLineReturn_t code
 */
static ModemReadLineReturn_t ModemExpect (UART_HandleTypeDef *huart, const char *expected, int max_len, 
                                          int timeout, unsigned char *rx_buffer) {
    ModemReadLineReturn_t status = ModemReadLine (huart, max_len, timeout, rx_buffer);
    if (status != MRL_OK) return status;
    if (strcmp (expected, (char *) rx_buffer) == 0) return MRL_OK;
    return MRL_UNEXPECTED;
}

/**
 * @brief read a line of input from the modem
 * 
 * A line is terminated by newline (\n). Carriage returns (\r) are
 * discarded. Newlines are also discarded from the output string.
 * If there is insufficient space in the rx_buffer, incoming data
 * is discarded.
 * 
 * @param huart the UART to read from
 * @param timeout the amount of time to wait, in mS - if timout is 0
 *        no data will be received
 * @param max_len size of rx_buffer in bytes, including space
 *        for the terminating null character
 * @param rx_buffer the place to put the received data - this will 
 *        alwasy be null terminated
 * @retval A code for the result. Usable data may have been received even
 *         when the return code is not MRL_OK.
 */
static ModemReadLineReturn_t ModemReadLine (UART_HandleTypeDef *huart, int max_len, int timeout, unsigned char *rx_buffer) {
    configASSERT(huart != NULL);
    configASSERT(rx_buffer != NULL);
    configASSERT(max_len >= 1);

    unsigned char byte = 0;
    int length = 0;
    BaseType_t discarded = pdFALSE;
    TickType_t start_ticks = xTaskGetTickCount();

    // Poll for up to the given timeout
    while ((xTaskGetTickCount() - start_ticks) < pdMS_TO_TICKS(timeout)) {
        // Read incoming response byte-by-byte into rx_buffer
        if (HAL_UART_Receive(huart, &byte, 1, 10) == HAL_OK) {
#if DEBUG >= 4
printf ("Modem RX: %02X\r\n", byte);
#endif
            switch (byte) {
                case '\n':
                    // terminate provided something has been received
                    if (length > 0) {
                        rx_buffer[length] = '\0';
                        if (discarded) return MRL_DISCARDED;
                        return MRL_OK;
                    }
                    break;
                case '\r':
                    // ignore
                    break;
                default:
                    // add to rx buffer, leaving space for null termination
                    if (length < max_len - 1) {
                        rx_buffer[length ++] = byte;
                    } else {
                        discarded = pdTRUE;
                    }
                    break;
            } 
        }
    }
    
    // return from timeout
    rx_buffer[length] = '\0';
    if (discarded) return MRL_DISCARDED_AND_TIMEOUT;
    return MRL_TIMEOUT;
}
