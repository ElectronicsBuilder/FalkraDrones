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
 * @file    driver_manager.cpp
 * @brief   Centralized driver initialization and management implementation
 */
#include "driver_manager.hpp"
#include "driver_health.hpp"
#include "log.hpp"
#include "main.h"

// Include driver headers for initialization
#include "nvram.hpp"
#include "spi_flash.hpp"
#include "qspi_flash.hpp"
#include "BatteryMonitor.hpp"
#include "user_config.h"
#include "nvram_wrapper.h"
#include "rtc.hpp"
#include "st7789.hpp"
#include "audio_manager.hpp"
#include "SHT4x.hpp"
#include "bmp581.hpp"
#include "BNO085.hpp"
#include "bq27441.hpp"
#include "max98357.hpp"
#include "tps2115.hpp"
#include "tps2121.hpp"
#include "ppm.hpp"
#include "radio_receiver.hpp"
#include "txs0108.hpp"
#include "vl53l5cx.hpp"
#include "TofProximityManager.hpp"
#include "app_tof.hpp"
#include "FreeRTOS.h"
#include "task.h"

// Forward declares
extern RtcDriver rtc;

#include "i2c2_bus_lock.hpp"  // I2C2 shared by SHT4x/BQ27441/BMP581 across tasks

// === BMP581 I2C Wrapper Functions (for raw function pointers) ===

static int8_t bmp581_i2c_read_wrapper(uint8_t reg_addr, uint8_t* data, uint32_t len, void* intf_ptr) {
    extern I2C_HandleTypeDef hi2c2;
    uint8_t addr = *(reinterpret_cast<uint8_t*>(intf_ptr));
    I2c2BusGuard guard;
    return HAL_I2C_Mem_Read(&hi2c2, addr << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 100) == HAL_OK ? 0 : -1;
}

static int8_t bmp581_i2c_write_wrapper(uint8_t reg_addr, const uint8_t* data, uint32_t len, void* intf_ptr) {
    extern I2C_HandleTypeDef hi2c2;
    uint8_t addr = *(reinterpret_cast<uint8_t*>(intf_ptr));
    I2c2BusGuard guard;
    return HAL_I2C_Mem_Write(&hi2c2, addr << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, const_cast<uint8_t*>(data), len, 100) == HAL_OK ? 0 : -1;
}

static void bmp581_delay_wrapper(uint32_t us, void*) {
    HAL_Delay((us + 999) / 1000);  // round up to 1 ms if < 1ms
}

// Static instance
DriverManager* DriverManager::instance = nullptr;

// === Constructor/Destructor ===

DriverManager::DriverManager(void) {
    // Initialize all states to UNINITIALIZED
    for (size_t i = 0; i < states.size(); i++) {
        states[i] = DriverState::UNINITIALIZED;
    }

    LOG_SYSSTATUS("[DRV_MGR] DriverManager singleton created");
}

DriverManager::~DriverManager(void) {
    LOG_SYSSTATUS("[DRV_MGR] DriverManager singleton destroyed");
}

// === Singleton Instance ===

DriverManager& DriverManager::getInstance(void) {
    if (!instance) {
        instance = new DriverManager();
    }
    return *instance;
}

// === Initialization ===

bool DriverManager::initializeAll(void) {
    LOG_INFO("[DRV_MGR] Starting full driver initialization...");

    bool critical_success = true;

    // Initialize all drivers in dependency order
    for (uint8_t i = 0; i < static_cast<uint8_t>(DriverId::COUNT); i++) {
        auto driver_id = static_cast<DriverId>(i);
        const auto* meta = getDriverMetadata(driver_id);

        if (!meta) {
            continue;
        }

        if (!initializeDriver(driver_id)) {
            if (meta->required) {
                LOG_ERROR("[DRV_MGR] Critical driver %s failed to initialize!", meta->name);
                critical_success = false;
                break;
            } else {
                LOG_WARN("[DRV_MGR] Optional driver %s failed to initialize", meta->name);
            }
        }
    }

    if (critical_success) {
        initialized = true;
        LOG_INFO("[DRV_MGR] All critical drivers initialized successfully");
    } else {
        LOG_ERROR("[DRV_MGR] Critical driver initialization failed!");
    }

    return critical_success;
}

