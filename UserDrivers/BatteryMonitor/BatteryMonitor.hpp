/**
 * @file    BatteryMonitor.hpp
 * @brief   ACS758-50A Hall Effect Current Sensor Driver (WCMCU-758)
 * @details Self-contained driver for monitoring drone current consumption using dual-output ACS758
 *          Hall sensor with ADC DMA capture, NVRAM calibration storage, and automatic callback routing.
 *
 * Hardware:
 *  - Module: WCMCU-758 (ACS758LCB-050B)
 *  - Configuration: Unidirectional current monitoring (0-50A)
 *  - Zero Point: ~1.65V (VCC/2) when no current flows
 *  - Sensitivity: 40mV/A (voltage increases with current consumption)
 *  - Connections:
 *    - VOUT1 (Raw) -> PA0 (ADC1_IN0) - BATMON_IO1
 *    - VOUT2 (Buffered) -> PA1 (ADC1_IN1) - BATMON_IO2
 *
 * Architecture:
 *  - Self-contained: Driver manages its own HAL callback registration
 *  - NVRAM-backed calibration: Calibration values stored in non-volatile memory
 *  - Continuous DMA: Always running in background, poll for latest data
 *  - Task-safe: Thread-safe data access for FreeRTOS environment
 *
 * Usage:
 *  1. Create instance: BatteryMonitor monitor(&hadc1, &userconfig);
 *  2. Initialize: monitor.init();
 *  3. Calibrate: Call monitor.calibrate() with NO current flowing (zero point at 1.65V)
 *  4. Start continuous sampling: monitor.start();
 *  5. Poll data: float current = monitor.getCurrent(); // Returns consumption in Amperes
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

#ifndef BATTERYMONITOR_HPP
#define BATTERYMONITOR_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include "user_config.h"
#include <stdint.h>
#include <math.h>
#include "app_defs.hpp"

#ifdef __cplusplus
}

/**
 * @brief ACS758 sensor specifications
 */
namespace ACS758 {
    constexpr float SENSITIVITY_V_PER_A = 0.040f;  // 40mV/A
    constexpr float MAX_CURRENT_A = 50.0f;         // 0-50A unidirectional range
    constexpr float ZERO_CURRENT_V = 1.65f;        // VCC/2 (nominal zero point)
    constexpr uint16_t ADC_RESOLUTION = 4096;      // 12-bit ADC
}

// Note: Calibration structure is defined in user_config.h as userconfig_batmon_t

/**
 * @brief Battery Monitor measurement data
 */
struct BatteryMonitorData {
    uint16_t adc_raw;              // Raw ADC value (VOUT1)
    uint16_t adc_buffered;         // Buffered ADC value (VOUT2)
    float voltage_raw;             // Voltage in V (VOUT1)
    float voltage_buffered;        // Voltage in V (VOUT2)
    float current_raw;             // Current in A (from raw channel)
    float current_buffered;        // Current in A (from buffered channel)
    float current_average;         // Average of both channels
    bool valid;                    // Data validity flag (true if calibrated)
};

/**
 * @brief Battery Monitor statistics for diagnostics
 */
struct BatteryMonitorStats {
    float current_min;             // Minimum current observed
    float current_max;             // Maximum current observed
    float current_avg;             // Running average current
    uint32_t sample_count;         // Total samples captured
    uint32_t dma_complete_count;   // DMA transfer complete callbacks
    uint32_t dma_error_count;      // DMA error count
};

/**
 * @class BatteryMonitor
 * @brief Self-contained driver for ACS758 Hall Effect Current Sensor
 *
 * Features:
 * - Automatic HAL callback registration (self-contained)
 * - NVRAM-backed calibration persistence
 * - Continuous DMA sampling in background
 * - Thread-safe data polling for FreeRTOS tasks
 * - Graceful handling of missing calibration
 */
class BatteryMonitor {
public:
    /**
     * @brief Constructor
     * @param hadc Pointer to ADC handle (must be configured with 2 channels in DMA mode)
     * @param userconfig Pointer to UserConfig instance for calibration storage
     */
    BatteryMonitor(ADC_HandleTypeDef* hadc, userconfig_t* userconfig);

    /**
     * @brief Initialize the battery monitor
     * @details Loads calibration from NVRAM. If missing/invalid, driver will not report data
     *          until calibrate() is called.
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Start continuous ADC DMA conversions
     * @return true if start successful
     */
    bool start();

    /**
     * @brief Stop ADC DMA conversions
     * @return true if stop successful
     */
    bool stop();

    /**
     * @brief Calibrate zero-current offset and save to NVRAM
     * @details CRITICAL: Ensure NO current flowing through sensor during calibration
     * @param samples Number of samples to average for calibration
     * @return true if calibration successful and saved to NVRAM
     */
    bool calibrate(uint16_t samples = 100);

