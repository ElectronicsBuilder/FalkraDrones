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
 * @file    max98357.cpp
 * @brief   MAX98357A I2S Audio Amplifier Driver Implementation
 * @details This driver provides control interface for the MAX98357A digital
 *          audio amplifier used for operational tones and audio feedback in
 *          the AidleyController drone system. Supports enable/disable control
 *          and left/right channel selection via GPIO pins.
 */

#include "max98357.hpp"
#include "log.hpp"
#include "cmsis_os.h"

// Global flag for audio playback completion (used by DMA callbacks)
bool playBackComplete = false;


// Constructor - Initialize with GPIO configuration
MAX98357::MAX98357(const Config& cfg) : cfg_(cfg) {}

// Destructor - Clean up amplifier state
MAX98357::~MAX98357() {
    deinit();
}

// Initialize amplifier - reset enable and mode pins to known state
void MAX98357::init() {
    HAL_GPIO_WritePin(cfg_.en_port, cfg_.en_pin, GPIO_PIN_RESET);     // Disable amplifier
    HAL_GPIO_WritePin(cfg_.mode_port, cfg_.mode_pin, GPIO_PIN_RESET); // Set to right channel
}

// Enable audio amplifier for playback
void MAX98357::enable() {
    HAL_GPIO_WritePin(cfg_.en_port, cfg_.en_pin, GPIO_PIN_SET);
}

// Disable audio amplifier to save power
void MAX98357::disable() {
    HAL_GPIO_WritePin(cfg_.en_port, cfg_.en_pin, GPIO_PIN_RESET);
}

// Configure for left channel output (MODE pin HIGH)
void MAX98357::setLeftChannel() {
    HAL_GPIO_WritePin(cfg_.mode_port, cfg_.mode_pin, GPIO_PIN_SET);  // >1.4V for Left channel
}

// Set amplifier to right channel mode or shutdown state
void MAX98357::shutdown() {
    HAL_GPIO_WritePin(cfg_.mode_port, cfg_.mode_pin, GPIO_PIN_RESET);  // <0.77V for Right channel
}

// Cleanup and reset amplifier state
void MAX98357::deinit() {
    // Disable amplifier
    HAL_GPIO_WritePin(cfg_.en_port, cfg_.en_pin, GPIO_PIN_RESET);

    // Reset mode pin to safe state (right channel)
    HAL_GPIO_WritePin(cfg_.mode_port, cfg_.mode_pin, GPIO_PIN_RESET);

    // Reset playback flag
    playBackComplete = false;

    LOG_SYSSTATUS("[MAX98357] MAX98357A amplifier cleanup complete");
}

