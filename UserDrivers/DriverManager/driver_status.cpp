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
 * @file    driver_status.cpp
 * @brief   Status collection and telemetry update implementation
 */
#include "driver_status.hpp"
#include "dm_opts.h"
#include "tof_speed_opts.h"
#include "status.hpp"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "log.hpp"
#include "driver_manager.hpp"
#include "SHT4x.hpp"
#include "bmp581.hpp"
#include "BNO085.hpp"
#include "sh2_SensorValue.h"
#include "bq27441.hpp"
#include "BatteryMonitor.hpp"
#include "TofProximityManager.hpp"
#include "ppm.hpp"
#include "main.h"
#include "nvram.hpp"
#include "spi_flash.hpp"
#include "qspi_flash.hpp"
#include "ffs.h"
#include "tps2121.hpp"
#include "tps2115.hpp"
#include "txs0108.hpp"
#include "user_config.h"
#include <cstring>
#include <cmath>

// FreeRTOS mutex for thread-safe status access
static SemaphoreHandle_t status_mutex = nullptr;

// === Initialization ===

bool DriverStatus::init(void) {
    if (status_mutex) {
        return true;  // Already initialized
    }

    status_mutex = xSemaphoreCreateMutex();
    if (!status_mutex) {
        LOG_ERROR("[STATUS] Failed to create status mutex!");
        return false;
    }

    LOG_INFO("[STATUS] DriverStatus system initialized");
    return true;
}

// === Sensor Update Functions ===

void DriverStatus::updateAllSensors(FalkraStatus& status) {
    updateEnvironmentalSensors(status);
    updateMotionSensors(status);
    updateBatteryData(status);
#if DM_OPT_TOF_INTEGRATION
    updateToFSensors(status);
#endif
    updateRCInput(status);
    updatePowerStatus(status);
    updateMemoryStatus(status);
}

void DriverStatus::updateAllSensorsDecoupled(uint32_t tick) {
    FalkraStatus working = getSnapshot();
    working.lastUpdateMs = HAL_GetTick();

    updateRCInput(working);
    updatePowerStatus(working);
#if DM_OPT_TOF_INTEGRATION
    updateToFSensors(working);
#endif

    if ((tick % 10U) == 0U) {
        updateEnvironmentalSensors(working);
        updateBatteryData(working);
    }

    if ((tick % 30U) == 0U) {
        updateMotionSensors(working);
    }

    if ((tick % 40U) == 0U) {
        updateMemoryStatus(working);
    }

    updateStatus([&working](FalkraStatus& status) {
        working.tofCmdStatus = status.tofCmdStatus;
        status = working;
    });
}

void DriverStatus::updateEnvironmentalSensors(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    // SHT4x - Temperature & Humidity
    auto* sht = dm.getSHT4x();
    if (sht) {
        float temp, humidity;
        if (sht->readTempAndHumidity(temp, humidity)) {
            status.temperatureC = temp;
            status.humidityPct = humidity;
            status.sht4xOk = true;
        } else {
            status.sht4xOk = false;
        }
    } else {
        status.sht4xOk = false;
    }

    // BMP581 - Pressure & Calculated Altitude
    auto* bmp = dm.getBMP581();
    if (bmp) {
        float temp, pressure;
        if (bmp->getSensorData(temp, pressure)) {
            status.pressurePa = pressure;
            // Calculate altitude from sea-level pressure (standard: 101325 Pa)
            // h = (1 - (P/P0)^0.190284) * 44330.77
             const float P0 = 101325.0f;
            status.altitudeM = (1.0f - powf(pressure / P0, 0.190284f)) * 44330.77f;
            status.bmp581Ok = true;
        } else {
            status.bmp581Ok = false;
        }
    } else {
        status.bmp581Ok = false;
    }
}

