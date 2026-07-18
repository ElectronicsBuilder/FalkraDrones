/**
 * @file    test_max98357.cpp
 * @brief   Audio Amplifier Test Implementation
 * @details Implementation of test suite for MAX98357 audio amplifier testing
 *          and tone generation for system feedback
 * 
 * Part of FalkraController - STM32F767-based drone controller firmware
 * 
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
 */

#include "test_max98357.hpp"
#include "max98357.hpp"
#include "tone_generator_simple.hpp"  // For simple generated tones
#include "main.h"
#include "log.hpp"
#include "audio_manager.hpp"  // For streaming support

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

// Forward declarations (in case of build issues)
extern "C" {
    bool is_simple_tone_playing(void);
    HAL_StatusTypeDef play_simple_tone_dma(I2S_HandleTypeDef* hi2s, simple_tone_type_t type);
    void simple_tone_dma_complete_callback(void);
}

extern I2S_HandleTypeDef hi2s2;

extern const uint16_t wav_data[];
extern const size_t wav_data_len;

extern const uint16_t initcompleted_no_silence_data[];
extern const size_t initcompleted_no_silence_data_len;


extern bool playBackComplete;

static void playSquareWave(I2S_HandleTypeDef* hi2s, uint16_t freq_hz, uint32_t duration_ms, uint32_t sample_rate = 8000)
{
    uint32_t total_samples = (duration_ms * sample_rate) / 1000;
    uint16_t samples_per_cycle = sample_rate / freq_hz;
    uint16_t half_cycle = samples_per_cycle / 2;

    constexpr size_t max_samples = 8000;
    if (total_samples > max_samples) total_samples = max_samples;

    static uint16_t audio_buffer[max_samples];
    for (size_t i = 0; i < total_samples; i++) {
        audio_buffer[i] = (i % samples_per_cycle < half_cycle) ? 0x7FFF : 0x0000;
    }

    HAL_I2S_Transmit(hi2s, audio_buffer, total_samples, HAL_MAX_DELAY);
}

static void playSquareWaveDMA(I2S_HandleTypeDef* hi2s, uint16_t freq_hz, uint32_t duration_ms, uint32_t sample_rate = 8000)
{
    uint32_t total_samples = (duration_ms * sample_rate) / 1000;
    uint16_t samples_per_cycle = sample_rate / freq_hz;
    uint16_t half_cycle = samples_per_cycle / 2;

    constexpr size_t max_samples = 8000;
    if (total_samples > max_samples) total_samples = max_samples;

    static uint16_t audio_buffer[max_samples];
    for (size_t i = 0; i < total_samples; i++) {
        audio_buffer[i] = (i % samples_per_cycle < half_cycle) ? 0x7FFF : 0x0000;
    }

    LOG_INFO("Starting DMA playback: %lu samples at %lu Hz", total_samples, sample_rate);
    

    if (HAL_I2S_Transmit_DMA(hi2s, audio_buffer, total_samples) != HAL_OK)
    {
        LOG_ERROR("[MAX98357] Failed to start DMA transmit");
        return;
    }

    // Optional: Wait for enough time before returning (since callback stops)
    HAL_Delay(duration_ms + 10);  // buffer plays ~duration_ms, pad a bit
}


void  test_max98357()
{
    LOG_INFO("[MAX98357] Starting tone test");

    MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };

    MAX98357 amp(cfg);
    amp.init();
    amp.enable();
    amp.setLeftChannel();

    //playSquareWave(&hi2s2, 1000, 1000, 16000);  // 1 kHz tone, 1 sec, 16 kHz
    playSquareWaveDMA(&hi2s2, 1000, 3000, 8000);  // 1 kHz tone, 1 sec, 16 kHz

    playBackComplete = false;
    
    // HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)wav_data, wav_data_len);

    // do
    // {
    //     HAL_Delay(1000);
    //     /* code */
    // } while (playBackComplete != true);
    
    amp.shutdown();
    amp.disable();

    LOG_INFO("[MAX98357] Tone test complete");
}

void play_startup_tone() 
{
    LOG_INFO("[MAX98357] Playing startup tone - improved version");

    MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };

    MAX98357 amp(cfg);
    amp.init();
    amp.enable();
    amp.setLeftChannel();

    // Generate complete startup sequence in one buffer (no delays!)
    static uint16_t startup_buffer[6000];  // ~700ms at 8kHz
    uint32_t idx = 0;
    
    // 400Hz for 150ms (1200 samples)
    uint32_t samples_400 = 1200;
    for (uint32_t i = 0; i < samples_400; i++) {
        startup_buffer[idx++] = ((i % 20) < 10) ? 0x7FFF : 0x0000;  // 400Hz square wave
    }
    // 25ms gap (200 samples silence)
    for (uint32_t i = 0; i < 200; i++) {
        startup_buffer[idx++] = 0x4000;  // Silence
    }
    // 500Hz for 150ms (1200 samples)
    for (uint32_t i = 0; i < 1200; i++) {
        startup_buffer[idx++] = ((i % 16) < 8) ? 0x7FFF : 0x0000;   // 500Hz square wave
    }
    // 25ms gap
    for (uint32_t i = 0; i < 200; i++) {
        startup_buffer[idx++] = 0x4000;
    }
    // 600Hz for 150ms (1200 samples)
    for (uint32_t i = 0; i < 1200; i++) {
        startup_buffer[idx++] = ((i % 13) < 7) ? 0x7FFF : 0x0000;   // 600Hz square wave
    }
    // 25ms gap
    for (uint32_t i = 0; i < 200; i++) {
        startup_buffer[idx++] = 0x4000;
    }
    // 800Hz for 200ms (1600 samples)
    for (uint32_t i = 0; i < 1600; i++) {
        startup_buffer[idx++] = ((i % 10) < 5) ? 0x7FFF : 0x0000;   // 800Hz square wave
    }
    
    LOG_INFO("[MAX98357] Generated %lu samples for startup tone", (unsigned long)idx);
    
    // Play entire sequence at once
    playBackComplete = false;
    HAL_I2S_Transmit_DMA(&hi2s2, startup_buffer, idx);
    
    // Wait for completion without blocking delays
    while (!playBackComplete) {
        osDelay(10);  // Yield to other tasks
    }

    amp.shutdown();
    amp.disable();
    LOG_INFO("[MAX98357] Startup tone complete");
}

