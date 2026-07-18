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
 * @file    TofSensor.cpp
 * @brief   VL53L5CX Time-of-Flight Sensor Class Implementation
 */

#include "TofSensor.hpp"
#include "tof_speed_opts.h"
#include <cstring>
#include <algorithm>

void TofSensor::configure(TofSensorId id, const char* name) {
    _id = id;
    _name = name;
    _status = TofSensorStatus::Reset;
    _present = false;
    _confidence = 0;
    _detectFlag = TofDetectionFlag::Init;
    _minDistanceValue = 0;
    std::memset(_distanceCm, 0, sizeof(_distanceCm));
}

void TofSensor::setThreshold(uint16_t detectCm, uint16_t minCm) {
    _detectDistanceCm = detectCm;
    _minDistanceThresholdCm = minCm;
}

void TofSensor::setConfidenceParams(uint8_t thresholdZones, uint8_t maxConfidence) {
    _thresholdZones = thresholdZones;
    _maxConfidence = maxConfidence;
}

void TofSensor::setDetectConfidenceThreshold(uint8_t threshold) {
    _detectConfidenceThreshold = threshold;
}

void TofSensor::updateZoneData(const uint16_t* zones, uint8_t zoneCount) {
    if (zones == nullptr || zoneCount == 0) {
        return;
    }

    uint8_t count = (zoneCount > MAX_ZONES_PER_SENSOR) ? MAX_ZONES_PER_SENSOR : zoneCount;
    std::memcpy(_distanceCm, zones, count * sizeof(uint16_t));
    if (count < MAX_ZONES_PER_SENSOR) {
        std::memset(_distanceCm + count, 0, (MAX_ZONES_PER_SENSOR - count) * sizeof(uint16_t));
    }

    _minDistanceValue = UINT16_MAX;
    for (uint8_t i = 0; i < count; i++) {
        if (_distanceCm[i] > 0 && _distanceCm[i] < _minDistanceValue) {
            _minDistanceValue = _distanceCm[i];
        }
    }

    if (_minDistanceValue == UINT16_MAX) {
        _minDistanceValue = 0;
    }
}

void TofSensor::updateDetectionState() {
    if (_status != TofSensorStatus::Active) {
        return;
    }

    _hitZones = 0;
#if TOF_OPT_MM_RESOLUTION
    uint16_t minThreshold = _minDistanceThresholdCm * 10U;
    uint16_t detectThreshold = _detectDistanceCm * 10U;
#else
    uint16_t minThreshold = _minDistanceThresholdCm;
    uint16_t detectThreshold = _detectDistanceCm;
#endif
    for (uint8_t z = 0; z < MAX_ZONES_PER_SENSOR; z++) {
        uint16_t dist = _distanceCm[z];
        if (dist > minThreshold && dist <= detectThreshold) {
            _hitZones++;
        }
    }

    if (_hitZones >= _thresholdZones) {
        if (_confidence < _maxConfidence) {
            _confidence++;
        }
    } else {
        if (_confidence > 0) {
            _confidence--;
        }
    }

    // Use configurable confidence threshold for detection
    if (_confidence >= _detectConfidenceThreshold) {
        _detectFlag = TofDetectionFlag::Set;
    } else if (_confidence == 0) {
        _detectFlag = TofDetectionFlag::Reset;
    }
}

void TofSensor::setStatus(TofSensorStatus status) {
    _status = status;
}

void TofSensor::setPresent(bool present) {
    _present = present;
    if (present && _status == TofSensorStatus::Reset) {
        _status = TofSensorStatus::Active;
    } else if (!present) {
        _status = TofSensorStatus::Inactive;
    }
}

bool TofSensor::isDetecting() const {
    return _detectFlag == TofDetectionFlag::Set;
}

uint16_t TofSensor::getMinDistance() const {
    return _minDistanceValue;
}

uint16_t TofSensor::getZoneDistance(uint8_t zone) const {
    if (zone >= MAX_ZONES_PER_SENSOR) {
        return 0;
    }
    return _distanceCm[zone];
}

TofDetectionFlag TofSensor::getDetectionFlag() const {
    return _detectFlag;
}

uint8_t TofSensor::getConfidence() const {
    return _confidence;
}

bool TofSensor::isPresent() const {
    return _present;
}

bool TofSensor::isActive() const {
    return _status == TofSensorStatus::Active;
}

const char* TofSensor::getName() const {
    return _name;
}

TofSensorId TofSensor::getId() const {
    return _id;
}

uint16_t TofSensor::getDetectDistanceCm() const {
    return _detectDistanceCm;
}

uint16_t TofSensor::getMinDistanceThresholdCm() const {
    return _minDistanceThresholdCm;
}
