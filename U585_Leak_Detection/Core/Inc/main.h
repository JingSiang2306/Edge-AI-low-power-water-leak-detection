/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "stdint.h"
#include "stddef.h"

#include "b_u585i_iot02a.h"
#include "b_u585i_iot02a_motion_sensors.h"
#include "b_u585i_iot02a_audio.h"
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
const char *get_datetime_string_ms(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WRLS_FLOW_Pin GPIO_PIN_15
#define WRLS_FLOW_GPIO_Port GPIOG
#define PH3_BOOT0_Pin GPIO_PIN_3
#define PH3_BOOT0_GPIO_Port GPIOH
#define UCPD_PWR_Pin GPIO_PIN_5
#define UCPD_PWR_GPIO_Port GPIOB
#define USER_Button_Pin GPIO_PIN_13
#define USER_Button_GPIO_Port GPIOC
#define LED_RED_Pin GPIO_PIN_6
#define LED_RED_GPIO_Port GPIOH
#define LED_GREEN_Pin GPIO_PIN_7
#define LED_GREEN_GPIO_Port GPIOH
#define T_VCP_RX_Pin GPIO_PIN_10
#define T_VCP_RX_GPIO_Port GPIOA
#define T_VCP_TX_Pin GPIO_PIN_9
#define T_VCP_TX_GPIO_Port GPIOA
#define WRLS_WKUP_B_Pin GPIO_PIN_6
#define WRLS_WKUP_B_GPIO_Port GPIOG
#define Mems_VL53_xshut_Pin GPIO_PIN_1
#define Mems_VL53_xshut_GPIO_Port GPIOH
#define Mems_VLX_GPIO_Pin GPIO_PIN_5
#define Mems_VLX_GPIO_GPIO_Port GPIOG
#define WRLS_NOTIFY_Pin GPIO_PIN_14
#define WRLS_NOTIFY_GPIO_Port GPIOD
#define USB_UCPD_FLT_Pin GPIO_PIN_8
#define USB_UCPD_FLT_GPIO_Port GPIOE
#define Mems_INT_IIS2MDC_Pin GPIO_PIN_10
#define Mems_INT_IIS2MDC_GPIO_Port GPIOD
#define USB_IANA_Pin GPIO_PIN_13
#define USB_IANA_GPIO_Port GPIOD
#define Mems_INT_LPS22HH_Pin GPIO_PIN_2
#define Mems_INT_LPS22HH_GPIO_Port GPIOG
#define USB_VBUS_SENSE_Pin GPIO_PIN_14
#define USB_VBUS_SENSE_GPIO_Port GPIOF
#define Mems_STSAFE_RESET_Pin GPIO_PIN_11
#define Mems_STSAFE_RESET_GPIO_Port GPIOF
#define WRLS_WKUP_W_Pin GPIO_PIN_15
#define WRLS_WKUP_W_GPIO_Port GPIOF

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
