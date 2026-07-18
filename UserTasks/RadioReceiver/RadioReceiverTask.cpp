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
 * @file    radio_receiver_task.cpp
 * @brief   Real-Time Radio Receiver Task Implementation
 */

#include "RadioReceiverTask.hpp"
#include "driver_manager.hpp"
#include "driver_status.hpp"
#include "status.hpp"
#include "ppm.hpp"
#include "log.hpp"
#include "cmsis_os.h"
#include <cstring>
#include <algorithm>

// === Configuration ===

// Task timing: 5ms wake period = responsive to 200Hz RC updates
#define RC_TASK_WAKE_MS         5

// Signal loss timeout: 25ms without frames = ~1.5 RC frame periods
#define RC_SIGNAL_TIMEOUT_MS    25

// Failsafe channel ranges (RC transmitter failsafe positions)
// All conditions must be met simultaneously to trigger failsafe
//Update these values based on your specific RC system and transmitter configuration

#define RC_ROLL_FAILSAFE_MIN        998     // Channel 0 (roll) at center (µs)
#define RC_ROLL_FAILSAFE_MAX        1002    // Channel 0 (roll) deadband
#define RC_PITCH_FAILSAFE_MIN       1995    // Channel 1 (pitch) at maximum up (µs)
#define RC_PITCH_FAILSAFE_MAX       2000    // Channel 1 (pitch) maximum range
#define RC_THROTTLE_FAILSAFE_MIN    1032    // Channel 2 (throttle) failsafe lower bound (µs)
#define RC_THROTTLE_FAILSAFE_MAX    1036    // Channel 2 (throttle) failsafe upper bound (µs)
#define RC_DISARM_FAILSAFE_MIN      998     // Channel 5 (arm/disarm key) failsafe lower bound (µs)
#define RC_DISARM_FAILSAFE_MAX      1002    // Channel 5 (arm/disarm key) failsafe upper bound (µs)

static RCFailsafeConfig rc_failsafe_config = {
    RC_ROLL_FAILSAFE_MIN,
    RC_ROLL_FAILSAFE_MAX,
    RC_PITCH_FAILSAFE_MIN,
    RC_PITCH_FAILSAFE_MAX,
    RC_THROTTLE_FAILSAFE_MIN,
    RC_THROTTLE_FAILSAFE_MAX,
    RC_DISARM_FAILSAFE_MIN,
    RC_DISARM_FAILSAFE_MAX,
};

// Arm sequence configuration
#define RC_ARM_PREP_MIN             1900    // Channel 5 arm prep position (engaged at 2000µs) (µs)
#define RC_ARM_PREP_MAX             2000    // Channel 5 arm prep position upper bound (µs)
#define RC_THROTTLE_MIN_ARM         1000    // Channel 2 minimum for arm (µs)
#define RC_THROTTLE_MIN_ARM_MAX     1050    // Channel 2 minimum deadband upper (µs)
#define RC_ROLL_ARM_MIN             940     // Channel 0 centered deadband lower (±60µs tolerance) (µs)
#define RC_ROLL_ARM_MAX             1060    // Channel 0 centered deadband upper (µs)
#define RC_PITCH_ARM_MIN            940     // Channel 1 centered deadband lower (±60µs tolerance) (µs)
#define RC_PITCH_ARM_MAX            1060    // Channel 1 centered deadband upper (µs)
#define RC_ARM_HOLD_TIME_MS         3000    // 3 seconds required to arm (milliseconds)

// Disarm sequence configuration
#define RC_DISARM_ROLL_MIN          1950    // Channel 0 right position (µs)
#define RC_DISARM_ROLL_MAX          2000    // Channel 0 right upper bound (µs)
#define RC_DISARM_PITCH_MIN         950     // Channel 1 down position (µs)
#define RC_DISARM_PITCH_MAX         1050    // Channel 1 down upper bound (µs)
#define RC_DISARM_HOLD_TIME_MS      3000    // 3 seconds required to disarm (milliseconds)

// === Module State ===

// Armed state tracking
typedef enum {
    DRONE_STATE_DISARMED = 0,
    DRONE_STATE_ARMED = 1
} DroneArmedState;

