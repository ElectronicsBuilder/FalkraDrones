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
 * @file    st7789.cpp
 * @brief   ST7789 TFT LCD Display Driver
 * @details High-performance driver for ST7789V 1.3" TFT LCD display with
 *          SPI interface. Provides graphics primitives, text rendering,
 *          and TouchGFX integration for drone status display and UI.
 */

#include "st7789.hpp"
#include "st7789_defs.h"
#include "st7789_opts.h"
#include "main.h"
#include "log.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include <cstring>

static constexpr uint32_t ST7789_SPI_TIMEOUT_MS = 25u;
static constexpr uint32_t ST7789_SPI_BUFFER_TIMEOUT_MS = 100u;

#if ST7789_OPT_BATCH_TX
// Shared byte-swap staging buffer. Single-writer: only the GUI task flushes
// pixel data, matching the driver's existing (non-thread-safe) contract.
static uint8_t st7789_tx_chunk[ST7789_BATCH_CHUNK_PIXELS * 2];
#endif

static void st7789_delay_ms(uint32_t ms) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        HAL_Delay(ms);
    }
}

ST7789::ST7789(const Config& cfg) : cfg_(cfg), width_(0), height_(0) {}

ST7789::~ST7789() {
    deinit();
}

void ST7789::deinit() {
    // Turn off backlight before cleanup
    setBacklight(false);
    // Reset internal state variables
    width_ = 0;
    height_ = 0;
    LOG_SYSSTATUS("[ST7789] ST7789 cleanup complete");
}

