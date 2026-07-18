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
 * @file    SHT4x.hpp
 * @brief   SHT4x Temperature and Humidity Sensor Driver Interface
 * @details Driver interface for Sensirion SHT4x series I2C environmental sensors.
 *          Provides high-precision temperature and humidity monitoring for:
 *          - Flight computer thermal protection and performance optimization
 *          - Payload environmental monitoring (camera condensation prevention)
 *          - Data logging for flight conditions analysis
 *          - Altitude-compensated environmental readings
 * 
 * Key Features:
 * - High accuracy: ±0.2°C temperature, ±1.8% RH humidity
 * - Wide operating range: -40°C to +125°C, 0-100% RH
 * - Fast measurement: 1-10ms depending on precision mode
 * - Built-in heater for condensation removal
 * - Ultra-low power consumption
 * - I2C interface with CRC checksum validation
 * 
 * Measurement Modes:
 * - High Precision: ±0.2°C, ±1.8% RH (10ms measurement)
 * - Medium Precision: ±0.3°C, ±2.0% RH (5ms measurement)  
 * - Low Precision: ±0.4°C, ±2.5% RH (2ms measurement)
 * 
 * Heater Functionality:
 * - Removes condensation from sensor surface
 * - Essential for accurate readings in high humidity
 * - Multiple power levels and duration options
 * - Automatic temperature compensation
 * 
 * Usage Example:
 * @code
 * SHT4x sensor(&hi2c1);
 * sensor.init();
 * sensor.setPrecision(SHT4X_HIGH_PRECISION);
 * 
 * float temperature, humidity;
 * if (sensor.readTempAndHumidity(temperature, humidity)) {
 *     // Check for thermal limits
 *     if (temperature > 70.0f) {
 *         LOG_WARN("High temperature: %.1f°C", temperature);
 *         activate_cooling_system();
 *     }
 *     
 *     // Check for condensation risk
 *     if (humidity > 95.0f) {
 *         sensor.setHeater(SHT4X_HIGH_HEATER_1S);  // Remove condensation
 *     }
 * }
 * @endcode
 */

#ifndef SHT4X_HPP
#define SHT4X_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

#define SHT4X_DEFAULT_ADDR (0x44 << 1)
#define SHT4X_CMD_SOFTRESET 0x94
#define SHT4X_CMD_MEASURE_HIGH_PRECISION 0xFD

typedef enum {
    SHT4X_HIGH_PRECISION,
    SHT4X_MED_PRECISION,
    SHT4X_LOW_PRECISION
} sht4x_precision_t;

typedef enum {
    SHT4X_NO_HEATER,
    SHT4X_HIGH_HEATER_1S,
    SHT4X_HIGH_HEATER_100MS,
    SHT4X_MED_HEATER_1S,
    SHT4X_MED_HEATER_100MS,
    SHT4X_LOW_HEATER_1S,
    SHT4X_LOW_HEATER_100MS
} sht4x_heater_t;

class SHT4x {
public:
    SHT4x(I2C_HandleTypeDef* i2cHandle);
    ~SHT4x();

    bool init();
    void deinit();
    bool reset();
    bool readTempAndHumidity(float& temp, float& humidity);

    void setPrecision(sht4x_precision_t prec);
    sht4x_precision_t getPrecision() const;
    void setHeater(sht4x_heater_t heat);
    sht4x_heater_t getHeater() const;

private:
    I2C_HandleTypeDef* i2cHandle;
    float _temperature;
    float _humidity;
    sht4x_precision_t _precision;
    sht4x_heater_t _heater;

    bool writeCommand(uint16_t cmd);
    bool readData(uint8_t* buffer, uint8_t num_bytes);
    bool triggerMeasurement(uint8_t cmd);
};


#ifdef __cplusplus
}
#endif

#endif // SHT4X_HPP