void DriverStatus::updateMotionSensors(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    auto* imu = dm.getIMU();
    if (imu) {
        sh2_SensorValue_t sensorValue;
        bool has_accel = false;
        bool has_rotation = false;

        // Poll multiple times to get both sensor types
        // getSensorEvent() returns different sensor types on sequential calls
        for (int i = 0; i < 10 && (!has_accel || !has_rotation); i++) {
            if (imu->getSensorEvent(&sensorValue)) {
                switch (sensorValue.sensorId) {
                    case SH2_ROTATION_VECTOR:
                        // Quaternion: r, i, j, k (w, x, y, z)
                        status.rotation[0] = sensorValue.un.rotationVector.real;
                        status.rotation[1] = sensorValue.un.rotationVector.i;
                        status.rotation[2] = sensorValue.un.rotationVector.j;
                        status.rotation[3] = sensorValue.un.rotationVector.k;
                        has_rotation = true;
                        break;

                    case SH2_GAME_ROTATION_VECTOR:
                        // Game rotation vector (no magnetometer, drift-free)
                        status.rotation[0] = sensorValue.un.gameRotationVector.real;
                        status.rotation[1] = sensorValue.un.gameRotationVector.i;
                        status.rotation[2] = sensorValue.un.gameRotationVector.j;
                        status.rotation[3] = sensorValue.un.gameRotationVector.k;
                        has_rotation = true;
                        break;

                    case SH2_ACCELEROMETER:
                        // Acceleration: X, Y, Z (m/s^2)
                        status.accel[0] = sensorValue.un.accelerometer.x;
                        status.accel[1] = sensorValue.un.accelerometer.y;
                        status.accel[2] = sensorValue.un.accelerometer.z;
                        has_accel = true;
                        break;

                    default:
                        // Ignore other sensor types for now
                        break;
                }
            } else {
                // No more events available
                break;
            }
        }

        status.bno085Ok = (has_accel && has_rotation);
    } else {
        status.bno085Ok = false;
    }
}

void DriverStatus::updateBatteryData(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    // Main Battery (BatteryMonitor - ACS758 current sensor)
    auto* main_bat = dm.getBatteryMonitor();
    if (main_bat && main_bat->isCalibrated()) {
        BatteryMonitorData data = main_bat->getData();
        //status.mainVoltageMv = data.voltage_buffered * 1000.0f;  // Convert V to mV
        status.mainVoltageMv = 0.0 * 1000.0f;  // Convert V to mV
        //todo -> This is 0 for now. Future implementation will add an INA219 voltage sensor for main battery voltage monitoring.
        status.mainCurrentMa = data.current_average * 1000.0f;   // Convert A to mA
        // Note: Main battery SOC not available from ACS758 (current-only sensor)
        // Would need voltage-based estimation or integration over time
        status.mainSocPct = 0.0f;  // Not available from ACS758
    }

    // Backup Battery (BQ27441 fuel gauge)
    auto* backup_bat = dm.getBackupBattery();
    if (backup_bat) {
        // voltage() returns uint16_t in mV
        status.backupVoltageMv = static_cast<float>(backup_bat->voltage());

        // soc() returns uint16_t percentage (0-100)
        status.backupSocPct = static_cast<float>(backup_bat->soc());

        status.bq27441Ok = true;
    } else {
        status.backupVoltageMv = 0.0f;
        status.backupSocPct = 0.0f;
        status.bq27441Ok = false;
    }
}

