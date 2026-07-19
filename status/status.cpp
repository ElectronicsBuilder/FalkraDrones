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
 * @file    status.cpp
 * @brief   Task Status 
 */
#include "status.hpp"
#include "dm_opts.h"
#include "driver_status.hpp"
#include "rtc.hpp"
#include "main.h"
#include <cstring>

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"

FalkraStatus g_status;

void status_init(void) {
    std::memset(&g_status, 0, sizeof(g_status));
    //g_status.rotation[0] = 1.0f; // default unit quaternion
}

 void status_task(void *arg) {
    status_init();

    const TickType_t delay = pdMS_TO_TICKS(50);
    uint32_t tick = 0;
    while (1) {
        // Update RTC time for display
        rtc_update_status();

#if DM_OPT_STATUS_DECOUPLED
        DriverStatus::updateAllSensorsDecoupled(tick++);
#else
        // Update all sensor readings with thread-safe mutex protection
        DriverStatus::updateStatus([](FalkraStatus& s) {
            s.lastUpdateMs = HAL_GetTick();
            DriverStatus::updateAllSensors(s);
        });
#endif

        osDelay(delay);
    }
}
