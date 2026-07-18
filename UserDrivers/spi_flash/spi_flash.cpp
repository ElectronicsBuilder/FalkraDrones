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
 * @file    spi_flash.cpp
 * @brief   SPI Flash Memory Driver for W25Q128 and compatible chips
 * @details Provides comprehensive interface for external SPI flash memory
 *          operations including read/write/erase, DMA support, and memory
 *          management. Used for storing TouchGFX assets, audio files, and
 *          configuration data on the FalkraController.
 */

#include "spi_flash.hpp"
#include "w25q128.h"
#include "main.h"
#include "test_spi_flash.hpp"
#include "log.hpp"
#include "../common/spi1_bus_lock.hpp"

#include <stdio.h>
#include <string.h>

// Forward declarations for Wi-Fi SPI callback functions
extern "C" {
    typedef void (*spi_transaction_complete_t)(void);
    //spi_transaction_complete_t spi_port_get_wifi_callback(void);
    SPI_HandleTypeDef* spi_port_get_wifi_spi_handle(void);

    extern spi_transaction_complete_t spi_port_transaction_complete_cb;
}
extern SPI_HandleTypeDef hspi1, hspi4;

SpiFlash spiFlash(&hspi1, FLASH_CS_GPIO_Port, FLASH_CS_Pin);


uint8_t SpiFlash::dmaCmdBuf[4];

static uint8_t spi_flash_dma_page_buf[4 + 256] __attribute__((section(".dma_buffer"), aligned(32)));

static void spi_flash_cs_high_guard(void) {
    /* W25Q page-program data wraps within the 256-byte page if extra clocks
       arrive before the chip sees CS high. Hold CS high long enough that the
       next status-poll command cannot be appended to the program stream. */
    __DSB();
    uint32_t guard_cycles = 256u;
    while (guard_cycles-- > 0u) {
        __NOP();
    }
    __DSB();
}

static bool spi_flash_wait_idle(SPI_HandleTypeDef* hspi, uint32_t timeout_ms) {
    uint32_t t0 = HAL_GetTick();
    while ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) == RESET) ||
           ((hspi->Instance->SR & SPI_FLAG_FTLVL) != SPI_FTLVL_EMPTY) ||
           (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY) != RESET)) {
        if ((HAL_GetTick() - t0) > timeout_ms) {
            return false;
        }
    }
    __DSB();
    return true;
}



SpiFlash::SpiFlash(SPI_HandleTypeDef* spiHandle, GPIO_TypeDef* csPort, uint16_t csPin)
    : spiHandle(spiHandle), csPort(csPort), csPin(csPin) {}

SpiFlash::~SpiFlash() {
    deinit();
}

void SpiFlash::deinit() {
    // Reset DMA callback state - do not stop DMA (main app handles that)
    txCallback = nullptr;
    rxCallback = nullptr;
    txDone = false;
    rxDone = false;
    txDone_IT = false;
    rxDone_IT = false;
    txUsingDma = false;
    rxUsingDma = false;
    LOG_SYSSTATUS("[SpiFlash] SpiFlash cleanup complete");
}

void SpiFlash::select() {
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_RESET);
}

void SpiFlash::deselect() {
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);
    spi_flash_cs_high_guard();
}

void SpiFlash::sendCommand(uint8_t cmd) {
    HAL_SPI_Transmit(spiHandle, &cmd, 1, HAL_MAX_DELAY);
}

void SpiFlash::init() {
    spi1_bus_lock_init();
    deselect();
}

void SpiFlash::writeEnable() {
    Spi1BusGuard bus;
    select();
    sendCommand(0x06); // Write Enable
    deselect();
}

