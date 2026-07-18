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
 * @file    MotorControlTask.cpp
 * @brief   Motor Control Task Implementation
 */

#include "MotorControlTask.hpp"
#include "RadioReceiverTask.hpp"
#include "driver_manager.hpp"
#include "txs0108.hpp"
#include "log.hpp"
#include "cmsis_os.h"
#include "stm32f7xx_hal.h"

// External timer handles from main.c (C linkage)
extern "C" {
    extern TIM_HandleTypeDef htim1;
    extern TIM_HandleTypeDef htim8;
}

namespace {
    // ESC pulse width constants (microseconds)
    constexpr uint16_t ESC_MIN_PULSE = 1000;      // 0% throttle (motor stop)
    constexpr uint16_t ESC_IDLE_PULSE = 1121;     // ~6% throttle (idle spin when armed)
    constexpr uint16_t ESC_MAX_PULSE = 2000;      // 100% throttle (full power)

    // Task timing
    constexpr uint32_t MOTOR_TASK_PERIOD_MS = 10; // 100Hz update rate

    // Module state
    bool motors_armed = false;
    float motor_throttle[MOTOR_COUNT] = {0.0f};
    TXS0108* level_shifter = nullptr;

    // Console override state (bypasses radio arm control)
    uint32_t console_override_end_tick = 0;  // HAL tick when override expires

    // Timer channel mapping for each motor
    struct MotorPWMConfig {
        TIM_HandleTypeDef* timer;
        uint32_t channel;
    };

    // Motor to PWM channel mapping (ArduPilot Hexa X)
    const MotorPWMConfig motor_config[MOTOR_COUNT] = {
        {&htim1, TIM_CHANNEL_1},  // M1: Right (90°), CW
        {&htim1, TIM_CHANNEL_2},  // M2: Left (-90°), CCW
        {&htim1, TIM_CHANNEL_3},  // M3: Front-Right (-30°), CW
        {&htim1, TIM_CHANNEL_4},  // M4: Rear-Left (150°), CCW
        {&htim8, TIM_CHANNEL_1},  // M5: Front-Left (30°), CCW
        {&htim8, TIM_CHANNEL_2},  // M6: Rear-Right (-150°), CW
    };

    /**
     * @brief Convert throttle (0.0-1.0) to pulse width (1000-2000µs)
     */
    uint16_t throttle_to_pulse(float throttle) {
        if (throttle < 0.0f) throttle = 0.0f;
        if (throttle > 1.0f) throttle = 1.0f;
        return ESC_MIN_PULSE + static_cast<uint16_t>(throttle * (ESC_MAX_PULSE - ESC_MIN_PULSE));
    }

    /**
     * @brief Set PWM pulse width for a motor
     */
    void set_motor_pulse(MotorIndex motor, uint16_t pulse) {
        if (motor >= MOTOR_COUNT) return;

        const auto& cfg = motor_config[motor];
        __HAL_TIM_SET_COMPARE(cfg.timer, cfg.channel, pulse);
    }

    /**
     * @brief Start PWM output on all motor channels
     */
    void start_all_pwm_channels(void) {
        HAL_StatusTypeDef status;

        // Start TIM1 channels (M1-M4) - Advanced timer
        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM1_CH1 start failed: %d", status);

        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM1_CH2 start failed: %d", status);

        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM1_CH3 start failed: %d", status);

        status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM1_CH4 start failed: %d", status);

        // Enable MOE for TIM1 (required for advanced timers)
        __HAL_TIM_MOE_ENABLE(&htim1);

        // Start TIM8 channels (M5-M6) - Advanced timer
        status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM8_CH1 start failed: %d", status);

        status = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
        if (status != HAL_OK) LOG_ERROR("[MOTOR] TIM8_CH2 start failed: %d", status);

        // Enable MOE for TIM8 (required for advanced timers)
        __HAL_TIM_MOE_ENABLE(&htim8);

        LOG_DEBUG("[MOTOR] PWM channels started");
    }

    /**
     * @brief Stop PWM output on all motor channels
     */
    void stop_all_pwm_channels(void) {
        // Disable MOE first (for advanced timers)
        __HAL_TIM_MOE_DISABLE(&htim1);
        __HAL_TIM_MOE_DISABLE(&htim8);

        // Stop TIM1 channels (M1-M4)
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);

        // Stop TIM8 channels (M5-M6)
        HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);

        LOG_DEBUG("[MOTOR] PWM channels stopped");
    }

    /**
     * @brief Set all motors to minimum throttle (0%)
     */
    void set_all_motors_minimum(void) {
        for (int i = 0; i < MOTOR_COUNT; i++) {
            motor_throttle[i] = 0.0f;
            set_motor_pulse(static_cast<MotorIndex>(i), ESC_MIN_PULSE);
        }
    }

    /**
     * @brief Set all motors to idle spin throttle
     */
    void set_all_motors_idle(void) {
        float idle_throttle = static_cast<float>(ESC_IDLE_PULSE - ESC_MIN_PULSE) /
                              static_cast<float>(ESC_MAX_PULSE - ESC_MIN_PULSE);
        for (int i = 0; i < MOTOR_COUNT; i++) {
            motor_throttle[i] = idle_throttle;
            set_motor_pulse(static_cast<MotorIndex>(i), ESC_IDLE_PULSE);
        }
    }
}