bool DriverManager::initializeCore(void) {
    LOG_INFO("[DRV_MGR] Starting core driver initialization (critical path only)...");

    // Initialize only critical dependencies: NVRAM -> UserConfig -> BatteryMonitor -> Log -> Display
    // Note: Using static_cast for RTC to avoid STM32 HAL macro collision
    const DriverId critical_drivers[] = {
        DriverId::NVRAM,
        DriverId::SPI_FLASH,
        DriverId::QSPI_FLASH,
        DriverId::USER_CONFIG,
        DriverId::FILE_SYSTEM,
        DriverId::RTC_MODULE,
        DriverId::LOG,
        DriverId::BATTERY_MONITOR,
        DriverId::TXS0108,
#if DM_OPT_TOF_INTEGRATION
        DriverId::TOF_PROXIMITY,
#endif
        DriverId::ST7789,
    };

    for (auto driver_id : critical_drivers) {
        if (!initializeDriver(driver_id)) {
            const auto* meta = getDriverMetadata(driver_id);
            const char* name = meta ? meta->name : "Unknown";

            if (driver_id == DriverId::TXS0108
#if DM_OPT_TOF_INTEGRATION
                || driver_id == DriverId::TOF_PROXIMITY
#endif
            ) {
                LOG_WARN("[DRV_MGR] Core driver failed, continuing: %s", name);
                continue;
            }

            LOG_ERROR("[DRV_MGR] Core driver failed: %s", name);
            return false;
        }
    }

    initialized = true;
    LOG_INFO("[DRV_MGR] Core drivers initialized successfully");
    return true;
}

bool DriverManager::isInitialized(void) const {
    return initialized;
}

// === Driver Initialization ===

