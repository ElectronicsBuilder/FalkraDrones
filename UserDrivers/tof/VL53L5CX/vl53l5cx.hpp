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
 * @file    vl53l5cx.hpp
 * @brief   VL53L5CX Time-of-Flight Proximity Sensor Wrapper
 * @details C++ wrapper class for STMicroelectronics VL53L5CX 8x8 multi-zone
 *          time-of-flight proximity sensor. Delegates to TofProximityManager
 *          for actual sensor management.
 *
 * Usage:
 * @code
 * VL53L5CX frontSensor;
 * frontSensor.init(TofSensorId::Front);
 * if (frontSensor.isDetectingObstacle()) {
 *     uint16_t dist = frontSensor.getMinDistance();
 *     // Handle obstacle
 * }
 * @endcode
 */

#ifndef __VL53L5CX_HPP
#define __VL53L5CX_HPP

#include <cstdint>
#include "TofSensor.hpp"

class VL53L5CX {
public:
    bool init(TofSensorId sensorId);

    bool isDetectingObstacle(void) const;
    uint16_t getMinDistance(void) const;
    uint16_t getDetectionDistance(void) const;

    void setThreshold(uint16_t detectCm, uint16_t minCm);

    const char* getName(void) const;
    TofSensorId getId(void) const;

    bool isActive(void) const;
    bool isPresent(void) const;

    TofDetectionFlag getDetectionFlag(void) const;
    uint8_t getConfidence(void) const;

private:
    TofSensorId _sensorId = TofSensorId::Top;
    bool _initialized = false;
};

#endif /* __VL53L5CX_HPP */
