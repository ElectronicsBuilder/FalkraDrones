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
 * @file    spi_flash_block_device.cpp
 * @brief   SPI Flash Block Device Implementation
 */

#include "spi_flash_block_device.hpp"
#include "spi_flash.hpp"  // SpiFlash class declaration
#include "log.hpp"

#if FFS_USE_RTOS
#include "cmsis_os2.h"
#endif 

extern SpiFlash spiFlash;

// Internal helper to convert block to address
static inline uint32_t block_to_address(uint32_t block) {
    return block * SPI_FLASH_BLOCK_SIZE;
}

int spi_flash_read(void* context, uint32_t addr, void* buffer, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    const size_t maxChunk = 256;   // same page size handling as DMA

    while (size > 0) {
        size_t chunk = (size > maxChunk) ? maxChunk : size;

#if LOG_FFS_READ_WRITE
        LOG_INFO("[SPI] Blocking Read @ 0x%08X, %u bytes", (unsigned int)absolute, chunk);
#endif
        // Perform the blocking read
        spiFlash.readData(absolute, dst, chunk);

        absolute += chunk;
        dst      += chunk;
        size     -= chunk;
    }

    return 0;
}

int spi_flash_read_IT(void* context, uint32_t addr, void* buffer, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    const size_t maxChunk = 256;   // same page size handling as DMA

    while (size > 0) {
        size_t chunk = (size > maxChunk) ? maxChunk : size;

#if LOG_FFS_READ_WRITE
        LOG_INFO("[SPI] IT Read @ 0x%08X, %u bytes", (unsigned int)absolute, chunk);
#endif
        // Perform interrupt-based read
        spiFlash.readData_IT(absolute, dst, chunk);

        absolute += chunk;
        dst      += chunk;
        size     -= chunk;
    }

    return 0;
}

int spi_flash_prog_IT(void* context, uint32_t addr, const void* data, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;

    const uint8_t* src = static_cast<const uint8_t*>(data);
    const size_t maxChunk = 256;   // Page size limit for flash programming

    while (size > 0) {
        size_t chunk = (size > maxChunk) ? maxChunk : size;

#if LOG_FFS_READ_WRITE
        LOG_INFO("[SPI] IT Write @ 0x%08X, %u bytes", (unsigned int)absolute, chunk);
#endif
        // Perform interrupt-based write
        spiFlash.writeData_IT(absolute, src, chunk);

        absolute += chunk;
        src      += chunk;
        size     -= chunk;
    }

    return 0;
}




int spi_flash_prog(void* context, uint32_t addr, const void* data, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;
    //LOG_INFO("[SPI] Write @ 0x%08X, %u bytes", (unsigned int)absolute, size);
    spiFlash.writeData(absolute, static_cast<const uint8_t*>(data), size);
    return 0;
}

int spi_flash_erase_block(void* context, uint32_t block) {
    spiFlash.eraseSector(block_to_address(block));
    return 0;
}

int spi_flash_erase_chip(void* context) {
    spiFlash.eraseChip();  // Much faster
    return 0;
}


int spi_flash_sync(void* context) {
    // Assume all operations are blocking and complete before return
    return 0;
}

int spi_flash_init(void* context) {
    spiFlash.init();
    return 0;
}


int spi_flash_prog_dma(void* context, uint32_t addr, const void* data, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const size_t maxChunk = 256;

    while (size > 0) {
        size_t chunk = (size > maxChunk) ? maxChunk : size;

        spiFlash.txDone = false;
#if LOG_FFS_READ_WRITE
        LOG_INFO("[SPI] DMA Write @ 0x%08X, %u bytes", (unsigned int)absolute, chunk);
#endif
        spiFlash.writeDataDMA(absolute, bytes, chunk);

        // writeDataDMA blocks internally (bounded). If its completion flag
        // is still unset here the write failed/timed out — report it instead
        // of hanging (was an unbounded wait).
        uint32_t waited = 0;
        while (!spiFlash.txDone) {
            if (++waited > 500) {
                LOG_ERROR("[SPI] DMA write completion timeout @ 0x%08X", (unsigned int)absolute);
                return -1;
            }
        #if FFS_USE_RTOS
        osDelay(1);
        #else
        HAL_Delay(1);
        #endif
        }

        absolute += chunk;
        bytes    += chunk;
        size     -= chunk;
    }

    return 0;
}

int spi_flash_read_dma(void* context, uint32_t addr, void* buffer, uint32_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(context);
    uintptr_t absolute = base + addr;

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    const size_t maxChunk = 256;

    while (size > 0) {
        size_t chunk = (size > maxChunk) ? maxChunk : size;

        spiFlash.rxDone = false;
#if LOG_FFS_READ_WRITE
        LOG_INFO("[SPI] DMA Read @ 0x%08X, %u bytes", (unsigned int)absolute, chunk);
#endif
        spiFlash.readDataDMA(absolute, dst, chunk);

        uint32_t waited = 0;
        while (!spiFlash.rxDone) {
            if (++waited > 500) {
                LOG_ERROR("[SPI] DMA read completion timeout @ 0x%08X", (unsigned int)absolute);
                return -1;
            }
        #if FFS_USE_RTOS
        osDelay(1);
        #else
        HAL_Delay(1);
        #endif
        }

        absolute += chunk;
        dst      += chunk;
        size     -= chunk;
    }

    return 0;
}





// Definition of the FFS configuration
ffs_config_t spi_flash_fs_config = {
    .block_size             = SPI_FLASH_BLOCK_SIZE,
    .block_count            = SPI_FLASH_BLOCK_COUNT,
    .context                = (void*)SPI_FLASH_BASE_ADDR,

    // Read operation mode selection:
  //  .read                   = spi_flash_read_dma,      // DMA mode (fastest, uses DMA)
  //  .read                   = spi_flash_read,          // Blocking mode (simplest, blocks CPU)
    .read                   = spi_flash_read_IT,         // Interrupt mode (balanced, frees DMA for WiFi)

    // Write operation mode selection:
   .prog                   = spi_flash_prog_dma,        // DMA mode (default)
   // .prog                   = spi_flash_prog,            // Blocking mode
  //  .prog                   = spi_flash_prog_IT,         // Interrupt mode
    .erase                  = spi_flash_erase_block,
    .eraseChip              = spi_flash_erase_chip,
    .sync                   = spi_flash_sync,
    .init                   = spi_flash_init,

    // Derived layout (computed for SPI flash)

    .blockmeta_start_block  = (SPI_FLASH_BLOCK_COUNT
                               - 1 /*header*/
                               - 1 /*table*/
                               - 2 /*reserved*/
                               - FFS_BLOCK_META_BLOCKS),
    .blockmeta_block_count  = FFS_BLOCK_META_BLOCKS,
    .header_block           = SPI_FLASH_BLOCK_COUNT - 1,
    .table_block            = SPI_FLASH_BLOCK_COUNT - 2,
    .reserved_blocks        = 2,
    .reserved_start_block   = SPI_FLASH_BLOCK_COUNT - 4,

    .header_addr            = (SPI_FLASH_BLOCK_COUNT - 1) * SPI_FLASH_BLOCK_SIZE,
    .table_addr             = (SPI_FLASH_BLOCK_COUNT - 2) * SPI_FLASH_BLOCK_SIZE,
    .file_data_addr         = (SPI_FLASH_BLOCK_COUNT - 4) * SPI_FLASH_BLOCK_SIZE,
    


    .max_files              = SPI_FLASH_MAX_FILES,
    .max_filename_len       = SPI_FLASH_MAX_FILE_NAME
};
