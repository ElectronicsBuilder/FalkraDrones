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
 * @file    TofProximityManager.cpp
 * @brief   VL53L5CX Multi-Sensor Proximity Manager Implementation
 */

#include "TofProximityManager.hpp"
#include "tof_interrupts.h"
#include "tof_perf.h"
#include "gpio_interrupts.h"
#include "app_tof.hpp"
#include "status.hpp"
#include "log.hpp"
#include "stm32f7xx_hal.h"
#include <cstring>

extern uint16_t proximity_devices[MAX_TOF_SENSORS][MAX_ZONES_PER_SENSOR];

static const char* SENSOR_NAMES[MAX_TOF_SENSORS] = {
    "TOP", "BOTTOM", "FRONT", "BACK", "LEFT", "RIGHT"
};

static uint16_t tof_internal_to_cm(uint16_t distance)
{
#if TOF_OPT_MM_RESOLUTION
    return distance / 10U;
#else
    return distance;
#endif
}

static uint16_t tof_internal_to_mm(uint16_t distance)
{
#if TOF_OPT_MM_RESOLUTION
    return distance;
#else
    uint32_t mm = (uint32_t)distance * 10U;
    return (mm > UINT16_MAX) ? UINT16_MAX : (uint16_t)mm;
#endif
}

TofProximityManager& TofProximityManager::getInstance() {
    static TofProximityManager instance;
    return instance;
}

bool TofProximityManager::init(uint16_t detectCm, uint16_t minCm) {
    if (_initialized) {
        LOG_WARN("[TOF_MGR] Already initialized");
        return true;
    }

    LOG_INFO("[TOF_MGR] Initializing TofProximityManager...");

    _mutex = osMutexNew(nullptr);
    if (_mutex == nullptr) {
        LOG_ERROR("[TOF_MGR] Failed to create mutex");
        return false;
    }

    _dataReadyFlags = osEventFlagsNew(nullptr);
    if (_dataReadyFlags == nullptr) {
        LOG_ERROR("[TOF_MGR] Failed to create event flags");
        osMutexDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    _newDataFlags = osEventFlagsNew(nullptr);
    if (_newDataFlags == nullptr) {
        LOG_ERROR("[TOF_MGR] Failed to create new-data event flags");
        osEventFlagsDelete(_dataReadyFlags);
        _dataReadyFlags = nullptr;
        osMutexDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
        TofSensorId id = static_cast<TofSensorId>(i);
        _sensors[i].configure(id, SENSOR_NAMES[i]);
        _sensors[i].setThreshold(detectCm, minCm);

        strncpy(g_status.tofDeviceName[i], SENSOR_NAMES[i], sizeof(g_status.tofDeviceName[i]) - 1);
        g_status.tofDeviceName[i][sizeof(g_status.tofDeviceName[i]) - 1] = '\0';
        g_status.tofDeviceLoc[i] = i;
        g_status.tofSensorDistance[i] = 0;
    }

    std::memset(&_currentSnapshot, 0, sizeof(_currentSnapshot));

    MX_TOF_Init();
    LOG_INFO("[TOF_MGR] Hardware initialized (MX_TOF_Init)");

    syncSensorPresence();

    // Register GPIO interrupt callbacks
    GPIO_Interrupts_Init();
    TOF_Interrupts_Register();
    LOG_INFO("[TOF_MGR] Interrupt callbacks registered");

    // Enable interrupt mode by default for high-speed obstacle avoidance
    _useInterrupts = true;
    LOG_INFO("[TOF_MGR] Interrupt mode ENABLED (high-speed mode)");

    _initialized = true;
    LOG_INFO("[TOF_MGR] Initialization complete");

    return true;
}

void TofProximityManager::startRanging() {
    if (!_initialized) {
        LOG_ERROR("[TOF_MGR] Cannot start ranging - not initialized");
        return;
    }

    if (_ranging) {
        LOG_WARN("[TOF_MGR] Ranging already started");
        return;
    }

    MX_TOF_Start();
    _ranging = true;
    LOG_INFO("[TOF_MGR] Ranging started");
}

void TofProximityManager::stopRanging() {
    if (!_ranging) {
        return;
    }

    _ranging = false;
    LOG_INFO("[TOF_MGR] Ranging stopped");
}

bool TofProximityManager::isRanging() const {
    return _ranging;
}

void TofProximityManager::taskEntry(void* arg) {
    (void)arg;
    TofProximityManager::getInstance().processTask();
}

void TofProximityManager::processTask() {
    LOG_INFO("[TOF_MGR] Task started");

    while (1) {
        if (_useInterrupts && _dataReadyFlags != nullptr) {
            uint32_t flags = osEventFlagsWait(
                _dataReadyFlags,
                EVENT_FLAG_ALL_SENSORS,
                osFlagsWaitAny,
                EVENT_WAIT_TIMEOUT_MS
            );

            if (!(flags & osFlagsError)) {
                // Process only sensors that signaled data ready
                for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
                    if (flags & (1 << i)) {
                        readSensorOnInterrupt(i);
                    }
                }
                updateSnapshot();
            }
            // On timeout, just continue waiting - don't block with polling
        } else {
            // Polling mode
            pollDistances();
            updateAllDetectionStates();
            updateSnapshot();
            osDelay(10);
        }
    }
}

