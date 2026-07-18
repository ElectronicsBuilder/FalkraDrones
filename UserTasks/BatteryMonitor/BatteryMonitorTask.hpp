/**
 * @file    BatteryMonitorTask.hpp
 * @brief   FreeRTOS Task for Battery Current Monitoring
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

#ifndef BATTERYMONITORTASK_HPP
#define BATTERYMONITORTASK_HPP

#include "cmsis_os2.h"
#include "BatteryMonitor.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS task for battery current monitoring
 * @param argument Task argument (unused)
 *
 * This task:
 * - Initializes the battery monitor
 * - Performs zero-current calibration
 * - Periodically samples battery current at 10 Hz
 * - Logs significant current changes
 * - Makes current data available to other tasks
 */
void batteryMonitorTask(void *argument);

/**
 * @brief Get the current battery current reading (thread-safe)
 * @return Current in Amperes (positive = charging, negative = discharging)
 */
float getBatteryCurrent(void);

/**
 * @brief Get the battery monitor instance (for advanced access)
 * @return Pointer to BatteryMonitor instance, or nullptr if not initialized
 * @note Do not delete this pointer - it's managed by the task
 */
BatteryMonitor* getBatteryMonitor(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERYMONITORTASK_HPP
