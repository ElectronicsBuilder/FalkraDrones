/**
 * @file    BatteryMonitor.cpp
 * @brief   ACS758-50A Hall Effect Current Sensor Driver Implementation
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

#include "BatteryMonitor.hpp"
#include "log.hpp"
#include "cmsis_os2.h"  // For osDelay() - FreeRTOS CMSIS-RTOS v2
#include <cstring>

#include "BatteryMonitor.hpp"
#include "BatteryMonitorTask.hpp"
#include "user_config.h"
#include "app_defs.hpp"

// Static singleton instance pointer
BatteryMonitor* BatteryMonitor::_instance = nullptr;

BatteryMonitor::BatteryMonitor(ADC_HandleTypeDef* hadc, userconfig_t* userconfig)
    : _hadc(hadc)
    , _userconfig(userconfig)
    , _running(false)
    , _raw_channel_enabled(true)      // Both channels enabled by default
    , _buffered_channel_enabled(true)
{
    // Set singleton instance
    _instance = this;

    // Initialize DMA buffer
    _adc_buffer[0] = 0;
    _adc_buffer[1] = 0;

    // Initialize statistics
    std::memset(&_stats, 0, sizeof(_stats));
    _stats.current_min = 1000.0f;  // Initialize to large value
    _stats.current_max = -1000.0f; // Initialize to small value
}

bool BatteryMonitor::init() {
    LOG_INFO("[BATMON] Initializing Battery Monitor (ACS758-50A)");

    if (!_hadc) {
        LOG_ERROR("[BATMON] Invalid ADC handle");
        return false;
    }

    if (!_userconfig) {
        LOG_ERROR("[BATMON] Invalid UserConfig handle");
        return false;
    }

    // Verify ADC is configured for DMA mode
    if (!(_hadc->Init.DMAContinuousRequests == ENABLE)) {
        LOG_WARN("[BATMON] ADC DMA continuous requests not enabled");
    }

    // Verify 2 channels are configured
    if (_hadc->Init.NbrOfConversion != 2) {
        LOG_ERROR("[BATMON] ADC must be configured with 2 channels (found %d)",
                  _hadc->Init.NbrOfConversion);
        return false;
    }

    // Load calibration from NVRAM via UserConfig
    if (loadCalibration()) {
        const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
        LOG_INFO("[BATMON] Calibration loaded from NVRAM");
        LOG_INFO("[BATMON] - VREF: %.4fV", cal->vref);
        LOG_INFO("[BATMON] - Zero raw: %.4fV", cal->zero_current_raw);
        LOG_INFO("[BATMON] - Zero buffered: %.4fV", cal->zero_current_buffered);
        LOG_INFO("[BATMON] - Polarity: %s (invert_polarity=%d)",
                 cal->invert_polarity ? "INVERTED (GND/return)" : "NORMAL (VCC+)",
                 cal->invert_polarity);

        // Load channel enable settings from NVRAM
        _raw_channel_enabled = cal->raw_channel_enabled ? true : false;
        _buffered_channel_enabled = cal->buffered_channel_enabled ? true : false;
        LOG_INFO("[BATMON] - Channels: Raw=%s, Buffered=%s",
                 _raw_channel_enabled ? "ENABLED" : "DISABLED",
                 _buffered_channel_enabled ? "ENABLED" : "DISABLED");
    } else {
        LOG_WARN("[BATMON] ========================================");
        LOG_WARN("[BATMON] NO CALIBRATION FOUND");
        LOG_WARN("[BATMON] ========================================");
        LOG_WARN("[BATMON] Driver will not report current measurements");
        LOG_WARN("[BATMON] To calibrate:");
        LOG_WARN("[BATMON]   1. Disconnect all loads (ensure 0A current)");
        LOG_WARN("[BATMON]   2. Send command: BATMON_CALIBRATE");
        LOG_WARN("[BATMON] ========================================");
    }

    LOG_INFO("[BATMON] Initialization complete");
    LOG_INFO("[BATMON] - Sensitivity: %.3fV/A", ACS758::SENSITIVITY_V_PER_A);
    LOG_INFO("[BATMON] - Range: ±%.0fA", ACS758::MAX_CURRENT_A);
    LOG_INFO("[BATMON] - Calibrated: %s", isCalibrated() ? "YES" : "NO");

    return true;
}

bool BatteryMonitor::start() {
    if (_running) {
        LOG_WARN("[BATMON] Already running");
        return true;
    }

    LOG_INFO("[BATMON] Starting continuous ADC DMA sampling");

    // Start ADC with DMA in circular mode
    HAL_StatusTypeDef status = HAL_ADC_Start_DMA(
        _hadc,
        (uint32_t*)_adc_buffer,
        2  // 2 channels
    );

    if (status != HAL_OK) {
        LOG_ERROR("[BATMON] Failed to start ADC DMA (status: %d)", status);
        return false;
    }

    _running = true;
    LOG_INFO("[BATMON] Continuous DMA sampling started");

    return true;
}

bool BatteryMonitor::stop() {
    if (!_running) {
        return true;
    }

    LOG_INFO("[BATMON] Stopping ADC DMA conversions");

    HAL_StatusTypeDef status = HAL_ADC_Stop_DMA(_hadc);
    if (status != HAL_OK) {
        LOG_ERROR("[BATMON] Failed to stop ADC DMA (status: %d)", status);
        return false;
    }

    _running = false;
    LOG_INFO("[BATMON] ADC DMA stopped");
    return true;
}

bool BatteryMonitor::calibrate(uint16_t samples) {
    LOG_INFO("[BATMON] ========================================");
    LOG_INFO("[BATMON] STARTING CALIBRATION");
    LOG_INFO("[BATMON] ========================================");
    LOG_WARN("[BATMON] CRITICAL: Ensure NO current flowing through sensor!");
    LOG_INFO("[BATMON] Collecting %d samples...", samples);

    if (!_running) {
        LOG_ERROR("[BATMON] Must call start() before calibration");
        return false;
    }

    // Accumulate samples from continuous DMA buffer
    uint32_t sum_raw = 0;
    uint32_t sum_buffered = 0;

    for (uint16_t i = 0; i < samples; i++) {
        // Wait for DMA to update buffer (circular mode runs continuously)
        osDelay(10);  // ~10ms between samples for fresh data

        sum_raw += _adc_buffer[0];
        sum_buffered += _adc_buffer[1];

        // Progress indicator
        if ((i + 1) % 20 == 0) {
            LOG_INFO("[BATMON] Progress: %d/%d samples", i + 1, samples);
        }
    }

    // Calculate averages
    uint16_t avg_raw = sum_raw / samples;
    uint16_t avg_buffered = sum_buffered / samples;

    // Get VREF from existing calibration or default
    float vref = 3.3f;
    const userconfig_batmon_t* existing_cal = userconfig_get_batmon_block(_userconfig);
    if (existing_cal) {
        vref = existing_cal->vref;
    }

    // Convert to voltages
    float zero_raw = (static_cast<float>(avg_raw) / ACS758::ADC_RESOLUTION) * vref;
    float zero_buffered = (static_cast<float>(avg_buffered) / ACS758::ADC_RESOLUTION) * vref;

    LOG_INFO("[BATMON] ========================================");
    LOG_INFO("[BATMON] Calibration Results:");
    LOG_INFO("[BATMON] - Zero offset (raw):      %.4fV (ADC: %d)", zero_raw, avg_raw);
    LOG_INFO("[BATMON] - Zero offset (buffered): %.4fV (ADC: %d)", zero_buffered, avg_buffered);
    LOG_INFO("[BATMON] - VREF:                   %.4fV", vref);
    LOG_INFO("[BATMON] - Sensitivity:            %.3fV/A", ACS758::SENSITIVITY_V_PER_A);
    LOG_INFO("[BATMON] ========================================");

    // Detect polarity: Check if sensor is on GND/return path
    // Default to invert (1) since sensor is typically on return path
    uint8_t invert_polarity = 1;

    // Check if we have existing calibration with polarity setting
    if (existing_cal) {
        invert_polarity = existing_cal->invert_polarity;
        LOG_INFO("[BATMON] Using existing polarity setting: %s",
                 invert_polarity ? "INVERTED (GND/return path)" : "NORMAL (VCC+ path)");
    } else {
        LOG_INFO("[BATMON] Defaulting to INVERTED polarity (sensor on GND/return path)");
        LOG_INFO("[BATMON] Use BATMON_SET_POLARITY command to change if needed");
    }

    // Save calibration to NVRAM via UserConfig
    bool success = userconfig_set_batmon_calibration(
        _userconfig,
        vref,
        zero_raw,
        zero_buffered,
        ACS758::SENSITIVITY_V_PER_A,
        ACS758::SENSITIVITY_V_PER_A,
        invert_polarity
    );

    if (!success) {
        LOG_ERROR("[BATMON] Failed to set calibration data");
        return false;
    }

    if (!saveCalibration()) {
        LOG_ERROR("[BATMON] Failed to save calibration to NVRAM");
        return false;
    }

    LOG_INFO("[BATMON] ✓ Calibration saved to NVRAM successfully");
    LOG_INFO("[BATMON] ========================================");

    return true;
}

bool BatteryMonitor::isCalibrated() const {
    return userconfig_has_batmon_calibration(_userconfig);
}

bool BatteryMonitor::loadCalibration() {
    if (!_userconfig) {
        return false;
    }

    return userconfig_load_batmon(_userconfig);
}

bool BatteryMonitor::saveCalibration() {
    if (!_userconfig) {
        return false;
    }

    return userconfig_save_batmon(_userconfig);
}

bool BatteryMonitor::eraseCalibration() {
    if (!_userconfig) {
        return false;
    }

    userconfig_reset_batmon(_userconfig);
    LOG_INFO("[BATMON] Calibration reset to defaults (uncalibrated)");

    return true;
}

BatteryMonitorData BatteryMonitor::getData() const {
    BatteryMonitorData data;

    // Read ADC values (volatile read from DMA buffer)
    data.adc_raw = _adc_buffer[0];
    data.adc_buffered = _adc_buffer[1];

    if (!isCalibrated()) {
        // Return invalid data if not calibrated
        data.voltage_raw = 0.0f;
        data.voltage_buffered = 0.0f;
        data.current_raw = 0.0f;
        data.current_buffered = 0.0f;
        data.current_average = 0.0f;
        data.valid = false;
        return data;
    }

    // Get calibration from userconfig
    const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
    if (!cal) {
        data.valid = false;
        return data;
    }

    // Convert to voltages
    data.voltage_raw = adcToVoltage(data.adc_raw);
    data.voltage_buffered = adcToVoltage(data.adc_buffered);

    // Calculate currents
    data.current_raw = voltageToCurrent(
        data.voltage_raw,
        cal->zero_current_raw,
        cal->sensitivity_raw
    );

    data.current_buffered = voltageToCurrent(
        data.voltage_buffered,
        cal->zero_current_buffered,
        cal->sensitivity_buffered
    );

    // Apply polarity inversion if sensor is on GND/return path
    // Use absolute value since consumption is always positive
    if (cal->invert_polarity) {
        float before_raw = data.current_raw;
        float before_buf = data.current_buffered;

        // Use basic absolute value (more reliable than fabsf)
        if (data.current_raw < 0.0f) {
            data.current_raw = -data.current_raw;
        }
        if (data.current_buffered < 0.0f) {
            data.current_buffered = -data.current_buffered;
        }

        // Debug log (remove after verification)
        // static uint32_t debug_count = 0;
        // if (debug_count++ % 100 == 0) {  // Log every 100 samples
        //     LOG_INFO("[BATMON] Polarity correction: raw %.3fA->%.3fA, buf %.3fA->%.3fA",
        //              before_raw, data.current_raw, before_buf, data.current_buffered);
        // }
    }

    // Calculate average based on enabled channels
    float sum = 0.0f;
    int count = 0;

    if (_raw_channel_enabled) {
        sum += data.current_raw;
        count++;
    }

    if (_buffered_channel_enabled) {
        sum += data.current_buffered;
        count++;
    }

    // Calculate average, or return 0 if no channels enabled
    data.current_average = (count > 0) ? (sum / count) : 0.0f;

    // Apply deadband to eliminate noise when current is near zero
    constexpr float DEADBAND_A = 0.015f;  // 15mA deadband
    if (data.current_average < DEADBAND_A) {
        data.current_raw = 0.0f;
        data.current_buffered = 0.0f;
        data.current_average = 0.0f;
    }

    data.valid = true;

    return data;
}

float BatteryMonitor::getCurrent() const {
    if (!isCalibrated()) {
        return 0.0f;
    }

    // Return average of enabled channels
    BatteryMonitorData data = getData();
    return data.current_average;
}

float BatteryMonitor::getCurrentRaw() const {
    if (!isCalibrated()) {
        return 0.0f;
    }

    const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
    if (!cal) {
        return 0.0f;
    }

    float voltage = adcToVoltage(_adc_buffer[0]);
    float current = voltageToCurrent(voltage, cal->zero_current_raw, cal->sensitivity_raw);

    // Apply polarity correction if needed
    if (cal->invert_polarity && current < 0.0f) {
        current = -current;
    }

    return current;
}

float BatteryMonitor::getCurrentBuffered() const {
    if (!isCalibrated()) {
        return 0.0f;
    }

    const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
    if (!cal) {
        return 0.0f;
    }

    float voltage = adcToVoltage(_adc_buffer[1]);
    float current = voltageToCurrent(voltage, cal->zero_current_buffered, cal->sensitivity_buffered);

    // Apply polarity correction if needed
    if (cal->invert_polarity && current < 0.0f) {
        current = -current;
    }

    return current;
}

BatteryMonitorStats BatteryMonitor::getStats() const {
    return _stats;
}

void BatteryMonitor::resetStats() {
    _stats.current_min = 1000.0f;
    _stats.current_max = -1000.0f;
    _stats.current_avg = 0.0f;
    _stats.sample_count = 0;
    _stats.dma_complete_count = 0;
    _stats.dma_error_count = 0;
}

bool BatteryMonitor::isRunning() const {
    return _running;
}

const userconfig_batmon_t* BatteryMonitor::getCalibration() const {
    return userconfig_get_batmon_block(_userconfig);
}

void BatteryMonitor::dmaConversionCompleteCallback() {
    // Invalidate D-Cache for the DMA buffer so CPU reads fresh data
    SCB_InvalidateDCache_by_Addr((uint32_t*)_adc_buffer, sizeof(_adc_buffer));

    _stats.dma_complete_count++;

    // Update statistics with current reading
    if (isCalibrated()) {
        float current = getCurrentBuffered();
        updateStats(current);
    }
}

void BatteryMonitor::dmaErrorCallback() {
    _stats.dma_error_count++;
    LOG_ERROR("[BATMON] DMA error occurred (total errors: %lu)", _stats.dma_error_count);
}

BatteryMonitor* BatteryMonitor::getInstance() {
    return _instance;
}

void BatteryMonitor::deinitInstance() {
    if (_instance) {
        _instance->deinit();
        _instance = nullptr;
        LOG_SYSSTATUS("[BATMON] BatteryMonitor singleton instance cleanup complete");
    }
}

void BatteryMonitor::deinit() {
    // Stop ADC if running (safe to call even if not started)
    if (_hadc) {
        HAL_ADC_Stop_DMA(_hadc);
    }

    // Reset running state and calibration flag
    _running = false;

    // Reset channel enable flags
    _raw_channel_enabled = true;
    _buffered_channel_enabled = true;

    // Reset DMA buffer
    _adc_buffer[0] = 0;
    _adc_buffer[1] = 0;

    // Reset statistics
    std::memset(&_stats, 0, sizeof(_stats));
    _stats.current_min = 1000.0f;
    _stats.current_max = -1000.0f;

    LOG_SYSSTATUS("[BATMON] BatteryMonitor cleanup complete");
}

float BatteryMonitor::adcToVoltage(uint16_t adc_value) const {
    // Get VREF from calibration
    const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
    float vref = cal ? cal->vref : 3.3f;  // Default to 3.3V if not calibrated

    return (static_cast<float>(adc_value) / static_cast<float>(ACS758::ADC_RESOLUTION)) * vref;
}

float BatteryMonitor::voltageToCurrent(float voltage, float zero_offset, float sensitivity) const {
    // Current = (Vmeasured - Vzero) / Sensitivity
    return (voltage - zero_offset) / sensitivity;
}

void BatteryMonitor::updateStats(float current) {
    _stats.sample_count++;

    // Update min/max
    if (current < _stats.current_min) {
        _stats.current_min = current;
    }
    if (current > _stats.current_max) {
        _stats.current_max = current;
    }

    // Update running average (exponential moving average)
    float alpha = 0.1f;  // Smoothing factor
    if (_stats.sample_count == 1) {
        _stats.current_avg = current;
    } else {
        _stats.current_avg = alpha * current + (1.0f - alpha) * _stats.current_avg;
    }
}

bool BatteryMonitor::setVref(float vref) {
    if (!isCalibrated()) {
        LOG_ERROR("[BATMON] Cannot set VREF - not calibrated");
        return false;
    }

    // Validate VREF range (2.5V - 3.6V reasonable for STM32)
    if (vref < 2.5f || vref > 3.6f) {
        LOG_ERROR("[BATMON] Invalid VREF: %.3fV (must be 2.5-3.6V)", vref);
        return false;
    }

    // Get current calibration
    const userconfig_batmon_t* cal = userconfig_get_batmon_block(_userconfig);
    if (!cal) {
        LOG_ERROR("[BATMON] Failed to get calibration data");
        return false;
    }

    // Update calibration with new VREF
    extern userconfig_t* g_userConfig;
    if (!g_userConfig) {
        LOG_ERROR("[BATMON] UserConfig not available");
        return false;
    }

    bool success = userconfig_set_batmon_calibration(
        g_userConfig,
        vref,  // NEW VREF
        cal->zero_current_raw,
        cal->zero_current_buffered,
        cal->sensitivity_raw,
        cal->sensitivity_buffered,
        cal->invert_polarity
    );

    if (!success) {
        LOG_ERROR("[BATMON] Failed to update VREF in calibration");
        return false;
    }

    // Save to NVRAM
    if (!saveCalibration()) {
        LOG_ERROR("[BATMON] Failed to save VREF to NVRAM");
        return false;
    }

    LOG_INFO("[BATMON] VREF updated to %.4fV and saved to NVRAM", vref);
    return true;
}

void BatteryMonitor::setRawChannelEnabled(bool enable) {
    _raw_channel_enabled = enable;

    // Save to NVRAM
    extern userconfig_t* g_userConfig;
    if (g_userConfig) {
        userconfig_set_batmon_channels(g_userConfig, enable ? 1 : 0, _buffered_channel_enabled ? 1 : 0);
    }

    LOG_INFO("[BATMON] Raw channel %s and saved to NVRAM", enable ? "ENABLED" : "DISABLED");
}

void BatteryMonitor::setBufferedChannelEnabled(bool enable) {
    _buffered_channel_enabled = enable;

    // Save to NVRAM
    extern userconfig_t* g_userConfig;
    if (g_userConfig) {
        userconfig_set_batmon_channels(g_userConfig, _raw_channel_enabled ? 1 : 0, enable ? 1 : 0);
    }

    LOG_INFO("[BATMON] Buffered channel %s and saved to NVRAM", enable ? "ENABLED" : "DISABLED");
}

bool BatteryMonitor::isRawChannelEnabled() const {
    return _raw_channel_enabled;
}

bool BatteryMonitor::isBufferedChannelEnabled() const {
    return _buffered_channel_enabled;
}

/**
 * @brief HAL ADC Conversion Complete Callback
 * @details Self-contained callback routing to singleton instance
 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        BatteryMonitor* instance = BatteryMonitor::getInstance();
        if (instance) {
            instance->dmaConversionCompleteCallback();
        }
    }
}


#if ENABLE_BATTERY_MONITOR_CWRAPPER
// ============================================================================
// C Wrapper Functions for C Compatibility
// ============================================================================

extern "C" {

int batmon_is_initialized(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    return (monitor != nullptr) ? 1 : 0;
}

int batmon_is_running(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->isRunning() ? 1 : 0;
}

int batmon_is_calibrated(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->isCalibrated() ? 1 : 0;
}

int batmon_calibrate(uint16_t samples) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->calibrate(samples) ? 1 : 0;
}

const userconfig_batmon_t* batmon_get_calibration(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return nullptr;
    return monitor->getCalibration();
}

float batmon_get_current(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    return monitor->getCurrent();
}

uint16_t batmon_get_adc_raw(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    BatteryMonitorData data = monitor->getData();
    return data.adc_raw;
}

uint16_t batmon_get_adc_buffered(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    BatteryMonitorData data = monitor->getData();
    return data.adc_buffered;
}

float batmon_get_voltage_raw(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    BatteryMonitorData data = monitor->getData();
    return data.voltage_raw;
}

float batmon_get_voltage_buffered(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    BatteryMonitorData data = monitor->getData();
    return data.voltage_buffered;
}

float batmon_get_stat_min(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    BatteryMonitorStats stats = monitor->getStats();
    return stats.current_min;
}

float batmon_get_stat_max(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    BatteryMonitorStats stats = monitor->getStats();
    return stats.current_max;
}

float batmon_get_stat_avg(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;
    BatteryMonitorStats stats = monitor->getStats();
    return stats.current_avg;
}

uint32_t batmon_get_stat_samples(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    BatteryMonitorStats stats = monitor->getStats();
    return stats.sample_count;
}

uint32_t batmon_get_stat_dma_callbacks(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    BatteryMonitorStats stats = monitor->getStats();
    return stats.dma_complete_count;
}

int batmon_erase_calibration(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->eraseCalibration() ? 1 : 0;
}

int batmon_set_polarity(uint8_t invert) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;

    // Get current calibration
    const userconfig_batmon_t* cal = monitor->getCalibration();
    if (!cal) return 0;

    // Update calibration with new polarity
    extern userconfig_t* g_userConfig;
    if (!g_userConfig) return 0;

    bool success = userconfig_set_batmon_calibration(
        g_userConfig,
        cal->vref,
        cal->zero_current_raw,
        cal->zero_current_buffered,
        cal->sensitivity_raw,
        cal->sensitivity_buffered,
        invert
    );

    if (!success) return 0;
    return monitor->saveCalibration() ? 1 : 0;
}

int batmon_get_polarity(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return -1;

    const userconfig_batmon_t* cal = monitor->getCalibration();
    if (!cal) return -1;

    return cal->invert_polarity ? 1 : 0;
}

int batmon_set_vref(double vref) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->setVref(vref) ? 1 : 0;
}

float batmon_get_vref(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0.0f;

    const userconfig_batmon_t* cal = monitor->getCalibration();
    if (!cal) return 0.0f;

    return cal->vref;
}

void batmon_set_raw_channel_enabled(int enable) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (monitor) {
        monitor->setRawChannelEnabled(enable != 0);
    }
}

void batmon_set_buffered_channel_enabled(int enable) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (monitor) {
        monitor->setBufferedChannelEnabled(enable != 0);
    }
}

int batmon_is_raw_channel_enabled(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->isRawChannelEnabled() ? 1 : 0;
}

int batmon_is_buffered_channel_enabled(void) {
    BatteryMonitor* monitor = BatteryMonitor::getInstance();
    if (!monitor) return 0;
    return monitor->isBufferedChannelEnabled() ? 1 : 0;
}

} // extern "C"

#endif 