void TofProximityManager::process() {
    if (!_initialized || !_ranging) {
        return;
    }

    MX_TOF_Process();
    pollDistances();
    updateAllDetectionStates();
    updateSnapshot();
} 

void TofProximityManager::pollDistances() {
    Return_distance();

    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
        if (_sensors[i].isPresent()) {
            _sensors[i].updateZoneData(proximity_devices[i], MAX_ZONES_PER_SENSOR);
        }
    }
}

void TofProximityManager::readSensorOnInterrupt(uint8_t sensorIndex) {
    if (sensorIndex >= MAX_TOF_SENSORS) {
        return;
    }

    if (!_sensors[sensorIndex].isPresent()) {
        return;
    }

    // Read distance from the specific sensor that signaled data ready
    if (MX_TOF_ReadSensorDistance(sensorIndex)) {
        // Update sensor with new zone data
        _sensors[sensorIndex].updateZoneData(proximity_devices[sensorIndex], MAX_ZONES_PER_SENSOR);

        // Update detection state for this sensor
        TofDetectionFlag prevFlag = _sensors[sensorIndex].getDetectionFlag();
        _sensors[sensorIndex].updateDetectionState();
        TofDetectionFlag newFlag = _sensors[sensorIndex].getDetectionFlag();

        // Update global status
        g_status.tofDetectingFlag[sensorIndex] = static_cast<uint8_t>(newFlag);
        g_status.tofSensorDistance[sensorIndex] =
            tof_internal_to_cm(_sensors[sensorIndex].getZoneDistance(0));

        // Log detection state changes
        if (newFlag != prevFlag) {
            const char* flagStr =
                (newFlag == TofDetectionFlag::Set) ? "SET" :
                (newFlag == TofDetectionFlag::Reset) ? "RESET" : "INIT";

            LOG_INFO("[TOF_MGR] %s | Zone0=%ucm | Flag=%s | Conf=%u (IRQ)",
                _sensors[sensorIndex].getName(),
                tof_internal_to_cm(_sensors[sensorIndex].getZoneDistance(0)),
                flagStr,
                _sensors[sensorIndex].getConfidence());
        }
    }
}

void TofProximityManager::setLoggingEnabled(bool enable, uint32_t period_ms) {
    _logEnabled = enable;
    _logPeriodMs = period_ms;
    LOG_INFO("[TOF_MGR] Fast logging %s (period=%lums)", enable ? "ON" : "OFF", period_ms);
}

void TofProximityManager::enableInterruptMode(bool enable) {
    _useInterrupts = enable;
    LOG_INFO("[TOF_MGR] Interrupt mode %s", enable ? "ENABLED" : "DISABLED");
}