void SpiFlash::writeData(uint32_t address, const uint8_t* data, size_t length) {
    Spi1BusGuard bus;
    const size_t pageSize = 256;

    while (length > 0) {
        size_t pageOffset = address % pageSize;
        size_t chunk = pageSize - pageOffset;
        if (chunk > length) {
            chunk = length;
        }

        writeEnable();
        select();

        uint8_t cmd[4] = {
            0x02,
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address)
        };

        HAL_SPI_Transmit(spiHandle, cmd, 4, HAL_MAX_DELAY);
        HAL_SPI_Transmit(spiHandle, const_cast<uint8_t*>(data), chunk, HAL_MAX_DELAY);

        deselect();
        pollBusy();

        address += (uint32_t)chunk;
        data += chunk;
        length -= chunk;
    }
}

/* SPI1 error capture — set from HAL_SPI_ErrorCallback (ISR context, flags
   only, no logging there). Without a user error callback, HAL's weak no-op
   swallowed DMA/SPI errors (e.g. OVR|DMA = 0x14): the transfer died, the
   done-flag never set, and waits "timed out" with no trace. */
volatile bool     spi1_error_flag = false;
volatile uint32_t spi1_error_code = 0;
volatile uint32_t spi1_dma_error_code = 0;   /* hdmatx ErrorCode: TE=0x1 FIFO=0x2 DME=0x4 */

/* Fully recover a wedged SPI handle. An abandoned/errored transfer (e.g. an
   IT receive that timed out) leaves State=BUSY_RX with interrupts armed —
   every later transmit then feeds the zombie RX, overruns, and errors. Abort
   kills the transfer; OVR flag must be cleared separately; if the state
   still isn't READY, re-init the peripheral outright. */
static void spi1_recover(SPI_HandleTypeDef* hspi) {
    HAL_SPI_Abort(hspi);
    /* HAL_SPI_Abort only aborts the DMA stream matching the SPI state — a
       TX stream orphaned while the SPI thought it was in RX stays BUSY
       forever and every later HAL_DMA_Start_IT fails (hal=HAL_ERROR,
       ErrorCode|=DMA). Abort the streams explicitly. */
    if (hspi->hdmatx != NULL) {
        (void)HAL_DMA_Abort(hspi->hdmatx);
    }
    if (hspi->hdmarx != NULL) {
        (void)HAL_DMA_Abort(hspi->hdmarx);
    }
    __HAL_SPI_CLEAR_OVRFLAG(hspi);
    __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_RXNE | SPI_IT_TXE | SPI_IT_ERR));
    hspi->ErrorCode = HAL_SPI_ERROR_NONE;

    bool dma_wedged =
        (hspi->hdmatx != NULL && hspi->hdmatx->State != HAL_DMA_STATE_READY) ||
        (hspi->hdmarx != NULL && hspi->hdmarx->State != HAL_DMA_STATE_READY);
    if (hspi->State != HAL_SPI_STATE_READY || dma_wedged) {
        HAL_SPI_DeInit(hspi);
        HAL_SPI_Init(hspi);
    }
    spi1_error_flag = false;
}

void SpiFlash::writeData_IT(uint32_t address, const uint8_t* data, size_t length) {
    Spi1BusGuard bus;
    const size_t pageSize = 256;

    while (length > 0) {
        size_t pageOffset = address % pageSize;
        size_t chunk = pageSize - pageOffset;
        if (chunk > length) {
            chunk = length;
        }

        writeEnable();
        select();

        uint8_t cmd[4] = {
            0x02,
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address)
        };

        HAL_SPI_Transmit(spiHandle, cmd, 4, HAL_MAX_DELAY);

        txDone_IT = false;
        txUsingDma = false;
        spi1_error_flag = false;
        HAL_SPI_Transmit_IT(spiHandle, const_cast<uint8_t*>(data), chunk);

        uint32_t timeout = 1000;
        while (!txDone_IT && !spi1_error_flag && timeout > 0) {
            HAL_Delay(1);
            timeout--;
        }

        if (!txDone_IT) {
            /* Recover the handle (never abandon a live transfer) and retry
               the page in blocking mode — NOR reprogram of identical data
               is idempotent, so a partial page is safe to rewrite. */
            LOG_ERROR("[SpiFlash] IT write fail @0x%08lX state=%d err=0x%08lX%s",
                      (unsigned long)address, (int)spiHandle->State,
                      (unsigned long)spi1_error_code,
                      spi1_error_flag ? " (error-cb)" : " (timeout)");
            spi1_recover(spiHandle);
            deselect();
            pollBusy();

            writeEnable();
            select();
            HAL_SPI_Transmit(spiHandle, cmd, 4, 1000);
            HAL_SPI_Transmit(spiHandle, const_cast<uint8_t*>(data), chunk, 2000);
        }

        deselect();
        pollBusy();

        address += (uint32_t)chunk;
        data += chunk;
        length -= chunk;
    }
}

