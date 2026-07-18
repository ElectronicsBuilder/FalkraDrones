/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    tof_proximity.hpp
 * @brief   VL53L5CX Proximity Detection - Compatibility Layer
 * @details This file provides backward compatibility with the legacy C-style API.
 *          All functionality is now delegated to TofProximityManager.
 *
 * @deprecated Use TofProximityManager and TofSensor classes directly for new code.
 */

#ifndef TOF_PROXIMITY_HPP
#define TOF_PROXIMITY_HPP

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_TOF_SENSOR
#define MAX_TOF_SENSOR          6
#endif
#ifndef MAX_ZONE_PER_SENSOR
#define MAX_ZONE_PER_SENSOR     16
#endif

#define TOF_DEFAULT_DETECT_DISTANCE_CM   15
#define TOF_DEFAULT_MIN_DISTANCE_CM      5
#define TOF_DEFAULT_THRESHOLD_ZONES      4
#define TOF_DEFAULT_MAX_CONFIDENCE       5

typedef enum {
    tof_top = 0,
    tof_bottom,
    tof_front,
    tof_back,
    tof_left,
    tof_right
} tof_sensor_id_t;

typedef enum {
    sensor_inactive = 0,
    sensor_active   = 1,
    sensor_reset    = 2
} tof_sensor_status_t;

typedef enum {
    detection_flag_reset = 0,
    detection_flag_set   = 1,
    detection_flag_init  = 255
} tof_detection_flag_t;

typedef enum {
    sensors_idle = 0,
    sensors_enabled,
    sensors_disabled,
    sensors_paused,
    sensors_resumed,
    sensors_init = 255
} tof_system_state_t;

typedef struct {
    const char* name;
    tof_sensor_id_t id;
    tof_sensor_status_t status;
    uint16_t distance_cm[MAX_ZONE_PER_SENSOR];
    uint8_t confidence;
    tof_detection_flag_t detect_flag;

    uint16_t detect_distance_cm;
    uint16_t min_distance_cm;
    uint8_t  detect_threshold_zones;
    uint8_t  max_confidence;
} tof_sensor_t;

extern uint16_t proximity_devices[MAX_TOF_SENSOR][MAX_ZONE_PER_SENSOR];
extern tof_sensor_t sensors[MAX_TOF_SENSOR];

#ifdef __cplusplus
}
#endif

#endif // TOF_PROXIMITY_HPP