bool DriverManager::initializeDriver(DriverId id) {
    if (id >= DriverId::COUNT) {
        return false;
    }

    // Already initialized?
    if (states[static_cast<size_t>(id)] == DriverState::READY) {
        return true;
    }

    const auto* meta = getDriverMetadata(id);
    if (!meta) {
        return false;
    }

    // Check dependencies
    if (!checkDependencies(id)) {
        LOG_WARN("[DRV_MGR] %s: dependencies not ready", meta->name);
        return false;
    }

    setDriverState(id, DriverState::INITIALIZING);

    uint32_t start_tick = HAL_GetTick();
    bool success = true;

    // Initialize driver based on ID
    switch (id) {
        case DriverId::NVRAM: {
            // Get SPI1 handle
            extern SPI_HandleTypeDef hspi1;

            // Initialize GPIO pins to correct states BEFORE creating NVRAM object
            // HOLD and WP pins must be HIGH to enable chip operation
            HAL_GPIO_WritePin(NVRAM_CS_GPIO_Port, NVRAM_CS_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(NVRAM_HOLD_GPIO_Port, NVRAM_HOLD_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(NVRAM_WP_GPIO_Port, NVRAM_WP_Pin, GPIO_PIN_SET);

            nvram_ptr = new NVRAM(
                &hspi1,
                NVRAM_CS_GPIO_Port, NVRAM_CS_Pin,
                NVRAM_HOLD_GPIO_Port, NVRAM_HOLD_Pin,
                NVRAM_WP_GPIO_Port, NVRAM_WP_Pin
            );
            if (!nvram_ptr) {
                success = false;
            }
            break;
        }

        case DriverId::SPI_FLASH: {
            extern SPI_HandleTypeDef hspi1;
            spi_flash_ptr = new SpiFlash(
                &hspi1,
                FLASH_CS_GPIO_Port, FLASH_CS_Pin
            );
            if (spi_flash_ptr) {
                spi_flash_ptr->init();  // init() returns void
            } else {
                success = false;
            }
            break;
        }

        case DriverId::QSPI_FLASH: {
            extern QSPI_HandleTypeDef hqspi;
            qspi_flash_ptr = new QspiFlash(&hqspi);
            // Note: QspiFlash init happens during STM32 HAL initialization
            break;
        }

        case DriverId::USER_CONFIG: {
            if (!nvram_ptr) {
                success = false;
                break;
            }
            // Create C-compatible NVRAM interface
            nvram_interface_t* nvram_if = nvram_interface_create(nvram_ptr);
            user_config_ptr = userconfig_create(nvram_if);
            if (user_config_ptr) {
                if (!userconfig_init(user_config_ptr)) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] UserConfig initialization failed");
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] UserConfig creation failed");
            }
            break;
        }

        case DriverId::FILE_SYSTEM: {
            if (!spi_flash_ptr) {
                success = false;
                break;
            }
            // FileSystem is lazily initialized, just mark as ready
            // (actual init happens in log_init or on first filesystem access)
            success = true;
            break;
        }

        case DriverId::RTC_MODULE: {
            rtc_ptr = &rtc;
            // RTC is already initialized by STM32CubeMX
            success = true;
            break;
        }

        case DriverId::LOG: {
            // Log system initialization is handled in UserDriverTask
            // Just mark as ready here
            success = true;
            break;
        }

        case DriverId::BATTERY_MONITOR: {
            if (!user_config_ptr) {
                success = false;
                break;
            }
            extern ADC_HandleTypeDef hadc1;
            battery_monitor_ptr = new BatteryMonitor(
                &hadc1,
                user_config_ptr
            );
            if (battery_monitor_ptr) {
                if (!battery_monitor_ptr->init()) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] BatteryMonitor initialization failed");
                    break;
                }
                // Check if calibration exists
                if (battery_monitor_ptr->isCalibrated()) {
                    LOG_INFO("[BATMON] Calibration found, starting continuous monitoring");
                    if (!battery_monitor_ptr->start()) {
                        success = false;
                        LOG_ERROR("[BATMON] Failed to start DMA sampling");
                    }
                } else {
                    LOG_WARN("[BATMON] No calibration found");
                    // Still start DMA for calibration purposes
                    if (!battery_monitor_ptr->start()) {
                        success = false;
                        LOG_ERROR("[BATMON] Failed to start uncalibrated DMA");
                    }
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] BatteryMonitor creation failed");
            }
            break;
        }

        case DriverId::ST7789: {
            // Display driver - Initialize ST7789 with config from display_driver.cpp
            extern ST7789::Config display_config;

            LOG_INFO("[DRV_MGR] Starting ST7789 display initialization");
            st7789_ptr = new ST7789(display_config);
            if (st7789_ptr) {
                st7789_ptr->init();  // init() returns void
                // Set backlight on by default
                st7789_ptr->setBacklight(true);
                LOG_INFO("[DRV_MGR] ST7789 display initialized");
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] ST7789 creation failed");
            }
            break;
        }

        case DriverId::SHT4X: {
            extern I2C_HandleTypeDef hi2c2;
            sht4x_ptr = new SHT4x(&hi2c2);
            if (sht4x_ptr) {
                if (!sht4x_ptr->init()) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] SHT4x initialization failed");
                } else {
                    LOG_INFO("[DRV_MGR] SHT4x initialized");
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] SHT4x creation failed");
            }
            break;
        }

        case DriverId::BMP581: {
            // BMP581 pressure sensor initialization (I2C2, address 0x47)
            // Note: hi2c2 is used inside the wrapper functions, not here directly
            static uint8_t bmp581_addr = 0x47;  // Use 0x47 if SDO is pulled high

            bmp581_ptr = new BMP581(
                bmp581_i2c_read_wrapper,
                bmp581_i2c_write_wrapper,
                bmp581_delay_wrapper,
                &bmp581_addr,
                BMP5_I2C_INTF
            );

            if (bmp581_ptr) {
                if (!bmp581_ptr->initialize()) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] BMP581 initialization failed");
                    delete bmp581_ptr;
                    bmp581_ptr = nullptr;
                } else {
                    LOG_INFO("[DRV_MGR] BMP581 initialized");
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] BMP581 creation failed");
            }
            break;
        }

        case DriverId::BNO085: {
            extern SPI_HandleTypeDef hspi3;
            bno085_ptr = new BNO085();
            if (bno085_ptr) {
                if (!bno085_ptr->begin_SPI(&hspi3, DOF_CS_GPIO_Port, DOF_CS_Pin,
                                          DOF_INT_GPIO_Port, DOF_INT_Pin,
                                          DOF_NRST_GPIO_Port, DOF_NRST_Pin)) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] BNO085 initialization failed");
                } else {
                    LOG_INFO("[DRV_MGR] BNO085 initialized");
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] BNO085 creation failed");
            }
            break;
        }

        case DriverId::BQ27441: {
            // Backup battery gauge on I2C2 (address 0x55)
            extern I2C_HandleTypeDef hi2c2;
            bq27441_ptr = new BQ27441(&hi2c2);
            if (bq27441_ptr && bq27441_ptr->init()) {
                LOG_INFO("[DRV_MGR] BQ27441 fuel gauge initialized");
            } else {
                LOG_WARN("[DRV_MGR] BQ27441 initialization failed - sensor may not be present");
                if (bq27441_ptr) {
                    delete bq27441_ptr;
                    bq27441_ptr = nullptr;
                }
                // Don't fail initialization - BQ27441 is optional
                success = true;
            }
            break;
        }

        case DriverId::MAX98357: {
            // Audio amplifier with GPIO control
            MAX98357::Config audio_config = {
                .en_port = AUDIO_EN_GPIO_Port,
                .en_pin = AUDIO_EN_Pin,
                .mode_port = AUDIO_MODE_GPIO_Port,
                .mode_pin = AUDIO_MODE_Pin
            };
            max98357_ptr = new MAX98357(audio_config);
            if (max98357_ptr) {
                max98357_ptr->init();  // init() returns void
                LOG_INFO("[DRV_MGR] MAX98357 audio amplifier initialized");
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] MAX98357 creation failed");
            }

            // Get and initialize AudioManager singleton
            extern AudioManager* getAudioManagerInstance(void);
            audio_manager_ptr = getAudioManagerInstance();
            if (audio_manager_ptr && audio_manager_ptr->init()) {
                LOG_INFO("[DRV_MGR] AudioManager initialized");
            } else {
                LOG_WARN("[DRV_MGR] AudioManager not available or initialization failed");
                audio_manager_ptr = nullptr;
            }
            break;
        }

        case DriverId::TPS2115: {
            // Power multiplexer for VPWM voltage selection (3.3V or 5V for ESC PWM)
            tps2115_ptr = new TPS2115(VPWM_STAT_GPIO_Port, VPWM_STAT_Pin,
                                     VPWM_D1_GPIO_Port, VPWM_D1_Pin);
            if (tps2115_ptr) {
                // Load PWM voltage setting from NVRAM user config
                // pwm_voltage: 0 = 5V (default), 1 = 3.3V
                uint8_t pwm_voltage = 0;  // Default to 5V
                if (user_config_ptr) {
                    pwm_voltage = userconfig_get_pwm_voltage(user_config_ptr);
                    if (pwm_voltage > 1) pwm_voltage = 0;  // Validate value
                }

                // Apply voltage setting: true=3.3V (pwm_voltage==1), false=5V (pwm_voltage==0)
                bool voltage_3v3 = (pwm_voltage == 1);
                tps2115_ptr->setD1(voltage_3v3);

                LOG_INFO("[DRV_MGR] TPS2115 VPWM initialized to %s (from NVRAM)",
                        voltage_3v3 ? "3.3V" : "5V");
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] TPS2115 creation failed");
            }
            break;
        }

        case DriverId::TPS2121: {
            // Power switch control - monitors VSEL pin for power source selection
            tps2121_ptr = new TPS2121(VSEL_OUT_GPIO_Port, VSEL_OUT_Pin);
            if (!tps2121_ptr) {
                success = false;
                LOG_ERROR("[DRV_MGR] TPS2121 creation failed");
            } else {
                LOG_INFO("[DRV_MGR] TPS2121 power switch initialized");
            }
            break;
        }

        case DriverId::TXS0108: {
            // Level shifter for PWM voltage translation (3.3V STM32 to VPWM via TPS2115)
            // Translates PWM[1..8] from 3.3V logic to VPWM voltage (3.3V or 5V)
            // VPWM voltage selected via TPS2115 D1 pin (user configurable)
            txs0108_ptr = new TXS0108(VPWM_OE_GPIO_Port, VPWM_OE_Pin);
            if (txs0108_ptr) {
                // Initialize with OE pin disabled (safe state)
                // Caller will enable OE when ready to output PWM signals
                txs0108_ptr->disable();
                LOG_INFO("[DRV_MGR] TXS0108 level shifter initialized (OE disabled)");
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] TXS0108 creation failed");
            }
            break;
        }

        case DriverId::PPM_DECODER: {
            // PPM decoder for RC receiver input on TIM2_CH1
            extern TIM_HandleTypeDef htim2;
            ppm_decoder_ptr = new PPMDecoder(&htim2, PPM_OE_GPIO_Port, PPM_OE_Pin);
            if (ppm_decoder_ptr) {
                if (!ppm_decoder_ptr->init()) {
                    success = false;
                    LOG_ERROR("[DRV_MGR] PPMDecoder initialization failed");
                    delete ppm_decoder_ptr;
                    ppm_decoder_ptr = nullptr;
                } else {
                    LOG_INFO("[DRV_MGR] PPMDecoder initialized");
                }
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] PPMDecoder creation failed");
            }
            break;
        }

        case DriverId::RADIO_RECEIVER: {
            // Radio receiver output enable control for PPM/SBUS routing
            radio_receiver_ptr = new RadioReceiver(PPM_OE_GPIO_Port, PPM_OE_Pin,
                                                  SBUS_OE_GPIO_Port, SBUS_OE_Pin);
            if (radio_receiver_ptr) {
                // Enable PPM output by default
                radio_receiver_ptr->enablePPM(true);
                LOG_INFO("[DRV_MGR] RadioReceiver initialized");
            } else {
                success = false;
                LOG_ERROR("[DRV_MGR] RadioReceiver creation failed");
            }
            break;
        }

