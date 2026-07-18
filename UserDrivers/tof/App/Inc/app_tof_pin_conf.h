/**
 ******************************************************************************
 * @file    app_tof_pin_conf.h
 * @author  IMG SW Application Team
 * @brief   This file contains definitions for TOF pins
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_TOF_PIN_CONF_H__
#define __APP_TOF_PIN_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "main.h"
/* Exported symbols ----------------------------------------------------------*/


#define VL53L5A1_PWR_EN_TOP_PIN         (TOF1_En_Pin)
#define VL53L5A1_PWR_EN_TOP_PORT        (TOF1_En_GPIO_Port)

#define VL53L5A1_PWR_EN_BOTTOM_PIN      (TOF2_En_Pin)
#define VL53L5A1_PWR_EN_BOTTOM_PORT     (TOF2_En_GPIO_Port)

#define VL53L5A1_PWR_EN_FRONT_PIN       (TOF3_En_Pin)
#define VL53L5A1_PWR_EN_FRONT_PORT      (TOF3_En_GPIO_Port)

#define VL53L5A1_PWR_EN_BACK_PIN        (TOF4_En_Pin)
#define VL53L5A1_PWR_EN_BACK_PORT       (TOF4_En_GPIO_Port)

#define VL53L5A1_PWR_EN_LEFT_PIN        (TOF5_En_Pin)
#define VL53L5A1_PWR_EN_LEFT_PORT       (TOF5_En_GPIO_Port)

#define VL53L5A1_PWR_EN_RIGHT_PIN       (TOF6_En_Pin)
#define VL53L5A1_PWR_EN_RIGHT_PORT      (TOF6_En_GPIO_Port)


#define VL53L5A1_LPn_TOP_PIN        (TOF1_LPn_Pin)
#define VL53L5A1_LPn_TOP_PORT       (TOF1_LPn_GPIO_Port)

#define VL53L5A1_LPn_BOTTOM_PIN     (TOF2_LPn_Pin)
#define VL53L5A1_LPn_BOTTOM_PORT    (TOF2_LPn_GPIO_Port)

#define VL53L5A1_LPn_FRONT_PIN      (TOF3_LPn_Pin)
#define VL53L5A1_LPn_FRONT_PORT     (TOF3_LPn_GPIO_Port)

#define VL53L5A1_LPn_BACK_PIN       (TOF4_LPn_Pin)
#define VL53L5A1_LPn_BACK_PORT      (TOF4_LPn_GPIO_Port)

#define VL53L5A1_LPn_LEFT_PIN       (TOF5_LPn_Pin)
#define VL53L5A1_LPn_LEFT_PORT      (TOF5_LPn_GPIO_Port)

#define VL53L5A1_LPn_RIGHT_PIN      (TOF6_LPn_Pin)
#define VL53L5A1_LPn_RIGHT_PORT     (TOF6_LPn_GPIO_Port)






#ifdef __cplusplus
}
#endif

#endif /* __APP_TOF_PIN_CONF_H__ */