void DriverStatus::updateToFSensors(FalkraStatus& status) {
#if DM_OPT_TOF_INTEGRATION
    auto& dm = DriverManager::getInstance();
    auto* mgr = dm.getTofProximity();
    if (mgr == nullptr) {
        return;
    }

    TofDistanceSnapshot snapshot = {};
    if (!mgr->getSnapshot(&snapshot)) {
        return;
    }

    uint32_t now_ms = HAL_GetTick();
    bool any_present = false;
    bool any_fresh = false;
    bool any_stale = false;
    static bool stale_reported = false;

    for (uint8_t i = 0; i < MAX_TOF_SENSOR; i++) {
        const auto& sensor = mgr->getSensor(static_cast<TofSensorId>(i));
        const char* name = sensor.getName();

        std::strncpy(status.tofDeviceName[i], name, sizeof(status.tofDeviceName[i]) - 1);
        status.tofDeviceName[i][sizeof(status.tofDeviceName[i]) - 1] = '\0';
        status.tofDeviceLoc[i] = i;
        status.tofDeviceStatus[i] = snapshot.sensor_valid[i];
        status.tofSensorDistance[i] = snapshot.distance_mm[i] / 10U;
        status.tofDetectingFlag[i] = static_cast<uint8_t>(sensor.getDetectionFlag());

        if (snapshot.sensor_valid[i]) {
            any_present = true;
            uint32_t last_update_ms = mgr->getLastUpdateMs(i);
            if (last_update_ms != 0U && (now_ms - last_update_ms) <= TOF_STALE_THRESHOLD_MS) {
                any_fresh = true;
            } else {
                any_stale = true;
            }
        }
    }

    if (any_present && !any_fresh && !stale_reported) {
        DriverHealthMonitor::reportError(DriverId::TOF_PROXIMITY, "all present ToF sensors stale");
        stale_reported = true;
    } else if (any_stale && !stale_reported) {
        DriverHealthMonitor::reportWarning(DriverId::TOF_PROXIMITY, "present sensor stale");
        stale_reported = true;
    } else if (any_fresh && !any_stale) {
        stale_reported = false;
    }
#else
    (void)status;  // Parameter not directly used - ToF updates handled by TofProximityManager
    // ToF proximity system uses TofProximityManager singleton
    // The manager internally updates g_status with sensor data
    auto& mgr = TofProximityManager::getInstance();
    if (mgr.isRanging()) {
        mgr.process();
    }
#endif
}

void DriverStatus::updateRCInput(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    // Get PPM decoder from DriverManager
    auto* ppm = dm.getPPMDecoder();
    if (!ppm) {
        // No RC receiver available
        status.ppmChannelCount = 0;
        status.ppmOk = false;
        return;
    }

    // Check signal validity
    if (ppm->isSignalValid()) {
        // Populate raw channel pulse widths (microseconds)
        for (uint8_t i = 0; i < 8; i++) {
            status.ppmChannels[i] = ppm->getChannel(i);
        }

        // Populate normalized control inputs for primary channels
        // Channel 0: Roll (Aileron)
        // Channel 1: Pitch (Elevator)
        // Channel 2: Throttle
        // Channel 3: Yaw (Rudder)
        status.ppmNormalized[0] = ppm->getChannelNormalized(0);  // Roll
        status.ppmNormalized[1] = ppm->getChannelNormalized(1);  // Pitch
        status.ppmNormalized[2] = ppm->getChannelNormalized(2);  // Throttle
        status.ppmNormalized[3] = ppm->getChannelNormalized(3);  // Yaw

        // Set channel count and timing info
        status.ppmChannelCount = ppm->getChannelCount();
        status.ppmTimeSinceLastFrame = ppm->getTimeSinceLastFrame();
        status.ppmOk = true;
    } else {
        // Signal invalid or lost
        status.ppmTimeSinceLastFrame = ppm->getTimeSinceLastFrame();
        status.ppmOk = false;
        // Keep previous channel values but mark as stale
    }
}

// === Thread-Safe Status Access ===

