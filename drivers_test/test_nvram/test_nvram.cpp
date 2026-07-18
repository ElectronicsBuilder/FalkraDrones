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
 * @file    test_nvram.cpp
 * @brief   NVRAM Validation Test Implementation
 * @details Implementation of NVRAM testing functionality, providing comprehensive
 *          validation of read/write operations, data persistence, and reliability
 *          of the non-volatile memory system.
 */

#include "nvram.hpp"
#include "driver_manager.hpp"
#include "log.hpp"
#include "main.h"
#include "cmsis_os.h"
#include "stm32f7xx_hal.h"
#include "task.h"
#include "string.h"

#include "test_nvram.hpp"

void test_nvram_class_driver()
{
    LOG_INFO("[NVRAM TEST] Starting NVRAM validation test via DriverManager");

    // Get NVRAM instance from DriverManager singleton
    auto& dm = DriverManager::getInstance();
    NVRAM* nvram = dm.getNVRAM();

    if (!nvram) {
        LOG_ERROR("[NVRAM TEST] NVRAM driver not available via DriverManager");
        return;
    }

    // Display device information
    auto info = nvram->getDeviceInfo();

    LOG_INFO("[NVRAM TEST] Device: %s, %lu KB, Max Addr: 0x%05lX, AutoStore: %s",
             info.part_number,
             info.capacity_kbyte,
             info.max_address,
             info.has_autostore ? "Yes" : "No");

    // Test 1: String write/read
    const char* message = "Falkra NVRAM!";
    uint8_t readback[32] = {0};
    LOG_INFO("[NVRAM TEST] Writing test string: %s", message);

    nvram->writeArray(0x0000, (uint8_t*)message, strlen(message));
    osDelay(100);  // Wait for write to persist

    nvram->readArray(0x0000, readback, strlen(message));
    LOG_INFO("[NVRAM TEST] Read back: %s", readback);

    if (memcmp(message, readback, strlen(message)) == 0) {
        LOG_INFO("[NVRAM TEST] String write/read test PASSED");
    } else {
        LOG_ERROR("[NVRAM TEST] String write/read test FAILED");
    }

    // Test 2: Pattern write/read
    LOG_INFO("[NVRAM TEST] Running pattern verification test");
    uint32_t test_pattern = 0xDEADBEEF;
    uint8_t write_data[4] = {
        (uint8_t)(test_pattern >> 0),
        (uint8_t)(test_pattern >> 8),
        (uint8_t)(test_pattern >> 16),
        (uint8_t)(test_pattern >> 24)
    };

    uint8_t read_data[4] = {0};
    nvram->writeArray(0x0100, write_data, 4);
    osDelay(100);

    nvram->readArray(0x0100, read_data, 4);

    uint32_t read_value = (read_data[3] << 24) | (read_data[2] << 16) |
                          (read_data[1] << 8) | read_data[0];

    if (read_value == test_pattern) {
        LOG_INFO("[NVRAM TEST] Pattern test PASSED: 0x%08lX", read_value);
    } else {
        LOG_ERROR("[NVRAM TEST] Pattern test FAILED: Expected 0x%08lX, got 0x%08lX",
                  test_pattern, read_value);
    }

    LOG_INFO("[NVRAM TEST] NVRAM validation test complete");
}