void SpiFlash::readData(uint32_t address, uint8_t* buffer, size_t length) {
    Spi1BusGuard bus;
    select();

    uint8_t cmd[4] = {
        0x03, // Read Data
        static_cast<uint8_t>(address >> 16),
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address)
    };

    HAL_SPI_Transmit(spiHandle, cmd, 4, HAL_MAX_DELAY);
    HAL_SPI_Receive(spiHandle, buffer, length, HAL_MAX_DELAY);

    deselect();
}

void SpiFlash::readData_IT(uint32_t address, uint8_t* buffer, size_t length) {
    Spi1BusGuard bus;
    select();

    uint8_t cmd[4] = {
        0x03, // Read Data
        static_cast<uint8_t>(address >> 16),
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address)
    };

    HAL_SPI_Transmit(spiHandle, cmd, 4, HAL_MAX_DELAY);

    rxDone_IT = false;
    rxUsingDma = false;
    spi1_error_flag = false;
    HAL_SPI_Receive_IT(spiHandle, buffer, length);

    uint32_t timeout = 1000;  // 1 second timeout
    while (!rxDone_IT && !spi1_error_flag && timeout > 0) {
        HAL_Delay(1);
        timeout--;
    }

    if (!rxDone_IT) {
        /* THE original wedge: this path used to abandon the live
           HAL_SPI_Receive_IT (State stuck BUSY_RX, RX interrupt armed) —
           every later transmit then fed the zombie RX, overran, and the
           whole SPI1 bus degraded into per-page failures. Recover the
           handle and redo the read in blocking mode. */
        LOG_ERROR("[SpiFlash] IT read fail @0x%08lX state=%d err=0x%08lX%s - recovering",
                  (unsigned long)address, (int)spiHandle->State,
                  (unsigned long)spi1_error_code,
                  spi1_error_flag ? " (error-cb)" : " (timeout)");
        spi1_recover(spiHandle);
        deselect();

        select();
        HAL_SPI_Transmit(spiHandle, cmd, 4, 1000);
        HAL_SPI_Receive(spiHandle, buffer, length, 2000);
    }

    deselect();
}



void SpiFlash::eraseSector(uint32_t address) {
    Spi1BusGuard bus;
    writeEnable();
    select();

    uint8_t cmd[4] = {
        0x20, // Sector Erase
        static_cast<uint8_t>(address >> 16),
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address)
    };

    HAL_SPI_Transmit(spiHandle, cmd, 4, HAL_MAX_DELAY);

    deselect();
    pollBusy(); // <-- Wait until erase completes
}

void SpiFlash::pollBusy() {
    uint8_t tx[2] = {0x05, 0xFF}; // Read Status Register + dummy clock
    uint8_t rx[2] = {0x00, 0x01};
    uint8_t status = 0x01; // Assume busy initially

    // Bounded: W25Q128 chip erase worst case is ~200s. A wedged chip must
    // not hang the caller forever.
    uint32_t t0 = HAL_GetTick();
    do {
        select();
        HAL_SPI_TransmitReceive(spiHandle, tx, rx, sizeof(tx), HAL_MAX_DELAY);
        deselect();
        status = rx[1];
        if ((HAL_GetTick() - t0) > 240000u) {
            LOG_ERROR("[SpiFlash] pollBusy timeout - flash stuck busy");
            break;
        }
    } while (status & 0x01);
}

