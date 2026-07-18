/**
 * @file    test_radio_receiverHW.cpp
 * @brief   Radio Receiver Hardware Test Implementation
 * @details Implementation of test suite for RC radio receiver hardware,
 *          validating PPM and SBUS signal paths
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

#include "test_radio_receiverHW.hpp"
#include "radio_receiver.hpp"
#include "log.hpp"
#include "main.h"
#include "driver_manager.hpp"

void test_radio_receiverHW()
{
    LOG_INFO("[TEST] Radio Receiver Test Begin");

    // Get radio receiver from DriverManager singleton
    auto& dm = DriverManager::getInstance();
    RadioReceiver* receiver = dm.getRadioReceiver();

    if (!receiver) {
        LOG_ERROR("[TEST] RadioReceiver not available via DriverManager");
        return;
    }

    // Check PPM output status
    HAL_Delay(50);
    LOG_INFO("[PPM] PPM Output Enabled: %s", receiver->isPPMEnabled() ? "YES" : "NO");

    // Optional SBUS enable
    receiver->enableSBUS(true);
    HAL_Delay(10);
    LOG_INFO("[SBUS] SBUS Output Enabled: %s", receiver->isSBUSEnabled() ? "YES" : "NO");

    LOG_INFO("[TEST] Radio Receiver Test Complete");
}