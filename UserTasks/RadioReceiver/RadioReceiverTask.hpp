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
 * @file    radio_receiver_task.hpp
 * @brief   Real-Time Radio Receiver Task for Flight Control
 * @details High-priority task for responsive RC input handling without waiting
 *          for status polling cycles. Provides immediate access to pilot control
 *          inputs for flight controller without blocking on telemetry updates.
 *
 * Design Principles:
 * - Runs at higher priority than status_task for responsive control
 * - Direct access to PPMDecoder without status synchronization
 * - Independent RC control structure separate from telemetry
 * - Graceful handling of signal loss and failsafe conditions
 * - Zero blocking - never waits on telemetry or other tasks
 */

#ifndef __RADIORECEIVERTASK_HPP
#define __RADIORECEIVERTASK_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Radio control input snapshot for flight control
 *
 * Fast-accessible copy of current RC state for flight controller
 * Updated at interrupt rate (microseconds) not polling rate (milliseconds)
 */
typedef struct {
    uint16_t roll_min;
    uint16_t roll_max;
    uint16_t pitch_min;
    uint16_t pitch_max;
    uint16_t throttle_min;
    uint16_t throttle_max;
    uint16_t disarm_min;
    uint16_t disarm_max;
} RCFailsafeConfig;

typedef struct {
    // Raw channel pulse widths (microseconds)
    uint16_t channels[8];       // 1000-2000µs range
    uint8_t channel_count;      // Number of valid channels detected

    // Normalized control inputs for primary axes
    float roll;                 // Channel 0: -1.0 to +1.0 (left/right)
    float pitch;                // Channel 1: -1.0 to +1.0 (back/forward)
    float throttle;             // Channel 2: -1.0 to +1.0 (down/up)
    float yaw;                  // Channel 3: -1.0 to +1.0 (left/right)

    // Auxiliary channels (switches, knobs)
    float aux[4];               // Channels 4-7 normalized

    // Signal quality metrics
    bool signal_valid;          // True if receiving valid PPM frames
    uint32_t time_since_frame;  // Milliseconds since last valid frame
    uint32_t frame_count;       // Total frames received (for diagnostics)
    uint32_t frame_errors;      // Lost/corrupted frames (for diagnostics)
} RadioControlInput;

/**
 * @brief Initialize radio receiver task infrastructure
 * @return true if initialization successful
 *
 * Call before creating the radio_receiver_task thread.
 * Sets up synchronization primitives for thread-safe RC data access.
 */
bool radio_receiver_init(void);

/**
 * @brief Get current RC input snapshot (thread-safe)
 * @param input Pointer to RadioControlInput structure to fill
 * @return true if data is fresh and valid
 *
 * Non-blocking snapshot of current RC state.
 * Can be called from any task at any time.
 * Returns immediately with last known state.
 */
bool radio_receiver_get_input(RadioControlInput* input);

/**
 * @brief Check if RC signal is currently valid
 * @return true if receiving valid frames
 *
 * Fast check for signal validity without full snapshot.
 */
bool radio_receiver_signal_valid(void);

/**
 * @brief Get failsafe state
 * @return true if in failsafe condition (signal lost)
 *
 * Triggered when no valid frames received for timeout period.
 * Used by flight controller for emergency procedures (land, RTH, etc).
 */
bool radio_receiver_failsafe_active(void);

/**
 * @brief Check if combined RC failsafe is triggered
 * @return true if BOTH throttle (ch2) AND arm/disarm (ch5) are in failsafe ranges
 *
 * Failsafe requires both conditions simultaneously:
 * - Channel 2 (throttle): 1032-1036µs
 * - Channel 5 (arm/disarm): 998-1002µs
 *
 * This dual-key design prevents accidental failsafe activation.
 * Used by flight controller to trigger emergency procedures (land, RTH, cut throttle).
 */
bool radio_receiver_combined_failsafe(void);

/**
 * @brief Check if channels are in arm-ready position
 * @return true if throttle (ch2) AND arm/disarm (ch5) are NOT in failsafe ranges
 *
 * Inverse of combined_failsafe - indicates all channels are in normal operating range.
 * Required for arming the ESCs or motors.
 */
bool radio_receiver_arm_ready(void);

/**
 * @brief Check if arm prep key is engaged
 * @return true if Channel 5 (arm/disarm) is in prep range (1900-2000µs, engaged position)
 *
 * Arm prep is the first step in arming sequence - pilot toggles arm/disarm switch
 * to engaged position (2000µs) to prepare for arm. This prevents accidental arming.
 * Once arm prep is active, controller can then check for throttle minimum hold.
 */
bool radio_receiver_arm_prep(void);

/**
 * @brief Check if arm conditions are met (3+ second hold)
 * @return true if throttle at minimum AND roll/pitch centered for 3+ seconds
 *
 * Final step in arming sequence. Once arm prep key engaged, pilot must hold
 * throttle at minimum and keep roll/pitch centered for at least 3 seconds.
 * This prevents accidental arm from brief stick movements.
 *
 * Returns immediately true if conditions met for 3+ seconds, false otherwise.
 */
bool radio_receiver_can_arm(void);

/**
 * @brief Reset arm timer (call when conditions not met)
 *
 * When pilot releases throttle or moves sticks away from arm position,
 * the timer resets. Must maintain all conditions for full 3 seconds to arm.
 * Automatically called by radio_receiver_task when conditions change.
 */
void radio_receiver_reset_arm_timer(void);

/**
 * @brief Get current armed state of drone
 * @return true if armed, false if disarmed
 *
 * This reflects the actual armed state after completing arm/disarm sequences.
 */
bool radio_receiver_is_armed(void);

/**
 * @brief Radio receiver FreeRTOS task
 * @param arg Unused parameter
 *
 * Real-time task for responsive RC input handling.
 * - Runs at configurable priority (default: higher than status_task)
 * - Sleeps minimal period to reduce latency
 * - Updates RC input snapshot continuously
 * - Monitors signal health and timeout conditions
 * - Never blocks on other tasks or drivers
 *
 * Task timing:
 * - Wakeup period: 5ms (responsive to 200Hz RC updates)
 * - Typical latency: 5-10ms from signal to flight control decision
 * - CPU usage: <1% on typical drone
 */
void RadioReceiverTask(void *arg);

/**
 * @brief Configure failsafe channel ranges at runtime
 * @param config Pointer to configuration values (ignored if nullptr)
 */
void radio_receiver_set_failsafe_config(const RCFailsafeConfig* config);

/**
 * @brief Get current failsafe channel configuration
 * @return Copy of active failsafe configuration
 */
RCFailsafeConfig radio_receiver_get_failsafe_config(void);

#ifdef __cplusplus
}
#endif

#endif /* __RADIO_RECEIVER_TASK_HPP */
