
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
 * @file    test_peripherals.hpp
 * @brief   Comprehensive Peripheral Test Orchestrator for FalkraController
 * @details Central test coordination system for validating all hardware peripherals
 *          and sensors on the STM32F767-based drone controller. Provides configurable
 *          test execution, systematic validation, and comprehensive hardware verification
 *          for production and development testing scenarios.
 * 
 * Test Orchestration Features:
 * - Configurable test execution for selective peripheral validation
 * - Systematic test sequencing with dependency management
 * - Comprehensive hardware coverage across all system components
 * - Production and development test mode support
 * - Real-time test status reporting and logging
 * - FreeRTOS task-based execution for non-blocking operation
 * 
 * Peripheral Test Coverage:
 * - Communication: UART, I2C, SPI, QSPI interfaces
 * - Storage: NVRAM, SPI Flash, QSPI Flash, File System
 * - Sensors: Environmental (SHT4x, BMP581), IMU (BNO085), ToF (VL53L5CX)
 * - Power: Battery monitor (BQ27441), power multiplexer
 * - Audio: MAX98357 amplifier and tone generation
 * - Display: ST7789 TFT LCD and TouchGFX integration
 * - Radio: RC receiver hardware and protocol validation
 * - Timing: Real-time clock accuracy and persistence
 * 
 * Test Configuration System:
 * - Selective test execution through TestConfig structure
 * - Runtime test configuration modification
 * - Development vs production test profiles
 * - Continuous monitoring mode for extended validation
 * - Individual peripheral enable/disable control
 * 
 * Integration Benefits:
 * - Automated hardware validation during development
 * - Production line testing for quality assurance
 * - Field diagnostic capabilities for maintenance
 * - Systematic fault isolation and identification
 * - Comprehensive system health verification
 * 
 * Usage Example:
 * @code
 * // Configure comprehensive test suite
 * TestConfig full_test = {
 *     .test_uart = true,
 *     .test_nvram = true,
 *     .test_spi_flash = true,
 *     .test_qspi_flash = true,
 *     .test_SHT4x = true,
 *     .test_BMP581 = true,
 *     .test_i2c2 = true,
 *     .test_BNO085 = true,
 *     .test_BQ27441 = true,
 *     .test_power_mux = true,
 *     .test_radio_receiverHW = true,
 *     .test_continous_data = false,
 *     .test_st7789 = true,
 *     .test_vl53l5cx = true,
 *     .test_max98357 = true,
 *     .test_fileSystem = true,
 *     .test_rtc = true
 * };
 * 
 * // Apply test configuration and start testing
 * set_test_config(&full_test);
 * 
 * // Test task runs automatically in FreeRTOS
 * // Results logged through system logging interface
 * 
 * // Production line testing - essential systems only
 * TestConfig production_test = {
 *     .test_uart = false,
 *     .test_nvram = true,
 *     .test_spi_flash = true,
 *     .test_qspi_flash = true,
 *     .test_SHT4x = true,
 *     .test_BMP581 = true,
 *     .test_i2c2 = true,
 *     .test_BNO085 = true,
 *     .test_BQ27441 = true,
 *     .test_power_mux = true,
 *     .test_radio_receiverHW = true,
 *     .test_continous_data = false,
 *     .test_st7789 = false,
 *     .test_vl53l5cx = true,
 *     .test_max98357 = false,
 *     .test_fileSystem = false,
 *     .test_rtc = true
 * };
 * set_test_config(&production_test);
 * @endcode
 */

#ifndef __TEST_PERIPHERALS_HPP
#define __TEST_PERIPHERALS_HPP

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {

    bool test_uart;
    bool test_nvram;
    bool test_spi_flash;
    bool test_qspi_flash;
    bool test_SHT4x; 
    bool test_BMP581;
    bool test_i2c2;
    bool test_spi4;
    bool test_BNO085;
    bool test_BQ27441;
    bool test_power_mux;
    bool test_radio_receiverHW;
    bool test_continous_data;
    bool test_st7789;
    bool test_vl53l5cx;
    bool test_max98357;
    bool test_fileSystem;
    bool test_rtc;
    bool test_ppm;
    bool test_wifi;
} TestConfig;


void test_peripheralsTask(void *argument);
void set_test_config(const TestConfig* cfg);

#ifdef __cplusplus
}
#endif



#endif /* __TEST_PERIPHERALS_HPP */
