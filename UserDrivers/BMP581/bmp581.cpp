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
 * @file    bmp581.cpp
 * @brief   BMP581 Pressure Sensor Driver Wrapper for STM32
 * @details C++ wrapper implementation for Bosch BMP581 barometric pressure
 *          sensor. Provides simplified interface over the Bosch BSD-licensed
 *          driver library, with STM32 HAL integration and calibration support.
 *          Used for altitude measurement and environmental monitoring on drone.
 */

#include "bmp581.hpp"
#include "bmp5.h"
#include "bmp5_defs.h"
#include "log.hpp"
#include <cstring>

BMP581::BMP581(bmp5_read_fptr_t read_func,
            bmp5_write_fptr_t write_func,
            bmp5_delay_us_fptr_t delay_func,
            void* intf_ptr,
            enum bmp5_intf interface_type)
{
    dev_.read = read_func;
    dev_.write = write_func;
    dev_.delay_us = delay_func;
    dev_.intf_ptr = intf_ptr;
    dev_.intf = interface_type;
}

BMP581::~BMP581() {
    deinit();
}

void BMP581::deinit() {
    // Reset internal state - do not call any HAL functions
    memset(&dev_, 0, sizeof(dev_));
    LOG_SYSSTATUS("[BMP581] BMP581 cleanup complete");
}

bool BMP581::initialize() {
    // Initialize the BMP581 device
    if (bmp5_init(&dev_) != BMP5_OK) {
        return false;
    }

    // Perform soft reset to ensure clean state
    if (bmp5_soft_reset(&dev_) != BMP5_OK) {
        return false;
    }
    dev_.delay_us(20000, dev_.intf_ptr);

    // Configure sensor with default oversampling and output data rate
    bmp5_osr_odr_press_config sensor_config = {};
    sensor_config.osr_t = BMP5_OVERSAMPLING_4X;   // Temperature oversampling
    sensor_config.osr_p = BMP5_OVERSAMPLING_4X;   // Pressure oversampling
    sensor_config.odr = BMP5_ODR_25_HZ;           // Output data rate 25 Hz
    sensor_config.press_en = BMP5_ENABLE;         // Enable pressure measurement

    if (bmp5_set_osr_odr_press_config(&sensor_config, &dev_) != BMP5_OK) {
        return false;
    }
    dev_.delay_us(5000, dev_.intf_ptr);  // Allow config to take effect

    // Set normal power mode for continuous measurements
    if (bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &dev_) != BMP5_OK) {
        return false;
    }
    dev_.delay_us(1000, dev_.intf_ptr);

    return true;
}

bool BMP581::softReset() {
    return bmp5_soft_reset(&dev_) == BMP5_OK;
}

bool BMP581::getSensorData(float& temperature, float& pressure) {
    bmp5_sensor_data data{};
    bmp5_osr_odr_press_config cfg{};
    bmp5_get_osr_odr_press_config(&cfg, &dev_);

    if (bmp5_get_sensor_data(&data, &cfg, &dev_) == BMP5_OK) {
        temperature = data.temperature;
        pressure = data.pressure;
        return true;
    }
    return false;
}

bool BMP581::configureSensor(const bmp5_osr_odr_press_config& cfg) {
    return bmp5_set_osr_odr_press_config(&cfg, &dev_) == BMP5_OK;
}

bool BMP581::configureIIR(const bmp5_iir_config& cfg) {
    return bmp5_set_iir_config(&cfg, &dev_) == BMP5_OK;
}

bool BMP581::setPowerMode(enum bmp5_powermode mode) {
    return bmp5_set_power_mode(mode, &dev_) == BMP5_OK;
}

bool BMP581::getPowerMode(enum bmp5_powermode& mode) {
    return bmp5_get_power_mode(&mode, &dev_) == BMP5_OK;
}

bool BMP581::getInterruptStatus(uint8_t& status) {
    return bmp5_get_interrupt_status(&status, &dev_) == BMP5_OK;
}

// bool BMP581::enableInterrupts(const bmp5_int_source_select& cfg) {
//     return ::bmp5_int_source_select(&cfg, &dev_) == BMP5_OK;
// }



bool BMP581::setupInterruptPin(enum bmp5_intr_mode mode,
                                enum bmp5_intr_polarity pol,
                                enum bmp5_intr_drive drive,
                                enum bmp5_intr_en_dis enable)
{
    return bmp5_configure_interrupt(mode, pol, drive, enable, &dev_) == BMP5_OK;
}

bmp5_dev* BMP581::dev() {
    return &dev_;
}
