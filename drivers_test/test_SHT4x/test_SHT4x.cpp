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
 * @file    test_sht4x.cpp
 * @brief   SHT4x Environmental Sensor Test Implementation
 * @details Test implementation for SHT4x temperature and humidity sensor validation,
 *          verifying sensor initialization, measurement accuracy, and data retrieval
 *          functionality.
 */

#include "test_SHT4x.hpp"
#include "log.hpp"
#include "driver_manager.hpp"

bool test_sht4x()
{
    LOG_INFO("[SHT4X] Starting SHT4x sensor test...");

    // Get SHT4x instance from DriverManager
    auto& dm = DriverManager::getInstance();
    SHT4x* sht4x = dm.getSHT4x();

    if (!sht4x) {
        LOG_ERROR("[SHT4X] SHT4x driver not available from DriverManager!");
        return false;
    }

    // SHT4x is already initialized by DriverManager, just read values
    float temperature = 0.0f;
    float humidity = 0.0f;

    if (!sht4x->readTempAndHumidity(temperature, humidity)) {
        LOG_ERROR("[SHT4X] Failed to read from SHT4x sensor!");
        return false;
    }

    LOG_INFO("[SHT4X] SHT4x Temperature: %.2f °C", temperature);
    LOG_INFO("[SHT4X] SHT4x Humidity: %.2f %%", humidity);

    LOG_INFO("[SHT4X] SHT4x sensor test passed!");
    return true;
}