#if DM_OPT_TOF_INTEGRATION
        case DriverId::TOF_PROXIMITY: {
            if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
                LOG_INFO("[DRV_MGR] ToF Proximity deferred until scheduler starts");
                setDriverState(id, DriverState::INITIALIZING);
                return true;
            }

            auto& tof_mgr = TofProximityManager::getInstance();
            if (!tof_mgr.init()) {
                success = false;
                LOG_ERROR("[DRV_MGR] ToF Proximity initialization failed");
            } else {
                tof_proximity_ptr = &tof_mgr;
                LOG_INFO("[DRV_MGR] ToF Proximity initialized");
            }
            break;
        }
#else
        case DriverId::VL53L5CX_1:
        case DriverId::VL53L5CX_2:
        case DriverId::VL53L5CX_3:
        case DriverId::VL53L5CX_4:
        case DriverId::VL53L5CX_5:
        case DriverId::VL53L5CX_6: {
            // TODO:
            LOG_WARN("[DRV_MGR] ToF sensor initialization disabled");
            success = true;
            break;
        }
#endif

        case DriverId::BOOT_FUSE: {
            // Boot fuse system for bootloader/application selection
            // Currently handled via dedicated boot_fuse driver
            // TODO: Implement proper BootFuse class and integrate with DriverManager
            LOG_WARN("[DRV_MGR] BOOT_FUSE: deferred (use dedicated boot_fuse API for now)");
            success = true;  // Optional
            break;
        }

        case DriverId::WIFI: {
            // WiFi module communication
            // Currently a stub - implementation depends on specific WiFi module
            LOG_WARN("[DRV_MGR] WIFI: deferred (implementation pending)");
            success = true;  // Optional
            break;
        }

        // Stub for other drivers
        default: {
            // Non-critical drivers can fail gracefully
            if (meta->required) {
                success = false;
            } else {
                success = true;  // Optional drivers default to success
            }
            break;
        }
    }

    uint32_t elapsed = HAL_GetTick() - start_tick;

    if (success) {
        setDriverState(id, DriverState::READY);
        LOG_INFO("[DRV_MGR] %s initialized (%lu ms)", meta->name, elapsed);
    } else {
        setDriverState(id, DriverState::ERROR);
        LOG_ERROR("[DRV_MGR] %s initialization failed", meta->name);
    }

    return success;
}

