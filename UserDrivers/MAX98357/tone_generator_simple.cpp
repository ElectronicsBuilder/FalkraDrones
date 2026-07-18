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
 * @file    tone_generator_simple.cpp
 * @brief   Simple tone generation implementation
 */

#include "tone_generator_simple.hpp"
#include "log.hpp"
#include <string.h>

// Buffer for tone generation (large enough for longest tone)
static uint16_t tone_buffer[8000];  // 1 second at 8kHz
static volatile bool simple_tone_playing = false;
static simple_tone_type_t current_simple_tone = TONE_SIMPLE_STARTUP;

// Generate square wave samples
static void generate_square_wave(uint16_t* buffer, uint32_t samples, uint32_t frequency, uint32_t sample_rate) {
    uint32_t samples_per_cycle = sample_rate / frequency;
    uint32_t half_cycle = samples_per_cycle / 2;
    
    for (uint32_t i = 0; i < samples; i++) {
        buffer[i] = ((i % samples_per_cycle) < half_cycle) ? 0xFFFF : 0x0000;
    }
}

// Generate silence samples
static void generate_silence(uint16_t* buffer, uint32_t samples) {
    for (uint32_t i = 0; i < samples; i++) {
        buffer[i] = 0x8000;  // Middle value = silence
    }
}

HAL_StatusTypeDef play_simple_tone_dma(I2S_HandleTypeDef* hi2s, simple_tone_type_t type) {
    if (!hi2s) {
        LOG_ERROR("[SIMPLE_TONE] Invalid I2S handle");
        return HAL_ERROR;
    }
    
    // Stop any current playback
    if (simple_tone_playing) {
        LOG_INFO("[SIMPLE_TONE] Stopping current playback");
        HAL_I2S_DMAStop(hi2s);
        simple_tone_playing = false;
    }
    
    current_simple_tone = type;
    uint32_t total_samples = 0;
    uint32_t offset = 0;
    
    switch (type) {
        case TONE_SIMPLE_STARTUP:
        {
            LOG_INFO("[SIMPLE_TONE] Generating startup tone");
            
            // 400Hz for 150ms (1200 samples)
            generate_square_wave(&tone_buffer[offset], 1200, 400, 8000);
            offset += 1200;
            
            // 25ms silence (200 samples)
            generate_silence(&tone_buffer[offset], 200);
            offset += 200;
            
            // 500Hz for 150ms
            generate_square_wave(&tone_buffer[offset], 1200, 500, 8000);
            offset += 1200;
            
            // 25ms silence
            generate_silence(&tone_buffer[offset], 200);
            offset += 200;
            
            // 600Hz for 150ms
            generate_square_wave(&tone_buffer[offset], 1200, 600, 8000);
            offset += 1200;
            
            // 25ms silence
            generate_silence(&tone_buffer[offset], 200);
            offset += 200;
            
            // 800Hz for 200ms
            generate_square_wave(&tone_buffer[offset], 1600, 800, 8000);
            offset += 1600;
            
            total_samples = offset;
            break;
        }
        
        case TONE_SIMPLE_FAILURE:
        {
            LOG_INFO("[SIMPLE_TONE] Generating failure tone");
            
            // First 900Hz beep for 100ms (800 samples)
            generate_square_wave(&tone_buffer[offset], 800, 900, 8000);
            offset += 800;
            
            // 50ms silence (400 samples)
            generate_silence(&tone_buffer[offset], 400);
            offset += 400;
            
            // Second 900Hz beep for 100ms
            generate_square_wave(&tone_buffer[offset], 800, 900, 8000);
            offset += 800;
            
            // 50ms silence
            generate_silence(&tone_buffer[offset], 400);
            offset += 400;
            
            // 600Hz warning tone for 150ms
            generate_square_wave(&tone_buffer[offset], 1200, 600, 8000);
            offset += 1200;
            
            total_samples = offset;
            break;
        }
        
        case TONE_SIMPLE_FINDER:
        {
            LOG_INFO("[SIMPLE_TONE] Generating finder tone");
            
            // Generate alternating beacon for 2 seconds (shorter for testing)
            for (int i = 0; i < 4; i++) {  // 4 cycles = 2 seconds
                // 1200Hz for 250ms
                generate_square_wave(&tone_buffer[offset], 2000, 1200, 8000);
                offset += 2000;
                
                // 1500Hz for 250ms
                generate_square_wave(&tone_buffer[offset], 2000, 1500, 8000);
                offset += 2000;
            }
            
            total_samples = offset;
            break;
        }
        
        default:
            LOG_ERROR("[SIMPLE_TONE] Unknown tone type: %d", type);
            return HAL_ERROR;
    }
    
    LOG_INFO("[SIMPLE_TONE] Generated %lu samples", (unsigned long)total_samples);
    
    // Start DMA playback
    HAL_StatusTypeDef status = HAL_I2S_Transmit_DMA(hi2s, tone_buffer, total_samples);
    if (status == HAL_OK) {
        simple_tone_playing = true;
        LOG_INFO("[SIMPLE_TONE] DMA started successfully");
    } else {
        LOG_ERROR("[SIMPLE_TONE] Failed to start DMA: %d", status);
    }
    
    return status;
}

bool is_simple_tone_playing(void) {
    return simple_tone_playing;
}

void stop_simple_tone_playback(I2S_HandleTypeDef* hi2s) {
    if (simple_tone_playing && hi2s) {
        LOG_INFO("[SIMPLE_TONE] Stopping tone playback");
        HAL_I2S_DMAStop(hi2s);
        simple_tone_playing = false;
    }
}

void simple_tone_dma_complete_callback(void) {
    if (simple_tone_playing) {
        const char* tone_names[] = {"Startup", "Failure", "Finder"};
        const char* name = (current_simple_tone < 3) ? tone_names[current_simple_tone] : "Unknown";
        LOG_INFO("[SIMPLE_TONE] %s tone playback complete", name);
        simple_tone_playing = false;
    }
}