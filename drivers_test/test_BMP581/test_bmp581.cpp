/**
 * @file    test_bmp581.cpp
 * @brief   Barometric Pressure Sensor Test Implementation
 * @details Implementation of test suite for BMP581 barometric pressure
 *          sensor testing and altitude estimation
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

#include "test_bmp581.hpp"
#include "bmp581.hpp"
#include "bmp5.h"
#include "bmp5_defs.h"
#include "log.hpp"
#include "main.h"
#include "stm32f7xx_hal.h"
#include "driver_manager.hpp"

void test_bmp581()
{
    LOG_INFO("[BMP581] BMP581 Test Starting");

    // Get BMP581 instance from DriverManager
    auto& dm = DriverManager::getInstance();
    BMP581* bmp = dm.getBMP581();

    if (!bmp) {
        LOG_ERROR("[BMP581] BMP581 driver not available from DriverManager!");
        return;
    }

    // BMP581 is already initialized by DriverManager
    uint8_t chip_id = 0;

    int8_t rslt = bmp5_get_regs(BMP5_REG_CHIP_ID, &chip_id, 1, bmp->dev());
    if (rslt == BMP5_OK) {
        //LOG_INFO("CHIP_ID: 0x%02X", chip_id);
    } else {
        LOG_ERROR("bmp5_get_regs failed: %d", rslt);
    }

    if (bmp5_get_regs(BMP5_REG_CHIP_ID, &chip_id, 1, bmp->dev()) == BMP5_OK) {
        LOG_INFO("[BMP581] Chip ID: 0x%02X", chip_id);

        if (chip_id == BMP5_CHIP_ID_PRIM || chip_id == BMP5_CHIP_ID_SEC) {
            LOG_INFO("[BMP581] Chip ID is valid");
        } else {
            LOG_WARN("[BMP581] Chip ID unexpected");
        }
    } else {
        LOG_ERROR("[BMP581] Failed to read chip ID");
    }

    float temp = 0.0f, pressure = 0.0f;
    if (bmp->getSensorData(temp, pressure)) {
        LOG_INFO("[BMP581] Temperature: %.2f °C", temp);
        LOG_INFO("[BMP581] Pressure: %.2f Pa", pressure);

        const float seaLevelPressure = 101325.0f; // Pa
        float altitude = 44330.0f * (1.0f - powf(pressure / seaLevelPressure, 0.1903f));

        LOG_INFO("[BMP581] Estimated Altitude: %.2f meters", altitude);
    } else {
        LOG_ERROR("[BMP581] Failed to read sensor data");
    }
}

void altitude_continous_read(void)
{
    LOG_INFO("[BMP581] BMP581 Continuous Read Starting");

    // Get BMP581 instance from DriverManager
    auto& dm = DriverManager::getInstance();
    BMP581* bmp = dm.getBMP581();

    if (!bmp) {
        LOG_ERROR("[BMP581] BMP581 driver not available from DriverManager!");
        return;
    }

    // BMP581 is already initialized by DriverManager
    uint8_t chip_id = 0;

    int8_t rslt = bmp5_get_regs(BMP5_REG_CHIP_ID, &chip_id, 1, bmp->dev());
    if (rslt == BMP5_OK) {
        //LOG_INFO("CHIP_ID: 0x%02X", chip_id);
    } else {
        LOG_ERROR("bmp5_get_regs failed: %d", rslt);
    }

    if (bmp5_get_regs(BMP5_REG_CHIP_ID, &chip_id, 1, bmp->dev()) == BMP5_OK) {
        LOG_INFO("[BMP581] Chip ID: 0x%02X", chip_id);

        if (chip_id == BMP5_CHIP_ID_PRIM || chip_id == BMP5_CHIP_ID_SEC) {
            LOG_INFO("[BMP581] Chip ID is valid");
        } else {
            LOG_WARN("[BMP581] Chip ID unexpected");
        }
    } else {
        LOG_ERROR("[BMP581] Failed to read chip ID");
    }

    while(1)
    {
        float temp = 0.0f, pressure = 0.0f;
        if (bmp->getSensorData(temp, pressure)) {
            LOG_INFO("[BMP581] Temperature: %.2f °C", temp);
            LOG_INFO("[BMP581] Pressure: %.2f Pa", pressure);
    
            const float seaLevelPressure = 101325.0f; // Pa
            float altitude = 44330.0f * (1.0f - powf(pressure / seaLevelPressure, 0.1903f));
    
            LOG_INFO("[BMP581] Estimated Altitude: %.2f meters", altitude);
        } else {
            LOG_ERROR("[BMP581] Failed to read sensor data");
        }
    }
}
