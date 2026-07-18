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
 * @file    display_driver.cpp
 * @brief   Global Display Driver Instance Implementation
 */

 #include "display_driver.hpp"
#include "main.h"

extern SPI_HandleTypeDef hspi6;

uint16_t DISPLAY_DONE_UPDATE = 0;

// Display configuration used by DriverManager for ST7789 initialization
ST7789::Config display_config = {
    .hspi = &hspi6,
    .cs_port = TFT_CS_GPIO_Port,
    .cs_pin = TFT_CS_Pin,
    .dc_port = TFT_DC_GPIO_Port,
    .dc_pin = TFT_DC_Pin,
    .reset_port = TFT_RESET_GPIO_Port,
    .reset_pin = TFT_RESET_Pin,
    .bkl_port = BKL_PWM_GPIO_Port,
    .bkl_pin = BKL_PWM_Pin,
    .scan_dir = ST7789_SCAN_DIR_VERTICAL
};

// NOTE: ST7789 instance is now managed by DriverManager singleton
// See DriverManager::initializeDriver(DriverId::ST7789) in driver_manager.cpp
