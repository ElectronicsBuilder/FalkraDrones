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
 * @file    driver_status.hpp
 * @brief   Status collection and telemetry data management
 *
 * Provides thread-safe access to driver health and sensor telemetry data.
 * Updates are performed periodically by status_task() with FreeRTOS mutex protection.
 */
#ifndef __DRIVER_STATUS_HPP
#define __DRIVER_STATUS_HPP

#include <cstdint>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct FalkraStatus;

/**
 * @brief Driver status and telemetry update system
 *
 * Maintains thread-safe access to all driver health and sensor readings.
 * Uses FreeRTOS mutex to protect g_status structure from concurrent access.
 *
 * Designed to be called from status_task() every 500ms:
 * @code
 * DriverStatus::updateStatus([&](FalkraStatus& s) {
 *     s.lastUpdateMs = HAL_GetTick();
 *     DriverStatus::updateAllSensors(s);
 * });
 * @endcode
 */
class DriverStatus {
public:
    /**
     * @brief Initialize DriverStatus system (creates FreeRTOS mutex)
     * @return true if initialization successful
     */
    static bool init(void);

    /**
     * @brief Update all sensor readings in FalkraStatus
     * Must be called within updateStatus() lambda
     * @param status Reference to FalkraStatus to update
     */
    static void updateAllSensors(FalkraStatus& status);

    /**
     * @brief Decoupled/tiered sensor update path.
     * Performs sensor I/O outside status_mutex, then commits a short memcpy.
     */
    static void updateAllSensorsDecoupled(uint32_t tick);

    /**
     * @brief Thread-safe update of FalkraStatus with callback
     * Acquires mutex, calls updater callback, releases mutex
     * @param updater Lambda or function that updates status fields
     *
     * Usage:
     * @code
     * DriverStatus::updateStatus([](FalkraStatus& s) {
     *     s.temperatureC = 25.5f;
     *     s.humidityPct = 45.0f;
     * });
     * @endcode
     */
    static void updateStatus(std::function<void(FalkraStatus&)> updater);

    /**
     * @brief Get thread-safe snapshot of current status
     * @return Copy of g_status under mutex protection
     */
    static FalkraStatus getSnapshot(void);

    /**
     * @brief Get environmental sensor data (temperature, humidity, pressure, altitude)
     * @return Struct with environmental readings and timestamp
     */
    static struct EnvironmentalData getEnvironmentalData(void);

    /**
     * @brief Get motion sensor data (acceleration, orientation)
     * @return Struct with motion readings and timestamp
     */
    static struct MotionData getMotionData(void);

    /**
     * @brief Get battery data (voltage, current, SOC)
     * @return Struct with battery readings and timestamp
     */
    static struct BatteryData getBatteryData(void);

private:
    /**
     * @brief Update environmental sensor readings (SHT4x, BMP581)
     */
    static void updateEnvironmentalSensors(FalkraStatus& status);

    /**
     * @brief Update motion sensor readings (BNO085)
     */
    static void updateMotionSensors(FalkraStatus& status);

    /**
     * @brief Update battery readings (BatteryMonitor, BQ27441)
     */
    static void updateBatteryData(FalkraStatus& status);

    /**
     * @brief Update ToF sensor readings (VL53L5CX array)
     */
    static void updateToFSensors(FalkraStatus& status);

    /**
     * @brief Update RC control input readings (PPM channels)
     */
    static void updateRCInput(FalkraStatus& status);

    /**
     * @brief Update power management status (TPS2121, TPS2115, TXS0108)
     *
     * Queries:
     * - TPS2121: Current power source (USB vs MAIN battery)
     * - TPS2115: VPWM voltage setting (3.3V or 5V) and config match
     * - TXS0108: Level shifter OE pin state
     */
    static void updatePowerStatus(FalkraStatus& status);

    /**
     * @brief Update memory device status (NVRAM, SPI Flash, QSPI Flash)
     */
    static void updateMemoryStatus(FalkraStatus& status);
};

/**
 * @brief Environmental sensor readings with timestamp
 */
struct EnvironmentalData {
    float temperatureC;     // Temperature in Celsius
    float humidityPct;      // Relative humidity in percent
    float pressurePa;       // Atmospheric pressure in Pascals
    float altitudeM;        // Calculated altitude in meters
    uint32_t timestamp_ms;  // Timestamp of reading
};

/**
 * @brief Motion sensor readings with timestamp
 */
struct MotionData {
    struct {
        float x, y, z;      // Acceleration in m/s²
    } acceleration;
    struct {
        float w, x, y, z;   // Quaternion (w, i, j, k)
    } orientation;
    uint32_t timestamp_ms;  // Timestamp of reading
};

/**
 * @brief Battery data with timestamp
 */
struct BatteryData {
    float mainVoltageMv;    // Main battery voltage in mV
    float mainCurrentMa;    // Main battery current in mA
    float mainSocPct;       // Main battery state of charge in percent
    float backupVoltageMv;  // Backup battery voltage in mV
    float backupSocPct;     // Backup battery state of charge in percent
    uint32_t timestamp_ms;  // Timestamp of reading
};

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_STATUS_HPP */