uint32_t SpiFlash::readDeviceID() {
    Spi1BusGuard bus;
    uint8_t id[3] = {};

    select();
    sendCommand(0x9F); // JEDEC ID
    HAL_SPI_Receive(spiHandle, id, 3, HAL_MAX_DELAY);
    deselect();

    return (id[0] << 16) | (id[1] << 8) | id[2];
}

FlashDeviceInfo SpiFlash::getDeviceInfo() {
    return {
        "W25Q128JVEIQ",
        128,
        256,
        4096,
        false
    };
}

void SpiFlash::eraseChip() {
    Spi1BusGuard bus;
    writeEnable();
    select();
    uint8_t cmd = 0xC7;  // Or 0x60
    HAL_SPI_Transmit(spiHandle, &cmd, 1, HAL_MAX_DELAY);
    deselect();
    pollBusy();  // Wait for completion (can take ~10 seconds)
}


void SpiFlash::reset() {
    // Reserved for future use
}


void SpiFlash::readDataDMA(uint32_t address, uint8_t* buffer, size_t length, SpiFlashDMACallback cb) {
    Spi1BusGuard bus;
    dmaRxBuf = buffer;
    rxCallback = cb;
    rxUsingDma = true;

    dmaCmdBuf[0] = 0x03;
    dmaCmdBuf[1] = address >> 16;
    dmaCmdBuf[2] = address >> 8;
    dmaCmdBuf[3] = address;

    select();
    HAL_SPI_Transmit(spiHandle, dmaCmdBuf, 4, HAL_MAX_DELAY);  // blocking header
    HAL_SPI_Receive_DMA(spiHandle, dmaRxBuf, length);
}

// void SpiFlash::writeDataDMA(uint32_t address, const uint8_t* data, size_t length, SpiFlashDMACallback cb) {
//     writeEnable();
//     txCallback = cb;

//     // Allocate full buffer on stack (or reuse static)
//     static uint8_t fullBuf[260];  // 4 + up to 256 bytes (you can adapt size)

//     fullBuf[0] = 0x02;
//     fullBuf[1] = address >> 16;
//     fullBuf[2] = address >> 8;
//     fullBuf[3] = address;

//     memcpy(&fullBuf[4], data, length);

//     select();
//     HAL_SPI_Transmit_DMA(spiHandle, fullBuf, length + 4);
// }

