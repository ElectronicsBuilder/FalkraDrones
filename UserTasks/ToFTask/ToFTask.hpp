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
 * @file    ToFTask.hpp
 * @brief   Time-of-Flight Sensor Task Header
 * @details FreeRTOS task for VL53L5CX ToF sensor ranging and proximity detection.
 *          Uses TofProximityManager for efficient interrupt-driven or polling-based
 *          sensor management with 6 sensors (Top, Bottom, Front, Back, Left, Right).
 */

#ifndef TOF_TASK_HPP
#define TOF_TASK_HPP

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ToF task resources
 * @return true if initialization successful, false otherwise
 */
bool tof_task_init(void);

/**
 * @brief Main ToF task function - called by FreeRTOS task
 * @param argument FreeRTOS task argument (unused)
 *
 * This task:
 * - Initializes TofProximityManager
 * - Starts sensor ranging
 * - Processes sensor data (interrupt or polling based)
 * - Updates detection states and snapshot
 */
void tof_task(void *argument);


/**
 * @brief Detection task - confidence algorithm, snapshot update
 * @note Spawned separately from main_cpp.cpp alongside tof_task.
 *       Uses ToFDistanceTask_attributes (osPriorityNormal, 1024 stack).
 */
void tof_detection_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif // TOF_TASK_HPP
