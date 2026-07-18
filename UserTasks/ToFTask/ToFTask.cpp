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
#include "app_tof.hpp"

#include "log.hpp"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"

namespace {
    constexpr uint16_t PROXIMITY_DETECT_DISTANCE_CM = 15;
    constexpr uint16_t PROXIMITY_MIN_DISTANCE_CM = 5;
    constexpr uint32_t TASK_PERIOD_MS = 10;
    constexpr uint32_t LOG_PERIOD_CYCLES = 5;
}

bool tof_task_init(void) {
    LOG_INFO("[TOF_TASK] Initializing ToF task");
    return true;
}

void tof_task(void *argument) {
    (void)argument;

    LOG_INFO("[TOF_TASK] ToF task started");

    auto& mgr = TofProximityManager::getInstance();

    if (!mgr.init(PROXIMITY_DETECT_DISTANCE_CM, PROXIMITY_MIN_DISTANCE_CM)) {
        LOG_ERROR("[TOF_TASK] Failed to initialize TofProximityManager");
        while (1) {
            osDelay(1000);
        }
    }

    mgr.enableInterruptMode(true);
    mgr.setHighSpeedMode(true);
    mgr.startRanging();
    LOG_INFO("[TOF_TASK] Ranging started — interrupt mode, high-speed");

    // High-priority data acquisition loop — responds to sensor INT pins
    mgr.dataTask();  // never returns


   // MX_TOF_Init();

    while(1)
    {
        MX_TOF_Process();
            osDelay(TASK_PERIOD_MS);
            // data_transport_poll(); --- IGNORE ---
            // LOG_INFO("[TOF_TASK] ToF task running");
            // osDelay(1000); --- IGNORE ---
    }


}





void tof_detection_task(void *argument) {

    (void)argument;
    // Normal-priority detection loop — runs confidence algorithm, updates snapshot
    TofProximityManager::getInstance().detectionTask();  // never returns

  

    while(1)
    {
       //  LOG_INFO("[TOF_TASK] ToF detection task running");
        osDelay(1000);
    }




}
