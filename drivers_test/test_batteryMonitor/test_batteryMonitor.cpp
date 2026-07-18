/**
 * @file    test_batteryMonitor.cpp
 * @brief   Battery Monitor Test Implementation
 * @details Implementation of test suite for ACS758 Hall Effect Current Sensor
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

#include "test_batteryMonitor.hpp"
#include "BatteryMonitor.hpp"
#include "log.hpp"
#include "main.h"
#include "stm32f7xx_hal.h"

extern ADC_HandleTypeDef hadc1;

// Global instance for callback access
static BatteryMonitor* g_testBatteryMonitor = nullptr;

/**
 * @brief ADC conversion complete callback
 *
 * NOTE: This callback is DISABLED when using BatteryMonitorTask.
 * The task has its own callback in BatteryMonitorCallbacks.cpp
 *
 * If you want to use the test function instead of the task, uncomment this.
 */
#if 0  // Disabled - using BatteryMonitorTask callback instead
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1 && g_testBatteryMonitor) {
        g_testBatteryMonitor->dmaConversionCompleteCallback();
    }
}
#endif

// External global userconfig instance (from main_cpp.cpp)
extern userconfig_t* g_userConfig;

void test_batteryMonitor(uint32_t duration_ms) {
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Battery Monitor Test Start");
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Test duration: %lu ms", duration_ms);

    if (!g_userConfig) {
        LOG_ERROR("[BATMON_TEST] UserConfig not initialized!");
        return;
    }

    // Create battery monitor instance (uses userconfig for calibration)
    BatteryMonitor monitor(&hadc1, g_userConfig);
    g_testBatteryMonitor = &monitor;

    // Initialize
    if (!monitor.init()) {
        LOG_ERROR("[BATMON_TEST] Initialization failed!");
        g_testBatteryMonitor = nullptr;
        return;
    }

    // Start ADC DMA
    if (!monitor.start()) {
        LOG_ERROR("[BATMON_TEST] Failed to start ADC DMA!");
        g_testBatteryMonitor = nullptr;
        return;
    }

    // Wait for initial data
    osDelay(100);

    // Display raw readings before calibration
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Pre-Calibration Readings:");
    LOG_INFO("[BATMON_TEST] ========================================");

    BatteryMonitorData data = monitor.getData();
    LOG_INFO("[BATMON_TEST] Raw ADC values:");
    LOG_INFO("[BATMON_TEST] - VOUT1 (Raw):      %u (%.4fV)",
             data.adc_raw, data.voltage_raw);
    LOG_INFO("[BATMON_TEST] - VOUT2 (Buffered): %u (%.4fV)",
             data.adc_buffered, data.voltage_buffered);
    LOG_INFO("[BATMON_TEST] Computed currents:");
    LOG_INFO("[BATMON_TEST] - Current (Raw):      %.3fA", data.current_raw);
    LOG_INFO("[BATMON_TEST] - Current (Buffered): %.3fA", data.current_buffered);
    LOG_INFO("[BATMON_TEST] - Current (Average):  %.3fA", data.current_average);

    // Prompt for calibration
    LOG_WARN("[BATMON_TEST] ========================================");
    LOG_WARN("[BATMON_TEST] CALIBRATION REQUIRED");
    LOG_WARN("[BATMON_TEST] ========================================");
    LOG_WARN("[BATMON_TEST] Disconnect all loads from the battery!");
    LOG_WARN("[BATMON_TEST] Ensure zero current flow.");
    LOG_WARN("[BATMON_TEST] Starting calibration in 3 seconds...");
    osDelay(3000);

    // Calibrate
    if (!monitor.calibrate(100)) {
        LOG_ERROR("[BATMON_TEST] Calibration failed!");
        monitor.stop();
        g_testBatteryMonitor = nullptr;
        return;
    }

    // Display calibration results
    const userconfig_batmon_t* cal = monitor.getCalibration();
    if (cal) {
        LOG_INFO("[BATMON_TEST] ========================================");
        LOG_INFO("[BATMON_TEST] Calibration Results:");
        LOG_INFO("[BATMON_TEST] ========================================");
        LOG_INFO("[BATMON_TEST] - VREF:               %.4fV", cal->vref);
        LOG_INFO("[BATMON_TEST] - Zero offset (Raw):  %.4fV", cal->zero_current_raw);
        LOG_INFO("[BATMON_TEST] - Zero offset (Buff): %.4fV", cal->zero_current_buffered);
        LOG_INFO("[BATMON_TEST] - Sensitivity:        %.3fV/A", cal->sensitivity_raw);
    }

    // Monitor current over duration
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Monitoring Current Flow");
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] You can now apply load to observe current changes");

    uint32_t start_time = HAL_GetTick();
    uint32_t last_print = start_time;
    uint32_t print_interval = 1000;  // Print every 1000ms

    while ((HAL_GetTick() - start_time) < duration_ms) {
        if ((HAL_GetTick() - last_print) >= print_interval) {
            data = monitor.getData();

            // Determine consumption state (unidirectional - only positive current)
            const char* state = "";
            if (data.current_average > 0.5f) {
                state = " [ACTIVE]";
            } else if (data.current_average > 0.05f) {
                state = " [STANDBY]";
            } else {
                state = " [IDLE]";
            }

            LOG_INFO("[BATMON_TEST] Consumption: %7.3fA (Raw: %7.3fA | Buff: %7.3fA)%s",
                     data.current_average,
                     data.current_raw,
                     data.current_buffered,
                     state);

            last_print = HAL_GetTick();
        }

        osDelay(10);
    }

    // Display final statistics
    BatteryMonitorStats stats = monitor.getStats();
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Test Statistics:");
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] - Sample count:       %lu", stats.sample_count);
    LOG_INFO("[BATMON_TEST] - DMA callbacks:      %lu", stats.dma_complete_count);
    LOG_INFO("[BATMON_TEST] - DMA errors:         %lu", stats.dma_error_count);
    LOG_INFO("[BATMON_TEST] - Current Min:        %.3fA", stats.current_min);
    LOG_INFO("[BATMON_TEST] - Current Max:        %.3fA", stats.current_max);
    LOG_INFO("[BATMON_TEST] - Current Average:    %.3fA", stats.current_avg);

    // Calculate power if voltage is known (example with 12V battery)
    float battery_voltage = 12.0f;  // Adjust based on actual battery
    float power = stats.current_avg * battery_voltage;
    LOG_INFO("[BATMON_TEST] - Avg Power (@ %.1fV): %.2fW", battery_voltage, power);

    // Interpretation guide
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Interpretation Guide:");
    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] - ACS758 monitors unidirectional current (drone consumption)");
    LOG_INFO("[BATMON_TEST] - Zero point: ~1.65V (VCC/2) when no current flows");
    LOG_INFO("[BATMON_TEST] - Current flow: Voltage increases from 1.65V");
    LOG_INFO("[BATMON_TEST] - Sensitivity: 40mV/A (0-50A range)");
    LOG_INFO("[BATMON_TEST] - Max consumption: 50A");

    // Stop monitor
    monitor.stop();
    g_testBatteryMonitor = nullptr;

    LOG_INFO("[BATMON_TEST] ========================================");
    LOG_INFO("[BATMON_TEST] Battery Monitor Test Complete");
    LOG_INFO("[BATMON_TEST] ========================================");
}
