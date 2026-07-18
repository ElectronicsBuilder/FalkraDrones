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
 * @file    tone_generator_simple.hpp
 * @brief   Real-time Square Wave Tone Generator for Operational Feedback
 * @details Runtime tone generation system using square wave synthesis for MAX98357A
 *          audio feedback. Generates tones on-demand without pre-calculated arrays,
 *          providing immediate audio feedback for critical drone operations:
 *          - Startup Tone: 4-note ascending sequence for system initialization
 *          - Failure Tone: Urgent warning beeps for critical errors
 *          - Finder Tone: Emergency locator beacon for crashed/lost drones
 * 
 * Technical Implementation:
 * - Real-time square wave synthesis at 8kHz sample rate
 * - Dynamic buffer generation eliminates flash memory usage
 * - DMA-compatible 16-bit signed PCM output format
 * - Non-blocking operation with FreeRTOS task compatibility
 * - Single static buffer reused for all tone types
 * 
 * Tone Specifications:
 * - Startup Tone: 400→500→600→800Hz progression (~700ms total)
 * - Failure Tone: Double 900Hz beeps + 600Hz warning (~450ms total)
 * - Finder Tone: Alternating 1200Hz/1500Hz beacon (~2 seconds)
 * 
 * Performance Characteristics:
 * - Generation Time: <1ms for any tone type
 * - Memory Usage: Single 8KB static buffer (reusable)
 * - CPU Overhead: Minimal after buffer generation
 * - Real-time Compatibility: No blocking operations during synthesis
 * 
 * Integration Features:
 * - Compatible with HAL I2S DMA transmission
 * - Thread-safe operation with status tracking
 * - Automatic cleanup on DMA completion
 * - Comprehensive logging for debugging
 */

#ifndef __TONE_GENERATOR_SIMPLE_HPP
#define __TONE_GENERATOR_SIMPLE_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TONE_SIMPLE_STARTUP = 0,
    TONE_SIMPLE_FAILURE,
    TONE_SIMPLE_FINDER
} simple_tone_type_t;

/**
 * @brief Play generated tone using DMA (non-blocking)
 * @param hi2s I2S handle for audio output
 * @param type Type of tone to play
 * @return HAL_OK if started successfully
 */
HAL_StatusTypeDef play_simple_tone_dma(I2S_HandleTypeDef* hi2s, simple_tone_type_t type);

/**
 * @brief Check if simple tone is currently playing
 * @return true if tone playback is active
 */
bool is_simple_tone_playing(void);

/**
 * @brief Stop current simple tone playback
 * @param hi2s I2S handle for audio output
 */
void stop_simple_tone_playback(I2S_HandleTypeDef* hi2s);

/**
 * @brief DMA completion callback for simple tones
 */
void simple_tone_dma_complete_callback(void);

#ifdef __cplusplus
}
#endif

#endif // __TONE_GENERATOR_SIMPLE_HPP