    /**
     * @brief Check if valid calibration is loaded
     * @return true if calibration is valid and current measurements are available
     */
    bool isCalibrated() const;

    /**
     * @brief Load calibration from NVRAM
     * @return true if valid calibration loaded, false if missing/corrupted
     */
    bool loadCalibration();

    /**
     * @brief Save current calibration to NVRAM
     * @return true if successfully saved
     */
    bool saveCalibration();

    /**
     * @brief Erase calibration from NVRAM
     * @return true if successfully erased
     */
    bool eraseCalibration();

    /**
     * @brief Get latest battery monitor data
     * @details Returns invalid data if not calibrated
     * @return BatteryMonitorData structure with current measurements
     */
    BatteryMonitorData getData() const;

    /**
     * @brief Get current consumption in Amperes (from buffered channel)
     * @return Current consumption in A (0-50A), or 0.0 if not calibrated
     */
    float getCurrent() const;

    /**
     * @brief Get current consumption from raw channel
     * @return Current consumption in A from raw ADC channel, or 0.0 if not calibrated
     */
    float getCurrentRaw() const;

    /**
     * @brief Get current consumption from buffered channel
     * @return Current consumption in A from buffered ADC channel, or 0.0 if not calibrated
     */
    float getCurrentBuffered() const;

    /**
     * @brief Get statistics
     * @return BatteryMonitorStats structure
     */
    BatteryMonitorStats getStats() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    /**
     * @brief Check if ADC DMA is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Get calibration data (read-only)
     * @return Pointer to calibration structure from userconfig, or NULL if not calibrated
     */
    const userconfig_batmon_t* getCalibration() const;

    /**
     * @brief Set ADC reference voltage (VREF)
     * @param vref Reference voltage in V (typically 3.3V but may vary)
     * @return true if successfully saved to NVRAM, false otherwise
     * @note Updates existing calibration, does not require recalibration
     */
    bool setVref(float vref);

    /**
     * @brief Enable/disable raw channel in current calculation
     * @param enable true to include raw channel, false to exclude
     */
    void setRawChannelEnabled(bool enable);

    /**
     * @brief Enable/disable buffered channel in current calculation
     * @param enable true to include buffered channel, false to exclude
     */
    void setBufferedChannelEnabled(bool enable);

    /**
     * @brief Check if raw channel is enabled
     * @return true if raw channel is enabled
     */
    bool isRawChannelEnabled() const;

    /**
     * @brief Check if buffered channel is enabled
     * @return true if buffered channel is enabled
     */
    bool isBufferedChannelEnabled() const;

    /**
     * @brief DMA conversion complete callback (INTERNAL - called by HAL)
     * @note This is public for HAL callback routing but should not be called by users
     */
    void dmaConversionCompleteCallback();

    /**
     * @brief DMA error callback (INTERNAL - called by HAL)
     * @note This is public for HAL callback routing but should not be called by users
     */
    void dmaErrorCallback();

    /**
     * @brief Get singleton instance (for HAL callback routing)
     * @return Pointer to the global instance, or nullptr if not created
     */
    static BatteryMonitor* getInstance();

    /**
     * @brief Deinitialize the singleton instance (for test cleanup)
     * @details Call this only from test code if you created the instance.
     *          Does NOT call HAL_ADC_Stop() - lets main app manage hardware.
     * @note Use this pattern in tests:
     *       auto instance = BatteryMonitor::getInstance();
     *       if (!instance) create test instance
     *       test code ...
     *       BatteryMonitor::deinitInstance();  // Only if you created it
     */
    static void deinitInstance();

    /**
     * @brief Reset internal driver state
     * @details Called by deinitInstance() or destructor.
     *          Resets member variables; does not call HAL functions.
     */
    void deinit();

private:
    ADC_HandleTypeDef* _hadc;                  // ADC handle
    userconfig_t* _userconfig;                 // UserConfig handle for calibration storage
    __attribute__((aligned(32))) volatile uint16_t _adc_buffer[2];  // DMA buffer (D-Cache aligned)
    BatteryMonitorStats _stats;                // Statistics
    volatile bool _running;                    // Running state
    bool _raw_channel_enabled;                 // Enable raw channel in calculations
    bool _buffered_channel_enabled;            // Enable buffered channel in calculations

    static BatteryMonitor* _instance;          // Singleton instance for HAL callbacks

    /**
     * @brief Convert ADC value to voltage
     * @param adc_value 12-bit ADC value
     * @return Voltage in V
     */
    float adcToVoltage(uint16_t adc_value) const;

