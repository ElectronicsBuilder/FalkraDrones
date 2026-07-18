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
 * @file    ppm.cpp
 * @brief   PPM Decoder Implementation
 * @details Interrupt-driven PPM signal decoder using hardware timer input capture
 */

#include "ppm.hpp"
#include "log.hpp"
#include "driver_manager.hpp"

PPMDecoder::PPMDecoder(TIM_HandleTypeDef* htim, GPIO_TypeDef* oePort, uint16_t oePin)
    : _htim(htim)
    , _oePort(oePort)
    , _oePin(oePin)
    , _channelCount(0)
    , _lastFrameTime(0)
    , _lastCaptureTime(0)
    , _currentChannel(0)
    , _signalValid(false)
    , _interruptCount(0)
    , _enabled(false)
{
    // Initialize channel array
    for (uint8_t i = 0; i < PPM_MAX_CHANNELS; i++) {
        _channels[i] = 1500; // Default center position
    }
}

PPMDecoder::~PPMDecoder() {
    deinit();
}

bool PPMDecoder::init()
{
    if (!_htim) {
        return false;
    }

    // Enable 74LVC1G125 buffer (active low)
    HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_RESET);

    // Initialize timer capture time
    _lastCaptureTime = 0;
    _lastFrameTime = HAL_GetTick();

    // Start input capture interrupt on TIM2 Channel 1
    if (HAL_TIM_IC_Start_IT(_htim, TIM_CHANNEL_1) != HAL_OK) {
        return false;
    }

    _enabled = true;
    return true;
}

void PPMDecoder::deinit()
{
    if (_htim) {
        HAL_TIM_IC_Stop_IT(_htim, TIM_CHANNEL_1);
    }

    // Disable output enable
    HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_SET);
    _enabled = false;
    _signalValid = false;

    // Reset state variables
    _channelCount = 0;
    _lastFrameTime = 0;
    _lastCaptureTime = 0;
    _currentChannel = 0;
    _interruptCount = 0;

    // Reset channel array to center
    for (uint8_t i = 0; i < PPM_MAX_CHANNELS; i++) {
        _channels[i] = 1500;
    }

    LOG_SYSSTATUS("[PPM] PPMDecoder cleanup complete");
}

void PPMDecoder::enable(bool enable)
{
    if (enable) {
        // Enable 74LVC1G125 buffer (active low)
        HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_RESET);
        _enabled = true;
    } else {
        // Disable 74LVC1G125 buffer
        HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_SET);
        _enabled = false;
        _signalValid = false;
    }
}

bool PPMDecoder::isSignalValid() const
{
    if (!_enabled) {
        return false;
    }

    // Check if we've received data recently
    uint32_t currentTime = HAL_GetTick();
    return (currentTime - _lastFrameTime) < PPM_TIMEOUT_MS && _signalValid;
}

uint16_t PPMDecoder::getChannel(uint8_t channel) const
{
    if (channel >= PPM_MAX_CHANNELS || !_signalValid) {
        return 0;
    }
    return _channels[channel];
}

float PPMDecoder::getChannelNormalized(uint8_t channel) const
{
    uint16_t rawValue = getChannel(channel);
    if (rawValue == 0) {
        return 0.0f;
    }

    // Convert 1000-2000µs to -1.0 to +1.0 (1500µs = 0.0)
    return (static_cast<float>(rawValue) - 1500.0f) / 500.0f;
}

uint8_t PPMDecoder::getChannelCount() const
{
    return _channelCount;
}

uint32_t PPMDecoder::getTimeSinceLastFrame() const
{
    return HAL_GetTick() - _lastFrameTime;
}

uint32_t PPMDecoder::getInterruptCount() const
{
    return _interruptCount;
}

void PPMDecoder::handleTimerInterrupt()
{
    if (!_enabled) {
        return;
    }

    _interruptCount++;

    // Read captured value
    uint32_t currentCapture = HAL_TIM_ReadCapturedValue(_htim, TIM_CHANNEL_1);

    // Calculate pulse width in timer ticks
    uint32_t pulseWidth;
    if (currentCapture >= _lastCaptureTime) {
        pulseWidth = currentCapture - _lastCaptureTime;
    } else {
        // Handle timer overflow
        pulseWidth = (0xFFFFFFFF - _lastCaptureTime) + currentCapture + 1;
    }

    _lastCaptureTime = currentCapture;

    // Convert timer ticks to microseconds
    // Assuming timer is configured with 1MHz (1µs resolution)
    // If different, adjust this conversion based on timer prescaler
    uint16_t pulseWidthUs = static_cast<uint16_t>(pulseWidth);

    // Check if this is a sync pulse
    if (isSyncPulse(pulseWidthUs)) {
        // Sync pulse detected - frame complete
        if (_currentChannel > 0) {
            // Valid frame received
            _channelCount = _currentChannel;
            _lastFrameTime = HAL_GetTick();
            _signalValid = true;
        }
        _currentChannel = 0;
    }
    else if (isValidPulse(pulseWidthUs)) {
        // Valid channel pulse
        if (_currentChannel < PPM_MAX_CHANNELS) {
            _channels[_currentChannel] = pulseWidthUs;
            _currentChannel++;
        }
    }
    else {
        // Invalid pulse - reset frame
        _currentChannel = 0;
    }
}

bool PPMDecoder::isValidPulse(uint16_t pulseWidth) const
{
    return (pulseWidth >= PPM_MIN_PULSE_US) && (pulseWidth <= PPM_MAX_PULSE_US);
}

bool PPMDecoder::isSyncPulse(uint16_t pulseWidth) const
{
    return pulseWidth >= PPM_SYNC_THRESHOLD_US;
}

// === Hardware Interrupt Callback ===

/**
 * @brief Timer input capture callback for PPM decoder
 * Routes TIM2 interrupts to PPMDecoder instance managed by DriverManager
 * Called from stm32f7xx_it.c when TIM2 CH1 input capture triggers
 */
extern "C" void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        auto* ppm = DriverManager::getInstance().getPPMDecoder();
        if (ppm) {
            ppm->handleTimerInterrupt();
        }
    }
}
