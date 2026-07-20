/**
  ******************************************************************************
  * @file          : app_tof.h
  * @author        : IMG SW Application Team
  * @brief         : This file provides code for the configuration
  *                  of the STMicroelectronics.X-CUBE-TOF1.3.1.0 instances.
  ******************************************************************************
  *
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
#ifndef __APP_TOF_H
#define __APP_TOF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <cstdint>

/* Exported defines ----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void MX_TOF_Init(void);
void MX_TOF_Start(void);

/**
 * @brief Synchronize sensor status based on I2C detection results
 * Called after MX_TOF_Init() completes to update runtime status
 *
 * Updates sensors[].status based on ToF_Present[] array detection results.
 * Only sensors actually detected on the I2C bus will be marked as active.
 */
void MX_TOF_SyncSensorStatus(void);

/**
 * @brief Check if a ToF sensor is physically present on the bus
 * @param sensor_index Sensor index (0-5 for TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT)
 * @return 1 if sensor detected, 0 if not present or invalid index
 */
uint8_t MX_TOF_IsSensorPresent(uint8_t sensor_index);

/**
 * @brief Read distance from a single sensor (interrupt-driven use)
 * @param sensor_index Logical sensor index (0-5 for TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT)
 * @return 1 if distance read successfully, 0 on error
 *
 * Call this when interrupt signals data ready for a specific sensor.
 * Updates proximity_devices[sensor_index][] with zone distances.
 */
uint8_t MX_TOF_ReadSensorDistance(uint8_t sensor_index);

/**
 * @brief Get BSP index for a logical sensor index
 * @param sensor_index Logical sensor index (0-5)
 * @return BSP index, or 0xFF if invalid/not present
 */
uint8_t MX_TOF_GetBspIndex(uint8_t sensor_index);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TOF_H */
