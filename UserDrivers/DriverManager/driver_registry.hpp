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
 * @file    driver_registry.hpp
 * @brief   Driver metadata and dependency graph for DriverManager
 */
#ifndef __DRIVER_REGISTRY_HPP
#define __DRIVER_REGISTRY_HPP

#include <cstdint>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Driver enumeration for dependency tracking and management
 * Organized by initialization level (0 = no dependencies to 4 = depends on others)
 */
enum class DriverId : uint8_t {
    // Storage Layer (Level 0 - no dependencies)
    NVRAM = 0,
    SPI_FLASH,
    QSPI_FLASH,

    // Configuration Layer (Level 1 - depends on storage)
    USER_CONFIG,        // depends: NVRAM
    BOOT_FUSE,         // depends: QSPI_FLASH

    // Infrastructure (Level 2 - depends on config/storage)
    FILE_SYSTEM,       // depends: SPI_FLASH
    LOG,               // depends: FILE_SYSTEM, RTC
    BATTERY_MONITOR,   // depends: USER_CONFIG

    // Sensors (Level 3 - independent or simple dependencies)
    RTC_MODULE,  // Note: Named RTC_MODULE to avoid STM32 HAL macro collision
    SHT4X,
    BMP581,
    BNO085,
    BQ27441,
    TXS0108,           // Level shifter, must init before PWM Motor control

    // Complex Sensors (Level 4 - depends on level shifter)
    VL53L5CX_1,
    VL53L5CX_2,
    VL53L5CX_3,
    VL53L5CX_4,
    VL53L5CX_5,
    VL53L5CX_6,

    // Peripherals (Level 3-4)
    ST7789,
    MAX98357,
    PPM_DECODER,
    RADIO_RECEIVER,
    TPS2115,
    TPS2121,
    WIFI,

    COUNT
};

/**
 * @brief Driver state enumeration for tracking initialization status
 */
enum class DriverState : uint8_t {
    UNINITIALIZED = 0,
    INITIALIZING,
    READY,
    ERROR,
    DEGRADED,
    DISABLED
};

/**
 * @brief Driver metadata for dependency resolution and initialization orchestration
 */
struct DriverMetadata {
    DriverId id;
    const char* name;
    bool required;                    // System cannot boot without this driver
    uint8_t dependency_count;         // Number of dependencies
    DriverId dependencies[4];         // Max 4 dependencies per driver
    uint32_t init_timeout_ms;         // Maximum initialization time
    uint8_t init_priority;            // Manual priority override (0 = auto)
};

/**
 * @brief Get metadata for a specific driver
 * @param id Driver ID
 * @return Pointer to metadata, or nullptr if not found
 */
const DriverMetadata* getDriverMetadata(DriverId id);

/**
 * @brief Get total number of registered drivers
 * @return Number of drivers in registry
 */
uint8_t getDriverCount(void);

/**
 * @brief Check if a driver has a specific dependency
 * @param driver_id Driver to check
 * @param dependency_id Dependency to look for
 * @return true if dependency exists, false otherwise
 */
bool driverHasDependency(DriverId driver_id, DriverId dependency_id);

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_REGISTRY_HPP */