void SpiFlash::writeDataDMA(uint32_t address, const uint8_t* data, size_t length, SpiFlashDMACallback cb) {
    Spi1BusGuard bus;
    const uint32_t PAGE_SIZE = 256;
    txCallback = nullptr;  // prevent premature trigger
    uint8_t* pageBuf = spi_flash_dma_page_buf;

    while (length > 0) {
        uint32_t page_offset = address % PAGE_SIZE;
        uint32_t bytes_left_in_page = PAGE_SIZE - page_offset;
        uint32_t chunk_size = (length < bytes_left_in_page) ? length : bytes_left_in_page;

        uint8_t cmd[4] = {
            0x02, // Page Program
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address)
        };
        memcpy(pageBuf, data, chunk_size);

        writeEnable();
        select();
	
        txDone = false;
        spi1_error_flag = false;
        spi1_dma_error_code = 0;
        txUsingDma = true;
	
        HAL_StatusTypeDef st = HAL_SPI_Transmit(spiHandle, cmd, sizeof(cmd), HAL_MAX_DELAY);
        if (st == HAL_OK) {
            st = HAL_SPI_Transmit_DMA(spiHandle, pageBuf, chunk_size);
        }
        if (st == HAL_OK) {
            __HAL_SPI_DISABLE_IT(spiHandle, SPI_IT_ERR);
            if (spiHandle->hdmatx != NULL) {
                __HAL_DMA_DISABLE_IT(spiHandle->hdmatx, DMA_IT_FE);
            }

            uint32_t t0 = HAL_GetTick();
            while (!txDone) {
                if ((HAL_GetTick() - t0) > 500u) break;
                HAL_Delay(1);
            }
        }

        bool idle_ok = txDone && spi_flash_wait_idle(spiHandle, 10u);
        if (!idle_ok) {
            txDone = false;
        }

        if (!txDone) {
            LOG_ERROR("[SpiFlash] DMA page fail @0x%08lX hal=%d state=%d err=0x%08lX dma=0x%08lX %s",
                      (unsigned long)address, (int)st, (int)spiHandle->State,
                      (unsigned long)spi1_error_code,
                      (unsigned long)spi1_dma_error_code,
                      spi1_error_flag ? "(error-cb)" : "(timeout)");
            txUsingDma = false;
            spi1_recover(spiHandle);
            deselect();
            return;
        }

        txUsingDma = false;
        deselect();
        __HAL_SPI_CLEAR_OVRFLAG(spiHandle);
        pollBusy();

        address += chunk_size;
        data += chunk_size;
        length -= chunk_size;
    }

    // Final notification
    if (cb) cb();
}


void SpiFlash::onTxComplete() {
    /* ISR context: flag only. deselect() + pollBusy() moved to the waiting
       task (writeDataDMA) — blocking SPI transactions inside the DMA
       completion ISR raced the end-of-transfer OVR window and wedged the
       DMA stream (see writeDataDMA). */
    txDone = true;
    if (txCallback) txCallback();
}

void SpiFlash::onTxComplete_IT() {
    txUsingDma = false;
    txDone_IT = true;
}

void SpiFlash::onRxComplete() {
    rxUsingDma = false;
    deselect();
    rxDone = true;
    if (rxCallback) rxCallback();
}

void SpiFlash::onRxComplete_IT() {
    rxUsingDma = false;
    rxDone_IT = true;
}

// void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
//     if (hspi == spiFlash.spiHandle) {
//         spiFlash.onTxComplete();
//     } else if (hspi == &hspi4) {
//       spi_port_transaction_complete_cb();
//     }
// }

// void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
//     if (hspi == spiFlash.spiHandle) {
//         spiFlash.onRxComplete();
//     } else if (hspi == &hspi4) {
//           spi_port_transaction_complete_cb();
   
//     }
// }

// void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
//     if (hspi == &hspi4) {
//         spi_port_transaction_complete_cb();
//     }
// }

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == spiFlash.spiHandle) {
        if (spiFlash.txUsingDma) {
            spiFlash.onTxComplete();
        } else {
            spiFlash.onTxComplete_IT();
        }
    } else if (hspi == &hspi4) {
        if (spi_port_transaction_complete_cb) {
            spi_port_transaction_complete_cb();
        }
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == spiFlash.spiHandle) {
        if (spiFlash.rxUsingDma) {
            spiFlash.onRxComplete();
        } else {
            spiFlash.onRxComplete_IT();
        }
    } else if (hspi == &hspi4) {
        if (spi_port_transaction_complete_cb) {
            spi_port_transaction_complete_cb();
        }
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi4) {
        if (spi_port_transaction_complete_cb) {
            spi_port_transaction_complete_cb();
        }
    }
}

/* ISR context: record only — the waiting task logs and recovers. Without
   this handler the HAL's weak no-op silently swallowed SPI/DMA errors. */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == spiFlash.spiHandle) {
        spi1_error_code = hspi->ErrorCode;
        spi1_dma_error_code = (hspi->hdmatx != NULL) ? hspi->hdmatx->ErrorCode : 0;
        spi1_error_flag = true;
    }
    /* hspi4 (WiFi NCP): its transport layer has its own bounded waits. */
}
