/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCO_Pin GPIO_PIN_0
#define MCO_GPIO_Port GPIOA
#define VCP_TX_Pin GPIO_PIN_2
#define VCP_TX_GPIO_Port GPIOA
#define BATTERY_1_Pin GPIO_PIN_3
#define BATTERY_1_GPIO_Port GPIOA
#define BATTERY_2_Pin GPIO_PIN_4
#define BATTERY_2_GPIO_Port GPIOA
#define BATTERY_3_Pin GPIO_PIN_5
#define BATTERY_3_GPIO_Port GPIOA
#define MAINS_USB_Pin GPIO_PIN_6
#define MAINS_USB_GPIO_Port GPIOA
#define FET_GATE_CTRL_Pin GPIO_PIN_7
#define FET_GATE_CTRL_GPIO_Port GPIOA
#define DHT22_1_Pin GPIO_PIN_0
#define DHT22_1_GPIO_Port GPIOB
#define MODEM_PWRKEY_Pin GPIO_PIN_8
#define MODEM_PWRKEY_GPIO_Port GPIOA
#define MODEM_RESET_Pin GPIO_PIN_11
#define MODEM_RESET_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define VCP_RX_Pin GPIO_PIN_15
#define VCP_RX_GPIO_Port GPIOA
#define DHT22_2_Pin GPIO_PIN_4
#define DHT22_2_GPIO_Port GPIOB
#define DHT22_3_Pin GPIO_PIN_5
#define DHT22_3_GPIO_Port GPIOB
#define DHT22_4_Pin GPIO_PIN_6
#define DHT22_4_GPIO_Port GPIOB
#define DHT22_5_Pin GPIO_PIN_7
#define DHT22_5_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
