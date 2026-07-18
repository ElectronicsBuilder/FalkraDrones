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
 * @file    driver_manager.hpp
 * @brief   Centralized driver initialization and management system
 *
 * DriverManager provides:
 * - Singleton pattern for unified driver access
 * - Automatic dependency resolution and ordered initialization
 * - Thread-safe accessors for all drivers
 * - Health monitoring and error tracking
 * - Resource arbitration for shared hardware (SPI/I2C buses)
 */
#ifndef __DRIVER_MANAGER_HPP
#define __DRIVER_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <array>
#include "driver_registry.hpp"
#include "driver_health.hpp"
#include "user_config.h"

// Forward declarations
class NVRAM;
class SpiFlash;
class QspiFlash;
class RtcDriver;
class SHT4x;
class BMP581;
class BNO085;
class BQ27441;
class TXS0108;
class VL53L5CX;
class ST7789;
class MAX98357;
class AudioManager;
class PPMDecoder;
class RadioReceiver;
class TPS2115;
class TPS2121;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Centralized driver manager and factory
 *
 * Singleton pattern provides unified access to all drivers with:
 * - Dependency-aware initialization
 * - Resource conflict prevention
 * - Health monitoring
 * - Thread-safe accessors
 *
 * Usage:
 * @code
 * auto& dm = DriverManager::getInstance();
 * if (dm.initializeAll()) {
 *     auto* imu = dm.getIMU();
 *     // Use driver...
 * }
 * @endcode
 */
class DriverManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to DriverManager singleton
     */
    static DriverManager& getInstance(void);

    // Prevent copying
    DriverManager(const DriverManager&) = delete;
    DriverManager& operator=(const DriverManager&) = delete;

    // === Initialization Control ===

    /**
     * @brief Initialize all drivers with dependency resolution
     * @return true if all critical drivers initialized, false if critical driver failed
     */
    bool initializeAll(void);

    /**
     * @brief Initialize only critical drivers (NVRAM, Config, BatteryMonitor, Log)
     * @return true if all critical drivers initialized
     */
    bool initializeCore(void);

    /**
     * @brief Check if DriverManager has completed initialization
     * @return true if initializeAll() or initializeCore() completed
     */
    bool isInitialized(void) const;

    // === Singleton Driver Accessors ===
    // Returns nullptr if driver not initialized or not required

    /**
     * @brief Get RTC driver instance
     * @return Pointer to RtcDriver, or nullptr if not initialized
     */
    RtcDriver* getRTC(void);

    /**
     * @brief Get BatteryMonitor (main battery monitoring)
     * @return Pointer to BatteryMonitor, or nullptr if not initialized
     */
    class BatteryMonitor* getBatteryMonitor(void);

    /**
     * @brief Get NVRAM driver
     * @return Pointer to NVRAM, or nullptr if not initialized
     */
    NVRAM* getNVRAM(void);

    /**
     * @brief Get SPI Flash driver
     * @return Pointer to SpiFlash, or nullptr if not initialized
     */
    SpiFlash* getSpiFlash(void);

    /**
     * @brief Get QSPI Flash driver
     * @return Pointer to QspiFlash, or nullptr if not initialized
     */
    QspiFlash* getQspiFlash(void);

    /**
     * @brief Get UserConfig structure
     * @return Pointer to userconfig_t, or nullptr if not initialized
     */
    userconfig_t* getUserConfig(void);

    // === Environmental Sensors ===

    /**
     * @brief Get SHT4x temperature/humidity sensor
     * @return Pointer to SHT4x, or nullptr if not initialized
     */
    SHT4x* getSHT4x(void);

    /**
     * @brief Get BMP581 pressure sensor
     * @return Pointer to BMP581, or nullptr if not initialized
     */
    BMP581* getBMP581(void);

    // === Motion & Navigation ===

    /**
     * @brief Get BNO085 IMU (semantic alias)
     * @return Pointer to BNO085, or nullptr if not initialized
     */
    BNO085* getIMU(void);

    /**
     * @brief Get BNO085 IMU
     * @return Pointer to BNO085, or nullptr if not initialized
     */
    BNO085* getBNO085(void);

    // === Power Management ===

    /**
     * @brief Get BQ27441 backup battery gauge
     * @return Pointer to BQ27441, or nullptr if not initialized
     */
    BQ27441* getBackupBattery(void);

    /**
     * @brief Get TPS2115 power multiplexer for VPWM voltage selection
     * @return Pointer to TPS2115, or nullptr if not initialized
     *
     * TPS2115 Controls ESC PWM Voltage (VPWM):
     * - Selects between 3.3V and 5V output for PWM[1..8] signals
     * - D1 pin drives voltage selection (HIGH=3.3V, LOW=5V)
     * - STAT pin provides voltage feedback (read-only)
     * - Output voltage fed to TXS0108 level shifter (VCCB pin)
     * - TXS0108 translates STM32 3.3V PWM to selected VPWM voltage
     *
     * Configuration:
     * - User selects PWM voltage via NVRAM: userconfig_system_t.pwm_voltage
     * - 0 = 5V (default, typical ESC voltage)
     * - 1 = 3.3V (modern logic-level ESCs)
     * - On startup, PWM voltage applied from saved configuration
     * - UI layer can dynamically change voltage at runtime
     */
    TPS2115* getPowerMux(void);

    /**
     * @brief Get TPS2121 power switch for main power source selection
     * @return Pointer to TPS2121, or nullptr if not initialized
     *
     * TPS2121 Selects Main Power Source:
     * - Dual-input automatic power source selection (MAIN vs USB)
     * - Powers main system logic and sensors
     * - Separate from VPWM voltage selection
     * - MAIN battery has priority, USB is backup
     */
    TPS2121* getPowerSwitch(void);

    /**
     * @brief Get TXS0108 level shifter for PWM voltage translation
     * @return Pointer to TXS0108, or nullptr if not initialized
     *
     * TXS0108 Level-Shifts PWM Signals:
     * - Translates 8 PWM channels from 3.3V (STM32) to VPWM voltage
     * - A-Side (VCCA): 3.3V STM32F767 (PWM[1..8] from timer outputs)
     * - B-Side (VCCB): VPWM from TPS2115 (3.3V or 5V)
     * - OE (VPWM_OE) pin enables/disables all 8 channels simultaneously
     * - When disabled, PWM lines high-impedance (safe for unpowered ESCs)
     *
     * Usage:
     * - Must enable TXS0108 OE pin to activate PWM output
     * - VPWM voltage set via TPS2115 before enabling PWM output
     * - For maximum safety, configure VPWM voltage before arming drone
     */
    TXS0108* getLevelShifter(void);

    // === Multi-Instance Drivers ===

    /**
     * @brief Get ToF sensor by index (0-5)
     * @param index Sensor index (0-5)
     * @return Pointer to VL53L5CX, or nullptr if index out of range or not initialized
     */
    VL53L5CX* getToFSensor(uint8_t index);

    // === Display & Audio ===

    /**
     * @brief Get ST7789 LCD display driver
     * @return Pointer to ST7789, or nullptr if not initialized
     */
    ST7789* getDisplay(void);

    /**
     * @brief Get MAX98357 audio amplifier (low-level control)
     * @return Pointer to MAX98357, or nullptr if not initialized
     */
    MAX98357* getAudio(void);

    /**
     * @brief Get AudioManager for high-level audio operations
     * @return Pointer to AudioManager, or nullptr if not initialized
     */
    AudioManager* getAudioManager(void);

    // === Communication ===

    /**
     * @brief Get PPM decoder
     * @return Pointer to PPMDecoder, or nullptr if not initialized
     */
    PPMDecoder* getPPMDecoder(void);

    /**
     * @brief Get radio receiver
     * @return Pointer to RadioReceiver, or nullptr if not initialized
     */
    RadioReceiver* getRadioReceiver(void);

    // === Status Queries ===

    /**
     * @brief Get driver initialization state
     * @param id Driver ID
     * @return Current DriverState
     */
    DriverState getDriverState(DriverId id);

    /**
     * @brief Get detailed driver health information
     * @param id Driver ID
     * @return Const reference to DriverHealth
     */
    const DriverHealth& getHealth(DriverId id);