void TofProximityManager::setHighSpeedMode(bool enable) {
    if (enable) {
        // Configure all sensors for fast response (aggressive detection)
        for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
            _sensors[i].setConfidenceParams(
                TOF_FAST_THRESHOLD_ZONES,   // 2 of 16 zones
                TOF_FAST_MAX_CONFIDENCE     // 3 frames max
            );
            _sensors[i].setDetectConfidenceThreshold(TOF_FAST_DETECT_CONFIDENCE);  // Detect after 1 frame
        }
        _useInterrupts = true;
        LOG_INFO("[TOF_MGR] High-speed mode ENABLED (threshZones=%u, maxConf=%u, detectConf=%u)",
            TOF_FAST_THRESHOLD_ZONES, TOF_FAST_MAX_CONFIDENCE, TOF_FAST_DETECT_CONFIDENCE);
    } else {
        // Restore conservative defaults
        for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
            _sensors[i].setConfidenceParams(
                TOF_DEFAULT_THRESHOLD_ZONES,
                TOF_DEFAULT_MAX_CONFIDENCE
            );
            _sensors[i].setDetectConfidenceThreshold(2);  // Default: require 2 frames
        }
        LOG_INFO("[TOF_MGR] High-speed mode DISABLED (conservative defaults)");
    }
}

void TofProximityManager::updateAllDetectionStates() {
    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
        if (!_sensors[i].isActive()) {
            continue;
        }

        TofDetectionFlag prevFlag = _sensors[i].getDetectionFlag();
        _sensors[i].updateDetectionState();
        TofDetectionFlag newFlag = _sensors[i].getDetectionFlag();

        g_status.tofDetectingFlag[i] = static_cast<uint8_t>(newFlag);
        g_status.tofSensorDistance[i] =
            tof_internal_to_cm(_sensors[i].getZoneDistance(0));

        if (newFlag != prevFlag) {
            const char* flagStr =
                (newFlag == TofDetectionFlag::Set) ? "SET" :
                (newFlag == TofDetectionFlag::Reset) ? "RESET" : "INIT";

            LOG_INFO("[TOF_MGR] %s | Zone0=%ucm | Flag=%s | Conf=%u",
                _sensors[i].getName(),
                tof_internal_to_cm(_sensors[i].getZoneDistance(0)),
                flagStr,
                _sensors[i].getConfidence());
        }
    }
}

void TofProximityManager::updateSnapshot() {
    if (_mutex == nullptr) {
        return;
    }

    if (osMutexAcquire(_mutex, MUTEX_TIMEOUT_MS) != osOK) {
        return;
    }

    _currentSnapshot.timestamp_ms = HAL_GetTick();
    uint8_t activeCount = 0;
    static uint32_t last_log_ms = 0;
    static constexpr uint32_t SNAPSHOT_LOG_PERIOD_MS = 10;

    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {

        if (_sensors[i].isPresent()) {
            _currentSnapshot.distance_mm[i] =
                tof_internal_to_mm(_sensors[i].getMinDistance());

            _currentSnapshot.sensor_valid[i] = 1;

            if (_currentSnapshot.sensor_valid[i]) {
                activeCount++;
            }
        } else {
            _currentSnapshot.distance_mm[i] = 0;
            _currentSnapshot.sensor_valid[i] = 0;
        }
    }

    _currentSnapshot.active_sensor_count = activeCount;
    uint32_t now_ms = _currentSnapshot.timestamp_ms;
    if ((now_ms - last_log_ms) >= SNAPSHOT_LOG_PERIOD_MS) {
        last_log_ms = now_ms;
        // LOG_INFO("[TOF_MGR] Active sensors: %u/%u Distances:%u, %u, %u, %u, %u, %u",
        //     activeCount, MAX_TOF_SENSORS,
        //     _currentSnapshot.distance_mm[0], _currentSnapshot.distance_mm[1],
        //     _currentSnapshot.distance_mm[2], _currentSnapshot.distance_mm[3],
        //     _currentSnapshot.distance_mm[4], _currentSnapshot.distance_mm[5]);
    }
    osMutexRelease(_mutex);
}

