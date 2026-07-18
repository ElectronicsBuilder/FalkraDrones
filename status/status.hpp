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
 * @file    status.hpp
 * @brief   Task Status
 */
#ifndef __STATUS_HPP
#define __STATUS_HPP

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "app_defs.hpp"
#include "tof_proximity.hpp"  // Legacy compatibility - provides tof_sensor_id_t, tof_system_state_t

void status_task(void *arg);
void status_init(void);

/**
 * @brief Power management and voltage control status
 *
 * Tracks power source selection (USB vs MAIN) and PWM voltage configuration.
 * Provides real-time feedback on which power source is active and ESC voltage.
 */
struct PowerStatus {
    // TPS2121 - Main power source selection (MAIN battery vs USB)
    bool powered_from_usb        = false;     // True if powered from USB, false if from MAIN battery
    bool main_power_available    = false;     // True if MAIN battery detected and providing power
    bool usb_power_available     = false;     // True if USB power detected

    // TPS2115 - PWM output voltage selection (VPWM)
    uint16_t vpwm_voltage_mv     = 3300;      // Current VPWM voltage: 5000mV (5V) or 3300mV (3.3V)
    uint8_t vpwm_config_setting  = 1;         // NVRAM setting: 0=5V, 1=3.3V (default)
    bool vpwm_voltage_match      = false;     // True if actual voltage matches config setting

    // TXS0108 - PWM level shifter control (VPWM_OE pin)
    bool vpwm_oe_enabled         = false;     // True if TXS0108 output enable pin is HIGH (level shifter active)
    bool vpwm_level_shifter_ok   = false;     // True if TXS0108 initialized successfully

    // Timestamp
    uint32_t timestamp_ms        = 0;
};

/**
 * @brief Memory device status and usage information
 */
struct MemoryStatus {
    // NVRAM (CY14B101Q2 - 128KB)
    uint32_t nvram_capacity_bytes      = 131072;  // 128KB fixed
    bool nvram_write_protected         = false;

    // SPI Flash (W25Q128J - 16MB)
    uint32_t spi_flash_capacity_bytes  = 16777216;  // 16MB fixed
    uint32_t spi_flash_used_bytes      = 0;

    // QSPI Flash (W25Q128J - 16MB)
    uint32_t qspi_flash_capacity_bytes = 16777216;  // 16MB fixed
    uint32_t qspi_flash_used_bytes     = 0;

    // Flash File System (FFS) statistics
    uint32_t ffs_total_blocks          = 0;
    uint32_t ffs_used_blocks           = 0;
    uint32_t ffs_free_blocks           = 0;
    uint32_t ffs_max_erase_count       = 0;  // Highest wear level
    uint32_t ffs_avg_erase_count       = 0;  // Average wear level

    // Combined memory health
    uint8_t memory_health_score        = 0;   // 0-100, higher is better

    // Timestamp
    uint32_t timestamp_ms              = 0;
};

struct FalkraStatus {
    // Peripheral health
    bool nvramOk         = false;
    bool flashOk         = false;
    bool qspiOk          = false;
    bool sht4xOk         = false;
    bool bmp581Ok        = false;
    bool bno085Ok        = false;
    bool bq27441Ok       = false;
    bool ppmOk           = false;
    bool sbusOk          = false;
    bool escPwmOk        = false;
    bool displayOk       = false;

    // Environmental readings
    float temperatureC   = 0.0f;
    float humidityPct    = 0.0f;
    float pressurePa     = 0.0f;
    float altitudeM      = 0.0f;

    // Motion data
    float accel[3]       = {0.0f, 0.0f, 0.0f};        // X, Y, Z
    float rotation[4]    = {1.0f, 0.0f, 0.0f, 0.0f};  // Quaternion: r, i, j, k

    // RC Control Input (PPM/SBUS)
    uint16_t ppmChannels[8] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};  // 1000-2000µs
    float ppmNormalized[4]   = {0.0f, 0.0f, 0.0f, 0.0f};  // Roll, Pitch, Throttle, Yaw (-1.0 to +1.0)
    uint8_t ppmChannelCount  = 0;     // Number of valid channels
    uint32_t ppmTimeSinceLastFrame = 0;  // ms since last valid PPM frame

    // Backup battery
    float backupVoltageMv = 0.0f;
    float backupSocPct    = 0.0f;

    // Main battery (future)
    float mainVoltageMv   = 0.0f;
    float mainCurrentMa   = 0.0f;
    float mainSocPct      = 0.0f;

    // ToF sensor data
    char  tofDeviceName[MAX_TOF_SENSOR][16]     = {};
    uint8_t tofDeviceLoc[MAX_TOF_SENSOR]        = {};
    uint8_t tofDeviceStatus[MAX_TOF_SENSOR]     = {};
    uint8_t tofDetectingFlag[MAX_TOF_SENSOR]    = {};
    uint16_t tofSensorDistance[MAX_TOF_SENSOR]  = {};  // cm
    tof_system_state_t tofCmdStatus             = tof_system_state_t::sensors_idle;

    // Power management status
    PowerStatus power;

    // Memory device status
    MemoryStatus memory;

    // Drone state
    bool droneArmed = false;  // True if drone is armed, false if disarmed

    // RTC Time (for display)
    uint8_t timeHours   = 0;
    uint8_t timeMinutes = 0;
    uint8_t timeSeconds = 0;
    uint16_t timeDays   = 0;

    // System timestamp
    uint32_t lastUpdateMs = 0;
};

// Global instance
extern FalkraStatus g_status;



#ifdef __cplusplus
}
#endif

#endif /* __STATUS_HPP */