void play_failure_tone()
{
    LOG_INFO("[MAX98357] Playing failure tone - improved version");

    MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };

    MAX98357 amp(cfg);
    amp.init();
    amp.enable();
    amp.setLeftChannel();

    // Generate complete failure sequence in one buffer
    static uint16_t failure_buffer[3600];  // ~450ms at 8kHz
    uint32_t idx = 0;
    
    // First 900Hz beep for 100ms (800 samples)
    for (uint32_t i = 0; i < 800; i++) {
        failure_buffer[idx++] = ((i % 9) < 4) ? 0x7FFF : 0x0000;  // 900Hz square wave
    }
    // 50ms gap (400 samples)
    for (uint32_t i = 0; i < 400; i++) {
        failure_buffer[idx++] = 0x4000;  // Silence
    }
    // Second 900Hz beep for 100ms (800 samples)
    for (uint32_t i = 0; i < 800; i++) {
        failure_buffer[idx++] = ((i % 9) < 4) ? 0x7FFF : 0x0000;  // 900Hz square wave
    }
    // 50ms gap (400 samples)
    for (uint32_t i = 0; i < 400; i++) {
        failure_buffer[idx++] = 0x4000;  // Silence
    }
    // 600Hz warning tone for 150ms (1200 samples)
    for (uint32_t i = 0; i < 1200; i++) {
        failure_buffer[idx++] = ((i % 13) < 7) ? 0x7FFF : 0x0000;  // 600Hz square wave
    }
    
    LOG_INFO("[MAX98357] Generated %lu samples for failure tone", (unsigned long)idx);
    
    // Play entire sequence at once
    playBackComplete = false;
    HAL_I2S_Transmit_DMA(&hi2s2, failure_buffer, idx);
    
    // Wait for completion without blocking delays
    while (!playBackComplete) {
        osDelay(10);  // Yield to other tasks
    }

    amp.shutdown();
    amp.disable();
    LOG_INFO("[MAX98357] Failure tone complete");
}

void play_finder_tone()
{
    LOG_INFO("[MAX98357] Playing finder tone - LOUD locator beacon (improved)");

    MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };

    MAX98357 amp(cfg);
    amp.init();
    amp.enable();
    amp.setLeftChannel();

    // Generate complete finder beacon sequence in one buffer (3 seconds)
    static uint16_t finder_buffer[24000];  // 3 seconds at 8kHz
    uint32_t idx = 0;
    
    // Generate 6 cycles of alternating tones (3 seconds total)
    for (int cycle = 0; cycle < 6; cycle++) {
        // 1200Hz for 250ms (2000 samples)
        for (uint32_t i = 0; i < 2000; i++) {
            finder_buffer[idx++] = ((i % 7) < 3) ? 0x7FFF : 0x0000;  // 1200Hz square wave
        }
        // 1500Hz for 250ms (2000 samples)
        for (uint32_t i = 0; i < 2000; i++) {
            finder_buffer[idx++] = ((i % 5) < 3) ? 0x7FFF : 0x0000;  // 1500Hz square wave
        }
    }
    
    LOG_INFO("[MAX98357] Generated %lu samples for finder tone (3 seconds)", (unsigned long)idx);
    
    // Play entire sequence at once
    playBackComplete = false;
    HAL_I2S_Transmit_DMA(&hi2s2, finder_buffer, idx);
    
    // Wait for completion without blocking delays
    while (!playBackComplete) {
        osDelay(100);  // Check every 100ms for long tone
    }

    amp.shutdown();
    amp.disable();
    LOG_INFO("[MAX98357] Finder tone complete");
}

void test_operational_tones()
{
    LOG_INFO("=== [MAX98357] Operational Tones Demo ===");
    
    LOG_INFO("[DEMO] 1. Startup Tone - Pleasant rising chord");
    play_startup_tone();
    HAL_Delay(1000);
    
    LOG_INFO("[DEMO] 2. Failure Tone - Warning descending tones"); 
    play_failure_tone();
    HAL_Delay(1000);
    
    LOG_INFO("[DEMO] 3. Finder Tone - Loud locator beacon (5 seconds)");
    play_finder_tone();
    
    LOG_INFO("=== [MAX98357] Operational Tones Demo Complete ===");
}

// NOTE: HAL_I2S_TxCpltCallback and HAL_I2S_TxHalfCpltCallback have been moved to
// audio_manager.cpp for proper AudioManager integration and singleton delegation