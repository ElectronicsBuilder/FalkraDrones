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
 * @file    TofProximityManager.hpp
 * @brief   VL53L5CX Multi-Sensor Proximity Manager
 * @details Singleton class managing all 6 VL53L5CX ToF sensors for drone
 *          collision avoidance. Provides interrupt-driven or polling-based
 *          ranging with thread-safe snapshot access.
 *
 * Features:
 * - Manages 6 sensors (Top, Bottom, Front, Back, Left, Right)
 * - Interrupt-based data ready notification for low latency
 * - FreeRTOS event flags for efficient task synchronization
 * - Mutex-protected snapshot access for thread safety
 * - Confidence-based obstacle detection per sensor
 *
 * Usage:
 * @code
 * auto& mgr = TofProximityManager::getInstance();
 * mgr.init(15, 5);  // 15cm detect, 5cm min threshold
 * mgr.startRanging();
 *
 * // In task loop or on demand:
 * TofDistanceSnapshot snap;
 * if (mgr.getSnapshot(&snap)) {
 *     if (snap.distance_mm[TofSensorId::Front] < 200) {
 *         // Obstacle ahead!
 *     }
 * }
 * @endcode
 */

#ifndef TOF_PROXIMITY_MANAGER_HPP
#define TOF_PROXIMITY_MANAGER_HPP

#include "TofSensor.hpp"
#include "tof_speed_opts.h"
#include "cmsis_os.h"
#if TOF_OPT_DIRECT_NOTIFY
#include "FreeRTOS.h"
#include "task.h"
#endif
#include <cstdint>

struct TofDistanceSnapshot {
    uint16_t distance_mm[MAX_TOF_SENSORS];
    uint8_t sensor_valid[MAX_TOF_SENSORS];
    uint32_t timestamp_ms;
    uint8_t active_sensor_count;
};

class TofProximityManager {
public:
    static TofProximityManager& getInstance();

    TofProximityManager(const TofProximityManager&) = delete;
    TofProximityManager& operator=(const TofProximityManager&) = delete;

    bool init(uint16_t detectCm = TOF_DEFAULT_DETECT_DISTANCE_CM,
              uint16_t minCm = TOF_DEFAULT_MIN_DISTANCE_CM);

    void startRanging();
    void stopRanging();
    bool isRanging() const;

    static void taskEntry(void* arg);

    void process();

    /**
     * @brief Main task loop - waits on interrupts or polls based on mode
     * @note This function never returns. Call from FreeRTOS task.
     */
    void processTask();

    /**
     * @brief Data acquisition task loop - responds to sensor INT pins, reads I2C data.
     * @note High-priority task. Never returns. Call from FreeRTOS task.
     */
    void dataTask();

    /**
     * @brief Detection task loop - runs confidence algorithm, updates snapshot.
     * @note Normal-priority task. Never returns. Call from FreeRTOS task.
     */
    void detectionTask();

    static void dataTaskEntry(void* arg);
    static void detectionTaskEntry(void* arg);

    bool getSnapshot(TofDistanceSnapshot* snapshot);
    bool getSensorDistance(TofSensorId id, uint16_t* mm);

    TofSensor& getSensor(TofSensorId id);
    const TofSensor& getSensor(TofSensorId id) const;

    bool isObstacleDetected(TofSensorId id) const;
    bool isAnyObstacleDetected() const;

    void onDataReady(TofSensorId id);

    void syncSensorPresence();

    /**
     * @brief Enable or disable interrupt-driven mode
     * @param enable true for interrupt mode, false for polling mode
     *
     * When enabled, the task waits on event flags set by EXTI callbacks.
     * When disabled, the task polls sensors at fixed intervals.
     */
    void enableInterruptMode(bool enable);

    /**
     * @brief Enable or disable fast distance logging to UART
     * @param enable    true = print distances on every detection cycle
     * @param period_ms minimum ms between prints (0 = every cycle, ~17ms at 60Hz)
     */
    void setLoggingEnabled(bool enable, uint32_t period_ms = 0);

    /**
     * @brief Enable or disable high-speed mode for obstacle avoidance
     * @param enable true for high-speed (aggressive) detection, false for conservative
     *
     * High-speed mode uses:
     * - Lower threshold zones (2 instead of 4)
     * - Lower max confidence (3 instead of 5)
     * - Interrupt mode enabled
     */
    void setHighSpeedMode(bool enable);

    static constexpr uint32_t EVENT_FLAG_SENSOR_0 = (1 << 0);
    static constexpr uint32_t EVENT_FLAG_SENSOR_1 = (1 << 1);
    static constexpr uint32_t EVENT_FLAG_SENSOR_2 = (1 << 2);
    static constexpr uint32_t EVENT_FLAG_SENSOR_3 = (1 << 3);
    static constexpr uint32_t EVENT_FLAG_SENSOR_4 = (1 << 4);
    static constexpr uint32_t EVENT_FLAG_SENSOR_5 = (1 << 5);
    static constexpr uint32_t EVENT_FLAG_ALL_SENSORS = 0x3F;

private:
    TofProximityManager() = default;

    void pollDistances();
    void updateAllDetectionStates();
    void updateSnapshot();
    void readSensorOnInterrupt(uint8_t sensorIndex);

    TofSensor _sensors[MAX_TOF_SENSORS];
    osMutexId_t _mutex = nullptr;
    osEventFlagsId_t _dataReadyFlags = nullptr;   // Set by GPIO ISR when sensor INT fires
    osEventFlagsId_t _newDataFlags = nullptr;      // Set by dataTask when zone data is ready
#if TOF_OPT_DIRECT_NOTIFY
    TaskHandle_t _dataTaskHandle = nullptr;
#endif

    TofDistanceSnapshot _currentSnapshot = {};

    bool _initialized = false;
    bool _ranging = false;
    bool _useInterrupts = false;
    volatile bool _logEnabled = false;
    volatile uint32_t _logPeriodMs = 0;

    static constexpr uint32_t MUTEX_TIMEOUT_MS = 5;
    static constexpr uint32_t EVENT_WAIT_TIMEOUT_MS = 5;  // Reduced for high-speed mode
    static constexpr uint16_t CM_TO_MM = 10;
};

#ifdef __cplusplus
extern "C" {
#endif

void TofProximityManager_OnDataReady(uint8_t sensorIndex);

#ifdef __cplusplus
}
#endif

#endif // TOF_PROXIMITY_MANAGER_HPP
