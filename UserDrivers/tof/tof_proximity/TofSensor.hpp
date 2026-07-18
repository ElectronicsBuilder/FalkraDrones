/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file    TofSensor.hpp
 * @brief   VL53L5CX Time-of-Flight Sensor Class
 * @details Encapsulates per-sensor state and detection logic for a single
 *          VL53L5CX multi-zone ToF sensor. Handles zone data storage,
 *          minimum distance calculation, and confidence-based detection.
 */

#ifndef TOF_SENSOR_HPP
#define TOF_SENSOR_HPP

#include <cstdint>

#define MAX_TOF_SENSORS         6
#define MAX_ZONES_PER_SENSOR    16

// Conservative defaults (lower false positive rate)
#define TOF_DEFAULT_DETECT_DISTANCE_CM   15
#define TOF_DEFAULT_MIN_DISTANCE_CM      5
#define TOF_DEFAULT_THRESHOLD_ZONES      4
#define TOF_DEFAULT_MAX_CONFIDENCE       5

// High-speed mode (aggressive detection for fast obstacle avoidance)
#define TOF_FAST_THRESHOLD_ZONES         2   // Detect with 2 of 16 zones hit
#define TOF_FAST_MAX_CONFIDENCE          3   // 3 frames max confidence
#define TOF_FAST_DETECT_CONFIDENCE       1   // Detect after 1 frame

enum class TofSensorId : uint8_t {
    Top = 0,
    Bottom,
    Front,
    Back,
    Left,
    Right
};

enum class TofSensorStatus : uint8_t {
    Inactive = 0,
    Active   = 1,
    Reset    = 2
};

enum class TofDetectionFlag : uint8_t {
    Reset = 0,
    Set   = 1,
    Init  = 255
};

class TofSensor {
public:
    TofSensor() = default;

    void configure(TofSensorId id, const char* name);

    void setThreshold(uint16_t detectCm, uint16_t minCm);
    void setConfidenceParams(uint8_t thresholdZones, uint8_t maxConfidence);
    void setDetectConfidenceThreshold(uint8_t threshold);

    void updateZoneData(const uint16_t* zones, uint8_t zoneCount);
    void updateDetectionState();

    void setStatus(TofSensorStatus status);
    void setPresent(bool present);

    bool isDetecting() const;
    uint16_t getMinDistance() const;
    uint16_t getZoneDistance(uint8_t zone) const;
    TofDetectionFlag getDetectionFlag() const;
    uint8_t getConfidence() const;
    bool isPresent() const;
    bool isActive() const;
    const char* getName() const;
    TofSensorId getId() const;
    uint16_t getDetectDistanceCm() const;
    uint16_t getMinDistanceThresholdCm() const;

private:
    TofSensorId _id = TofSensorId::Top;
    const char* _name = "UNKNOWN";
    TofSensorStatus _status = TofSensorStatus::Reset;
    bool _present = false;

    uint16_t _distanceCm[MAX_ZONES_PER_SENSOR] = {0};
    uint16_t _minDistanceValue = 0;

    uint16_t _detectDistanceCm = TOF_DEFAULT_DETECT_DISTANCE_CM;
    uint16_t _minDistanceThresholdCm = TOF_DEFAULT_MIN_DISTANCE_CM;
    uint8_t _thresholdZones = TOF_DEFAULT_THRESHOLD_ZONES;
    uint8_t _maxConfidence = TOF_DEFAULT_MAX_CONFIDENCE;

    uint8_t _confidence = 0;
    uint8_t _detectConfidenceThreshold = 2;  // Default: require 2 frames before detection
    TofDetectionFlag _detectFlag = TofDetectionFlag::Init;
    uint8_t _hitZones = 0;
};

#endif // TOF_SENSOR_HPP
