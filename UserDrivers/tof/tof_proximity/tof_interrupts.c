/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    tof_interrupts.c
 * @brief   ToF Sensor GPIO Interrupt Registration Implementation
 */

#include "tof_interrupts.h"
#include "gpio_interrupts.h"
#include "main.h"  // For TOFx_GPIO_Pin definitions
#include "53l5a1_ranging_sensor.h"  // For VL53L5A1_RANGING_SENSOR_ConfigIT
#include "tof_perf.h"

uint8_t MX_TOF_IsSensorPresent(uint8_t sensor_index);
uint8_t MX_TOF_GetBspIndex(uint8_t sensor_index);

/**
 * ToF sensor GPIO pin to logical index mapping
 * Adjust these if your pin definitions differ
 */
#ifndef TOF1_GPIO_Pin
#define TOF1_GPIO_Pin GPIO_PIN_2   // PD2 - TOP
#endif
#ifndef TOF2_GPIO_Pin
#define TOF2_GPIO_Pin GPIO_PIN_3   // PI3 - BOTTOM
#endif
#ifndef TOF3_GPIO_Pin
#define TOF3_GPIO_Pin GPIO_PIN_1   // PI1 - FRONT
#endif
#ifndef TOF4_GPIO_Pin
#define TOF4_GPIO_Pin GPIO_PIN_11  // PH11 - BACK
#endif
#ifndef TOF5_GPIO_Pin
#define TOF5_GPIO_Pin GPIO_PIN_7   // PH7 - LEFT
#endif
#ifndef TOF6_GPIO_Pin
#define TOF6_GPIO_Pin GPIO_PIN_10  // PE10 - RIGHT
#endif

static void tof_gpio_callback(uint16_t gpio_pin);

static const struct {
    uint16_t pin;
    uint8_t sensor_index;
} tof_pin_map[] = {
    { TOF1_GPIO_Pin, 0 },  // TOP
    { TOF2_GPIO_Pin, 1 },  // BOTTOM
    { TOF3_GPIO_Pin, 2 },  // FRONT
    { TOF4_GPIO_Pin, 3 },  // BACK
    { TOF5_GPIO_Pin, 4 },  // LEFT
    { TOF6_GPIO_Pin, 5 },  // RIGHT
};

#define TOF_PIN_MAP_SIZE (sizeof(tof_pin_map) / sizeof(tof_pin_map[0]))

void TOF_Interrupts_Register(void) {
    for (uint8_t i = 0; i < TOF_PIN_MAP_SIZE; i++) {
        GPIO_Interrupts_RegisterCallback(tof_pin_map[i].pin, tof_gpio_callback);
    }
}

void TOF_Interrupts_Unregister(void) {
    for (uint8_t i = 0; i < TOF_PIN_MAP_SIZE; i++) {
        GPIO_Interrupts_UnregisterCallback(tof_pin_map[i].pin);
    }
}

static void tof_gpio_callback(uint16_t gpio_pin) {
    for (uint8_t i = 0; i < TOF_PIN_MAP_SIZE; i++) {
        if (tof_pin_map[i].pin == gpio_pin) {
            TOF_PERF_MARK_IRQ(tof_pin_map[i].sensor_index);
            TofProximityManager_OnDataReady(tof_pin_map[i].sensor_index);
            return;
        }
    }
}

// ============================================================================
// Sensor-side Interrupt Threshold Configuration
// ============================================================================

int32_t TOF_Interrupts_ConfigThreshold(uint8_t sensorIndex, uint32_t criteria,
                                       uint32_t lowThresholdMm, uint32_t highThresholdMm) {
    if (sensorIndex >= TOF_PIN_MAP_SIZE) {
        return -1;
    }

    if (!MX_TOF_IsSensorPresent(sensorIndex)) {
        return -1;
    }

    uint8_t bsp_idx = MX_TOF_GetBspIndex(sensorIndex);
    if (bsp_idx == 0xFF) {
        return -1;
    }

    RANGING_SENSOR_ITConfig_t config = {
        .Criteria = criteria,
        .LowThreshold = lowThresholdMm,
        .HighThreshold = highThresholdMm
    };

    return VL53L5A1_RANGING_SENSOR_ConfigIT(bsp_idx, &config);
}

int32_t TOF_Interrupts_ConfigProximityThreshold(uint32_t detectDistanceMm) {
    int32_t result = 0;

    for (uint8_t i = 0; i < TOF_PIN_MAP_SIZE; i++) {
        int32_t ret = TOF_Interrupts_ConfigThreshold(
            i,
            TOF_IT_BELOW_LOW,
            detectDistanceMm,
            detectDistanceMm * 2  // High threshold not used for BELOW_LOW
        );
        if (ret < 0 && result == 0) {
            result = ret;  // Return first error
        }
    }

    return result;
}

int32_t TOF_Interrupts_DisableThreshold(uint8_t sensorIndex) {
    return TOF_Interrupts_ConfigThreshold(sensorIndex, TOF_IT_DEFAULT, 0, 0);
}
