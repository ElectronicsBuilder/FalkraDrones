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
 * @file    cmd_motor.cpp
 * @brief   Motor control console commands (MOTOR_*)
 * 
 *             Front (0°)
        M5(CCW)   M3(CW)
           30°    -30°
             \    /
              \  /
    M2(CCW) ----+---- M1(CW)
      -90°      |      90°
              /  \
             /    \
        M4(CCW)   M6(CW)
          150°    -150°
            Rear (180°)
Motor	Timer	Channel	    GPIO	    Position	    Angle	Direction
M1	    TIM1	CH1	        PE9	        Right	        90°	    CW
M2	    TIM1	CH2	        PE11	    Left	        -90°	CCW
M3	    TIM1	CH3	        PE13	    Front-Right	    -30°	CW
M4	    TIM1	CH4	        PE14	    Rear-Left	    150°	CCW
M5	    TIM8	CH1	        PI5	        Front-Left	    30°	    CCW
M6	    TIM8	CH2	        PC7	        Rear-Right	    -150°	CW


 */

#include "console_internal.h"
#include "MotorControlTask.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External timer handles from main.c
extern "C" {
    extern TIM_HandleTypeDef htim1;
    extern TIM_HandleTypeDef htim8;
}

// ============================================================================
// Internal PWM Control for Testing (bypasses arm check)
// ============================================================================

namespace {
    // Timer channel mapping for each motor (matches MotorControlTask)
    struct MotorPWMConfig {
        TIM_HandleTypeDef* timer;
        uint32_t channel;
        const char* name;
    };

    const MotorPWMConfig motor_config[MOTOR_COUNT] = {
        {&htim1, TIM_CHANNEL_1, "M1 (Right, CW)"},
        {&htim1, TIM_CHANNEL_2, "M2 (Left, CCW)"},
        {&htim1, TIM_CHANNEL_3, "M3 (Front-Right, CW)"},
        {&htim1, TIM_CHANNEL_4, "M4 (Rear-Left, CCW)"},
        {&htim8, TIM_CHANNEL_1, "M5 (Front-Left, CCW)"},
        {&htim8, TIM_CHANNEL_2, "M6 (Rear-Right, CW)"},
    };

    void set_motor_pulse_direct(int motor, uint16_t pulse) {
        if (motor < 0 || motor >= MOTOR_COUNT) return;
        const auto& cfg = motor_config[motor];
        __HAL_TIM_SET_COMPARE(cfg.timer, cfg.channel, pulse);
    }

    uint16_t get_motor_pulse(int motor) {
        if (motor < 0 || motor >= MOTOR_COUNT) return 0;
        const auto& cfg = motor_config[motor];
        return __HAL_TIM_GET_COMPARE(cfg.timer, cfg.channel);
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_motor_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    char msg[1024];
    int offset = 0;

    bool armed = motor_control_is_armed();
    bool override_active = motor_control_is_console_override_active();
    uint32_t override_remaining = motor_control_get_console_override_remaining();

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "MOTOR CONTROL STATUS\r\n"
            "========================================\r\n"
            "Armed: %s\r\n"
            "Console Override: %s",
            armed ? "YES" : "NO",
            override_active ? "ACTIVE" : "OFF");

    if (override_active) {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                " (%lu sec remaining)", override_remaining / 1000);
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n----------------------------------------\r\n"
            "Motor PWM Values (microseconds):\r\n");

    for (int i = 0; i < MOTOR_COUNT; i++) {
        uint16_t pulse = get_motor_pulse(i);
        float throttle = motor_control_get_throttle((MotorIndex)i);
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "  %s: %4d us (%.1f%%)\r\n",
                motor_config[i].name, pulse, throttle * 100.0f);
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;
}

static bool cmd_motor_pwm(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "MOTOR_PWM");
    if (!arg) {
        const char* usage =
            "\r\n[ERROR] Usage: MOTOR_PWM <motor> <pulse_us>\r\n"
            "  motor: 1-6 (motor index)\r\n"
            "  pulse_us: 1000-2000 (pulse width in microseconds)\r\n"
            "  Example: MOTOR_PWM 1 1500\r\n"
            "\r\n[WARNING] This command directly sets PWM bypassing arm check!\r\n"
            "          Ensure propellers are removed for testing.\r\n";
        console_send(huart, usage);
        return true;
    }

    int motor = 0;
    int pulse = 0;
    if (sscanf(arg, "%d %d", &motor, &pulse) != 2) {
        console_send(huart, "\r\n[ERROR] Invalid arguments. Usage: MOTOR_PWM <motor> <pulse_us>\r\n");
        return true;
    }

    if (motor < 1 || motor > MOTOR_COUNT) {
        char err[128];
        snprintf(err, sizeof(err), "\r\n[ERROR] Invalid motor %d. Valid range: 1-%d\r\n", motor, MOTOR_COUNT);
        console_send(huart, err);
        return true;
    }

    if (pulse < 1000 || pulse > 2000) {
        console_send(huart, "\r\n[ERROR] Invalid pulse width. Valid range: 1000-2000 us\r\n");
        return true;
    }

    // Convert to 0-based index
    int motor_idx = motor - 1;
    set_motor_pulse_direct(motor_idx, (uint16_t)pulse);

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[OK] Motor %d (%s) set to %d us\r\n",
            motor, motor_config[motor_idx].name, pulse);
    console_send(huart, msg);

    return true;
}

