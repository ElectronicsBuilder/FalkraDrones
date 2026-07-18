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
 * @file    vl53l5cx.cpp
 * @brief   VL53L5CX Time-of-Flight Proximity Sensor Wrapper Implementation
 * @details Delegates to TofProximityManager for sensor management.
 */

#include "vl53l5cx.hpp"
#include "TofProximityManager.hpp"

bool VL53L5CX::init(TofSensorId sensorId) {
    _sensorId = sensorId;
    _initialized = true;
    return true;
}

bool VL53L5CX::isDetectingObstacle(void) const {
    if (!_initialized) {
        return false;
    }
    return TofProximityManager::getInstance().isObstacleDetected(_sensorId);
}

uint16_t VL53L5CX::getMinDistance(void) const {
    if (!_initialized) {
        return 0;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).getMinDistance();
}

uint16_t VL53L5CX::getDetectionDistance(void) const {
    if (!_initialized) {
        return 0;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).getDetectDistanceCm();
}

void VL53L5CX::setThreshold(uint16_t detectCm, uint16_t minCm) {
    if (!_initialized) {
        return;
    }
    TofProximityManager::getInstance().getSensor(_sensorId).setThreshold(detectCm, minCm);
}

const char* VL53L5CX::getName(void) const {
    if (!_initialized) {
        return "UNINITIALIZED";
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).getName();
}

TofSensorId VL53L5CX::getId(void) const {
    return _sensorId;
}

bool VL53L5CX::isActive(void) const {
    if (!_initialized) {
        return false;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).isActive();
}

bool VL53L5CX::isPresent(void) const {
    if (!_initialized) {
        return false;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).isPresent();
}

TofDetectionFlag VL53L5CX::getDetectionFlag(void) const {
    if (!_initialized) {
        return TofDetectionFlag::Init;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).getDetectionFlag();
}

uint8_t VL53L5CX::getConfidence(void) const {
    if (!_initialized) {
        return 0;
    }
    return TofProximityManager::getInstance().getSensor(_sensorId).getConfidence();
}
