/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    tof_interrupts.h
 * @brief   ToF Sensor GPIO Interrupt Registration
 * @details Registers VL53L5CX data-ready interrupts with the GPIO interrupt system.
 */

#ifndef TOF_INTERRUPTS_H
#define TOF_INTERRUPTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Register ToF sensor interrupts with the GPIO interrupt system
 * @note Call this after GPIO_Interrupts_Init() and before starting ranging
 */
void TOF_Interrupts_Register(void);

/**
 * @brief Unregister ToF sensor interrupts
 */
void TOF_Interrupts_Unregister(void);

/**
 * @brief Callback invoked by TofProximityManager when sensor data is ready
 * @param sensorIndex Logical sensor index (0-5)
 */
void TofProximityManager_OnDataReady(uint8_t sensorIndex);

/**
 * @brief Configure sensor-side interrupt threshold for a ToF sensor
 * @param sensorIndex Logical sensor index (0-5)
 * @param criteria Interrupt criteria (TOF_IT_DEFAULT, TOF_IT_BELOW_LOW, etc.)
 * @param lowThresholdMm Low threshold in millimeters
 * @param highThresholdMm High threshold in millimeters
 * @return 0 on success, negative on error
 *
 * @note This configures the sensor's internal interrupt generation.
 *       The sensor will only trigger its INT pin when the criteria is met.
 */
int32_t TOF_Interrupts_ConfigThreshold(uint8_t sensorIndex, uint32_t criteria,
                                       uint32_t lowThresholdMm, uint32_t highThresholdMm);

/**
 * @brief Configure all sensors with the same proximity threshold
 * @param detectDistanceMm Distance in mm below which interrupt triggers
 * @return 0 on success, negative on first error
 */
int32_t TOF_Interrupts_ConfigProximityThreshold(uint32_t detectDistanceMm);

/**
 * @brief Disable threshold-based interrupts (return to default data-ready mode)
 * @param sensorIndex Logical sensor index (0-5)
 * @return 0 on success, negative on error
 */
int32_t TOF_Interrupts_DisableThreshold(uint8_t sensorIndex);

/* Interrupt criteria constants (from VL53L5CX BSP) */
#define TOF_IT_DEFAULT        0xFFU  /* Interrupt on any new measurement */
#define TOF_IT_BELOW_LOW      0x02U  /* Interrupt when distance <= LowThreshold */
#define TOF_IT_ABOVE_HIGH     0x03U  /* Interrupt when distance > HighThreshold */
#define TOF_IT_IN_WINDOW      0x00U  /* Interrupt when in threshold window */
#define TOF_IT_OUT_OF_WINDOW  0x01U  /* Interrupt when outside thresholds */

#ifdef __cplusplus
}
#endif

#endif /* TOF_INTERRUPTS_H */