private:
    DriverManager(void);
    ~DriverManager(void);

    /**
     * @brief Initialize single driver with dependencies checked
     * @param id Driver ID to initialize
     * @return true if successful or already initialized
     */
    bool initializeDriver(DriverId id);

    /**
     * @brief Check if all dependencies for a driver are ready
     * @param id Driver ID
     * @return true if all dependencies in READY state
     */
    bool checkDependencies(DriverId id);

    /**
     * @brief Set driver state and log state change
     * @param id Driver ID
     * @param state New state
     */
    void setDriverState(DriverId id, DriverState state);

    // === Driver Instance Pointers ===
    // All drivers stored as raw pointers (owned by DriverManager for C++ drivers)
    // RTC and ST7789 are global singletons from their .cpp files

    NVRAM* nvram_ptr = nullptr;
    SpiFlash* spi_flash_ptr = nullptr;
    QspiFlash* qspi_flash_ptr = nullptr;
    userconfig_t* user_config_ptr = nullptr;
    class BatteryMonitor* battery_monitor_ptr = nullptr;
    RtcDriver* rtc_ptr = nullptr;
    SHT4x* sht4x_ptr = nullptr;
    BMP581* bmp581_ptr = nullptr;
    BNO085* bno085_ptr = nullptr;
    BQ27441* bq27441_ptr = nullptr;
    TXS0108* txs0108_ptr = nullptr;
    std::array<VL53L5CX*, 6> tof_sensors = {};
    ST7789* st7789_ptr = nullptr;
    MAX98357* max98357_ptr = nullptr;
    AudioManager* audio_manager_ptr = nullptr;
    PPMDecoder* ppm_decoder_ptr = nullptr;
    RadioReceiver* radio_receiver_ptr = nullptr;
    TPS2115* tps2115_ptr = nullptr;
    TPS2121* tps2121_ptr = nullptr;

    // === State Tracking ===
    std::array<DriverState, static_cast<size_t>(DriverId::COUNT)> states;
    std::array<DriverHealth, static_cast<size_t>(DriverId::COUNT)> health;

    bool initialized = false;

    static DriverManager* instance;
};

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_MANAGER_HPP */
