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
 * @file    SHT4x.cpp
 * @brief   SHT4x Temperature and Humidity Sensor Driver Implementation
 * @details Implementation for Sensirion SHT4x I2C environmental sensor.
 *          Provides high-precision temperature and humidity measurements
 *          for environmental monitoring and flight system protection.
 */

#include "SHT4x.hpp"
#include "log.hpp"
#include "i2c2_bus_lock.hpp"  // SHT4x shares I2C2 with BQ27441/BMP581 on this board

#define I2C_TIMEOUT_MS 1000

SHT4x::SHT4x(I2C_HandleTypeDef* i2cHandle)
    : i2cHandle(i2cHandle),
      _temperature(0.0f),
      _humidity(0.0f),
      _precision(SHT4X_HIGH_PRECISION),
      _heater(SHT4X_NO_HEATER)
{}

SHT4x::~SHT4x() {
    deinit();
}

void SHT4x::deinit() {
    _temperature = 0.0f;
    _humidity = 0.0f;
    _precision = SHT4X_HIGH_PRECISION;
    _heater = SHT4X_NO_HEATER;
    LOG_SYSSTATUS("[SHT4x] SHT4x cleanup complete");
}

bool SHT4x::init()
{
    if (!reset()) {
        LOG_ERROR("[SHT4x] Reset failed during init");
        return false;
    }

    HAL_Delay(10); // <-- Add small 10ms delay after reset

    setPrecision(SHT4X_HIGH_PRECISION);
    setHeater(SHT4X_NO_HEATER);

    return true;
}

bool SHT4x::reset()
{
    return writeCommand(SHT4X_CMD_SOFTRESET);
}

bool SHT4x::readTempAndHumidity(float& temp, float& humidity)
{
    uint8_t buffer[6] = {0};
    I2c2BusGuard guard;  // hold across trigger + measure delay + read (recursive)

    if (!triggerMeasurement(SHT4X_CMD_MEASURE_HIGH_PRECISION)) {
        LOG_ERROR("[SHT4x] Failed to start measurement");
        return false;
    }

    HAL_Delay(10); // <-- small delay before reading data!

    if (!readData(buffer, sizeof(buffer))) {
        LOG_ERROR("[SHT4x] Failed to read measurement data");
        return false;
    }

    uint16_t temp_raw = (buffer[0] << 8) | buffer[1];
    uint16_t humidity_raw = (buffer[3] << 8) | buffer[4];

    temp = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    humidity = 100.0f * (float)humidity_raw / 65535.0f;

    _temperature = temp;
    _humidity = humidity;

    return true;
}

void SHT4x::setPrecision(sht4x_precision_t prec)
{
    _precision = prec;
}

sht4x_precision_t SHT4x::getPrecision() const
{
    return _precision;
}

void SHT4x::setHeater(sht4x_heater_t heat)
{
    _heater = heat;
}

sht4x_heater_t SHT4x::getHeater() const
{
    return _heater;
}

bool SHT4x::writeCommand(uint16_t cmd)
{
    I2c2BusGuard guard;
    if (HAL_I2C_Master_Transmit(i2cHandle, SHT4X_DEFAULT_ADDR, reinterpret_cast<uint8_t*>(&cmd), 1, I2C_TIMEOUT_MS) != HAL_OK) {
        LOG_ERROR("[SHT4x] Failed to write command 0x%02X", cmd);
        return false;
    }
    return true;
}

bool SHT4x::readData(uint8_t* buffer, uint8_t num_bytes)
{
    I2c2BusGuard guard;
    if (HAL_I2C_Master_Receive(i2cHandle, SHT4X_DEFAULT_ADDR, buffer, num_bytes, I2C_TIMEOUT_MS) != HAL_OK) {
        LOG_ERROR("[SHT4x] Failed to read data");
        return false;
    }
    return true;
}

bool SHT4x::triggerMeasurement(uint8_t cmd)
{
    return writeCommand(cmd);
}
