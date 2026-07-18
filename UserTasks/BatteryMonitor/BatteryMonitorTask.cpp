/**
 * @file    BatteryMonitorTask.cpp
 * @brief   FreeRTOS Task for Battery Current Monitoring
 *
 * Simple polling task that reads current measurements from the BatteryMonitor driver.
 * The driver handles all ADC DMA operations and calibration storage internally.
 *
 * Part of FalkraController - STM32F767-based drone controller firmware
 *
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
 */

#include "BatteryMonitorTask.hpp"
#include "log.hpp"
#include "BatteryMonitor.hpp"
#include "cmsis_os2.h"
#include <cmath>

extern bool TouchGFX_init;
extern bool wifiDriverInit;
extern bool PeripheralsTestComplete;

// Cached current reading (thread-safe via FreeRTOS scheduler)
static volatile float g_currentReading = 0.0f;

// Task configuration
namespace BatteryMonitorConfig {
    constexpr uint32_t SAMPLING_INTERVAL_MS = 1000;      // 1 Hz logging rate
    constexpr float LOG_THRESHOLD_A = 0.1f;              // Log if change > 100mA
    constexpr float ACTIVE_THRESHOLD_A = 0.05f;          // Current > this = active consumption
    constexpr float IDLE_THRESHOLD_A = 0.02f;            // Current < this = idle/standby
}

void batteryMonitorTask(void *argument) {
    // Wait for system initialization
    while (!TouchGFX_init && !PeripheralsTestComplete && !wifiDriverInit) {
        osDelay(100);
    }

    LOG_INFO("[BATMON_TASK] Starting Battery Monitor Task");

    // Get singleton instance (created in main)
    BatteryMonitor* monitor = BatteryMonitor::getInstance();

    if (!monitor) {
        LOG_ERROR("[BATMON_TASK] BatteryMonitor instance not found!");
        LOG_ERROR("[BATMON_TASK] Ensure BatteryMonitor is created in main before task starts");
        osThreadExit();
        return;
    }

    if (!monitor->isRunning()) {
        LOG_WARN("[BATMON_TASK] BatteryMonitor not running - attempting to start...");
        if (!monitor->start()) {
            LOG_ERROR("[BATMON_TASK] Failed to start BatteryMonitor!");
            osThreadExit();
            return;
        }
    }

    // Check calibration status
    if (!monitor->isCalibrated()) {
        LOG_WARN("[BATMON_TASK] ========================================");
        LOG_WARN("[BATMON_TASK] NO CALIBRATION DETECTED");
        LOG_WARN("[BATMON_TASK] ========================================");
        LOG_WARN("[BATMON_TASK] Current readings will be 0.0A");
        LOG_WARN("[BATMON_TASK] To calibrate, send: BATMON_CALIBRATE");
        LOG_WARN("[BATMON_TASK] ========================================");
        LOG_WARN("[BATMON_TASK] Task will continue monitoring...");
    } else {
        LOG_INFO("[BATMON_TASK] Calibration detected - monitoring started");
        const userconfig_batmon_t* cal = monitor->getCalibration();
        if (cal) {
            LOG_INFO("[BATMON_TASK] - VREF: %.4fV", cal->vref);
            LOG_INFO("[BATMON_TASK] - Zero (raw): %.4fV", cal->zero_current_raw);
            LOG_INFO("[BATMON_TASK] - Zero (buffered): %.4fV", cal->zero_current_buffered);
        }
    }

    LOG_INFO("[BATMON_TASK] Entering monitoring loop (1 Hz)");

    // Monitoring loop
    float last_logged_current = 0.0f;
    uint32_t sample_count = 0;

    while (1) {
        if (monitor->isCalibrated()) {
            // Get detailed data from driver
            BatteryMonitorData data = monitor->getData();
            g_currentReading = data.current_average;

            // Only log if significant change or every 10 samples
            bool significant_change = std::fabs(data.current_average - last_logged_current) > BatteryMonitorConfig::LOG_THRESHOLD_A;
            bool periodic_log = (sample_count % 10 == 0);

            if (significant_change || periodic_log) {
                // Determine consumption state (unidirectional - only positive current)
                const char* state = "";
                if (data.current_average > BatteryMonitorConfig::ACTIVE_THRESHOLD_A) {
                    state = " [ACTIVE]";
                } else if (data.current_average > BatteryMonitorConfig::IDLE_THRESHOLD_A) {
                    state = " [STANDBY]";
                } else {
                    state = " [IDLE]";
                }

                // LOG_INFO("[BATTERY] Avg: %.3fA%s | Raw: %.4fV/%.3fA | Buf: %.4fV/%.3fA",
                //          data.current_average, state,
                //          data.voltage_raw, data.current_raw,
                //          data.voltage_buffered, data.current_buffered);
                // last_logged_current = data.current_average;
            }

            sample_count++;
        } else {
            // Not calibrated - log reminder every 10 seconds
            if (sample_count % 10 == 0) {
                LOG_WARN("[BATTERY] Not calibrated - send BATMON_CALIBRATE command");
            }
            g_currentReading = 0.0f;
            sample_count++;
        }

        osDelay(BatteryMonitorConfig::SAMPLING_INTERVAL_MS);
    }
}

/**
 * @brief Get latest battery current reading
 * @return Current in A (thread-safe)
 */
float getBatteryCurrent(void) {
    return g_currentReading;
}

/**
 * @brief Get BatteryMonitor instance (for external access)
 * @return Pointer to singleton instance
 */
BatteryMonitor* getBatteryMonitor(void) {
    return BatteryMonitor::getInstance();
}
