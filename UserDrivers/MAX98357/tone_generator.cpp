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
 * @file    tone_generator.cpp
 * @brief   Pre-generated tone arrays implementation
 */
#include "tone_generator.hpp"
#include "log.hpp"

// External tone data from tone_data.c
extern const uint16_t startup_tone_data[];
extern const size_t startup_tone_samples;
extern const uint16_t failure_tone_data[];
extern const size_t failure_tone_samples;
extern const uint16_t finder_tone_data[];
extern const size_t finder_tone_samples;

// Playback state
static volatile bool tone_playing = false;
static tone_type_t current_tone_type = TONE_STARTUP;

// Tone information table
static const tone_info_t tone_info_table[] = {
    [TONE_STARTUP] = { startup_tone_data, startup_tone_samples, "Startup" },
    [TONE_FAILURE] = { failure_tone_data, failure_tone_samples, "Failure" },
    [TONE_FINDER]  = { finder_tone_data, finder_tone_samples, "Finder" }
};

const tone_info_t* get_tone_info(tone_type_t type) {
    if (type >= sizeof(tone_info_table) / sizeof(tone_info_table[0])) {
        return nullptr;
    }
    return &tone_info_table[type];
}

HAL_StatusTypeDef play_tone_dma(I2S_HandleTypeDef* hi2s, tone_type_t type) {
    const tone_info_t* tone = get_tone_info(type);
    if (!tone || !hi2s) {
        LOG_ERROR("[TONE] Invalid tone type or I2S handle");
        return HAL_ERROR;
    }
    
    // Stop any current playback
    if (tone_playing) {
        LOG_INFO("[TONE] Stopping current playback");
        HAL_I2S_DMAStop(hi2s);
        tone_playing = false;
    }
    
    // Store current tone info
    current_tone_type = type;
    
    LOG_INFO("[TONE] Starting %s tone (%lu samples)", tone->name, (unsigned long)tone->samples);
    
    // Start DMA playback (non-blocking)
    HAL_StatusTypeDef status = HAL_I2S_Transmit_DMA(hi2s, (uint16_t*)tone->data, tone->samples);
    if (status == HAL_OK) {
        tone_playing = true;
        LOG_INFO("[TONE] DMA started successfully");
    } else {
        LOG_ERROR("[TONE] Failed to start DMA: %d", status);
    }
    
    return status;
}

bool is_tone_playing(void) {
    return tone_playing;
}

void stop_tone_playback(I2S_HandleTypeDef* hi2s) {
    if (tone_playing && hi2s) {
        LOG_INFO("[TONE] Stopping tone playback");
        HAL_I2S_DMAStop(hi2s);
        tone_playing = false;
    }
}

// DMA completion callback (called from I2S DMA complete callback)
void tone_dma_complete_callback(void) {
    if (tone_playing) {
        const tone_info_t* tone = get_tone_info(current_tone_type);
        LOG_INFO("[TONE] %s tone playback complete", tone ? tone->name : "Unknown");
        tone_playing = false;
    }
}