    /**
     * @brief Convert voltage to current
     * @param voltage Voltage in V
     * @param zero_offset Zero current offset voltage
     * @param sensitivity Sensor sensitivity
     * @return Current in A
     */
    float voltageToCurrent(float voltage, float zero_offset, float sensitivity) const;

    /**
     * @brief Update statistics with new current measurement
     * @param current Current value in A
     */
    void updateStats(float current);
};

#if ENABLE_BATTERY_MONITOR_CWRAPPER
// ============================================================================
// C Interface Functions - Extern "C" Linkage
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HAL ADC Conversion Complete Callback (auto-registered)
 * @note This is defined in BatteryMonitor.cpp and routes to BatteryMonitor::getInstance()
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);

/**
 * @brief Check if BatteryMonitor is initialized
 * @return 1 if initialized, 0 if not
 */
int batmon_is_initialized(void);

/**
 * @brief Check if BatteryMonitor is running (DMA active)
 * @return 1 if running, 0 if not
 */
int batmon_is_running(void);

/**
 * @brief Check if BatteryMonitor is calibrated
 * @return 1 if calibrated, 0 if not
 */
int batmon_is_calibrated(void);

/**
 * @brief Perform battery monitor calibration
 * @param samples Number of samples to average (default: 100)
 * @return 1 if successful, 0 if failed
 */
int batmon_calibrate(uint16_t samples);

/**
 * @brief Get calibration data
 * @return Pointer to calibration structure, or NULL if not calibrated
 */
const userconfig_batmon_t* batmon_get_calibration(void);

/**
 * @brief Get current consumption (average of both channels)
 * @return Current in Amperes (0-50A), or 0.0 if not calibrated
 */
float batmon_get_current(void);

/**
 * @brief Get ADC raw value from channel 0
 * @return ADC value (0-4095)
 */
uint16_t batmon_get_adc_raw(void);

/**
 * @brief Get ADC buffered value from channel 1
 * @return ADC value (0-4095)
 */
uint16_t batmon_get_adc_buffered(void);

/**
 * @brief Get voltage from raw channel
 * @return Voltage in V
 */
float batmon_get_voltage_raw(void);

/**
 * @brief Get voltage from buffered channel
 * @return Voltage in V
 */
float batmon_get_voltage_buffered(void);

/**
 * @brief Get statistics - minimum current
 * @return Minimum current observed in A
 */
float batmon_get_stat_min(void);

/**
 * @brief Get statistics - maximum current
 * @return Maximum current observed in A
 */
float batmon_get_stat_max(void);

/**
 * @brief Get statistics - average current
 * @return Average current in A
 */
float batmon_get_stat_avg(void);

/**
 * @brief Get statistics - sample count
 * @return Total number of samples collected
 */
uint32_t batmon_get_stat_samples(void);

/**
 * @brief Get statistics - DMA callback count
 * @return Total number of DMA callbacks
 */
uint32_t batmon_get_stat_dma_callbacks(void);

/**
 * @brief Erase calibration from NVRAM
 * @return 1 if successful, 0 if failed
 */
int batmon_erase_calibration(void);

/**
 * @brief Set polarity inversion
 * @param invert 1 = invert (sensor on GND/return path), 0 = normal (sensor on VCC+ path)
 * @return 1 if successful, 0 if failed
 */
int batmon_set_polarity(uint8_t invert);

/**
 * @brief Get polarity inversion setting
 * @return 1 if inverted, 0 if normal, -1 if not calibrated
 */
int batmon_get_polarity(void);

/**
 * @brief Set ADC reference voltage (VREF)
 * @param vref Reference voltage in V (e.g., 3.3)
 * @return 1 if successful, 0 if failed
 */
int batmon_set_vref(double vref);

/**
 * @brief Get current VREF setting
 * @return VREF in V, or 0.0 if not calibrated
 */
float batmon_get_vref(void);

/**
 * @brief Enable/disable raw channel in current calculation
 * @param enable 1 to enable, 0 to disable
 */
void batmon_set_raw_channel_enabled(int enable);

/**
 * @brief Enable/disable buffered channel in current calculation
 * @param enable 1 to enable, 0 to disable
 */
void batmon_set_buffered_channel_enabled(int enable);

/**
 * @brief Check if raw channel is enabled
 * @return 1 if enabled, 0 if disabled
 */
int batmon_is_raw_channel_enabled(void);

/**
 * @brief Check if buffered channel is enabled
 * @return 1 if enabled, 0 if disabled
 */
int batmon_is_buffered_channel_enabled(void);

#ifdef __cplusplus
}
#endif

#endif 

#endif 
#endif // BATTERYMONITOR_HPP
