/**
 * @file    test_st7789.cpp
 * @brief   LCD Display Test Implementation
 * @details Implementation of test suite for ST7789 LCD display testing,
 *          including screen initialization and color fill operations
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

#include "test_st7789.hpp"
#include "st7789.hpp"
#include "st7789_defs.h"
#include "main.h"
#include "log.hpp"
#include "driver_manager.hpp"

void test_st7789_sequence()
{
    LOG_INFO("[ST7789] Starting test sequence");

    // Get ST7789 display instance from DriverManager
    auto& dm = DriverManager::getInstance();
    ST7789* display = dm.getDisplay();

    if (!display) {
        LOG_ERROR("[ST7789] Display driver not available via DriverManager");
        return;
    }

    LOG_INFO("[ST7789] Fill RED");
    display->fillScreen(RED);
    HAL_Delay(1000);

    LOG_INFO("[ST7789] Fill GREEN");
    display->fillScreen(GREEN);
    HAL_Delay(1000);

    LOG_INFO("[ST7789] Fill BLUE");
    display->fillScreen(BLUE);
    HAL_Delay(1000);

    LOG_INFO("[ST7789] Test complete");
}