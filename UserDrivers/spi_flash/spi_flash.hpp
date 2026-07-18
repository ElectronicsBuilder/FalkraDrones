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
 * @file    spi_flash.hpp
 * @brief   SPI Flash Memory Driver Interface - High Capacity Storage for Assets
 * @details Interface for Winbond W25Q128 (16MB) and compatible SPI flash memory.
 *          Provides non-volatile storage for large data assets including:
 *          - TouchGFX graphics and font assets
 *          - Audio files (WAV format for operational tones)
 *          - Firmware updates and bootloader images
 *          - Data logging and telemetry archives
 * 
 * Technical Specifications (W25Q128):
 * - Capacity: 16MB (128Mbit)
 * - Page Size: 256 bytes (programmable unit)
 * - Sector Size: 4KB (erasable unit)
 * - Block Size: 64KB (fast erase unit)
 * - Write Endurance: 100,000 erase/program cycles
 * - Data Retention: 20 years at 85°C
 * 
 * Memory Organization:
 * | Address Range      | Size    | Purpose                |
 * |--------------------|---------|------------------------|
 * | 0x000000-0x3FFFFF  | 4MB     | TouchGFX Font Assets   |
 * | 0x400000-0x7FFFFF  | 4MB     | TouchGFX Image Assets  |
 * | 0x800000-0x9FFFFF  | 2MB     | Audio Files (WAV)      |
 * | 0xA00000-0xEFFFFF  | 5MB     | Firmware Storage       |
 * | 0xF00000-0xFFFFFF  | 1MB     | Data Logs & Config     |
 * 
 * Performance Characteristics:
 * | Operation          | Speed     | DMA Support | Typical Use      |
 * |--------------------|-----------|-------------|------------------|
 * | Page Program       | 1-3ms     | Yes         | Asset upload     |
 * | Sector Erase (4KB) | 50-200ms  | No          | File deletion    |
 * | Chip Erase         | 50-200s   | No          | Factory reset    |
 * | Read (Sequential)  | 50MB/s    | Yes         | Asset streaming  |
 * | Read (Random)      | ~25MB/s   | Yes         | File access      |
 * 
 * Usage Example:
 * @code
 * SpiFlash flash(&hspi1, FLASH_CS_GPIO_Port, FLASH_CS_Pin);
 * flash.init();
 * 
 * // Store audio file
 * uint8_t audio_data[1024] = {...};
 * flash.eraseSector(0x800000);  // Erase before write
 * flash.writeData(0x800000, audio_data, sizeof(audio_data));
 * 
 * // Read back with DMA for streaming
 * uint8_t buffer[256];
 * flash.readDataDMA(0x800000, buffer, 256, audio_dma_callback);
 * @endcode
 */

#ifndef SPI_FLASH_HPP
#define SPI_FLASH_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stddef.h>

struct FlashDeviceInfo {
    const char* part_number;
    uint32_t capacity_mbit;
    uint32_t page_size;
    uint32_t sector_size;
    bool supports_quad;
};

typedef void (*SpiFlashDMACallback)(void);

class SpiFlash {
public:
    SpiFlash(SPI_HandleTypeDef* spiHandle, GPIO_TypeDef* csPort, uint16_t csPin);
    ~SpiFlash();

    void init();
    void deinit();
    void writeEnable();
    void reset();

    void writeData(uint32_t address, const uint8_t* data, size_t length);
    void writeData_IT(uint32_t address, const uint8_t* data, size_t length);
    void readData(uint32_t address, uint8_t* buffer, size_t length);
    void readData_IT(uint32_t address, uint8_t* buffer, size_t length);
    void eraseSector(uint32_t address);
    void eraseChip();
    void writeDataDMA(uint32_t address, const uint8_t* data, size_t length, SpiFlashDMACallback cb = nullptr);
    void readDataDMA(uint32_t address, uint8_t* buffer, size_t length, SpiFlashDMACallback cb = nullptr);

    uint32_t readDeviceID();
    FlashDeviceInfo getDeviceInfo();

    // DMA handler access for IRQs
    void onTxComplete();
    void onTxComplete_IT();  // IT-specific TX complete handler
    void onRxComplete();
    void onRxComplete_IT();  // IT-specific RX complete handler

    volatile bool txDone = false;
    volatile bool txDone_IT = false;  // Separate flag for IT operations
    volatile bool rxDone = false;
    volatile bool rxDone_IT = false;  // Separate flag for IT operations
    volatile bool txUsingDma = false;
    volatile bool rxUsingDma = false;

    SPI_HandleTypeDef* spiHandle;

private:
    void select();
    void deselect();
    void sendCommand(uint8_t cmd);
    void pollBusy();


    GPIO_TypeDef* csPort;
    uint16_t csPin;

    static uint8_t dmaCmdBuf[4];
    const uint8_t* dmaTxBuf;
    uint8_t* dmaRxBuf;
    SpiFlashDMACallback txCallback;
    SpiFlashDMACallback rxCallback;
};


#ifdef __cplusplus
}
#endif


#endif // SPI_FLASH_HPP