bool TofProximityManager::getSnapshot(TofDistanceSnapshot* snapshot) {
    if (snapshot == nullptr || _mutex == nullptr) {
        return false;
    }

    if (osMutexAcquire(_mutex, MUTEX_TIMEOUT_MS) != osOK) {
        return false;
    }

    *snapshot = _currentSnapshot;
    osMutexRelease(_mutex);

    return true;
}

bool TofProximityManager::getSensorDistance(TofSensorId id, uint16_t* mm) {
    if (mm == nullptr || static_cast<uint8_t>(id) >= MAX_TOF_SENSORS || _mutex == nullptr) {
        return false;
    }

    if (osMutexAcquire(_mutex, MUTEX_TIMEOUT_MS) != osOK) {
        return false;
    }

    uint8_t idx = static_cast<uint8_t>(id);
    *mm = _currentSnapshot.distance_mm[idx];
    bool valid = (_currentSnapshot.sensor_valid[idx] == 1);

    osMutexRelease(_mutex);

    return valid;
}

TofSensor& TofProximityManager::getSensor(TofSensorId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= MAX_TOF_SENSORS) {
        idx = 0;
    }
    return _sensors[idx];
}

const TofSensor& TofProximityManager::getSensor(TofSensorId id) const {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= MAX_TOF_SENSORS) {
        idx = 0;
    }
    return _sensors[idx];
}

bool TofProximityManager::isObstacleDetected(TofSensorId id) const {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= MAX_TOF_SENSORS) {
        return false;
    }
    return _sensors[idx].isDetecting();
}

bool TofProximityManager::isAnyObstacleDetected() const {
    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
        if (_sensors[i].isDetecting()) {
            return true;
        }
    }
    return false;
}

void TofProximityManager::onDataReady(TofSensorId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= MAX_TOF_SENSORS) {
        return;
    }

#if TOF_OPT_DIRECT_NOTIFY
    if (_dataTaskHandle != nullptr) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(
            _dataTaskHandle,
            (1u << idx),
            eSetBits,
            &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
        return;
    }
#endif

    if (_dataReadyFlags == nullptr) {
        return;
    }

    osEventFlagsSet(_dataReadyFlags, (1 << idx));
}

void TofProximityManager::syncSensorPresence() {
    LOG_INFO("[TOF_MGR] Syncing sensor presence...");

    MX_TOF_SyncSensorStatus();

    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
        bool present = (MX_TOF_IsSensorPresent(i) == 1);
        _sensors[i].setPresent(present);

        g_status.tofDeviceStatus[i] = present ? 1 : 0;

        if (present) {
            activeCount++;
            LOG_INFO("[TOF_MGR] %s: PRESENT", _sensors[i].getName());
        } else {
            LOG_INFO("[TOF_MGR] %s: NOT PRESENT", _sensors[i].getName());
        }
    }

    LOG_INFO("[TOF_MGR] Active sensors: %u/%u", activeCount, MAX_TOF_SENSORS);
}

// ---------------------------------------------------------------------------
// Two-task interrupt-driven architecture
// ---------------------------------------------------------------------------

void TofProximityManager::dataTaskEntry(void* arg) {
    (void)arg;
    TofProximityManager::getInstance().dataTask();
}

void TofProximityManager::detectionTaskEntry(void* arg) {
    (void)arg;
    TofProximityManager::getInstance().detectionTask();
}

/**
 * @brief High-priority data acquisition loop.
 *
 * Waits on _dataReadyFlags set by the GPIO ISR (sensor INT pin).
 * For each flagged sensor: reads I2C distance, updates zone buffer,
 * then signals _newDataFlags to wake detectionTask().
 *
 * Does NOT run detection logic — keeps I2C reads responsive.
 */