// Current RC input snapshot (updated continuously)
static RadioControlInput rc_input_current = {
    .channels = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500},
    .channel_count = 0,
    .roll = 0.0f,
    .pitch = 0.0f,
    .throttle = 0.0f,
    .yaw = 0.0f,
    .aux = {0.0f, 0.0f, 0.0f, 0.0f},
    .signal_valid = false,
    .time_since_frame = 0,
    .frame_count = 0,
    .frame_errors = 0,
};

// Synchronization
static osMutexId_t rc_input_mutex = nullptr;
static bool rc_failsafe_active = false;
static bool rc_failsafe_logged = false;  // Only log WARN once, then INFO periodically
static uint32_t rc_last_valid_frame_time = 0;
static uint32_t rc_failsafe_log_counter = 0;  // Counter for periodic INFO logging

// Periodic logging: log every Nth cycles (every 5 seconds at 5ms = 1000 cycles)
#define RC_FAILSAFE_LOG_PERIOD  1000

// Channel-specific failsafe detection
static bool rc_throttle_failsafe = false;      // Channel 2 in failsafe range
static bool rc_disarm_failsafe = false;        // Channel 5 in failsafe range
static bool rc_combined_failsafe = false;      // Both channels in failsafe (actual failsafe trigger)
static bool rc_combined_failsafe_logged = false;  // Log combined failsafe once

// Arm sequence state
static bool rc_arm_prep_active = false;        // Channel 5 in prep position (1900-2000µs)
static uint32_t rc_arm_timer_start = 0;        // Timer start for 3-second arm hold
static bool rc_arm_conditions_met = false;     // All conditions met (throttle min + centered)
static bool rc_arm_transition_complete = false;  // True after 3s arm hold completed

// Disarm sequence state
static bool rc_disarm_conditions_met = false;  // Ch0=2000 + Ch1=1000 detected
static uint32_t rc_disarm_timer_start = 0;     // Disarm timer start time

// Drone armed state machine
static DroneArmedState rc_drone_armed_state = DRONE_STATE_DISARMED;

// === Module Functions ===

bool radio_receiver_init(void) {
    rc_input_mutex = osMutexNew(nullptr);
    if (!rc_input_mutex) {
        LOG_ERROR("[RC] Failed to create RC input mutex");
        return false;
    }

    rc_last_valid_frame_time = HAL_GetTick();
    rc_failsafe_active = false;
    rc_failsafe_logged = false;
    rc_failsafe_log_counter = 0;

    LOG_INFO("[RC] Radio receiver task initialized");
    return true;
}

bool radio_receiver_get_input(RadioControlInput* input) {
    if (!input || !rc_input_mutex) {
        return false;
    }

    if (osMutexAcquire(rc_input_mutex, 10) != osOK) {
        // Timeout - return stale data
        *input = rc_input_current;
        return false;
    }

    *input = rc_input_current;
    osMutexRelease(rc_input_mutex);

    return rc_input_current.signal_valid;
}

bool radio_receiver_signal_valid(void) {
    return rc_input_current.signal_valid;
}

bool radio_receiver_failsafe_active(void) {
    return rc_failsafe_active;
}

bool radio_receiver_combined_failsafe(void) {
    return rc_combined_failsafe;
}

void radio_receiver_set_failsafe_config(const RCFailsafeConfig* config) {
    if (!config) {
        return;
    }
    rc_failsafe_config = *config;
}

RCFailsafeConfig radio_receiver_get_failsafe_config(void) {
    return rc_failsafe_config;
}

bool radio_receiver_arm_ready(void) {
    // Arm ready when NOT in combined failsafe (all four conditions NOT simultaneously met)
    return !rc_combined_failsafe;
}

bool radio_receiver_arm_prep(void) {
    // Arm prep is when Channel 5 (arm/disarm) is in middle position (1400-1600µs)
    return rc_arm_prep_active;
}