void ST7789::init() {
    LOG_INFO("[ST7789] Init begin");
    if (HAL_SPI_GetState(cfg_.hspi) != HAL_SPI_STATE_READY) {
        LOG_WARN("[ST7789] SPI not ready before init (state=%d err=0x%08lX) - aborting stale transfer",
                 (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
        HAL_SPI_Abort(cfg_.hspi);
    }

    setBacklight(false);
    st7789_delay_ms(100);
    LOG_INFO("[ST7789] Backlight guard delay complete");
    setBacklight(true);
    reset();
    LOG_INFO("[ST7789] Reset complete");
    st7789_delay_ms(100);
    setDir(cfg_.scan_dir);
    writeInitSequence();
    LOG_INFO("[ST7789] Init sequence complete");
}

void ST7789::reset() {
    HAL_GPIO_WritePin(cfg_.reset_port, cfg_.reset_pin, GPIO_PIN_SET);
    st7789_delay_ms(100);
    HAL_GPIO_WritePin(cfg_.reset_port, cfg_.reset_pin, GPIO_PIN_RESET);
    st7789_delay_ms(100);
    HAL_GPIO_WritePin(cfg_.reset_port, cfg_.reset_pin, GPIO_PIN_SET);
    st7789_delay_ms(150);
}

void ST7789::setBacklight(bool on) {
    HAL_GPIO_WritePin(cfg_.bkl_port, cfg_.bkl_pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void ST7789::sendCommand(uint8_t c) {
    HAL_GPIO_WritePin(cfg_.dc_port, cfg_.dc_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, &c, 1, ST7789_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
    if (st != HAL_OK) {
        LOG_ERROR("[ST7789] command 0x%02X tx failed st=%d state=%d err=0x%08lX",
                  c, (int)st, (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
        HAL_SPI_Abort(cfg_.hspi);
    }
}

void ST7789::sendData(uint8_t c) {
    HAL_GPIO_WritePin(cfg_.dc_port, cfg_.dc_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, &c, 1, ST7789_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
    if (st != HAL_OK) {
        LOG_ERROR("[ST7789] data 0x%02X tx failed st=%d state=%d err=0x%08lX",
                  c, (int)st, (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
        HAL_SPI_Abort(cfg_.hspi);
    }
}

void ST7789::sendBuffer(const uint8_t *data, size_t size) {
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(cfg_.dc_port, cfg_.dc_pin, GPIO_PIN_SET);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, (uint8_t*)data, size, ST7789_SPI_BUFFER_TIMEOUT_MS);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
    if (st != HAL_OK) {
        LOG_ERROR("[ST7789] buffer tx failed size=%lu st=%d state=%d err=0x%08lX",
                  (unsigned long)size, (int)st, (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
        HAL_SPI_Abort(cfg_.hspi);
    }
}

#if ST7789_OPT_BATCH_TX
void ST7789::sendColorData(const uint16_t *data, uint32_t length) {
    HAL_GPIO_WritePin(cfg_.dc_port, cfg_.dc_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_RESET);

    while (length > 0) {
        uint32_t n = (length > ST7789_BATCH_CHUNK_PIXELS) ? ST7789_BATCH_CHUNK_PIXELS : length;
        for (uint32_t i = 0; i < n; i++) {
            st7789_tx_chunk[2 * i]     = (uint8_t)(data[i] >> 8);
            st7789_tx_chunk[2 * i + 1] = (uint8_t)(data[i] & 0xFF);
        }

        HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, st7789_tx_chunk, n * 2,
                                                ST7789_SPI_BUFFER_TIMEOUT_MS);
        if (st != HAL_OK) {
            LOG_ERROR("[ST7789] color tx failed rem=%lu st=%d state=%d err=0x%08lX",
                      (unsigned long)length, (int)st, (int)cfg_.hspi->State,
                      (unsigned long)cfg_.hspi->ErrorCode);
            HAL_SPI_Abort(cfg_.hspi);
            break;
        }

        data += n;
        length -= n;
    }

    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
}
#else
void ST7789::sendColorData(const uint16_t *data, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        uint8_t bytes[2] = { (uint8_t)(data[i] >> 8), (uint8_t)(data[i] & 0xFF) };
        sendBuffer(bytes, 2);
    }
}
#endif

void ST7789::setDir(st7789_scan_dir_t dir) {
    uint8_t memAccess = 0x00;
    if (dir == ST7789_SCAN_DIR_HORIZONTAL) {
        width_ = LCD_HEIGHT;
        height_ = LCD_WIDTH;
        memAccess = 0x70;  // Optional: adjust based on specific rotation config
    } else {
        width_ = LCD_WIDTH;
        height_ = LCD_HEIGHT;
        memAccess = 0x00;
    }
    sendCommand(ST7789_MADCTL);
    sendData(memAccess);
}

void ST7789::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint16_t X_OFFSET = 35;
    x0 += X_OFFSET;
    x1 += X_OFFSET;
    sendCommand(ST7789_CASET);
    sendData(x0 >> 8); sendData(x0 & 0xFF);
    sendData(x1 >> 8); sendData(x1 & 0xFF);

    sendCommand(ST7789_PASET);
    sendData(y0 >> 8); sendData(y0 & 0xFF);
    sendData(y1 >> 8); sendData(y1 & 0xFF);

    sendCommand(ST7789_RAMWR);
}

void ST7789::drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data) {
    DisplayBusyStatus = 0;
    if ((x >= width_) || (y >= height_)) return;
    if ((x + w - 1) >= width_) w = width_ - x;
    if ((y + h - 1) >= height_) h = height_ - y;
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    sendColorData(data, w * h);
    DisplayBusyStatus = 1;
}

void ST7789::fillScreen(uint16_t color) {
    fillRect(0, 0, width_, height_, color);
}

void ST7789::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if ((x >= width_) || (y >= height_)) return;
    if ((x + w - 1) >= width_) w = width_ - x;
    if ((y + h - 1) >= height_) h = height_ - y;
    setAddrWindow(x, y, x + w - 1, y + h - 1);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(cfg_.dc_port, cfg_.dc_pin, GPIO_PIN_SET);
#if ST7789_OPT_BATCH_TX
    uint32_t remaining = (uint32_t)w * (uint32_t)h;
    uint32_t prefill = (remaining > ST7789_BATCH_CHUNK_PIXELS) ? ST7789_BATCH_CHUNK_PIXELS : remaining;
    for (uint32_t i = 0; i < prefill; i++) {
        st7789_tx_chunk[2 * i]     = hi;
        st7789_tx_chunk[2 * i + 1] = lo;
    }
    while (remaining > 0) {
        uint32_t n = (remaining > ST7789_BATCH_CHUNK_PIXELS) ? ST7789_BATCH_CHUNK_PIXELS : remaining;
        HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, st7789_tx_chunk, n * 2,
                                                ST7789_SPI_BUFFER_TIMEOUT_MS);
        if (st != HAL_OK) {
            LOG_ERROR("[ST7789] fillRect tx failed st=%d state=%d err=0x%08lX",
                      (int)st, (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
            HAL_SPI_Abort(cfg_.hspi);
            break;
        }
        remaining -= n;
    }
#else
    for (int i = 0; i < w * h; i++) {
        HAL_StatusTypeDef st = HAL_SPI_Transmit(cfg_.hspi, &hi, 1, ST7789_SPI_TIMEOUT_MS);
        if (st == HAL_OK) {
            st = HAL_SPI_Transmit(cfg_.hspi, &lo, 1, ST7789_SPI_TIMEOUT_MS);
        }
        if (st != HAL_OK) {
            HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
            LOG_ERROR("[ST7789] fillRect tx failed st=%d state=%d err=0x%08lX",
                      (int)st, (int)cfg_.hspi->State, (unsigned long)cfg_.hspi->ErrorCode);
            HAL_SPI_Abort(cfg_.hspi);
            return;
        }
    }
#endif
    HAL_GPIO_WritePin(cfg_.cs_port, cfg_.cs_pin, GPIO_PIN_SET);
}

void ST7789::setRotation(uint8_t m) {
    sendCommand(ST7789_MADCTL);
    switch (m) {
        case 0: sendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB); break;
        case 1: sendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB); break;
        case 2: sendData(ST7789_MADCTL_RGB); break;
        case 3: sendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB); break;
    }
}

void ST7789::writeInitSequence() {
    sendCommand(ST7789_COLMOD); sendData(ST7789_COLOR_MODE_16bit);

    sendCommand(ST7789_PORCH_CTRL_CMD);
    {
        uint8_t buf[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        sendBuffer(buf, sizeof(buf));
    }

    sendCommand(ST7789_GATE_CTRL_CMD);   sendData(ST7789_GATE_CTRL_DATA);
    sendCommand(ST7789_VCOM_CMD);        sendData(ST7789_VCOM_DATA);
    sendCommand(ST7789_PWR_CTRL1_CMD);   sendData(ST7789_PWR1_DATA);
    sendCommand(ST7789_PWR_CTRL2_CMD);   sendData(ST7789_PWR2_DATA);
    sendCommand(ST7789_VRH_VDV_CTRL_CMD);sendData(ST7789_VRH_VDV_DATA);
    sendCommand(ST7789_VDV_SETTING_CMD); sendData(ST7789_VDV_SETTING_DATA);
    sendCommand(ST7789_FR_CTRL_CMD);     sendData(ST7789_FR_CTRL_DATA);
    sendCommand(ST7789_PWR_CTRL3_CMD);   sendData(ST7789_PWR3_DATA1); sendData(ST7789_PWR3_DATA2);

    sendCommand(ST7789_GAMMA_POS_CMD);
    {
        uint8_t pos[] = {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30};
        sendBuffer(pos, sizeof(pos));
    }

    sendCommand(ST7789_GAMMA_NEG_CMD);
    {
        uint8_t neg[] = {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32};
        sendBuffer(neg, sizeof(neg));
    }

    sendCommand(ST7789_INVON);
    setRotation(ST7789_ROTATION);
    sendCommand(ST7789_SLPOUT);
    st7789_delay_ms(120);
    sendCommand(ST7789_DISPON);
    st7789_delay_ms(50);
}