bool DriverManager::checkDependencies(DriverId id) {
    const auto* meta = getDriverMetadata(id);
    if (!meta) {
        return false;
    }

    for (uint8_t i = 0; i < meta->dependency_count; i++) {
        auto dep_id = meta->dependencies[i];
        if (dep_id >= DriverId::COUNT) {
            continue;
        }

        if (states[static_cast<size_t>(dep_id)] != DriverState::READY) {
            return false;
        }
    }

    return true;
}

void DriverManager::setDriverState(DriverId id, DriverState state) {
    if (id >= DriverId::COUNT) {
        return;
    }

    states[static_cast<size_t>(id)] = state;
    DriverHealthMonitor::setState(id, state);
}

// === Accessors: Singleton Drivers ===

RtcDriver* DriverManager::getRTC(void) {
    return rtc_ptr;
}

BatteryMonitor* DriverManager::getBatteryMonitor(void) {
    return battery_monitor_ptr;
}

NVRAM* DriverManager::getNVRAM(void) {
    return nvram_ptr;
}

SpiFlash* DriverManager::getSpiFlash(void) {
    return spi_flash_ptr;
}

QspiFlash* DriverManager::getQspiFlash(void) {
    return qspi_flash_ptr;
}

userconfig_t* DriverManager::getUserConfig(void) {
    return user_config_ptr;
}

