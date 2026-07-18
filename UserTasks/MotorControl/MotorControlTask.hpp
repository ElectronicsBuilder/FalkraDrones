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
 * @file    MotorControlTask.hpp
 * @brief   Motor Control Task for Hexacopter ESC/PWM Management
 * @details FreeRTOS task for controlling 6 ESC/motors via PWM. Monitors arm state
 *          from RadioReceiver and controls PWM output accordingly.
 *
 * Motor Layout (ArduPilot Hexa X Frame):
 *
 *              Front (0°)
 *          M5(CCW)   M3(CW)
 *             30°    -30°
 *               \    /
 *                \  /
 *      M2(CCW) ---+--- M1(CW)
 *        -90°     |     90°
 *                /  \
 *               /    \
 *          M4(CCW)   M6(CW)
 *            150°    -150°
 *              Rear (180°)
 *
 * PWM Channel Mapping:
 * - M1: TIM1_CH1 (PE9)  - Right, CW
 * - M2: TIM1_CH2 (PE11) - Left, CCW
 * - M3: TIM1_CH3 (PE13) - Front-Right, CW
 * - M4: TIM1_CH4 (PE14) - Rear-Left, CCW
 * - M5: TIM8_CH1 (PI5)  - Front-Left, CCW
 * - M6: TIM8_CH2 (PC7)  - Rear-Right, CW
 *
 * Safety Features:
 * - Motors disabled by default at startup
 * - TXS0108 level shifter disabled when disarmed (high-Z outputs)
 * - Auto-disarm on RadioReceiver failsafe
 * - ESCs receive 1000µs (0% throttle) when disarmed
 */

#ifndef __MOTORCONTROLTASK_HPP
#define __MOTORCONTROLTASK_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of motors in hexacopter configuration */
#define MOTOR_COUNT 6

/** Motor indices (ArduPilot Hexa X naming) */
typedef enum {
    MOTOR_1 = 0,  // Right (90°), CW
    MOTOR_2 = 1,  // Left (-90°), CCW
    MOTOR_3 = 2,  // Front-Right (-30°), CW
    MOTOR_4 = 3,  // Rear-Left (150°), CCW
    MOTOR_5 = 4,  // Front-Left (30°), CCW
    MOTOR_6 = 5,  // Rear-Right (-150°), CW
} MotorIndex;

/**
 * @brief Initialize motor control task infrastructure
 * @return true if initialization successful
 *
 * Call before creating the MotorControlTask thread.
 * Sets up PWM timers and TXS0108 level shifter reference.
 */
bool motor_control_init(void);

/**
 * @brief Set throttle for individual motor
 * @param motor Motor index (MOTOR_1 to MOTOR_6)
 * @param throttle Throttle value 0.0 to 1.0 (maps to 1000-2000µs)
 *
 * Only applies if motors are armed. Ignored when disarmed.
 */
void motor_control_set_throttle(MotorIndex motor, float throttle);

/**
 * @brief Set throttle for all motors
 * @param throttle Throttle value 0.0 to 1.0 (maps to 1000-2000µs)
 *
 * Only applies if motors are armed. Ignored when disarmed.
 */
void motor_control_set_all_throttle(float throttle);

/**
 * @brief Get current throttle for a motor
 * @param motor Motor index (MOTOR_1 to MOTOR_6)
 * @return Current throttle value 0.0 to 1.0
 */
float motor_control_get_throttle(MotorIndex motor);

/**
 * @brief Arm motors (enable PWM output with idle spin)
 *
 * Enables TXS0108 level shifter, starts PWM channels,
 * and sets all motors to idle spin (~5% throttle).
 */
void motor_control_arm(void);

/**
 * @brief Disarm motors (disable PWM output)
 *
 * Sets all motors to 0% throttle, stops PWM channels,
 * and disables TXS0108 level shifter (high-Z outputs).
 */
void motor_control_disarm(void);

/**
 * @brief Check if motors are currently armed
 * @return true if motors are armed and PWM is active
 */
bool motor_control_is_armed(void);

/**
 * @brief Set console override to bypass radio arm/disarm control
 * @param duration_ms Duration in milliseconds (0 to disable)
 *
 * When active, the motor task will not change arm state based on radio.
 * Used by console commands for testing without radio interference.
 */
void motor_control_set_console_override(uint32_t duration_ms);

/**
 * @brief Check if console override is currently active
 * @return true if console override is active
 */
bool motor_control_is_console_override_active(void);

/**
 * @brief Get remaining console override time
 * @return Remaining time in milliseconds (0 if not active)
 */
uint32_t motor_control_get_console_override_remaining(void);

/**
 * @brief Motor control FreeRTOS task
 * @param arg Unused parameter
 *
 * Task that monitors RadioReceiver arm state and controls motor output.
 * - Runs at 100Hz (10ms period)
 * - Automatically arms/disarms based on RadioReceiver state
 * - Handles failsafe conditions
 */
void MotorControlTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* __MOTORCONTROLTASK_HPP */
