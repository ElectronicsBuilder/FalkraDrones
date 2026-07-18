/**
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
 * @file    test_max98357_enhanced.cpp
 * @brief   Enhanced MAX98357 test using AudioManager with WAV file parsing
 * 
 * This demonstrates playing WAV files directly from FFS instead of hardcoded arrays
 */

#include "test_max98357.hpp"
#include "audio_manager.hpp"  // New WAV file audio manager
#include "max98357.hpp"
#include "main.h"
#include "log.hpp"
#include <string.h>
#include "cmsis_os.h"

extern I2S_HandleTypeDef hi2s2;
extern bool playBackComplete;

/**
 * @brief Enhanced audio test using WAV files from filesystem
 */
void test_max98357_wav_files(void) {
    LOG_INFO("=== [MAX98357] Enhanced WAV File Test ===");
   // playBackComplete = false;

    // Initialize MAX98357 amplifier
    //MAX98357 amp(&hi2s2);

        MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };

    MAX98357 amp(cfg);
    amp.init();
    LOG_INFO("[AUDIO] MAX98357 amplifier initialized");
    amp.enable();
    LOG_INFO("[AUDIO] MAX98357 amplifier enabled");
    amp.setLeftChannel();  // Ensure proper channel mode for mono audio
    LOG_INFO("[AUDIO] MAX98357 set to left channel mode");

    
    
    // Initialize Audio Manager
    if (!audio_manager_init()) {
        LOG_ERROR("[AUDIO] Failed to initialize audio manager");
        amp.shutdown();
        return;
    }
    
    // List available WAV files
    char wav_files[10][64];
    int wav_count = audio_list_wav_files(wav_files, 10);
    
    if (wav_count == 0) {
        LOG_WARN("[AUDIO] No WAV files found in filesystem");
        LOG_INFO("[AUDIO] Upload WAV files using the file browser first");
        amp.shutdown();
        return;
    }
    
    LOG_INFO("[AUDIO] Found %d WAV files:", wav_count);
    for (int i = 0; i < wav_count; i++) {
        LOG_INFO("[AUDIO]   %d: %s", i + 1, wav_files[i]);
    }
    
    // Play each WAV file found
    for (int i = 0; i < wav_count; i++) {
        audio_file_t audio_file;
        
        LOG_INFO("[AUDIO] Loading WAV file: %s", wav_files[i]);
        
        // Load and parse WAV file
        if (!audio_load_wav_file(wav_files[i], &audio_file)) {
            LOG_ERROR("[AUDIO] Failed to load WAV file: %s", wav_files[i]);
            continue;
        }
        
        // Validate audio format compatibility
        if (!audio_validate_format(&audio_file)) {
            LOG_ERROR("[AUDIO] Unsupported audio format in: %s", wav_files[i]);
            continue;
        }
        
        LOG_INFO("[AUDIO] Playing: %s", wav_files[i]);
        LOG_INFO("[AUDIO] Format: %dHz, %d-bit, %d channels", 
                 audio_file.sample_rate, audio_file.bits_per_sample, audio_file.channels);
        
        // Play the WAV file
        if (audio_play_wav_file(&audio_file)) {
            // Wait for playback to complete with progress updates
            uint8_t last_progress = 0;
            
            while (audio_is_playing()) {
                uint8_t progress = audio_get_progress(&audio_file);
                
                // Show progress every 10%
                if (progress >= last_progress + 10) {
                    LOG_INFO("[AUDIO] Progress: %d%%", progress);
                    last_progress = progress;
                }
                
                osDelay(100);  // Check every 100ms
            }
            
            LOG_INFO("[AUDIO] Playback complete: %s", wav_files[i]);
        } else {
            LOG_ERROR("[AUDIO] Failed to start playback: %s", wav_files[i]);
        }
        
        // Brief pause between files

           do
            {
                HAL_Delay(1000);
                /* code */
            } while (playBackComplete != true);

        osDelay(1000);
    }
    
    // Cleanup
    audio_stop_playback();
    amp.shutdown();
    amp.disable();
    
    LOG_INFO("=== [MAX98357] Enhanced WAV File Test Complete ===");
}

/**
 * @brief Test specific WAV file by name
 * @param filename WAV file to play
 */
void test_max98357_play_file(const char* filename) {
    LOG_INFO("=== [MAX98357] Playing Specific File: %s ===", filename);
    
    // Initialize MAX98357 amplifier
        MAX98357::Config cfg = {
        .en_port = AUDIO_EN_GPIO_Port,
        .en_pin  = AUDIO_EN_Pin,
        .mode_port = AUDIO_MODE_GPIO_Port,  
        .mode_pin  = AUDIO_MODE_Pin         
    };
    
    MAX98357 amp(cfg);
    amp.init();
    amp.enable();
    
    // Initialize Audio Manager
    if (!audio_manager_init()) {
        LOG_ERROR("[AUDIO] Failed to initialize audio manager");
        amp.shutdown();
        return;
    }
    
    audio_file_t audio_file;
    
    // Load specific WAV file
    if (!audio_load_wav_file(filename, &audio_file)) {
        LOG_ERROR("[AUDIO] Failed to load WAV file: %s", filename);
        amp.shutdown();
        return;
    }
    
    // Validate and play
    if (audio_validate_format(&audio_file) && audio_play_wav_file(&audio_file)) {
        LOG_INFO("[AUDIO] Playing: %s (%dHz, %d-bit, %d channels)", 
                 filename, audio_file.sample_rate, 
                 audio_file.bits_per_sample, audio_file.channels);
        
        // Wait for completion
        while (audio_is_playing()) {
            osDelay(100);
        }
        
        LOG_INFO("[AUDIO] Playback complete");
    } else {
        LOG_ERROR("[AUDIO] Cannot play file: %s", filename);
    }
    
    // Cleanup
    amp.shutdown();
    amp.disable();
    
    LOG_INFO("=== [MAX98357] Specific File Test Complete ===");
}

/**
 * @brief Demo function showing various audio capabilities
 */
void demo_audio_capabilities(void) {
    LOG_INFO("=== [MAX98357] Audio Capabilities Demo ===");
    
    // Example usage scenarios:
    // Files must be located on the FlashFs file system
    // 1. Play startup sound
    LOG_INFO("[DEMO] Scenario 1: Startup Sound");
    test_max98357_play_file("startup.wav");
    osDelay(500);
    
    // 2. Play notification sound  
    LOG_INFO("[DEMO] Scenario 2: Notification");
    test_max98357_play_file("beep.wav");
    osDelay(500);
    
    // 3. Play alert sound
    LOG_INFO("[DEMO] Scenario 3: Alert");
    test_max98357_play_file("alert.wav");
    osDelay(500);
    
    // 4. Play all available files
    LOG_INFO("[DEMO] Scenario 4: Play All Available WAV Files");
    test_max98357_wav_files();
    
    LOG_INFO("=== [MAX98357] Audio Capabilities Demo Complete ===");
}
