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
 * @file    driver_registry.cpp
 * @brief   Driver metadata registry with dependency graph
 */
#include "driver_registry.hpp"
#include "log.hpp"

/**
 * @brief Complete driver metadata registry
 * Organized by initialization dependency level
 */
static const DriverMetadata driver_registry[] = {
    // Storage Layer (Level 0 - no dependencies)
    {DriverId::NVRAM, "NVRAM (47L04)", true, 0, {}, 500, 0},
    {DriverId::SPI_FLASH, "SPI Flash (W25Q128)", true, 0, {}, 500, 0},
    {DriverId::QSPI_FLASH, "QSPI Flash", true, 0, {}, 500, 0},

    // Configuration Layer (Level 1 - depends on storage)
    {DriverId::USER_CONFIG, "UserConfig", true, 1,
        {DriverId::NVRAM, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1000, 10},
    {DriverId::BOOT_FUSE, "Boot Fuse System", false, 1,
        {DriverId::QSPI_FLASH, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 500, 5},

    // Infrastructure (Level 2 - depends on storage/config)
    {DriverId::FILE_SYSTEM, "FileSystem (FFS)", true, 1,
        {DriverId::SPI_FLASH, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 2000, 15},
    {DriverId::LOG, "Log System", true, 2,
        {DriverId::FILE_SYSTEM, DriverId::RTC_MODULE, DriverId::COUNT, DriverId::COUNT}, 3000, 25},
    {DriverId::BATTERY_MONITOR, "BatteryMonitor (ADC+DMA)", true, 1,
        {DriverId::USER_CONFIG, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1000, 20},

    // Sensors (Level 3 - simple or no dependencies)
    {DriverId::RTC_MODULE, "RTC", true, 0, {}, 200, 5},
    {DriverId::SHT4X, "SHT4x (Temp/Humidity)", false, 0, {}, 500, 30},
    {DriverId::BMP581, "BMP581 (Pressure)", false, 0, {}, 500, 30},
    {DriverId::BNO085, "BNO085 (IMU)", false, 0, {}, 1500, 30},
    {DriverId::BQ27441, "BQ27441 (Fuel Gauge)", false, 0, {}, 500, 30},
    {DriverId::TXS0108, "TXS0108 (Level Shifter)", false, 0, {}, 100, 30},

    // Complex Sensors (Level 4 - depends on level shifter)
#if DM_OPT_TOF_INTEGRATION
    {DriverId::TOF_PROXIMITY, "ToF Proximity", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 100, 35},
#else
    {DriverId::VL53L5CX_1, "ToF Sensor 1", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
    {DriverId::VL53L5CX_2, "ToF Sensor 2", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
    {DriverId::VL53L5CX_3, "ToF Sensor 3", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
    {DriverId::VL53L5CX_4, "ToF Sensor 4", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
    {DriverId::VL53L5CX_5, "ToF Sensor 5", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
    {DriverId::VL53L5CX_6, "ToF Sensor 6", false, 1,
        {DriverId::TXS0108, DriverId::COUNT, DriverId::COUNT, DriverId::COUNT}, 1500, 35},
#endif

    // Peripherals (Level 3-4)
    {DriverId::ST7789, "ST7789 (Display)", false, 0, {}, 500, 40},
    {DriverId::MAX98357, "MAX98357 (Audio)", false, 0, {}, 200, 40},
    {DriverId::PPM_DECODER, "PPM Decoder", false, 0, {}, 200, 40},
    {DriverId::RADIO_RECEIVER, "Radio Receiver", false, 0, {}, 200, 40},
    {DriverId::TPS2115, "TPS2115 (Power Mux)", false, 0, {}, 100, 40},
    {DriverId::TPS2121, "TPS2121 (Power Switch)", false, 0, {}, 100, 40},
    {DriverId::WIFI, "WiFi (ST67W611M1)", false, 0, {}, 3000, 45},
};

// Verify registry size matches DriverId::COUNT
static_assert(sizeof(driver_registry) / sizeof(driver_registry[0]) ==
              static_cast<size_t>(DriverId::COUNT),
              "Driver registry size mismatch with DriverId::COUNT");

const DriverMetadata* getDriverMetadata(DriverId id) {
    if (id >= DriverId::COUNT) {
        return nullptr;
    }
    return &driver_registry[static_cast<size_t>(id)];
}

uint8_t getDriverCount(void) {
    return static_cast<uint8_t>(DriverId::COUNT);
}

bool driverHasDependency(DriverId driver_id, DriverId dependency_id) {
    const auto* metadata = getDriverMetadata(driver_id);
    if (!metadata) {
        return false;
    }

    for (uint8_t i = 0; i < metadata->dependency_count; i++) {
        if (metadata->dependencies[i] == dependency_id) {
            return true;
        }
    }

    return false;
}
