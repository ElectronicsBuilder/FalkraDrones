/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    tof_proximity.cpp
 * @brief   VL53L5CX Proximity Detection - Compatibility Layer
 * @details Provides global arrays for backward compatibility with app_tof.cpp.
 *          New code should use TofProximityManager and TofSensor classes directly.
 *
 * @deprecated Use TofProximityManager for new code.
 */

#include "tof_proximity.hpp"

uint16_t proximity_devices[MAX_TOF_SENSOR][MAX_ZONE_PER_SENSOR] = {0};

tof_sensor_t sensors[MAX_TOF_SENSOR] = {
    { "TOP",    tof_top,    sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE },
    { "BOTTOM", tof_bottom, sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE },
    { "FRONT",  tof_front,  sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE },
    { "BACK",   tof_back,   sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE },
    { "LEFT",   tof_left,   sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE },
    { "RIGHT",  tof_right,  sensor_reset, {0}, 0, detection_flag_init, TOF_DEFAULT_DETECT_DISTANCE_CM, TOF_DEFAULT_MIN_DISTANCE_CM, TOF_DEFAULT_THRESHOLD_ZONES, TOF_DEFAULT_MAX_CONFIDENCE }
};