void DriverStatus::updateStatus(std::function<void(FalkraStatus&)> updater) {
    if (!status_mutex) {
        LOG_ERROR("[STATUS] Status mutex not initialized!");
        return;
    }

    // Try up to 3 times with shorter timeout (reduces contention impact)
    for (int retry = 0; retry < 3; retry++) {
        if (xSemaphoreTake(status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            updater(g_status);
            xSemaphoreGive(status_mutex);
            return;
        }
    }
    // Non-critical: status will be updated next cycle
    LOG_WARN("[STATUS] Status update skipped (mutex busy)");
}

FalkraStatus DriverStatus::getSnapshot(void) {
    FalkraStatus snapshot = {};

    if (!status_mutex) {
        LOG_ERROR("[STATUS] Status mutex not initialized!");
        return snapshot;
    }

    // Try up to 3 times with shorter timeout
    for (int retry = 0; retry < 3; retry++) {
        if (xSemaphoreTake(status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            snapshot = g_status;
            xSemaphoreGive(status_mutex);
            return snapshot;
        }
    }
    LOG_WARN("[STATUS] Snapshot skipped (mutex busy)");

    return snapshot;
}

// === Grouped Data Access ===

EnvironmentalData DriverStatus::getEnvironmentalData(void) {
    auto snapshot = getSnapshot();
    return {
        snapshot.temperatureC,
        snapshot.humidityPct,
        snapshot.pressurePa,
        snapshot.altitudeM,
        snapshot.lastUpdateMs
    };
}

MotionData DriverStatus::getMotionData(void) {
    auto snapshot = getSnapshot();
    return {
        {snapshot.accel[0], snapshot.accel[1], snapshot.accel[2]},
        {snapshot.rotation[0], snapshot.rotation[1], snapshot.rotation[2], snapshot.rotation[3]},
        snapshot.lastUpdateMs
    };
}

BatteryData DriverStatus::getBatteryData(void) {
    auto snapshot = getSnapshot();
    return {
        snapshot.mainVoltageMv,
        snapshot.mainCurrentMa,
        snapshot.mainSocPct,
        snapshot.backupVoltageMv,
        snapshot.backupSocPct,
        snapshot.lastUpdateMs
    };
}

void DriverStatus::updatePowerStatus(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    // Initialize timestamp
    status.power.timestamp_ms = HAL_GetTick();

    // === TPS2121 - Main Power Source Selection ===
    // Tracks whether powered from USB or MAIN battery
    auto* powerSwitch = dm.getPowerSwitch();
    if (powerSwitch) {
        // TPS2121::getSelectedInput() returns MAIN or USB
        // HIGH on VSEL pin = USB, LOW on VSEL pin = MAIN
        auto selected = powerSwitch->getSelectedInput();
        status.power.powered_from_usb = (selected == TPS2121::InputSource::USB);

        // Note: TPS2121 doesn't provide individual power detection
        // In a real system, you'd monitor MAIN and USB rails separately
        // For now, we can set availability based on which is selected
        status.power.main_power_available = !status.power.powered_from_usb;
        status.power.usb_power_available = status.power.powered_from_usb;
    } else {
        status.power.powered_from_usb = false;
        status.power.main_power_available = false;
        status.power.usb_power_available = false;
    }

    // === TPS2115 - PWM Output Voltage Selection (VPWM) ===
    // Tracks which voltage is being used for ESC PWM signals
    auto* powerMux = dm.getPowerMux();
    if (powerMux) {
        // Load config setting from NVRAM
        auto* user_config = dm.getUserConfig();
        if (user_config) {
            status.power.vpwm_config_setting = userconfig_get_pwm_voltage(user_config);
        } else {
            status.power.vpwm_config_setting = 0;  // Default to 5V
        }

        // Read actual voltage from STAT pin feedback
        // getActiveInput() returns IN1 (3.3V) or IN2 (5V)
        auto active = powerMux->getActiveInput();
        bool voltage_is_3v3 = (active == TPS2115::InputSource::IN1);
        status.power.vpwm_voltage_mv = voltage_is_3v3 ? 3300 : 5000;

        // Check if actual matches configuration
        bool config_is_3v3 = (status.power.vpwm_config_setting == 1);
        status.power.vpwm_voltage_match = (voltage_is_3v3 == config_is_3v3);
    } else {
        status.power.vpwm_voltage_mv = 5000;      // Default to 5V
        status.power.vpwm_config_setting = 0;
        status.power.vpwm_voltage_match = false;
    }

    // === TXS0108 - PWM Level Shifter ===
    // Tracks whether level shifter is enabled (OE pin state)
    auto* levelShifter = dm.getLevelShifter();
    if (levelShifter) {
        status.power.vpwm_oe_enabled = levelShifter->isEnabled();
        status.power.vpwm_level_shifter_ok = true;
    } else {
        status.power.vpwm_oe_enabled = false;
        status.power.vpwm_level_shifter_ok = false;
    }
}

void DriverStatus::updateMemoryStatus(FalkraStatus& status) {
    auto& dm = DriverManager::getInstance();

    // Initialize timestamp
    status.memory.timestamp_ms = HAL_GetTick();

    // === NVRAM Status ===
    auto* nvram = dm.getNVRAM();
    if (nvram) {
        // NVRAM is always at full capacity (128KB) - no growth
        status.nvramOk = true;
        status.memory.nvram_capacity_bytes = 131072;  // 128KB

        // Check write protection status
        auto status_reg = nvram->readStatusRegister();
        status.memory.nvram_write_protected = (status_reg.WPEN == 1);
    } else {
        status.nvramOk = false;
    }

    // === SPI Flash Status ===
    auto* spi_flash = dm.getSpiFlash();
    if (spi_flash) {
        status.flashOk = true;
        status.memory.spi_flash_capacity_bytes = 16777216;  // 16MB (W25Q128J)
        // SPI flash usage would require scanning which is expensive
        // For now, mark as 0 (could be updated periodically in background)
        status.memory.spi_flash_used_bytes = 0;
    } else {
        status.flashOk = false;
    }

    // === QSPI Flash Status ===
    auto* qspi_flash = dm.getQspiFlash();
    if (qspi_flash) {
        status.qspiOk = true;
        status.memory.qspi_flash_capacity_bytes = 16777216;  // 16MB (W25Q128J)
        // QSPI flash is used for TouchGFX assets - capacity is fixed
        // Usage tracking would require scanning which is expensive
        status.memory.qspi_flash_used_bytes = 0;
    } else {
        status.qspiOk = false;
    }

    // === Flash File System (FFS) Status ===
    // Probe the FFS to get block allocation statistics
    status.memory.ffs_total_blocks = ffs_config.block_count;
    status.memory.ffs_used_blocks = 0;
    status.memory.ffs_free_blocks = 0;
    status.memory.ffs_max_erase_count = 0;
    status.memory.ffs_avg_erase_count = 0;

    // Scan block table to count used blocks and wear statistics
    if (block_table && ffs_config.block_count > 0) {
        uint32_t total_erase_count = 0;
        uint32_t counted_blocks = 0;

        for (uint32_t i = 0; i < ffs_config.block_count; i++) {
            if (block_table[i].in_use) {
                status.memory.ffs_used_blocks++;
            }

            // Track wear leveling statistics
            if (block_table[i].erase_count > status.memory.ffs_max_erase_count) {
                status.memory.ffs_max_erase_count = block_table[i].erase_count;
            }
            total_erase_count += block_table[i].erase_count;
            counted_blocks++;
        }

        // Calculate free blocks
        status.memory.ffs_free_blocks = ffs_config.block_count - status.memory.ffs_used_blocks;

        // Calculate average erase count
        if (counted_blocks > 0) {
            status.memory.ffs_avg_erase_count = total_erase_count / counted_blocks;
        }
    }

    // === Calculate combined memory health score ===
    // Health is based on device availability and write protection status
    uint32_t healthy_devices = 0;
    uint32_t total_devices = 3;  // NVRAM, SPI Flash, QSPI Flash

    if (status.nvramOk) healthy_devices++;
    if (status.flashOk) healthy_devices++;
    if (status.qspiOk) healthy_devices++;

    // Calculate health score: 100 = all devices healthy, 0 = all devices offline
    status.memory.memory_health_score = (healthy_devices * 100) / total_devices;

    // Reduce score if NVRAM is write-protected (potential configuration issue)
    if (status.nvramOk && status.memory.nvram_write_protected) {
        status.memory.memory_health_score = (status.memory.memory_health_score * 80) / 100;
    }

    // Consider FFS wear health - reduce score if max erase count is high
    if (status.memory.ffs_max_erase_count > 10000) {
        status.memory.memory_health_score = (status.memory.memory_health_score * 90) / 100;
    }
}
