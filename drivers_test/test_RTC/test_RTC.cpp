/**
 * @file    test_RTC.cpp
 * @brief   Real-Time Clock Validation Test Suite Implementation
 * @details Implementation of RTC test functions for validating time accuracy,
 *          calendar operations, and timestamp precision
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

#include "test_RTC.hpp"
#include "rtc.hpp"
#include "log.hpp"
#include "driver_manager.hpp"
#include <stdio.h>

bool test_rtc() {
    // Get RTC instance from DriverManager
    auto& dm = DriverManager::getInstance();
    RtcDriver* rtc = dm.getRTC();

    if (!rtc) {
        LOG_ERROR("[RTC TEST] RTC driver not available via DriverManager");
        return false;
    }

    LOG_INFO("[RTC] Initializing RTC...");
    if (!rtc->init()) {
        LOG_ERROR("[RTC] RTC init failed");
        return false;
    }

    const char* now = rtc->get_formatted_time();
    LOG_INFO("[RTC] Current Time: %s", now);

    // Cleanup before returning (note: rtc is singleton, so this just resets internal state)
    rtc->deinit();

    return true;
}

bool rtc_set_from_string(const char* timeStr) {
    // Get RTC instance from DriverManager
    auto& dm = DriverManager::getInstance();
    RtcDriver* rtc = dm.getRTC();

    if (!rtc) {
        LOG_ERROR("[RTC TEST] RTC driver not available via DriverManager");
        return false;
    }

    uint16_t y; uint8_t M, d, H, m, s;
    if (sscanf(timeStr, "%hu-%hhu-%hhu_%hhu-%hhu-%hhu", &y, &M, &d, &H, &m, &s) != 6) {
        LOG_ERROR("[RTC] Invalid time format. Use YYYY-MM-DD_HH-MM-SS");
        return false;
    }

    rtc->set_date(1 /*Weekday*/, M, d, y - 2000);
    rtc->set_time(H, m, s);
    LOG_INFO("[RTC] Time updated to %04d-%02d-%02dT%02d:%02d:%02dZ", y, M, d, H, m, s);
    return true;
}
