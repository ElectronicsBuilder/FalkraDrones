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
 * @file    tone_generator.hpp
 * @brief   Pre-Generated Audio Tone Arrays for Drone Operational Feedback
 * @details Contains pre-calculated audio waveform data for MAX98357A operational tones.
 *          Eliminates real-time computation delays and ensures consistent audio timing
 *          for critical drone feedback systems:
 *          - Startup Tone: Pleasant ascending chord for system initialization
 *          - Failure Tone: Urgent warning beeps for system errors or failures
 *          - Finder Tone: Loud locator beacon for crashed/lost drone recovery
 * 
 * Audio Specifications:
 * - Sample Rate: 8kHz (optimized for speech/tone clarity)
 * - Bit Depth: 16-bit signed samples
 * - Format: Mono PCM data arrays stored in flash memory
 * - Compression: None (raw PCM for minimal processing overhead)
 * 
 * Tone Characteristics:
 * - Startup Tone: 4-note ascending sequence (400→500→600→800Hz) ~700ms
 * - Failure Tone: Double 900Hz beeps + 600Hz warning ~450ms
 * - Finder Tone: Alternating 1200Hz/1500Hz beacon pattern ~3 seconds
 * 
 * Integration Features:
 * - DMA-compatible data format for non-blocking audio playback
 * - Flash-resident arrays to preserve RAM for flight operations
 * - Consistent timing regardless of CPU load or system state
 * - Compatible with FreeRTOS task scheduling
 */

#ifndef __TONE_GENERATOR_HPP
#define __TONE_GENERATOR_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pre-generated tone data (8kHz sample rate, 16-bit samples)
extern const uint16_t startup_tone_data[];
extern const size_t startup_tone_samples;

extern const uint16_t failure_tone_data[];
extern const size_t failure_tone_samples;

extern const uint16_t finder_tone_data[];
extern const size_t finder_tone_samples;

// Tone playback functions using DMA (no blocking delays)
typedef enum {
    TONE_STARTUP = 0,
    TONE_FAILURE,
    TONE_FINDER
} tone_type_t;

typedef struct {
    const uint16_t* data;
    size_t samples;
    const char* name;
} tone_info_t;

/**
 * @brief Get tone information by type
 * @param type Tone type to retrieve
 * @return Pointer to tone info structure
 */
const tone_info_t* get_tone_info(tone_type_t type);

/**
 * @brief Play pre-generated tone using DMA (non-blocking)
 * @param hi2s I2S handle for audio output
 * @param type Type of tone to play
 * @return HAL_OK if started successfully
 */
HAL_StatusTypeDef play_tone_dma(I2S_HandleTypeDef* hi2s, tone_type_t type);

/**
 * @brief Check if any tone is currently playing
 * @return true if tone playback is active
 */
bool is_tone_playing(void);

/**
 * @brief Stop current tone playback
 * @param hi2s I2S handle for audio output
 */
void stop_tone_playback(I2S_HandleTypeDef* hi2s);

/**
 * @brief DMA completion callback (call from I2S complete callback)
 */
void tone_dma_complete_callback(void);

#ifdef __cplusplus
}
#endif

#endif // __TONE_GENERATOR_HPP