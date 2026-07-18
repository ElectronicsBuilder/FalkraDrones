/**
 * @file    test_i2c.cpp
 * @brief   I2C Bus Test Implementation
 * @details Implementation of I2C bus scanning and device detection routines
 * 
 * Part of FalkraController - STM32F767-based drone controller firmware
 * 
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
 */

#include "test_i2c.hpp"
#include "stm32f7xx_hal.h"
#include "log.hpp"

extern I2C_HandleTypeDef hi2c2;

static const char* identify_i2c_device(uint8_t addr)
{
    switch (addr)
    {
        case 0x18: return "LIS3DH (Accel)";
        case 0x1C: return "LIS3MDL (Magnetometer)";
        case 0x20: return "MCP23008 (GPIO Expander)";
        case 0x27: return "LCD Backpack (PCF8574)";
        case 0x28: return "VL53L0X (ToF Distance)";
        case 0x29: return "VL53L1X (ToF Distance)";
        case 0x3C: return "SSD1306 (OLED Display)";
        case 0x3D: return "SSD1306 Alt Addr";
        case 0x40: return "INA219 (Current/Power Monitor)";
        case 0x41: return "INA219 Alt Addr";
        case 0x44: return "SHT4x (Temp/Humidity)";
        case 0x45: return "SHT4x (Alt Addr)";
        case 0x46: return "BMP581 (SDO=Low, I2C)";
        case 0x47: return "BMP581 (SDO=High, I2C)";
        case 0x48: return "ADS1115 (ADC)";
        case 0x49: return "ADS1115 Alt Addr";
        case 0x50: return "EEPROM (AT24Cxx / 64Kb)";
        case 0x51: return "PCF8523 (RTC)";
        case 0x53: return "ADXL345 (Accel)";
        case 0x55: return "BQ27441 Battery Monitor";
        case 0x57: return "AT24MAC (EEPROM with MAC)";
        case 0x58: return "TSL2561 (Light Sensor)";
        case 0x5A: return "MLX90614 (IR Temp Sensor)";
        case 0x60: return "MCP4725 (DAC)";
        case 0x68: return "MPU6050 / RTC / Accel/Gyro";
        case 0x69: return "MPU6050 Alt Addr";
        case 0x70: return "TCA9548A (I2C Multiplexer)";
        case 0x76: return "BMP280 / BME280 / BMP388";
        case 0x77: return "BMP280 Alt Addr";
        case 0x7E: return "BMP581 (I3C Mode Detected)";
        default:   return nullptr;
    }
}

void test_i2c2_scan()
{
    LOG_INFO("[I2C] Starting scan on hi2c2...");

    for (uint8_t addr = 1; addr < 128; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2, addr << 1, 2, 2) == HAL_OK)
        {
            const char* desc = identify_i2c_device(addr);
            if (desc) {
                LOG_INFO("[I2C] Found 0x%02X → %s", addr, desc);

                if (addr == 0x7E) {
                    LOG_WARN("[I2C] BMP581 responded in I3C mode");
                }

            } else {
                LOG_INFO("[I2C] Found device at 0x%02X", addr);
            }
        }
    }

    LOG_INFO("[I2C] Scan complete.");
}
