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
 * @file    spi_flash_block_device.hpp
 * @brief   SPI Flash Block Device Interface for Flash File System (FFS)
 * @details Hardware abstraction layer connecting SPI flash memory to Flash File
 *          System (FFS) providing reliable file storage for audio files, graphics
 *          assets, configuration data, and flight logs. Implements block device
 *          interface with DMA support for efficient data operations.
 * 
 * Block Device Features:
 * - FFS integration for reliable file system operations
 * - DMA-accelerated read/write operations for performance
 * - Block-level erase and programming interface
 * - Wear leveling support through FFS layer
 * - Error detection and recovery mechanisms
 * - RTOS integration for concurrent file operations
 * 
 * 
 * Performance Characteristics (Storage Device):
 * | Operation Type | Speed      | Block Size | Use Case              |
 * |----------------|------------|------------|-----------------------|
 * | DMA Read       | 50 MB/s    | 4KB        | Asset streaming       |
 * | DMA Program    | 2 MB/s     | 256 bytes  | Data logging          |
 * | Block Erase    | 25ms       | 4KB        | File deletion         |
 * | Sync Operation | <1ms       | N/A        | Data integrity        |
 * 
 * FFS Integration Benefits:
 * - Automatic wear leveling extends flash memory life
 * - Power-loss recovery ensures data integrity
 * - File system consistency checks and repair
 * - Efficient space utilization with garbage collection
 * - Thread-safe file operations for multi-task access
 * 
 * Drone File System Applications:
 * - WAV audio file storage for voice announcements
 * - TouchGFX graphics asset streaming for real-time UI
 * - Flight log storage for post-flight analysis
 * - Configuration backup and recovery
 * - Firmware update storage and verification
 */

#ifndef __SPI_FLASH_BLOCK_DEVICE_HPP
#define __SPI_FLASH_BLOCK_DEVICE_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ffs_config.h"  // Include config first for all FFS settings
#include "ffs.h"         // Include FFS after config

#define LOG_FFS_READ_WRITE        0U
#define FFS_USE_RTOS              1U

#define SPI_FLASH_BASE_ADDR       0x00000000UL
#define SPI_FLASH_TOTAL_SIZE      (16 * 1024 * 1024UL) // 16MB
#define SPI_FLASH_BLOCK_SIZE      FFS_DEFAULT_BLOCK_SIZE      // Use config value
#define SPI_FLASH_BLOCK_COUNT     (SPI_FLASH_TOTAL_SIZE / SPI_FLASH_BLOCK_SIZE)

// Aliases for backwards compatibility
#define SPI_FLASH_MAX_FILES         FFS_MAX_FILES
#define SPI_FLASH_MAX_FILE_NAME     FFS_MAX_FILENAME


// FFS backend config for this block device
extern ffs_config_t spi_flash_fs_config;

// Functions implemented in spi_flash_block_device.cpp
int spi_flash_read_dma(void* context, uint32_t addr, void* buffer, uint32_t size);
int spi_flash_prog_dma(void* context, uint32_t addr, const void* data, uint32_t size);
int spi_flash_read(void* context, uint32_t addr, void* buffer, uint32_t size);
int spi_flash_read_IT(void* context, uint32_t addr, void* buffer, uint32_t size);
int spi_flash_prog(void* context, uint32_t addr, const void* data, uint32_t size);
int spi_flash_prog_IT(void* context, uint32_t addr, const void* data, uint32_t size);
int spi_flash_erase_block(void* context, uint32_t block);
int spi_flash_sync(void* context);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_FLASH_BLOCK_DEVICE_HPP */
