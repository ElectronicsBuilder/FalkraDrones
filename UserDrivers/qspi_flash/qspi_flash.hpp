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
 * @file    qspi_flash.hpp
 * @brief   High-Performance QSPI Flash Memory Driver for TouchGFX Graphics Storage
 * @details Comprehensive driver for external Quad-SPI flash memory providing
 *          ultra-high-speed access to graphics assets, firmware images, and
 *          flight data. Optimized for STM32F767 QUADSPI peripheral with
 *          memory-mapped mode support for zero-latency TouchGFX asset access.
 * 
 * QSPI Flash Features:
 * - Quad-SPI interface for 4x faster data transfer than standard SPI
 * - Memory-mapped mode for direct CPU access without transfers
 * - DMA support for background data operations
 * - Hardware acceleration for graphics asset streaming
 * - Sector and chip erase operations for maintenance
 * - Device identification and status monitoring
 * 
 * Performance Characteristics (Storage Device):
 * | Operation Type    | Speed         | Use Case                    |
 * |-------------------|---------------|----------------------------|
 * | Quad Read         | 104 MHz       | TouchGFX asset streaming   |
 * | Memory-Mapped     | Zero latency  | Direct graphics access     |
 * | DMA Transfer      | Background    | Non-blocking data ops      |
 * | Page Program      | 256 bytes     | Configuration updates      |
 * | Sector Erase      | 4KB sectors   | Selective data cleanup     |
 * | Chip Erase        | Full device   | Complete memory reset      |
 * 
 * Memory Organization:
 * - Capacity: Device-dependent (typically 16MB-128MB)
 * - Page Size: 256 bytes (programming unit)
 * - Sector Size: 4KB (erase unit)
 * - Block Size: 64KB (fast erase unit)
 * - Address Space: Linear addressing for memory mapping
 * 
 * TouchGFX Integration:
 * - Memory-mapped asset access for real-time graphics
 * - Zero-copy texture streaming for smooth animations
 * - Background loading of graphics resources via DMA
 * - Firmware storage for over-the-air updates
 * 
 * Drone Application:
 * - High-resolution display assets and UI graphics
 * - Flight data logging with high-speed burst writes
 * - Firmware update storage and verification
 * - Configuration data backup and recovery
 */

#ifndef QSPI_FLASH_HPP
#define QSPI_FLASH_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stddef.h>

struct QFlashDeviceInfo {
    const char* part_number;
    uint32_t capacity_mbit;
    uint32_t page_size;
    uint32_t sector_size;
    bool supports_quad;
};



class QspiFlash {
public:
    QspiFlash(QSPI_HandleTypeDef* qspiHandle);
    ~QspiFlash();

    void init();
    void deinit();
    void reset();
    void readID(uint8_t* idBuffer);

    void readData(uint32_t address, uint8_t* buffer, size_t size);
    void readDataQuad(uint32_t address, uint8_t* buffer, size_t size);   
    bool readDataQuadDMA(uint32_t address, uint8_t* buffer, size_t size);

    void writeData(uint32_t address, const uint8_t* data, size_t size);
    void writeDataQuad(uint32_t address, const uint8_t* data, size_t size);
    bool writeDataQuadDMA(uint32_t address, const uint8_t* data, size_t size);


    void eraseSector(uint32_t address);
    void eraseChip();

    void setQuadEnable();
    void enableMemoryMappedMode();
    void enableQuadMemoryMappedMode();
    void enableDualMemoryMappedMode();
    void readIDQuad(uint8_t* idBuffer);
    QFlashDeviceInfo getDeviceInfo();
    bool waitDmaComplete(uint32_t timeout_ms);
    void autoPollingMemReady(uint32_t timeout = HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
    bool disableMemoryMappedMode();


private:
    QSPI_HandleTypeDef* qspiHandle;
    void inlineWriteEnable();
   // void autoPollingMemReady(uint32_t timeout = HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
    
    uint8_t getStatus();
};


#ifdef __cplusplus
}
#endif


#endif // QSPI_FLASH_HPP
