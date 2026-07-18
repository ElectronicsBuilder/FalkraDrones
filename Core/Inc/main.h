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
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os2.h"
#define STM32F7xx
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_I2C1_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TFT_CS_Pin GPIO_PIN_2
#define TFT_CS_GPIO_Port GPIOE
#define TFT_EN_Pin GPIO_PIN_3
#define TFT_EN_GPIO_Port GPIOE
#define VPWM_OE_Pin GPIO_PIN_4
#define VPWM_OE_GPIO_Port GPIOE
#define VPWM_D1_Pin GPIO_PIN_8
#define VPWM_D1_GPIO_Port GPIOI
#define VPWM_STAT_Pin GPIO_PIN_13
#define VPWM_STAT_GPIO_Port GPIOC
#define SBUS_IN_Pin GPIO_PIN_9
#define SBUS_IN_GPIO_Port GPIOI
#define VPWM_D0_Pin GPIO_PIN_10
#define VPWM_D0_GPIO_Port GPIOI
#define TLM_EN_Pin GPIO_PIN_11
#define TLM_EN_GPIO_Port GPIOI
#define WIFI_PWREN_Pin GPIO_PIN_2
#define WIFI_PWREN_GPIO_Port GPIOF
#define WIFI_CS_Pin GPIO_PIN_3
#define WIFI_CS_GPIO_Port GPIOF
#define WIFI_WAKE_Pin GPIO_PIN_4
#define WIFI_WAKE_GPIO_Port GPIOF
#define WIFI_IRQ_Pin GPIO_PIN_5
#define WIFI_IRQ_GPIO_Port GPIOF
#define WIFI_IRQ_EXTI_IRQn EXTI9_5_IRQn
#define TLM_IO3_Pin GPIO_PIN_2
#define TLM_IO3_GPIO_Port GPIOC
#define TLM_IO2_Pin GPIO_PIN_3
#define TLM_IO2_GPIO_Port GPIOC
#define BATMON_IO1_Pin GPIO_PIN_0
#define BATMON_IO1_GPIO_Port GPIOA
#define BATMON_IO2_Pin GPIO_PIN_1
#define BATMON_IO2_GPIO_Port GPIOA
#define WIFI_EN_Pin GPIO_PIN_2
#define WIFI_EN_GPIO_Port GPIOH
#define WIFI_RESET_Pin GPIO_PIN_3
#define WIFI_RESET_GPIO_Port GPIOH
#define TLM_IO4_Pin GPIO_PIN_4
#define TLM_IO4_GPIO_Port GPIOH
#define TLM_IO5_Pin GPIO_PIN_5
#define TLM_IO5_GPIO_Port GPIOH
#define PPM_IN_Pin GPIO_PIN_5
#define PPM_IN_GPIO_Port GPIOA
#define RADIO_IO1_Pin GPIO_PIN_4
#define RADIO_IO1_GPIO_Port GPIOC
#define RADIO_IO2_Pin GPIO_PIN_5
#define RADIO_IO2_GPIO_Port GPIOC
#define RADIO_IO3_Pin GPIO_PIN_0
#define RADIO_IO3_GPIO_Port GPIOB
#define RADIO_IO5_Pin GPIO_PIN_1
#define RADIO_IO5_GPIO_Port GPIOB
#define RADIO_IO4_Pin GPIO_PIN_11
#define RADIO_IO4_GPIO_Port GPIOF
#define LED_ERROR_Pin GPIO_PIN_12
#define LED_ERROR_GPIO_Port GPIOF
#define LED_COMM_Pin GPIO_PIN_13
#define LED_COMM_GPIO_Port GPIOF
#define LED_HB_Pin GPIO_PIN_0
#define LED_HB_GPIO_Port GPIOG
#define LED_ACTY_Pin GPIO_PIN_1
#define LED_ACTY_GPIO_Port GPIOG
#define LED_READY_Pin GPIO_PIN_7
#define LED_READY_GPIO_Port GPIOE
#define TOF6_En_Pin GPIO_PIN_8
#define TOF6_En_GPIO_Port GPIOE
#define PWM_CH1_Pin GPIO_PIN_9
#define PWM_CH1_GPIO_Port GPIOE
#define TOF6_GPIO_Pin GPIO_PIN_10
#define TOF6_GPIO_GPIO_Port GPIOE
#define TOF6_GPIO_EXTI_IRQn EXTI15_10_IRQn
#define PWM_CH2_Pin GPIO_PIN_11
#define PWM_CH2_GPIO_Port GPIOE
#define PWM_CH3_Pin GPIO_PIN_13
#define PWM_CH3_GPIO_Port GPIOE
#define PWM_CH4_Pin GPIO_PIN_14
#define PWM_CH4_GPIO_Port GPIOE
#define TOF6_LPn_Pin GPIO_PIN_15
#define TOF6_LPn_GPIO_Port GPIOE
#define TOF5_En_Pin GPIO_PIN_6
#define TOF5_En_GPIO_Port GPIOH
#define TOF5_GPIO_Pin GPIO_PIN_7
#define TOF5_GPIO_GPIO_Port GPIOH
#define TOF5_GPIO_EXTI_IRQn EXTI9_5_IRQn
#define TOF5_LPn_Pin GPIO_PIN_8
#define TOF5_LPn_GPIO_Port GPIOH
#define TOF4_En_Pin GPIO_PIN_9
#define TOF4_En_GPIO_Port GPIOH
#define TOF4_LPn_Pin GPIO_PIN_10
#define TOF4_LPn_GPIO_Port GPIOH
#define TOF4_GPIO_Pin GPIO_PIN_11
#define TOF4_GPIO_GPIO_Port GPIOH
#define TOF4_GPIO_EXTI_IRQn EXTI15_10_IRQn
#define PSENS_INT_Pin GPIO_PIN_12
#define PSENS_INT_GPIO_Port GPIOH
#define DOF_INT_Pin GPIO_PIN_9
#define DOF_INT_GPIO_Port GPIOD
#define DOF_NRST_Pin GPIO_PIN_10
#define DOF_NRST_GPIO_Port GPIOD
#define DOF_CLKSEL_Pin GPIO_PIN_11
#define DOF_CLKSEL_GPIO_Port GPIOD
#define DOF_PS0_Pin GPIO_PIN_12
#define DOF_PS0_GPIO_Port GPIOD
#define DOF_PS1_Pin GPIO_PIN_13
#define DOF_PS1_GPIO_Port GPIOD
#define DOF_BOOTN_Pin GPIO_PIN_14
#define DOF_BOOTN_GPIO_Port GPIOD
#define NVRAM_HOLD_Pin GPIO_PIN_15
#define NVRAM_HOLD_GPIO_Port GPIOD
#define NVRAM_WP_Pin GPIO_PIN_2
#define NVRAM_WP_GPIO_Port GPIOG
#define NVRAM_CS_Pin GPIO_PIN_3
#define NVRAM_CS_GPIO_Port GPIOG
#define FLASH_HOLD_Pin GPIO_PIN_4
#define FLASH_HOLD_GPIO_Port GPIOG
#define FLASH_CS_Pin GPIO_PIN_5
#define FLASH_CS_GPIO_Port GPIOG
#define FLASH_WP_Pin GPIO_PIN_6
#define FLASH_WP_GPIO_Port GPIOG
#define PGOOD_5V_Pin GPIO_PIN_7
#define PGOOD_5V_GPIO_Port GPIOG
#define PGOOD_3V3_Pin GPIO_PIN_8
#define PGOOD_3V3_GPIO_Port GPIOG
#define PWM_CH6_Pin GPIO_PIN_7
#define PWM_CH6_GPIO_Port GPIOC
#define PWM_CH7_Pin GPIO_PIN_8
#define PWM_CH7_GPIO_Port GPIOC
#define PWM_CH8_Pin GPIO_PIN_9
#define PWM_CH8_GPIO_Port GPIOC
#define SBUS_OE_Pin GPIO_PIN_10
#define SBUS_OE_GPIO_Port GPIOA
#define GPOUT_Pin GPIO_PIN_13
#define GPOUT_GPIO_Port GPIOH
#define VSEL_OUT_Pin GPIO_PIN_14
#define VSEL_OUT_GPIO_Port GPIOH
#define TOF3_En_Pin GPIO_PIN_15
#define TOF3_En_GPIO_Port GPIOH
#define TOF3_LPn_Pin GPIO_PIN_0
#define TOF3_LPn_GPIO_Port GPIOI
#define TOF3_GPIO_Pin GPIO_PIN_1
#define TOF3_GPIO_GPIO_Port GPIOI
#define TOF3_GPIO_EXTI_IRQn EXTI1_IRQn
#define TOF2_LPn_Pin GPIO_PIN_2
#define TOF2_LPn_GPIO_Port GPIOI
#define TOF2_GPIO_Pin GPIO_PIN_3
#define TOF2_GPIO_GPIO_Port GPIOI
#define TOF2_GPIO_EXTI_IRQn EXTI3_IRQn
#define TOF2_En_Pin GPIO_PIN_0
#define TOF2_En_GPIO_Port GPIOD
#define TOF1_LPn_Pin GPIO_PIN_1
#define TOF1_LPn_GPIO_Port GPIOD
#define TOF1_GPIO_Pin GPIO_PIN_2
#define TOF1_GPIO_GPIO_Port GPIOD
#define TOF1_GPIO_EXTI_IRQn EXTI2_IRQn
#define TOF1_En_Pin GPIO_PIN_4
#define TOF1_En_GPIO_Port GPIOD
#define DOF_CS_Pin GPIO_PIN_5
#define DOF_CS_GPIO_Port GPIOD
#define CPT_INT_Pin GPIO_PIN_15
#define CPT_INT_GPIO_Port GPIOG
#define PPM_OE_Pin GPIO_PIN_4
#define PPM_OE_GPIO_Port GPIOB
#define BKL_PWM_Pin GPIO_PIN_5
#define BKL_PWM_GPIO_Port GPIOB
#define AUDIO_EN_Pin GPIO_PIN_8
#define AUDIO_EN_GPIO_Port GPIOB
#define AUDIO_GAIN_Pin GPIO_PIN_9
#define AUDIO_GAIN_GPIO_Port GPIOB
#define AUDIO_MODE_Pin GPIO_PIN_0
#define AUDIO_MODE_GPIO_Port GPIOE
#define CTP_RESET_Pin GPIO_PIN_1
#define CTP_RESET_GPIO_Port GPIOE
#define TFT_RESET_Pin GPIO_PIN_4
#define TFT_RESET_GPIO_Port GPIOI
#define PWM_CH5_Pin GPIO_PIN_5
#define PWM_CH5_GPIO_Port GPIOI
#define TFT_RD_Pin GPIO_PIN_6
#define TFT_RD_GPIO_Port GPIOI
#define TFT_DC_Pin GPIO_PIN_7
#define TFT_DC_GPIO_Port GPIOI

/* USER CODE BEGIN Private defines */
#define CHIP_EN_GPIO_Port       WIFI_EN_GPIO_Port       // PH2 - Chip Enable
#define CHIP_EN_Pin             WIFI_EN_Pin             // PH2
#define SPI_RDY_GPIO_Port       WIFI_IRQ_GPIO_Port      // PF5 - Ready/Data Available Signal
#define SPI_RDY_Pin             WIFI_IRQ_Pin            // PF5
#define SPI_CS_GPIO_Port        WIFI_CS_GPIO_Port       // PF3 - SPI Chip Select
#define SPI_CS_Pin              WIFI_CS_Pin             // PF3

// WiFi Bootloader Mode Control (WAKE pin used for bootloader entry)
#define WIFI_BOOT_GPIO_Port     WIFI_WAKE_GPIO_Port     // PF4 - Bootloader Mode Control
#define WIFI_BOOT_Pin           WIFI_WAKE_Pin           // PF4
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
