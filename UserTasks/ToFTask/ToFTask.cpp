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
 * @file    ToFTask.cpp
 * @brief   Time-of-Flight Sensor Task Implementation
 * @details FreeRTOS task using TofProximityManager for VL53L5CX sensor ranging.
 */

#include "ToFTask.hpp"
#include "TofProximityManager.hpp"
#include "driver_manager.hpp"
#include "dm_opts.h"

#include "log.hpp"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"

namespace {
    constexpr uint16_t PROXIMITY_DETECT_DISTANCE_CM = 15;
    constexpr uint16_t PROXIMITY_MIN_DISTANCE_CM = 5;
}

bool tof_task_init(void) {
    LOG_INFO("[TOF_TASK] Initializing ToF task");
    return true;
}

void tof_task(void *argument) {
    (void)argument;

    LOG_INFO("[TOF_TASK] ToF task started");

#if DM_OPT_TOF_INTEGRATION
    auto& dm = DriverManager::getInstance();
    if (!dm.initializeDriver(DriverId::TOF_PROXIMITY)) {
        LOG_ERROR("[TOF_TASK] Failed to initialize ToF through DriverManager");
        while (1) {
            osDelay(1000);
        }
    }

    auto* mgr_ptr = dm.getTofProximity();
    if (mgr_ptr == nullptr) {
        LOG_ERROR("[TOF_TASK] DriverManager did not return ToF proximity manager");
        while (1) {
            osDelay(1000);
        }
    }
    auto& mgr = *mgr_ptr;
#else
    auto& mgr = TofProximityManager::getInstance();
    if (!mgr.init(PROXIMITY_DETECT_DISTANCE_CM, PROXIMITY_MIN_DISTANCE_CM)) {
        LOG_ERROR("[TOF_TASK] Failed to initialize TofProximityManager");
        while (1) {
            osDelay(1000);
        }
    }
#endif

    mgr.enableInterruptMode(true);
    mgr.setHighSpeedMode(true);
    mgr.startRanging();
    LOG_INFO("[TOF_TASK] Ranging started — interrupt mode, high-speed");

    // High-priority data acquisition loop — responds to sensor INT pins
    mgr.dataTask();  // never returns
}





void tof_detection_task(void *argument) {

    (void)argument;
    // Normal-priority detection loop — runs confidence algorithm, updates snapshot
    TofProximityManager::getInstance().detectionTask();  // never returns
}
