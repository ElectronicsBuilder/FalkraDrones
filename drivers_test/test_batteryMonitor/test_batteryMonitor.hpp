/**
 * @file    test_batteryMonitor.hpp
 * @brief   Battery Monitor Test Header
 * @details Test suite for ACS758 Hall Effect Current Sensor driver
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

#ifndef TEST_BATTERYMONITOR_HPP
#define TEST_BATTERYMONITOR_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test the Battery Monitor driver
 * @param duration_ms Test duration in milliseconds
 */
void test_batteryMonitor(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // TEST_BATTERYMONITOR_HPP