bool motor_control_init(void) {
    LOG_INFO("[MOTOR] Initializing motor control task");

    // Get TXS0108 level shifter from DriverManager
    auto& dm = DriverManager::getInstance();
    level_shifter = dm.getLevelShifter();

    if (!level_shifter) {
        LOG_ERROR("[MOTOR] TXS0108 level shifter not available");
        return false;
    }

    // Enable level shifter at startup (PWM always active to prevent ESC beeping)
    level_shifter->enable();

    // Start PWM on all channels at minimum (1000µs)
    // This keeps ESCs quiet while disarmed (they see valid 1000µs signal)
    set_all_motors_minimum();
    start_all_pwm_channels();

    motors_armed = false;

    LOG_INFO("[MOTOR] Motor control initialized (6 motors, disarmed, PWM at 1000us)");
    return true;
}

void motor_control_set_throttle(MotorIndex motor, float throttle) {
    if (motor >= MOTOR_COUNT) return;
    if (!motors_armed) return;

    motor_throttle[motor] = throttle;
    set_motor_pulse(motor, throttle_to_pulse(throttle));
}

void motor_control_set_all_throttle(float throttle) {
    if (!motors_armed) return;

    for (int i = 0; i < MOTOR_COUNT; i++) {
        motor_throttle[i] = throttle;
        set_motor_pulse(static_cast<MotorIndex>(i), throttle_to_pulse(throttle));
    }
}

float motor_control_get_throttle(MotorIndex motor) {
    if (motor >= MOTOR_COUNT) return 0.0f;
    return motor_throttle[motor];
}

void motor_control_arm(void) {
    if (motors_armed) return;

    LOG_INFO("[MOTOR] Arming motors...");

    // Ensure level shifter is enabled (should already be from init)
    if (level_shifter && !level_shifter->isEnabled()) {
        level_shifter->enable();
    }

    // PWM channels should already be running from init
    // Just set motors to idle spin
    set_all_motors_idle();

    motors_armed = true;

    LOG_INFO("[MOTOR] Motors ARMED");
}

void motor_control_disarm(void) {
    if (!motors_armed) return;

    LOG_SYSSTATUS("[MOTOR] Disarming motors");

    // Set all motors to minimum throttle (1000µs)
    // PWM keeps running to prevent ESC beeping
    set_all_motors_minimum();

    // Small delay to ensure ESCs receive stop command
    osDelay(50);

    // Note: PWM channels stay running at 1000µs to keep ESCs quiet
    // Level shifter stays enabled so ESCs receive valid signal

    motors_armed = false;

    LOG_SYSSTATUS("[MOTOR] Motors DISARMED (PWM at 1000us)");
}

bool motor_control_is_armed(void) {
    return motors_armed;
}

void motor_control_set_console_override(uint32_t duration_ms) {
    if (duration_ms > 0) {
        console_override_end_tick = HAL_GetTick() + duration_ms;
        LOG_INFO("[MOTOR] Console override active for %lu ms", duration_ms);
    } else {
        console_override_end_tick = 0;
        LOG_INFO("[MOTOR] Console override disabled");
    }
}

bool motor_control_is_console_override_active(void) {
    if (console_override_end_tick == 0) return false;
    return HAL_GetTick() < console_override_end_tick;
}

uint32_t motor_control_get_console_override_remaining(void) {
    if (console_override_end_tick == 0) return 0;
    uint32_t now = HAL_GetTick();
    if (now >= console_override_end_tick) return 0;
    return console_override_end_tick - now;
}

void MotorControlTask(void *arg) {
    (void)arg;

    LOG_INFO("[MOTOR] Motor control task started (100Hz update rate)");

    // Initialize motor control
    if (!motor_control_init()) {
        LOG_ERROR("[MOTOR] Failed to initialize motor control, task halted");
        while (1) {
            osDelay(1000);
        }
    }

    bool prev_radio_armed = false;

    while (1) {
        // Skip radio arm control if console override is active
        if (!motor_control_is_console_override_active()) {
            // Get arm state from RadioReceiver
            bool radio_armed = radio_receiver_is_armed();
            bool failsafe = radio_receiver_combined_failsafe();

            // Handle arm state transitions
            if (radio_armed && !motors_armed && !failsafe) {
                // Radio armed, motors not armed, no failsafe -> ARM
                motor_control_arm();
            }
            else if ((!radio_armed || failsafe) && motors_armed) {
                // Radio disarmed OR failsafe active, motors armed -> DISARM
                if (failsafe && prev_radio_armed) {
                    LOG_WARN("[MOTOR] FAILSAFE: Disarming motors");
                }
                motor_control_disarm();
            }

            prev_radio_armed = radio_armed;
        }

        // Future: Apply flight control mixer output here
        // if (motors_armed) {
        //     apply_mixer_output();
        // }
       // throttle_to_pulse()
        osDelay(MOTOR_TASK_PERIOD_MS);
    }
}