static bool cmd_motor_test_all(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "MOTOR_TEST_ALL");
    if (!arg) {
        const char* usage =
            "\r\n[ERROR] Usage: MOTOR_TEST_ALL <pulse_us>\r\n"
            "  pulse_us: 1000-2000 (pulse width in microseconds)\r\n"
            "  Example: MOTOR_TEST_ALL 1100\r\n"
            "\r\n[WARNING] This sets ALL motors directly!\r\n"
            "          Ensure propellers are removed for testing.\r\n";
        console_send(huart, usage);
        return true;
    }

    int pulse = atoi(arg);
    if (pulse < 1000 || pulse > 2000) {
        console_send(huart, "\r\n[ERROR] Invalid pulse width. Valid range: 1000-2000 us\r\n");
        return true;
    }

    for (int i = 0; i < MOTOR_COUNT; i++) {
        set_motor_pulse_direct(i, (uint16_t)pulse);
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[OK] All motors set to %d us\r\n", pulse);
    console_send(huart, msg);

    return true;
}

static bool cmd_motor_arm(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "MOTOR_ARM");

    // Default 30 second override
    uint32_t override_ms = 30000;
    if (arg) {
        int seconds = atoi(arg);
        if (seconds > 0 && seconds <= 300) {
            override_ms = seconds * 1000;
        }
    }

    if (motor_control_is_armed()) {
        // Extend override even if already armed
        motor_control_set_console_override(override_ms);
        char msg[128];
        snprintf(msg, sizeof(msg), "\r\n[INFO] Motors already armed. Override extended for %lu seconds.\r\n",
                override_ms / 1000);
        console_send(huart, msg);
        return true;
    }

    char warn_msg[256];
    snprintf(warn_msg, sizeof(warn_msg),
            "\r\n[WARNING] Manually arming motors!\r\n"
            "          Ensure propellers are removed.\r\n"
            "          Override duration: %lu seconds\r\n"
            "          Arming in 2 seconds...\r\n",
            override_ms / 1000);
    console_send(huart, warn_msg);
    HAL_Delay(2000);

    motor_control_set_console_override(override_ms);
    motor_control_arm();

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[OK] Motors ARMED (idle spin, override %lu sec)\r\n",
            override_ms / 1000);
    console_send(huart, msg);

    return true;
}

static bool cmd_motor_disarm(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "MOTOR_DISARM");

    // Default 30 second override to prevent immediate re-arm from radio
    uint32_t override_ms = 30000;
    if (arg) {
        int seconds = atoi(arg);
        if (seconds > 0 && seconds <= 300) {
            override_ms = seconds * 1000;
        }
    }

    if (!motor_control_is_armed()) {
        // Set override to prevent radio from arming
        motor_control_set_console_override(override_ms);
        char msg[128];
        snprintf(msg, sizeof(msg), "\r\n[INFO] Motors already disarmed. Override set for %lu seconds.\r\n",
                override_ms / 1000);
        console_send(huart, msg);
        return true;
    }

    motor_control_set_console_override(override_ms);
    motor_control_disarm();

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[OK] Motors DISARMED (override %lu sec)\r\n",
            override_ms / 1000);
    console_send(huart, msg);

    return true;
}

static bool cmd_motor_sweep(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "MOTOR_SWEEP");
    if (!arg) {
        const char* usage =
            "\r\n[ERROR] Usage: MOTOR_SWEEP <motor>\r\n"
            "  motor: 1-6 (motor index)\r\n"
            "  Sweeps motor from 1000 to 1500 us over 5 seconds.\r\n"
            "\r\n[WARNING] Ensure propellers are removed!\r\n";
        console_send(huart, usage);
        return true;
    }

    int motor = atoi(arg);
    if (motor < 1 || motor > MOTOR_COUNT) {
        char err[128];
        snprintf(err, sizeof(err), "\r\n[ERROR] Invalid motor %d. Valid range: 1-%d\r\n", motor, MOTOR_COUNT);
        console_send(huart, err);
        return true;
    }

    int motor_idx = motor - 1;

    console_send(huart, "\r\n[INFO] Starting PWM sweep (1000 -> 1500 us over 5 seconds)...\r\n");

    // Sweep from 1000 to 1500 over 5 seconds (50 steps, 100ms each)
    for (int pulse = 1000; pulse <= 1500; pulse += 10) {
        set_motor_pulse_direct(motor_idx, (uint16_t)pulse);

        char progress[64];
        snprintf(progress, sizeof(progress), "  Pulse: %d us\r", pulse);
        console_send(huart, progress);

        HAL_Delay(100);
    }

    // Return to minimum
    set_motor_pulse_direct(motor_idx, 1000);

    console_send(huart, "\r\n[OK] Sweep complete. Motor returned to 1000 us.\r\n");

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t motor_commands[] = {
    {
        .command = "MOTOR_STATUS",
        .handler = cmd_motor_status,
        .help_text = "Show motor control status and PWM values"
    },
    {
        .command = "MOTOR_PWM",
        .handler = cmd_motor_pwm,
        .help_text = "Set PWM for single motor: MOTOR_PWM <1-6> <1000-2000>"
    },
    {
        .command = "MOTOR_TEST_ALL",
        .handler = cmd_motor_test_all,
        .help_text = "Set PWM for all motors: MOTOR_TEST_ALL <1000-2000>"
    },
    {
        .command = "MOTOR_ARM",
        .handler = cmd_motor_arm,
        .help_text = "Arm motors: MOTOR_ARM [seconds] (default 30s override)"
    },
    {
        .command = "MOTOR_DISARM",
        .handler = cmd_motor_disarm,
        .help_text = "Disarm motors: MOTOR_DISARM [seconds] (default 30s override)"
    },
    {
        .command = "MOTOR_SWEEP",
        .handler = cmd_motor_sweep,
        .help_text = "PWM sweep test for single motor: MOTOR_SWEEP <1-6>"
    }
};

const size_t motor_commands_count = sizeof(motor_commands) / sizeof(motor_commands[0]);
