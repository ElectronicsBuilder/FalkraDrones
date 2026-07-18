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
 * @file    ppm.hpp
 * @brief   PPM (Pulse Position Modulation) Decoder for RC Receiver Input
 * @details Hardware timer-based PPM signal decoder for capturing up to 8 RC channels
 *          from radio receiver. Uses TIM2_CH1 input capture with interrupt-driven
 *          decoding for precise timing measurement. Signal buffered through 74LVC1G125
 *          level shifter for 5V to 3.3V conversion with output enable control.
 *
 * PPM Signal Characteristics:
 * - Standard frame rate: 20ms (50Hz)
 * - Channel pulse width: 1000-2000µs (1ms-2ms)
 * - Sync pulse: >2500µs (typically 3-4ms)
 * - Number of channels: 8 (typical RC configuration)
 * - Signal level: 5V from receiver, converted to 3.3V via 74LVC1G125
 *
 * Hardware Configuration:
 * - Timer: TIM2 Channel 1 (input capture mode)
 * - GPIO: Timer input pin configured in STM32CubeMX
 * - Level Shifter: 74LVC1G125 non-inverting buffer
 * - Output Enable: PPM_OE GPIO control (active low)
 * - Signal Flow: Receiver 5V → 74LVC1G125 → 3.3V → STM32 TIM2_CH1
 *
 * PPM Frame Format:
 * ```
 * |<--- 20ms frame --->|
 * | Ch1 | Ch2 | ... | Ch8 | Sync |
 * |1-2ms|1-2ms|     |1-2ms| >2.5ms|
 * ```
 *
 * Channel Mapping (Standard RC):
 * - Channel 1: Aileron (Roll)
 * - Channel 2: Elevator (Pitch)
 * - Channel 3: Throttle
 * - Channel 4: Rudder (Yaw)
 * - Channels 5-8: Auxiliary switches/knobs
 *
 * Timing Specifications:
 * - Timer resolution: 1µs (1MHz timer clock recommended)
 * - Pulse measurement accuracy: ±1µs
 * - Valid pulse range: 800-2200µs (with margins)
 * - Sync detection threshold: >2500µs
 * - Timeout detection: 25ms (no signal)
 *
 * Signal Quality Features:
 * - Automatic frame sync detection
 * - Invalid pulse rejection (out of range)
 * - Signal timeout detection for failsafe
 * - Glitch filtering through pulse validation
 * - Frame rate monitoring for signal quality
 */

#ifndef __PPM_HPP
#define __PPM_HPP

#include "stm32f7xx_hal.h"
#include <stdint.h>

// PPM decoder configuration constants
#define PPM_MAX_CHANNELS        8       // Maximum number of RC channels
#define PPM_MIN_PULSE_US        800     // Minimum valid pulse width (µs)
#define PPM_MAX_PULSE_US        2200    // Maximum valid pulse width (µs)
#define PPM_SYNC_THRESHOLD_US   2500    // Sync pulse detection threshold (µs)
#define PPM_TIMEOUT_MS          25      // Signal loss timeout (ms)
#define PPM_FRAME_PERIOD_MS     20      // Expected frame period (ms)

class PPMDecoder {
public:
    /**
     * @brief Constructor for PPM decoder
     * @param htim Pointer to timer handle (should be configured for input capture)
     * @param oePort GPIO port for output enable signal (74LVC1G125)
     * @param oePin GPIO pin for output enable signal (active low)
     */
    PPMDecoder(TIM_HandleTypeDef* htim, GPIO_TypeDef* oePort, uint16_t oePin);

    /**
     * @brief Destructor - stops input capture and cleans up
     */
    ~PPMDecoder();

    /**
     * @brief Initialize PPM decoder and start input capture
     * @return true if initialization successful, false otherwise
     */
    bool init();

    /**
     * @brief Deinitialize PPM decoder and stop input capture
     */
    void deinit();

    /**
     * @brief Enable/disable PPM signal reception
     * @param enable true to enable, false to disable
     */
    void enable(bool enable);

    /**
     * @brief Check if PPM signal is currently valid
     * @return true if valid signal received within timeout period
     */
    bool isSignalValid() const;

    /**
     * @brief Get raw channel pulse width in microseconds
     * @param channel Channel number (0-7 for channels 1-8)
     * @return Pulse width in microseconds (1000-2000), or 0 if invalid
     */
    uint16_t getChannel(uint8_t channel) const;

    /**
     * @brief Get normalized channel value (-1.0 to +1.0)
     * @param channel Channel number (0-7 for channels 1-8)
     * @return Normalized value where 1500µs = 0.0, 1000µs = -1.0, 2000µs = +1.0
     */
    float getChannelNormalized(uint8_t channel) const;

    /**
     * @brief Get number of valid channels in current frame
     * @return Number of channels (0-8)
     */
    uint8_t getChannelCount() const;

    /**
     * @brief Get time since last valid frame in milliseconds
     * @return Time in milliseconds since last frame
     */
    uint32_t getTimeSinceLastFrame() const;

    /**
     * @brief Get total interrupt count for diagnostics
     * @return Number of timer interrupts received
     */
    uint32_t getInterruptCount() const;

    /**
     * @brief Timer input capture interrupt handler
     * @note This should be called from HAL_TIM_IC_CaptureCallback()
     */
    void handleTimerInterrupt();

private:
    TIM_HandleTypeDef* _htim;           // Timer handle
    GPIO_TypeDef* _oePort;              // Output enable GPIO port
    uint16_t _oePin;                    // Output enable GPIO pin

    volatile uint16_t _channels[PPM_MAX_CHANNELS];  // Channel pulse widths
    volatile uint8_t _channelCount;     // Number of valid channels
    volatile uint32_t _lastFrameTime;   // Timestamp of last valid frame
    volatile uint32_t _lastCaptureTime; // Last timer capture value
    volatile uint8_t _currentChannel;   // Current channel being decoded
    volatile bool _signalValid;         // Signal validity flag
    volatile uint32_t _interruptCount;  // Total interrupts for diagnostics
    bool _enabled;                      // Enable state

    /**
     * @brief Validate pulse width is within acceptable range
     * @param pulseWidth Pulse width in microseconds
     * @return true if pulse is valid
     */
    bool isValidPulse(uint16_t pulseWidth) const;

    /**
     * @brief Check if pulse is a sync pulse
     * @param pulseWidth Pulse width in microseconds
     * @return true if pulse is a sync pulse
     */
    bool isSyncPulse(uint16_t pulseWidth) const;
};

#endif /* __PPM_HPP */
