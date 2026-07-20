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
 * @file    test_vl53l5cx.cpp
 * @brief   VL53L5CX Time-of-Flight Sensor Test Suite
 * @details Test suite for TofProximityManager and VL53L5CX sensors.
 */

#include "test_vl53l5cx.hpp"
#include "TofProximityManager.hpp"
#include "log.hpp"
#include "cmsis_os.h"

void test_vl53l5cx()
{
    test_vl53l5cx_polling();
}

void test_vl53l5cx_polling()
{
    LOG_INFO("[TEST_TOF] Starting VL53L5CX snapshot test...");

    auto& mgr = TofProximityManager::getInstance();

    if (!mgr.init(15, 1)) {
        LOG_ERROR("[TEST_TOF] Failed to initialize TofProximityManager");
        return;
    }

    mgr.startRanging();
    LOG_INFO("[TEST_TOF] Ranging started");

    for (uint16_t i = 0; i < 50; i++) {
        TofDistanceSnapshot snapshot;
        if (mgr.getSnapshot(&snapshot)) {
            LOG_INFO("[TEST_TOF] Cycle %u - T:%u B:%u F:%u Bk:%u L:%u R:%u mm",
                i,
                snapshot.distance_mm[0],
                snapshot.distance_mm[1],
                snapshot.distance_mm[2],
                snapshot.distance_mm[3],
                snapshot.distance_mm[4],
                snapshot.distance_mm[5]);
        }

        if (mgr.isAnyObstacleDetected()) {
            LOG_INFO("[TEST_TOF] Obstacle detected!");
            for (uint8_t s = 0; s < MAX_TOF_SENSORS; s++) {
                TofSensorId id = static_cast<TofSensorId>(s);
                if (mgr.isObstacleDetected(id)) {
                    LOG_INFO("[TEST_TOF]   %s: %u cm",
                        mgr.getSensor(id).getName(),
                        mgr.getSensor(id).getMinDistance());
                }
            }
        }

        osDelay(10);
    }

    mgr.stopRanging();
    LOG_INFO("[TEST_TOF] Snapshot test complete");
}