bool radio_receiver_can_arm(void) {
    // Can arm if all conditions met for 3+ seconds
    // rc_arm_conditions_met tracks throttle minimum + roll/pitch centered
    // rc_arm_prep_active tracks Channel 5 in prep position
    if (!rc_arm_prep_active || !rc_arm_conditions_met) {
        return false;
    }

    // Check if 3 seconds have elapsed since conditions were met
    uint32_t elapsed = HAL_GetTick() - rc_arm_timer_start;
    return (elapsed >= RC_ARM_HOLD_TIME_MS);
}

void radio_receiver_reset_arm_timer(void) {
    // Reset timer when conditions not met
    rc_arm_timer_start = 0;
    rc_arm_conditions_met = false;
    rc_arm_transition_complete = false;  // Reset transition flag
}

bool radio_receiver_is_armed(void) {
    return (rc_drone_armed_state == DRONE_STATE_ARMED);
}

/**
 * @brief Update RC input from PPMDecoder
 * Called at task wake frequency (5ms)
 * Updates local RC snapshot with latest decoded channels
 */
static void update_rc_from_ppm(void) {
    auto& dm = DriverManager::getInstance();
    auto* ppm = dm.getPPMDecoder();

    if (!ppm) {
        // No PPM decoder available
        rc_input_current.signal_valid = false;
        rc_failsafe_active = true;
        return;
    }

    // Check signal validity
    bool signal_valid = ppm->isSignalValid();
    uint32_t time_since_frame = ppm->getTimeSinceLastFrame();

    if (signal_valid) {
        // Update raw channels
        for (uint8_t i = 0; i < 8; i++) {
            rc_input_current.channels[i] = ppm->getChannel(i);
        }

        // Update normalized control inputs
        rc_input_current.roll = ppm->getChannelNormalized(0);
        rc_input_current.pitch = ppm->getChannelNormalized(1);
        rc_input_current.throttle = ppm->getChannelNormalized(2);
        rc_input_current.yaw = ppm->getChannelNormalized(3);

        // Update auxiliary channels (5-8 -> aux[0-3])
        for (uint8_t i = 4; i < 8; i++) {
            rc_input_current.aux[i - 4] = ppm->getChannelNormalized(i);
        }

        // Update metadata
        rc_input_current.channel_count = ppm->getChannelCount();
        rc_input_current.time_since_frame = time_since_frame;
        rc_input_current.signal_valid = true;
        rc_input_current.frame_count++;

        // Check for channel-specific failsafe conditions
        // Failsafe requires ALL four channels in specific positions simultaneously

        // Channel 0 (roll): 998-1002µs (centered at 1000µs)
        uint16_t roll_ch = rc_input_current.channels[0];
        bool roll_in_failsafe = (roll_ch >= rc_failsafe_config.roll_min &&
                                 roll_ch <= rc_failsafe_config.roll_max);

        // Channel 1 (pitch): 1995-2000µs (maximum up position)
        uint16_t pitch_ch = rc_input_current.channels[1];
        bool pitch_in_failsafe = (pitch_ch >= rc_failsafe_config.pitch_min &&
                                  pitch_ch <= rc_failsafe_config.pitch_max);

        // Channel 2 (throttle): 1032-1036µs (failsafe range)
        uint16_t throttle_ch = rc_input_current.channels[2];
        bool throttle_in_failsafe = (throttle_ch >= rc_failsafe_config.throttle_min &&
                                     throttle_ch <= rc_failsafe_config.throttle_max);

        // Channel 5 (arm/disarm): 998-1002µs (failsafe/safety key range)
        uint16_t disarm_ch = rc_input_current.channels[5];
        bool disarm_in_failsafe = (disarm_ch >= rc_failsafe_config.disarm_min &&
                                   disarm_ch <= rc_failsafe_config.disarm_max);

        // Check for COMBINED failsafe: ALL four channels must be in their failsafe ranges
        // This prevents accidental failsafe from any single stick movement
        // Required stick position: Roll centered, Pitch up, Throttle failsafe, Arm key engaged
        bool all_in_failsafe = roll_in_failsafe && pitch_in_failsafe &&
                               throttle_in_failsafe && disarm_in_failsafe;

        if (all_in_failsafe && !rc_combined_failsafe) {
            // Failsafe just activated (all four conditions met)
            rc_combined_failsafe = true;
            rc_combined_failsafe_logged = false;
            // Only log warning if drone is ARMED (failsafe during disarmed is just stick position)
            if (rc_drone_armed_state == DRONE_STATE_ARMED) {
                LOG_WARN("[RC] FAILSAFE TRIGGERED: Ch0=%dµs + Ch1=%dµs + Ch2=%dµs + Ch5=%dµs",
                         roll_ch, pitch_ch, throttle_ch, disarm_ch);
            } else {
                LOG_DEBUG("[RC] Failsafe stick position detected while disarmed (no action needed)");
            }
        } else if (!all_in_failsafe && rc_combined_failsafe) {
            // Failsafe deactivated (at least one channel moved out of range)
            rc_combined_failsafe = false;
            rc_combined_failsafe_logged = false;
            // Only log warning if drone was ARMED when failsafe was active
            if (rc_drone_armed_state == DRONE_STATE_ARMED) {
                LOG_WARN("[RC] FAILSAFE DEACTIVATED: Ch0=%dµs, Ch1=%dµs, Ch2=%dµs, Ch5=%dµs",
                         roll_ch, pitch_ch, throttle_ch, disarm_ch);
            }
        }

        // Update individual channel flags for diagnostics (debug level only)
        rc_throttle_failsafe = throttle_in_failsafe;
        rc_disarm_failsafe = disarm_in_failsafe;

        // === ARM SEQUENCE DETECTION ===
        // Step 1: Check if arm prep key is engaged (Channel 5 in engaged position 1900-2000µs)
        bool arm_prep_in_range = (disarm_ch >= RC_ARM_PREP_MIN && disarm_ch <= RC_ARM_PREP_MAX);

        if (arm_prep_in_range && !rc_arm_prep_active) {
            // Arm prep just activated
            rc_arm_prep_active = true;
            LOG_DEBUG("[RC] Arm prep key engaged: Ch5=%dµs", disarm_ch);
        } else if (!arm_prep_in_range && rc_arm_prep_active) {
            // Arm prep released - reset timer and clear countdown display
            rc_arm_prep_active = false;
            if (rc_arm_conditions_met) {
                printf("\n");  // Clear countdown line if it was showing
            }
            radio_receiver_reset_arm_timer();
            LOG_DEBUG("[RC] Arm prep key released: Ch5=%dµs", disarm_ch);
        }

        // Step 2: ONLY check arm conditions if arm prep key is active
        if (rc_arm_prep_active) {
            uint16_t throttle_arm_ch = rc_input_current.channels[2];
            uint16_t roll_arm_ch = rc_input_current.channels[0];
            uint16_t pitch_arm_ch = rc_input_current.channels[1];

            bool throttle_at_min = (throttle_arm_ch >= RC_THROTTLE_MIN_ARM &&
                                    throttle_arm_ch <= RC_THROTTLE_MIN_ARM_MAX);
            bool roll_centered = (roll_arm_ch >= RC_ROLL_ARM_MIN &&
                                  roll_arm_ch <= RC_ROLL_ARM_MAX);
            bool pitch_centered = (pitch_arm_ch >= RC_PITCH_ARM_MIN &&
                                   pitch_arm_ch <= RC_PITCH_ARM_MAX);

            // All arm position conditions met
            bool all_arm_positions = throttle_at_min && roll_centered && pitch_centered;

            if (all_arm_positions) {
                // Conditions are met
                if (!rc_arm_conditions_met) {
                    // First detection - start timer
                    rc_arm_conditions_met = true;
                    rc_arm_timer_start = HAL_GetTick();
                    rc_arm_transition_complete = false;
                    LOG_DEBUG("[RC] Arm conditions met: starting 3s timer (Th=%dµs, Ro=%dµs, Pi=%dµs)",
                              throttle_arm_ch, roll_arm_ch, pitch_arm_ch);
                } else {
                    // Conditions still held - check timer
                    uint32_t elapsed = HAL_GetTick() - rc_arm_timer_start;

                    // Only show countdown if not yet armed
                    if (!rc_arm_transition_complete) {
                        uint32_t remaining_ms = (RC_ARM_HOLD_TIME_MS > elapsed) ?
                                                (RC_ARM_HOLD_TIME_MS - elapsed) : 0;

                        // Print countdown every 100ms (20 cycles at 5ms rate)
                        static uint8_t countdown_counter = 0;
                        if (++countdown_counter >= 20) {
                            countdown_counter = 0;
                            printf("\r[RC] Arming in: %.1fs (Th=%d Ro=%d Pi=%d)   ",
                                   remaining_ms / 1000.0f, throttle_arm_ch, roll_arm_ch, pitch_arm_ch);
                            fflush(stdout);
                        }

                        // Check if timer completed AND not already armed
                        if (elapsed >= RC_ARM_HOLD_TIME_MS && rc_drone_armed_state == DRONE_STATE_DISARMED) {
                            // TRANSITION TO ARMED (happens only once)
                            rc_drone_armed_state = DRONE_STATE_ARMED;
                            rc_arm_transition_complete = true;
                            LOG_SYSSTATUS("[RC] Drone ARMED");
                            // Update global status
                            DriverStatus::updateStatus([](FalkraStatus& s) {
                                s.droneArmed = true;
                            });
                        }
                    }
                    // If already armed and transition complete, do nothing (holding sticks is OK)
                }
            } else {
                // Conditions not met anymore - reset arm timer
                if (rc_arm_conditions_met) {
                    radio_receiver_reset_arm_timer();
                    printf("\n");  // Clear countdown line
                    LOG_DEBUG("[RC] Arm conditions lost: timer reset (Th=%dµs, Ro=%dµs, Pi=%dµs)",
                              throttle_arm_ch, roll_arm_ch, pitch_arm_ch);
                }
            }
        } else {
            // Arm prep NOT active - ensure timer is cleared
            if (rc_arm_conditions_met) {
                radio_receiver_reset_arm_timer();
            }
        }

        // === DISARM SEQUENCE DETECTION ===
        // Only check disarm if currently armed
        if (rc_drone_armed_state == DRONE_STATE_ARMED) {
            uint16_t disarm_roll_ch = rc_input_current.channels[0];
            uint16_t disarm_pitch_ch = rc_input_current.channels[1];

            bool roll_at_max = (disarm_roll_ch >= RC_DISARM_ROLL_MIN &&
                                disarm_roll_ch <= RC_DISARM_ROLL_MAX);
            bool pitch_at_center = (disarm_pitch_ch >= RC_DISARM_PITCH_MIN &&
                                    disarm_pitch_ch <= RC_DISARM_PITCH_MAX);

            bool all_disarm_positions = roll_at_max && pitch_at_center;

            if (all_disarm_positions) {
                if (!rc_disarm_conditions_met) {
                    // First detection - start disarm timer
                    rc_disarm_conditions_met = true;
                    rc_disarm_timer_start = HAL_GetTick();
                    LOG_DEBUG("[RC] Disarm conditions met: starting 3s timer (Ro=%dµs, Pi=%dµs)",
                              disarm_roll_ch, disarm_pitch_ch);
                } else {
                    // Conditions still held - check timer
                    uint32_t elapsed = HAL_GetTick() - rc_disarm_timer_start;
                    uint32_t remaining_ms = (RC_DISARM_HOLD_TIME_MS > elapsed) ?
                                            (RC_DISARM_HOLD_TIME_MS - elapsed) : 0;

                    // Print countdown every 100ms
                    static uint8_t disarm_countdown_counter = 0;
                    if (++disarm_countdown_counter >= 20) {
                        disarm_countdown_counter = 0;
                        printf("\r[RC] Disarming in: %.1fs (Ro=%d Pi=%d)   ",
                               remaining_ms / 1000.0f, disarm_roll_ch, disarm_pitch_ch);
                        fflush(stdout);
                    }

                    // Check if timer completed
                    if (elapsed >= RC_DISARM_HOLD_TIME_MS) {
                        // TRANSITION TO DISARMED (happens only once)
                        rc_drone_armed_state = DRONE_STATE_DISARMED;
                        rc_disarm_conditions_met = false;
                        rc_disarm_timer_start = 0;
                        LOG_SYSSTATUS("[RC] Drone DISARMED");

                        // Update global status
                        DriverStatus::updateStatus([](FalkraStatus& s) {
                            s.droneArmed = false;
                        });

                        // Note: Holding sticks beyond 3s is OK - won't re-trigger
                    }
                }
            } else {
                // Conditions not met - reset disarm timer
                if (rc_disarm_conditions_met) {
                    rc_disarm_conditions_met = false;
                    rc_disarm_timer_start = 0;
                    printf("\n");  // Clear countdown line
                    LOG_DEBUG("[RC] Disarm conditions lost: timer reset (Ro=%dµs, Pi=%dµs)",
                              disarm_roll_ch, disarm_pitch_ch);
                }
            }
        }

        // Clear signal loss failsafe (but NOT channel-specific failsafes)
        rc_failsafe_active = false;
        rc_failsafe_logged = false;
        rc_failsafe_log_counter = 0;
        rc_last_valid_frame_time = HAL_GetTick();

    } else {
        // Signal invalid or lost
        rc_input_current.signal_valid = false;
        rc_input_current.time_since_frame = time_since_frame;

        // Reset all channels to neutral/center on signal loss (CRITICAL for safety)
        // This prevents "sticky" control inputs if signal drops
        for (uint8_t i = 0; i < 8; i++) {
            rc_input_current.channels[i] = 1500;  // Center position
        }
        rc_input_current.roll = 0.0f;
        rc_input_current.pitch = 0.0f;
        rc_input_current.throttle = 0.0f;
        rc_input_current.yaw = 0.0f;
        for (uint8_t i = 0; i < 4; i++) {
            rc_input_current.aux[i] = 0.0f;
        }
        rc_input_current.channel_count = 0;

        // Clear all failsafe flags on signal loss
        if (rc_combined_failsafe) {
            rc_combined_failsafe = false;
            rc_combined_failsafe_logged = false;
            LOG_WARN("[RC] Combined failsafe cleared (signal lost)");
        }
        rc_throttle_failsafe = false;
        rc_disarm_failsafe = false;

        // Clear arm sequence state on signal loss
        if (rc_arm_prep_active || rc_arm_conditions_met) {
            rc_arm_prep_active = false;
            radio_receiver_reset_arm_timer();
            LOG_INFO("[RC] Arm sequence cleared (signal lost)");
        }

        // Check for timeout and trigger failsafe
        if (time_since_frame > RC_SIGNAL_TIMEOUT_MS) {
            if (!rc_failsafe_active) {
                // Failsafe just triggered - log WARNING once
                rc_failsafe_active = true;
                rc_failsafe_logged = true;
                rc_failsafe_log_counter = 0;
                LOG_WARN("[RC] FAILSAFE: Signal lost (timeout %dms)", time_since_frame);
            } else if (rc_failsafe_active && !rc_failsafe_logged) {
                // Already logged, don't repeat
            } else if (++rc_failsafe_log_counter >= RC_FAILSAFE_LOG_PERIOD) {
                // Log periodic INFO every 5 seconds to show failsafe is ongoing
                LOG_INFO("[RC] Failsafe ongoing: signal lost for %dms", time_since_frame);
                rc_failsafe_log_counter = 0;
            }
        }
    }
}

// === FreeRTOS Task ===

void RadioReceiverTask(void *arg) {
    (void)arg;

    LOG_INFO("[RC] Radio receiver task started (5ms update rate)");

    while (1) {
        // Update RC input from PPMDecoder
        update_rc_from_ppm();

        // Atomically update shared state
        if (osMutexAcquire(rc_input_mutex, 10) == osOK) {
            // rc_input_current already updated above
            // Just release the lock
            osMutexRelease(rc_input_mutex);
        }

        // Sleep for minimal period to keep responsiveness high
        // 5ms = 200Hz update rate, responsive to typical RC receiver rates
        osDelay(RC_TASK_WAKE_MS);
    }
}