// === Accessors: Environmental Sensors ===

SHT4x* DriverManager::getSHT4x(void) {
    return sht4x_ptr;
}

BMP581* DriverManager::getBMP581(void) {
    return bmp581_ptr;
}

// === Accessors: Motion ===

BNO085* DriverManager::getIMU(void) {
    return bno085_ptr;
}

BNO085* DriverManager::getBNO085(void) {
    return bno085_ptr;
}

// === Accessors: Power Management ===

BQ27441* DriverManager::getBackupBattery(void) {
    return bq27441_ptr;
}

TPS2115* DriverManager::getPowerMux(void) {
    return tps2115_ptr;
}

TPS2121* DriverManager::getPowerSwitch(void) {
    return tps2121_ptr;
}

#if DM_OPT_TOF_INTEGRATION
TofProximityManager* DriverManager::getTofProximity(void) {
    if (states[static_cast<size_t>(DriverId::TOF_PROXIMITY)] != DriverState::READY) {
        return nullptr;
    }
    return tof_proximity_ptr;
}
#else
// === Accessors: Multi-Instance ===
VL53L5CX* DriverManager::getToFSensor(uint8_t index) {
    if (index >= 6) {
        return nullptr;
    }
    return tof_sensors[index];
}
#endif

TXS0108* DriverManager::getLevelShifter(void) {
    return txs0108_ptr;
}

// === Accessors: Display & Audio ===

ST7789* DriverManager::getDisplay(void) {
    return st7789_ptr;
}

MAX98357* DriverManager::getAudio(void) {
    return max98357_ptr;
}

AudioManager* DriverManager::getAudioManager(void) {
    return audio_manager_ptr;
}

// === Accessors: Communication ===

PPMDecoder* DriverManager::getPPMDecoder(void) {
    return ppm_decoder_ptr;
}

RadioReceiver* DriverManager::getRadioReceiver(void) {
    return radio_receiver_ptr;
}

// === Status Queries ===

DriverState DriverManager::getDriverState(DriverId id) {
    if (id >= DriverId::COUNT) {
        return DriverState::UNINITIALIZED;
    }
    return states[static_cast<size_t>(id)];
}

const DriverHealth& DriverManager::getHealth(DriverId id) {
    return DriverHealthMonitor::getHealth(id);
}