void TofProximityManager::dataTask() {
    LOG_INFO("[TOF_DATA] Data task started");

#if TOF_OPT_DIRECT_NOTIFY
    _dataTaskHandle = xTaskGetCurrentTaskHandle();
#endif

    while (1) {
#if TOF_OPT_DIRECT_NOTIFY
        uint32_t flags = 0;
        if (xTaskNotifyWait(
                0,
                EVENT_FLAG_ALL_SENSORS,
                &flags,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }
#else
        uint32_t flags = osEventFlagsWait(
            _dataReadyFlags,
            EVENT_FLAG_ALL_SENSORS,
            osFlagsWaitAny,
            EVENT_WAIT_TIMEOUT_MS
        );

        if (flags & osFlagsError) {
            continue;
        }
#endif

        for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
            if (!(flags & (1u << i))) {
                continue;
            }

            if (!_sensors[i].isPresent()) {
                continue;
            }

            if (TOF_PERF_READ_SENSOR(i, MX_TOF_ReadSensorDistance)) {
                _sensors[i].updateZoneData(proximity_devices[i], MAX_ZONES_PER_SENSOR);
                osEventFlagsSet(_newDataFlags, (1u << i));
#if TOF_OPT_DIRECT_NOTIFY
                taskYIELD();
#endif
            }
        }
    }
}

/**
 * @brief Normal-priority detection loop.
 *
 * Waits on _newDataFlags set by dataTask().
 * For each flagged sensor: runs confidence-based detection, updates
 * global status, then refreshes the mutex-protected snapshot.
 */
void TofProximityManager::detectionTask() {
    LOG_INFO("[TOF_DET] Detection task started");

    while (1) {
        uint32_t flags = osEventFlagsWait(
            _newDataFlags,
            EVENT_FLAG_ALL_SENSORS,
            osFlagsWaitAny,
            20  // 20ms timeout — at 60Hz, each sensor updates every ~17ms
        );

        if (flags & osFlagsError) {
            continue;
        }

#if TOF_PERF_MONITOR
        uint32_t presentMask = 0;
        for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
            if (_sensors[i].isPresent()) {
                presentMask |= (1u << i);
            }
        }
#endif

        for (uint8_t i = 0; i < MAX_TOF_SENSORS; i++) {
            if (!(flags & (1u << i))) {
                continue;
            }

            if (!_sensors[i].isPresent()) {
                continue;
            }

            TofDetectionFlag prevFlag = _sensors[i].getDetectionFlag();
            _sensors[i].updateDetectionState();
            TofDetectionFlag newFlag = _sensors[i].getDetectionFlag();

            g_status.tofDetectingFlag[i] = static_cast<uint8_t>(newFlag);
            g_status.tofSensorDistance[i] =
                tof_internal_to_cm(_sensors[i].getZoneDistance(0));

            if (newFlag != prevFlag) {
                const char* flagStr =
                    (newFlag == TofDetectionFlag::Set)   ? "SET"   :
                    (newFlag == TofDetectionFlag::Reset) ? "RESET" : "INIT";

                LOG_INFO("[TOF_DET] %s | Zone0=%ucm | Flag=%s | Conf=%u",
                    _sensors[i].getName(),
                    tof_internal_to_cm(_sensors[i].getZoneDistance(0)),
                    flagStr,
                    _sensors[i].getConfidence());
            }
        }

        if (_logEnabled) {
            static uint32_t lastLogMs = 0;
            uint32_t now = HAL_GetTick();
            if ((now - lastLogMs) >= _logPeriodMs) {
                lastLogMs = now;
                LOG_INFO("[TOF] TOP:%4umm BOT:%4umm FWD:%4umm BCK:%4umm LFT:%4umm RGT:%4umm | t=%lums",
                    tof_internal_to_mm(_sensors[0].getMinDistance()),
                    tof_internal_to_mm(_sensors[1].getMinDistance()),
                    tof_internal_to_mm(_sensors[2].getMinDistance()),
                    tof_internal_to_mm(_sensors[3].getMinDistance()),
                    tof_internal_to_mm(_sensors[4].getMinDistance()),
                    tof_internal_to_mm(_sensors[5].getMinDistance()),
                    now);
            }
        }

        updateSnapshot();
        TOF_PERF_SNAPSHOT_AND_REPORT(flags, presentMask);
    }
}

extern "C" void TofProximityManager_OnDataReady(uint8_t sensorIndex) {
    if (sensorIndex >= MAX_TOF_SENSORS) {
        return;
    }
    TofProximityManager::getInstance().onDataReady(static_cast<TofSensorId>(sensorIndex));
}
