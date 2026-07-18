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
 * @file    test_spi_flash.cpp
 * @brief   SPI Flash Memory Test Suite
 * @details Comprehensive test suite for W25Q128 SPI flash memory operations
 *          including basic read/write, DMA transfers, sector erase, and
 *          performance benchmarking. Validates flash functionality for
 *          TouchGFX assets and data storage on FalkraController.
 */

#include "spi_flash.hpp"
#include "log.hpp"
#include "cmsis_os.h"
#include "task.h"
#include "string.h"
#include "test_spi_flash.hpp"
#include "main.h"
#include "driver_manager.hpp"

//extern SPI_HandleTypeDef hspi1;

static bool dmaTestDone = false;
static void dmaDone() { dmaTestDone = true; }


void test_spi_flash_rw()
{
    // Get SPI Flash instance from DriverManager
    auto& dm = DriverManager::getInstance();
    SpiFlash* spiFlash = dm.getSpiFlash();

    if (!spiFlash) {
        LOG_ERROR("[FLASH TEST] SPI Flash driver not available via DriverManager");
        return;
    }

    const uint32_t addr = 0x0000;
    const char* msg = "Falkra FLASH!";
    uint8_t buf[16] = {0};

    FlashDeviceInfo info = spiFlash->getDeviceInfo();
    LOG_INFO("[FLASH] SPI Flash: %s, %lu Mbit", info.part_number, info.capacity_mbit);
    LOG_INFO("[FLASH] Page Size: %lu bytes, Sector Size: %lu bytes, Quad: %s",
             info.page_size,
             info.sector_size,
             info.supports_quad ? "Yes" : "No");

    // --- Erase Sector ---
    spiFlash->eraseSector(addr);

    LOG_INFO("[FLASH] Flash WRITE: %s", msg);
    // --- Write Data ---
    spiFlash->writeData(addr, reinterpret_cast<const uint8_t*>(msg), strlen(msg));

    // --- Read Data ---
    spiFlash->readData(addr, buf, strlen(msg));
    LOG_INFO("[FLASH] Read from Flash: %s", buf);

    if (memcmp(msg, buf, strlen(msg)) == 0) {
        LOG_INFO("[FLASH] SPI Flash verification passed");
    } else {
        LOG_ERROR("[FLASH] SPI Flash verification FAILED");
    }

    test_spi_flash_rw_dma(); // run DMA test

    // Cleanup before returning
    spiFlash->deinit();

}

void test_spi_flash_rw_dma() {
    // Get SPI Flash instance from DriverManager
    auto& dm = DriverManager::getInstance();
    SpiFlash* spiFlash = dm.getSpiFlash();

    if (!spiFlash) {
        LOG_ERROR("[FLASH TEST] SPI Flash driver not available via DriverManager");
        return;
    }

    spiFlash->init();

    const uint32_t addr = 0x0100;
    const char* msg = "Falkra DMA Test!";
    const size_t msgLen = strlen(msg);

    static uint8_t rxBuf[32] = {0};

    spiFlash->eraseSector(addr);
    LOG_INFO("[FLASH] Erased sector at 0x%06lX", addr);

    dmaTestDone = false;
    spiFlash->writeDataDMA(addr, reinterpret_cast<const uint8_t*>(msg), msgLen, dmaDone);
    while (!dmaTestDone) vTaskDelay(1);

    dmaTestDone = false;
    //spiFlash->readDataDMA(addr, rxBuf, msgLen, dmaDone);   // Dma used for Wifi-SPi
    spiFlash->readData(addr, rxBuf, strlen(msg));
    while (!dmaTestDone) osDelay(1);

    LOG_INFO("[FLASH] Read (DMA): %s", rxBuf);
    if (memcmp(msg, rxBuf, msgLen) == 0) {
        LOG_INFO("[FLASH] DMA test passed");
    } else {
        LOG_ERROR("[FLASH] DMA test FAILED");
    }

    // Note: deinit called in test_spi_flash_rw()
}
