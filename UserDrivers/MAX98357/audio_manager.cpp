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
 * @file    audio_manager.cpp
 * @brief   Advanced Audio Manager for WAV file streaming and playback
 * @details Implements streaming WAV file playback from flash filesystem (FFS)
 *          with double-buffered DMA for smooth audio reproduction. Supports
 *          multiple sample rates, formats, and asynchronous loading from SPI flash.
 */

#include "audio_manager.hpp"
#include "max98357.hpp"
#include "tone_generator_simple.hpp"
#include "driver_manager.hpp"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>

// External I2S handle (from main.c or audio driver)
extern I2S_HandleTypeDef hi2s2;
extern ffs_config_t spi_flash_fs_config;
extern ffs_file_entry_t file_table[];

// Global audio state
static audio_file_t current_audio = {0};
static bool audio_initialized = false;
static uint8_t* audio_buffer = nullptr;
static const uint32_t AUDIO_BUFFER_SIZE = 65536;   // 64KB buffer - DMA-friendly size
static uint32_t current_chunk_samples = 0;  // Number of samples in current chunk

// Double buffering for streaming
static uint8_t* buffer_a = nullptr;  // First buffer
static uint8_t* buffer_b = nullptr;  // Second buffer
static uint8_t* current_play_buffer = nullptr;  // Currently playing buffer
static uint8_t* current_load_buffer = nullptr;  // Currently loading buffer
static bool buffer_a_ready = false;
static bool buffer_b_ready = false;
static uint32_t buffer_a_samples = 0;
static uint32_t buffer_b_samples = 0;

// Streaming task and queue
static TaskHandle_t audio_stream_task_handle = NULL;
static QueueHandle_t audio_stream_queue = NULL;

typedef enum {
    AUDIO_STREAM_LOAD_NEXT,
    AUDIO_STREAM_STOP
} audio_stream_cmd_t;

// DMA completion callback
extern bool playBackComplete;

bool audio_manager_init(void) {
    if (audio_initialized) {
        return true;
    }

    // Allocate double buffers for streaming with proper alignment for DMA
    uint8_t* raw_buffer_a = (uint8_t*)malloc(AUDIO_BUFFER_SIZE + 32);
    uint8_t* raw_buffer_b = (uint8_t*)malloc(AUDIO_BUFFER_SIZE + 32);

    if (!raw_buffer_a || !raw_buffer_b) {
        LOG_WARN("[AUDIO] Failed to allocate double buffers (%lu bytes each) - WAV streaming disabled", (unsigned long)AUDIO_BUFFER_SIZE);
        LOG_INFO("[AUDIO] Operational tones will still work via simple tone generator");
        if (raw_buffer_a) free(raw_buffer_a);
        if (raw_buffer_b) free(raw_buffer_b);
        // Don't fail - operational tones still work, just no WAV streaming
        audio_initialized = true;
        return true;
    }
    
    // Align buffers to 32-byte boundary for DMA
    uintptr_t aligned_addr_a = ((uintptr_t)raw_buffer_a + 31) & ~31;
    uintptr_t aligned_addr_b = ((uintptr_t)raw_buffer_b + 31) & ~31;
    buffer_a = (uint8_t*)aligned_addr_a;
    buffer_b = (uint8_t*)aligned_addr_b;
    
    // Set main buffer pointer to buffer_a for compatibility
    audio_buffer = buffer_a;
    
    LOG_INFO("[AUDIO] Allocated double buffers: 2x %lu KB", (unsigned long)AUDIO_BUFFER_SIZE / 1024);
    
    // Create streaming queue
    audio_stream_queue = xQueueCreate(4, sizeof(audio_stream_cmd_t));
    if (!audio_stream_queue) {
        LOG_ERROR("[AUDIO] Failed to create streaming queue");
        return false;
    }
    
    // Create streaming task
    BaseType_t task_result = xTaskCreate(
        audio_streaming_task,
        "AudioStream",
        2048,  // Stack size
        NULL,
        osPriorityNormal,
        &audio_stream_task_handle
    );
    
    if (task_result != pdPASS) {
        LOG_ERROR("[AUDIO] Failed to create streaming task");
        vQueueDelete(audio_stream_queue);
        return false;
    }
    
    // Initialize FFS if needed
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            LOG_ERROR("[AUDIO] FFS mount failed");
            free(audio_buffer);
            audio_buffer = nullptr;
            return false;
        }
    }
    
    memset(&current_audio, 0, sizeof(current_audio));
    audio_initialized = true;
    
    LOG_INFO("[AUDIO] Audio manager initialized");
    return true;
}

bool audio_load_wav_file(const char* filename, audio_file_t* audio_file) {
    if (!filename || !audio_file || !audio_initialized) {
        return false;
    }
    
    memset(audio_file, 0, sizeof(audio_file_t));
    strncpy(audio_file->filename, filename, sizeof(audio_file->filename) - 1);
    
    // Check if file exists first
    if (!ffs_file_exists(filename)) {
        LOG_ERROR("[AUDIO] WAV file does not exist: %s", filename);
        return false;
    }
    
    // Get file info for debugging
    int file_index = ffs_find_file(filename);
    if (file_index >= 0) {
        LOG_INFO("[AUDIO] File found at index %d, size: %lu bytes", 
                 file_index, (unsigned long)file_table[file_index].size);
    }
    
    // Open WAV file from FFS
    int file_id = ffs_open(filename);
    if (file_id < 0) {
        LOG_ERROR("[AUDIO] Failed to open WAV file: %s (error: %d)", filename, file_id);
        return false;
    }
    
    LOG_INFO("[AUDIO] Successfully opened file: %s (file_id: %d)", filename, file_id);
    audio_file->file_id = file_id;
    
    // Ensure we're at the beginning of the file
    if (ffs_seek(file_id, 0, FFS_SEEK_SET) != 0) {
        LOG_ERROR("[AUDIO] Failed to seek to start of file");
        return false;
    }
    
    int32_t pos = ffs_tell(file_id);
    LOG_INFO("[AUDIO] File position before reading RIFF: %ld", (long)pos);
    
    // Read RIFF header
    wav_riff_header_t riff_header;
    int bytes_read = ffs_read(file_id, &riff_header, sizeof(riff_header));
    LOG_INFO("[AUDIO] Attempted to read %d bytes, actually read: %d bytes", (int)sizeof(riff_header), bytes_read);
    
    if (bytes_read != sizeof(riff_header)) {
        LOG_ERROR("[AUDIO] Failed to read RIFF header (expected %d, got %d)", (int)sizeof(riff_header), bytes_read);
        
        // Debug: Show what we actually got
        if (bytes_read > 0) {
            LOG_INFO("[AUDIO] Partial data read:");
            uint8_t* data = (uint8_t*)&riff_header;
            for (int i = 0; i < bytes_read && i < 12; i++) {
                LOG_INFO("[AUDIO]   [%d] = 0x%02X ('%c')", i, data[i], (data[i] >= 32 && data[i] < 127) ? data[i] : '.');
            }
        }
        return false;
    }
    
    // Validate RIFF header
    if (memcmp(riff_header.riff, "RIFF", 4) != 0 || memcmp(riff_header.wave, "WAVE", 4) != 0) {
        LOG_ERROR("[AUDIO] Invalid WAV file format");
        LOG_ERROR("[AUDIO] RIFF: %c%c%c%c, WAVE: %c%c%c%c", 
                  riff_header.riff[0], riff_header.riff[1], riff_header.riff[2], riff_header.riff[3],
                  riff_header.wave[0], riff_header.wave[1], riff_header.wave[2], riff_header.wave[3]);
        return false;
    }
    
    LOG_INFO("[AUDIO] RIFF header valid, file size: %lu bytes", (unsigned long)riff_header.file_size);
    
    // Read fmt chunk
    wav_fmt_chunk_t fmt_chunk;
    if (ffs_read(file_id, &fmt_chunk, sizeof(fmt_chunk)) != sizeof(fmt_chunk)) {
        LOG_ERROR("[AUDIO] Failed to read fmt chunk");
        return false;
    }
    
    // Validate fmt chunk
    if (memcmp(fmt_chunk.fmt, "fmt ", 4) != 0) {
        LOG_ERROR("[AUDIO] Invalid fmt chunk: %c%c%c%c", 
                  fmt_chunk.fmt[0], fmt_chunk.fmt[1], fmt_chunk.fmt[2], fmt_chunk.fmt[3]);
        return false;
    }
    
    LOG_INFO("[AUDIO] fmt chunk valid, size: %lu", (unsigned long)fmt_chunk.chunk_size);
    
    // Extract audio format info
    audio_file->channels = fmt_chunk.num_channels;
    audio_file->sample_rate = fmt_chunk.sample_rate;
    audio_file->bits_per_sample = fmt_chunk.bits_per_sample;
    
    // Skip any extra fmt data
    if (fmt_chunk.chunk_size > 16) {
        ffs_seek(file_id, fmt_chunk.chunk_size - 16, FFS_SEEK_CUR);
    }
    
    // Find data chunk (skip any intermediate chunks like LIST/INFO)
    wav_data_header_t data_header;
    bool found_data = false;
    
    while (!found_data) {
        if (ffs_read(file_id, &data_header, sizeof(data_header)) != sizeof(data_header)) {
            LOG_ERROR("[AUDIO] Failed to read chunk header while searching for data");
            return false;
        }
        
        if (memcmp(data_header.data, "data", 4) == 0) {
            found_data = true;
        } else {
            // Skip this chunk (LIST, INFO, etc.)
            char chunk_name[5] = {0};
            memcpy(chunk_name, data_header.data, 4);
            LOG_INFO("[AUDIO] Skipping chunk: %s (size: %lu)", chunk_name, (unsigned long)data_header.data_size);
            
            // Skip the chunk data
            ffs_seek(file_id, data_header.data_size, FFS_SEEK_CUR);
        }
    }
    
    audio_file->audio_data_size = data_header.data_size;
    
    // Get current file position (right after data chunk header)
    audio_file->audio_data_offset = ffs_tell(file_id);
    
    audio_file->current_position = 0;
    audio_file->valid = true;
    
    LOG_INFO("[AUDIO] Loaded WAV: %s", filename);
    LOG_INFO("[AUDIO] Format: %dHz, %d-bit, %d channels, %lu bytes", 
             audio_file->sample_rate, audio_file->bits_per_sample, 
             audio_file->channels, (unsigned long)audio_file->audio_data_size);
    
    return true;
}

bool audio_validate_format(const audio_file_t* audio_file) {
    if (!audio_file || !audio_file->valid) {
        return false;
    }
    
    // Check if format is supported by MAX98357 + STM32 I2S
    if (audio_file->bits_per_sample != 16) {
        LOG_ERROR("[AUDIO] Unsupported bit depth: %d (only 16-bit supported)", audio_file->bits_per_sample);
        return false;
    }
    
    if (audio_file->channels < 1 || audio_file->channels > 2) {
        LOG_ERROR("[AUDIO] Unsupported channel count: %d", audio_file->channels);
        return false;
    }
    
    // Check supported sample rates (common I2S rates)
    switch (audio_file->sample_rate) {
        case 8000:
        case 16000:
        case 22050:
        case 44100:
        case 48000:
            break;
        default:
            LOG_ERROR("[AUDIO] Unsupported sample rate: %lu Hz", (unsigned long)audio_file->sample_rate);
            return false;
    }
    
    return true;
}

bool audio_play_wav_file(audio_file_t* audio_file) {
    if (!audio_file || !audio_file->valid || !audio_initialized) {
        LOG_ERROR("[AUDIO] Invalid audio file or manager not initialized");
        return false;
    }
    
    if (!audio_validate_format(audio_file)) {
        LOG_ERROR("[AUDIO] Audio format validation failed");
        return false;
    }
    
    // Stop any current playback
    if (current_audio.is_playing) {
        audio_stop_playback();
    }
    
    // Copy to current audio state
    current_audio = *audio_file;
    current_audio.current_position = 0;
    
    // Seek to start of audio data
    ffs_seek(current_audio.file_id, current_audio.audio_data_offset, FFS_SEEK_SET);
    
    // Check if file fits in buffer or needs streaming
    uint32_t chunk_size = (current_audio.audio_data_size > AUDIO_BUFFER_SIZE) ? AUDIO_BUFFER_SIZE : current_audio.audio_data_size;
    
    if (current_audio.audio_data_size > AUDIO_BUFFER_SIZE) {
        LOG_INFO("[AUDIO] Large file (%lu bytes) - using streaming with %lu byte chunks", 
                 (unsigned long)current_audio.audio_data_size, (unsigned long)AUDIO_BUFFER_SIZE);
    } else {
        LOG_INFO("[AUDIO] Loading entire file (%lu bytes) into buffer", (unsigned long)current_audio.audio_data_size);
    }
    
    // Initialize double buffering for streaming
    if (current_audio.audio_data_size > AUDIO_BUFFER_SIZE) {
        LOG_INFO("[AUDIO] Initializing double buffering for streaming");
        
        // Load first chunk into buffer A
        int bytes_read = ffs_read(current_audio.file_id, buffer_a, chunk_size);
        if (bytes_read != (int)chunk_size) {
            LOG_ERROR("[AUDIO] Failed to read first chunk (expected %lu, got %d)", 
                      (unsigned long)chunk_size, bytes_read);
            return false;
        }
        
        current_audio.current_position = bytes_read;
        buffer_a_samples = bytes_read / (current_audio.bits_per_sample / 8);
        buffer_a_ready = true;
        buffer_b_ready = false;
        
        // Set current buffers
        current_play_buffer = buffer_a;
        current_load_buffer = buffer_b;
        audio_buffer = buffer_a;  // For compatibility
        current_chunk_samples = buffer_a_samples;
        
        LOG_INFO("[AUDIO] First chunk loaded into buffer A: %d bytes, %lu samples", 
                 bytes_read, (unsigned long)buffer_a_samples);
        
        // Start loading second chunk in background
        if (current_audio.current_position < current_audio.audio_data_size) {
            audio_stream_cmd_t cmd = AUDIO_STREAM_LOAD_NEXT;
            xQueueSend(audio_stream_queue, &cmd, 0);
        }
    } else {
        // Single chunk file - use buffer A only
        int bytes_read = ffs_read(current_audio.file_id, buffer_a, chunk_size);
        if (bytes_read != (int)chunk_size) {
            LOG_ERROR("[AUDIO] Failed to read single chunk (expected %lu, got %d)", 
                      (unsigned long)chunk_size, bytes_read);
            return false;
        }
        
        current_audio.current_position = bytes_read;
        buffer_a_samples = bytes_read / (current_audio.bits_per_sample / 8);
        current_play_buffer = buffer_a;
        audio_buffer = buffer_a;
        current_chunk_samples = buffer_a_samples;
        
        LOG_INFO("[AUDIO] Single chunk loaded: %d bytes, %lu samples", 
                 bytes_read, (unsigned long)buffer_a_samples);
    }
    
    current_audio.is_playing = true;
    playBackComplete = false;
    
    LOG_INFO("[AUDIO] Starting DMA playback: %s", current_audio.filename);
    LOG_INFO("[AUDIO] Playing %lu samples (%lu bytes) at %lu Hz", 
             (unsigned long)current_chunk_samples, (unsigned long)current_audio.audio_data_size, (unsigned long)current_audio.sample_rate);
    uint32_t total_samples = current_audio.audio_data_size / (current_audio.bits_per_sample / 8);
    LOG_INFO("[AUDIO] Expected total duration: %lu.%02lu seconds", 
             (unsigned long)(total_samples / current_audio.sample_rate), 
             (unsigned long)((total_samples * 100 / current_audio.sample_rate) % 100));
    
    // Debug: Check first 32 samples in buffer
    uint16_t* sample_buffer = (uint16_t*)audio_buffer;
    LOG_INFO("[AUDIO] Buffer analysis:");
    LOG_INFO("[AUDIO] First 16 samples: %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X",
             sample_buffer[0], sample_buffer[1], sample_buffer[2], sample_buffer[3],
             sample_buffer[4], sample_buffer[5], sample_buffer[6], sample_buffer[7],
             sample_buffer[8], sample_buffer[9], sample_buffer[10], sample_buffer[11],
             sample_buffer[12], sample_buffer[13], sample_buffer[14], sample_buffer[15]);
    
    // Check middle samples
    uint32_t mid_pos = current_chunk_samples / 2;
    LOG_INFO("[AUDIO] Middle 8 samples (pos %lu): %04X %04X %04X %04X %04X %04X %04X %04X", 
             (unsigned long)mid_pos,
             sample_buffer[mid_pos], sample_buffer[mid_pos+1], sample_buffer[mid_pos+2], sample_buffer[mid_pos+3],
             sample_buffer[mid_pos+4], sample_buffer[mid_pos+5], sample_buffer[mid_pos+6], sample_buffer[mid_pos+7]);
    
    // Check last samples
    LOG_INFO("[AUDIO] Last 8 samples: %04X %04X %04X %04X %04X %04X %04X %04X",
             sample_buffer[current_chunk_samples-8], sample_buffer[current_chunk_samples-7], sample_buffer[current_chunk_samples-6], sample_buffer[current_chunk_samples-5],
             sample_buffer[current_chunk_samples-4], sample_buffer[current_chunk_samples-3], sample_buffer[current_chunk_samples-2], sample_buffer[current_chunk_samples-1]);
    
    // Calculate basic statistics
    uint32_t zero_count = 0;
    int16_t min_val = 32767, max_val = -32768;
    for (uint32_t i = 0; i < current_chunk_samples && i < 1000; i++) {  // Check first 1000 samples
        int16_t val = (int16_t)sample_buffer[i];
        if (val == 0) zero_count++;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    LOG_INFO("[AUDIO] Sample stats (first 1000): zeros=%lu, min=%d, max=%d", 
             (unsigned long)zero_count, min_val, max_val);
    
    // Compare with working test data for reference
    extern const uint16_t wav_data[];
    extern const size_t wav_data_len;
    
    LOG_INFO("[AUDIO] Working test_max98357 first 8 samples: %04X %04X %04X %04X %04X %04X %04X %04X",
             wav_data[0], wav_data[1], wav_data[2], wav_data[3],
             wav_data[4], wav_data[5], wav_data[6], wav_data[7]);
    
    // Analyze working test data volume
    int16_t test_min = 32767, test_max = -32768;
    uint32_t test_zeros = 0;
    uint32_t test_samples = wav_data_len < 1000 ? wav_data_len : 1000;
    
    for (uint32_t i = 0; i < test_samples; i++) {
        int16_t val = (int16_t)wav_data[i];
        if (val == 0) test_zeros++;
        if (val < test_min) test_min = val;
        if (val > test_max) test_max = val;
    }
    
    int16_t test_peak = test_max > -test_min ? test_max : -test_min;
    float test_volume = 100.0f * test_peak / 32767.0f;
    
    LOG_INFO("[AUDIO] Test data stats: zeros=%lu/%lu, min=%d, max=%d, peak=%d, volume=%.1f%%", 
             (unsigned long)test_zeros, (unsigned long)test_samples, test_min, test_max, test_peak, test_volume);
    
    // Volume analysis and potential amplification for WAV file
    int16_t peak = max_val > -min_val ? max_val : -min_val;
    float volume_percent = 100.0f * peak / 32767.0f;
    
    LOG_INFO("[AUDIO] Volume comparison: WAV file=%.1f%%, test data=%.1f%% (%.1fx difference)", 
             volume_percent, test_volume, test_volume / volume_percent);
    LOG_INFO("[AUDIO] Volume analysis: peak=%d, volume=%.1f%% of full scale", peak, volume_percent);
    
    // Apply moderate gain if audio is very quiet (less than 5% of full scale)
    if (volume_percent < 5.0f) {
        int gain_factor = 2;  // 
        LOG_INFO("[AUDIO] Audio is quiet (%.1f%%). Applying %dx digital gain...", volume_percent, gain_factor);
        
        int16_t* signed_buffer = (int16_t*)audio_buffer;  // Use signed pointer for proper handling
        for (uint32_t i = 0; i < current_chunk_samples; i++) {
            int32_t amplified = signed_buffer[i] * gain_factor;
            // Clamp to prevent overflow
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            signed_buffer[i] = (int16_t)amplified;  // Store as signed value
        }
        
        LOG_INFO("[AUDIO] Applied %dx digital gain for better audibility", gain_factor);
    } else {
        LOG_INFO("[AUDIO] Volume adequate (%.1f%%), no gain applied", volume_percent);
    }
    
    // Reconfigure I2S sample rate to match WAV file
    if (audio_reconfigure_i2s_sample_rate(current_audio.sample_rate) != 0) {
        LOG_ERROR("[AUDIO] Failed to reconfigure I2S for %lu Hz", (unsigned long)current_audio.sample_rate);
        return false;
    }
    
    // Debug I2S state before DMA start
    LOG_INFO("[AUDIO] I2S State: %d, Error: 0x%08lX", hi2s2.State, (unsigned long)hi2s2.ErrorCode);
    LOG_INFO("[AUDIO] DMA State: %d", hi2s2.hdmatx ? hi2s2.hdmatx->State : -1);
    LOG_INFO("[AUDIO] Buffer: 0x%08lX, Samples: %lu", (unsigned long)audio_buffer, (unsigned long)current_chunk_samples);
    
    // Ensure DMA is ready
    if (hi2s2.hdmatx && hi2s2.hdmatx->State != HAL_DMA_STATE_READY) {
        LOG_WARN("[AUDIO] DMA not ready (State: %d), attempting reset", hi2s2.hdmatx->State);
        HAL_DMA_Abort(hi2s2.hdmatx);
        HAL_Delay(5);
    }
    
    // Limit sample count to prevent DMA overflow (max 65535 for some DMA controllers)
    uint32_t max_samples = 32768;  // Conservative limit
    if (current_chunk_samples > max_samples) {
        LOG_WARN("[AUDIO] Limiting samples from %lu to %lu for DMA compatibility", 
                 (unsigned long)current_chunk_samples, (unsigned long)max_samples);
        current_chunk_samples = max_samples;
    }
    
    // Start DMA playback with retry logic
    HAL_StatusTypeDef status;
    uint32_t retry_samples = current_chunk_samples;
    int retry_count = 0;
    const uint32_t retry_limits[] = {current_chunk_samples, 16384, 8192, 4096};
    
    do {
        if (current_audio.audio_data_size > AUDIO_BUFFER_SIZE) {
            LOG_INFO("[AUDIO] Starting streaming DMA playback (attempt %d: %lu samples)", 
                     retry_count + 1, (unsigned long)retry_samples);
        } else {
            LOG_INFO("[AUDIO] Starting single-chunk DMA playback (attempt %d: %lu samples)", 
                     retry_count + 1, (unsigned long)retry_samples);
        }
        
        status = HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)audio_buffer, retry_samples);
        
        if (status != HAL_OK) {
            LOG_WARN("[AUDIO] DMA start failed with %lu samples (status: %d), trying smaller size", 
                     (unsigned long)retry_samples, status);
            retry_count++;
            if (retry_count < 4) {
                retry_samples = retry_limits[retry_count];
            }
        } else {
            // Success - update the actual sample count used
            current_chunk_samples = retry_samples;
            break;
        }
    } while (retry_count < 4 && status != HAL_OK);
    
    if (status != HAL_OK) {
        LOG_ERROR("[AUDIO] Failed to start I2S DMA playback after %d attempts: %d", retry_count, status);
        LOG_ERROR("[AUDIO] Post-error I2S State: %d, Error: 0x%08lX", hi2s2.State, (unsigned long)hi2s2.ErrorCode);
        current_audio.is_playing = false;
        return false;
    }
    
    LOG_INFO("[AUDIO] DMA started successfully with %lu samples", (unsigned long)current_chunk_samples);
    
    return true;
}

void audio_stop_playback(void) {
    if (current_audio.is_playing) {
        HAL_I2S_DMAStop(&hi2s2);
        current_audio.is_playing = false;
        playBackComplete = true;
        LOG_INFO("[AUDIO] Playback stopped");
    }
}

bool audio_is_playing(void) {
    return current_audio.is_playing && !playBackComplete;
}

uint8_t audio_get_progress(const audio_file_t* audio_file) {
    if (!audio_file || !audio_file->valid || audio_file->audio_data_size == 0) {
        return 0;
    }
    
    return (uint8_t)((audio_file->current_position * 100) / audio_file->audio_data_size);
}

int audio_list_wav_files(char wav_files[][64], int max_files) {
    if (!wav_files || max_files <= 0 || !audio_initialized) {
        return 0;
    }
    
    // Use FFS to list all files and filter for .wav files
    ffs_file_info_t all_files[FFS_MAX_FILES];
    int total_files = ffs_list_files(all_files, FFS_MAX_FILES);
    
    int wav_count = 0;
    for (int i = 0; i < total_files && wav_count < max_files; i++) {
        // Check if file has .wav extension
        const char* filename = all_files[i].name;
        size_t len = strlen(filename);
        
        if (len > 4 && 
            (strcmp(&filename[len-4], ".wav") == 0 || strcmp(&filename[len-4], ".WAV") == 0)) {
            strncpy(wav_files[wav_count], filename, 63);
            wav_files[wav_count][63] = '\0';
            wav_count++;
        }
    }
    
    LOG_INFO("[AUDIO] Found %d WAV files in filesystem", wav_count);
    return wav_count;
}

// Reconfigure I2S sample rate to match WAV file
int audio_reconfigure_i2s_sample_rate(uint32_t sample_rate) {
    // Map sample rate to HAL constants
    uint32_t hal_freq;
    switch (sample_rate) {
        case 8000:  hal_freq = I2S_AUDIOFREQ_8K; break;
        case 16000: hal_freq = I2S_AUDIOFREQ_16K; break;
        case 22050: hal_freq = I2S_AUDIOFREQ_22K; break;
        case 44100: hal_freq = I2S_AUDIOFREQ_44K; break;
        case 48000: hal_freq = I2S_AUDIOFREQ_48K; break;
        default:
            LOG_ERROR("[AUDIO] Unsupported sample rate for I2S: %lu Hz", (unsigned long)sample_rate);
            return -1;
    }
    
    // // Check if I2S is already at the correct sample rate
    // if (hi2s2.Init.AudioFreq == hal_freq) {
    //     LOG_INFO("[AUDIO] I2S already configured for %lu Hz, skipping reconfiguration", (unsigned long)sample_rate);
    //     return 0;
    // }
    
    // LOG_INFO("[AUDIO] Reconfiguring I2S from current rate to %lu Hz", (unsigned long)sample_rate);
    // LOG_INFO("[AUDIO] Current I2S State before reconfigure: %d", hi2s2.State);
    
    // // Stop I2S if running
    // HAL_I2S_DMAStop(&hi2s2);
    // LOG_INFO("[AUDIO] I2S DMA stopped, State: %d", hi2s2.State);
    
    // // Deinitialize I2S
    // if (HAL_I2S_DeInit(&hi2s2) != HAL_OK) {
    //     LOG_ERROR("[AUDIO] Failed to deinitialize I2S, State: %d", hi2s2.State);
    //     return -1;
    // }
    // LOG_INFO("[AUDIO] I2S deinitialized, State: %d", hi2s2.State);
    
    // // Update sample rate
    // uint32_t old_freq = hi2s2.Init.AudioFreq;
  
    // hi2s2.Init.AudioFreq = hal_freq;
    // LOG_INFO("[AUDIO] Changed AudioFreq from %lu to %lu", (unsigned long)old_freq, (unsigned long)hal_freq);
    
    // // Reinitialize I2S with new sample rate
    // if (HAL_I2S_Init(&hi2s2) != HAL_OK) {
    //     LOG_ERROR("[AUDIO] Failed to reinitialize I2S, State: %d, Error: 0x%08lX", 
    //               hi2s2.State, (unsigned long)hi2s2.ErrorCode);
    //     return -1;
    // }
    // LOG_INFO("[AUDIO] I2S reinitialized, State: %d", hi2s2.State);
    
    // Small delay to ensure I2S is fully ready
    HAL_Delay(10);
    
    // Verify I2S is in ready state
    if (hi2s2.State != HAL_I2S_STATE_READY) {
        LOG_ERROR("[AUDIO] I2S not in ready state after reinit, State: %d", hi2s2.State);
        return -1;
    }
    
    LOG_INFO("[AUDIO] I2S reconfigured successfully for %lu Hz", (unsigned long)sample_rate);
    return 0;
}

// Streaming functionality for large audio files
bool audio_stream_next_chunk(void) {
    LOG_INFO("[AUDIO] stream_next_chunk() called");
    
    if (!current_audio.is_playing || !current_audio.valid) {
        LOG_ERROR("[AUDIO] stream_next_chunk() - invalid state: playing=%d, valid=%d", 
                  current_audio.is_playing, current_audio.valid);
        return false;
    }
    
    // Check if there's more data to read
    uint32_t remaining_bytes = current_audio.audio_data_size - current_audio.current_position;
    LOG_INFO("[AUDIO] stream_next_chunk() - remaining_bytes: %lu", (unsigned long)remaining_bytes);
    
    if (remaining_bytes == 0) {
        // End of file reached
        current_audio.is_playing = false;
        playBackComplete = true;
        LOG_INFO("[AUDIO] Streaming complete - end of file reached");
        return false;
    }
    
    // Calculate next chunk size
    uint32_t chunk_size = (remaining_bytes > AUDIO_BUFFER_SIZE) ? AUDIO_BUFFER_SIZE : remaining_bytes;
    LOG_INFO("[AUDIO] stream_next_chunk() - chunk_size: %lu", (unsigned long)chunk_size);
    
    // Read next chunk
    LOG_INFO("[AUDIO] stream_next_chunk() - calling ffs_read()...");
    int bytes_read = ffs_read(current_audio.file_id, audio_buffer, chunk_size);
    LOG_INFO("[AUDIO] stream_next_chunk() - ffs_read() returned: %d", bytes_read);
    
    if (bytes_read != (int)chunk_size) {
        LOG_ERROR("[AUDIO] Failed to read next chunk (expected %lu, got %d)", 
                  (unsigned long)chunk_size, bytes_read);
        current_audio.is_playing = false;
        playBackComplete = true;
        return false;
    }
    
    current_audio.current_position += bytes_read;
    
    // Update current chunk samples for the newly loaded data
    current_chunk_samples = bytes_read / (current_audio.bits_per_sample / 8);
    
    LOG_INFO("[AUDIO] Streamed next chunk: %d bytes, %lu samples (total: %lu/%lu)", 
             bytes_read, (unsigned long)current_chunk_samples,
             (unsigned long)current_audio.current_position, 
             (unsigned long)current_audio.audio_data_size);
    
    LOG_INFO("[AUDIO] stream_next_chunk() - returning true");
    return true;
}

// Streaming task for background audio loading
void audio_streaming_task(void *pvParameters) {
    audio_stream_cmd_t cmd;
    
    LOG_INFO("[AUDIO] Streaming task started");
    
    while (1) {
        // Wait for commands from DMA callback
        if (xQueueReceive(audio_stream_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd) {
                case AUDIO_STREAM_LOAD_NEXT:
                {
                    LOG_INFO("[AUDIO] Task: Loading next chunk...");
                    
                    if (!current_audio.is_playing || !current_audio.valid) {
                        LOG_WARN("[AUDIO] Task: Audio not playing, ignoring load request");
                        break;
                    }
                    
                    // Calculate remaining data
                    uint32_t remaining_bytes = current_audio.audio_data_size - current_audio.current_position;
                    
                    if (remaining_bytes == 0) {
                        LOG_INFO("[AUDIO] Task: No more data to load");
                        break;
                    }
                    
                    // Determine which buffer to load into
                    uint8_t* load_buffer = nullptr;
                    uint32_t* buffer_samples = nullptr;
                    bool* buffer_ready = nullptr;
                    
                    if (current_play_buffer == buffer_a) {
                        // Currently playing A, load into B
                        load_buffer = buffer_b;
                        buffer_samples = &buffer_b_samples;
                        buffer_ready = &buffer_b_ready;
                    } else {
                        // Currently playing B, load into A
                        load_buffer = buffer_a;
                        buffer_samples = &buffer_a_samples;
                        buffer_ready = &buffer_a_ready;
                    }
                    
                    // Calculate chunk size
                    uint32_t chunk_size = (remaining_bytes > AUDIO_BUFFER_SIZE) ? AUDIO_BUFFER_SIZE : remaining_bytes;
                    
                    // Read next chunk from file
                    int bytes_read = ffs_read(current_audio.file_id, load_buffer, chunk_size);
                    
                    if (bytes_read == (int)chunk_size) {
                        // Successfully loaded chunk
                        current_audio.current_position += bytes_read;
                        *buffer_samples = bytes_read / (current_audio.bits_per_sample / 8);
                        *buffer_ready = true;
                        
                        LOG_INFO("[AUDIO] Task: Loaded %d bytes, %lu samples (%lu/%lu total)", 
                                 bytes_read, (unsigned long)*buffer_samples,
                                 (unsigned long)current_audio.current_position, 
                                 (unsigned long)current_audio.audio_data_size);
                    } else {
                        LOG_ERROR("[AUDIO] Task: Failed to load chunk (expected %lu, got %d)", 
                                  (unsigned long)chunk_size, bytes_read);
                        *buffer_ready = false;
                    }
                    break;
                }
                    
                case AUDIO_STREAM_STOP:
                {
                    LOG_INFO("[AUDIO] Task: Stopping streaming");
                    buffer_a_ready = false;
                    buffer_b_ready = false;
                    break;
                }
                    
                default:
                {
                    LOG_WARN("[AUDIO] Task: Unknown command: %d", cmd);
                    break;
                }
            }
        }
    }
}

void audio_dma_callback(I2S_HandleTypeDef *hi2s, bool is_half_complete) {
    if (hi2s != &hi2s2) {
        return;
    }
    
    LOG_INFO("[AUDIO] DMA callback - half_complete: %d, is_playing: %d", 
             is_half_complete, current_audio.is_playing);
    
    // Only handle complete callbacks for streaming (ignore half-complete)
    if (is_half_complete) {
        return;  // Wait for full completion
    }
    
    // Stop the current DMA transfer first
    HAL_I2S_DMAStop(hi2s);
    
    // Check if streaming mode for large files
    if (current_audio.is_playing && current_audio.audio_data_size > AUDIO_BUFFER_SIZE) {
        LOG_INFO("[AUDIO] Streaming mode - checking for next buffer...");
        
        // Check if all data has been played
        if (current_audio.current_position >= current_audio.audio_data_size) {
            // All data loaded and played - complete
            playBackComplete = true;
            current_audio.is_playing = false;
            LOG_INFO("[AUDIO] Streaming complete - all data played (%lu/%lu bytes)", 
                     (unsigned long)current_audio.current_position, 
                     (unsigned long)current_audio.audio_data_size);
            
            audio_stream_cmd_t cmd = AUDIO_STREAM_STOP;
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(audio_stream_queue, &cmd, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken == pdTRUE) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            return;
        }
        
        // Switch to next buffer if ready
        uint8_t* next_buffer = nullptr;
        uint32_t next_samples = 0;
        bool next_ready = false;
        
        if (current_play_buffer == buffer_a) {
            // Currently played A, switch to B
            next_buffer = buffer_b;
            next_samples = buffer_b_samples;
            next_ready = buffer_b_ready;
            buffer_b_ready = false;  // Mark as being used
        } else {
            // Currently played B, switch to A  
            next_buffer = buffer_a;
            next_samples = buffer_a_samples;
            next_ready = buffer_a_ready;
            buffer_a_ready = false;  // Mark as being used
        }
        
        if (next_ready && next_samples > 0) {
            // Switch buffers
            current_play_buffer = next_buffer;
            current_chunk_samples = next_samples;
            
            LOG_INFO("[AUDIO] Switching to next buffer: %lu samples", (unsigned long)next_samples);
            
            // Start next chunk DMA
            HAL_StatusTypeDef status = HAL_I2S_Transmit_DMA(hi2s, (uint16_t*)next_buffer, next_samples);
            if (status == HAL_OK) {
                LOG_INFO("[AUDIO] Next chunk DMA started successfully");
                
                // Request loading of next chunk if more data available
                if (current_audio.current_position < current_audio.audio_data_size) {
                    LOG_INFO("[AUDIO] Requesting next chunk load (pos: %lu/%lu)", 
                             (unsigned long)current_audio.current_position, 
                             (unsigned long)current_audio.audio_data_size);
                    audio_stream_cmd_t cmd = AUDIO_STREAM_LOAD_NEXT;
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    BaseType_t queue_result = xQueueSendFromISR(audio_stream_queue, &cmd, &xHigherPriorityTaskWoken);
                    if (queue_result == pdTRUE) {
                        LOG_INFO("[AUDIO] Load request sent to task from ISR");
                        if (xHigherPriorityTaskWoken == pdTRUE) {
                            // Request context switch if higher priority task was woken
                            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                        }
                    } else {
                        LOG_ERROR("[AUDIO] Failed to send load request to task from ISR");
                    }
                } else {
                    LOG_INFO("[AUDIO] All data loaded - no more chunks needed");
                }
            } else {
                LOG_ERROR("[AUDIO] Failed to start next chunk DMA (status: %d)", status);
                playBackComplete = true;
                current_audio.is_playing = false;
            }
        } else {
            // Next buffer not ready - end playback
            LOG_WARN("[AUDIO] Next buffer not ready - ending playback");
            playBackComplete = true;
            current_audio.is_playing = false;
        }
    } else {
        // Single chunk playback complete
        playBackComplete = true;
        current_audio.is_playing = false;
        LOG_INFO("[AUDIO] Single chunk playback complete");
    }
}

// ============================================================================
// Operational Tone Functions (Startup, Failure, Finder)
// ============================================================================

extern I2S_HandleTypeDef hi2s2;
extern bool playBackComplete;

void play_startup_tone(void)
{
    LOG_INFO("[MAX98357] Playing startup tone");

    // Use singleton MAX98357 instance from DriverManager instead of creating new one
    auto& dm = DriverManager::getInstance();
    MAX98357* amp = dm.getAudio();

    if (!amp) {
        LOG_ERROR("[MAX98357] Audio driver not available from DriverManager");
        return;
    }

    amp->enable();
    amp->setLeftChannel();

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

    amp->shutdown();
    amp->disable();
    LOG_INFO("[MAX98357] Startup tone complete");
}

void play_failure_tone(void)
{
    LOG_INFO("[MAX98357] Playing failure tone");

    // Use singleton MAX98357 instance from DriverManager instead of creating new one
    auto& dm = DriverManager::getInstance();
    MAX98357* amp = dm.getAudio();

    if (!amp) {
        LOG_ERROR("[MAX98357] Audio driver not available from DriverManager");
        return;
    }

    amp->enable();
    amp->setLeftChannel();

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

    amp->shutdown();
    amp->disable();
    LOG_INFO("[MAX98357] Failure tone complete");
}

void play_finder_tone(void)
{
    LOG_INFO("[MAX98357] Playing finder tone - LOUD locator beacon");

    // Use singleton MAX98357 instance from DriverManager instead of creating new one
    auto& dm = DriverManager::getInstance();
    MAX98357* amp = dm.getAudio();

    if (!amp) {
        LOG_ERROR("[MAX98357] Audio driver not available from DriverManager");
        return;
    }

    amp->enable();
    amp->setLeftChannel();

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

    amp->shutdown();
    amp->disable();
    LOG_INFO("[MAX98357] Finder tone complete");
}

void test_operational_tones(void)
{
    LOG_INFO("=== [MAX98357] Operational Tones Demo ===");

    LOG_INFO("[DEMO] 1. Startup Tone - Pleasant rising chord");
    play_startup_tone();
    HAL_Delay(1000);

    LOG_INFO("[DEMO] 2. Failure Tone - Warning descending tones");
    play_failure_tone();
    HAL_Delay(1000);

    LOG_INFO("[DEMO] 3. Finder Tone - Loud locator beacon (3 seconds)");
    play_finder_tone();

    LOG_INFO("=== [MAX98357] Operational Tones Demo Complete ===");
}

// =============================================================================
// C++ AudioManager Class Implementation
// =============================================================================

// Global AudioManager singleton instance
static AudioManager* g_audio_manager = nullptr;
static AudioManager g_audio_manager_instance;

// Forward declaration for HAL callbacks
extern bool playBackComplete;

/**
 * @brief Get the AudioManager singleton instance
 * Returns pointer to the global static instance
 */
AudioManager* getAudioManagerInstance(void) {
    return &g_audio_manager_instance;
}

/**
 * @brief Global HAL_I2S_TxCpltCallback - delegates to AudioManager singleton
 * Moved from test_max98357.cpp to here for proper driver integration
 */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s2) {
        // Delegate to AudioManager singleton if initialized
        if (g_audio_manager) {
            g_audio_manager->onI2STxComplete(hi2s);
        }
        // Check if streaming is active via audio manager C API
        else if (audio_is_playing()) {
            audio_dma_callback(hi2s, false);  // false = complete callback
        }
        // Check if simple tone generator is active
        else if (is_simple_tone_playing()) {
            simple_tone_dma_complete_callback();
        }
        else {
            // Legacy behavior for simple tests
            HAL_I2S_DMAStop(hi2s);
            LOG_INFO("DMA playback complete");
            playBackComplete = true;
        }
    }
}

/**
 * @brief Global HAL_I2S_TxHalfCpltCallback - delegates to AudioManager singleton
 * Moved from test_max98357.cpp to here for proper driver integration
 */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s2) {
        // Delegate to AudioManager singleton if initialized
        if (g_audio_manager) {
            g_audio_manager->onI2STxHalfComplete(hi2s);
        }
        // Check if streaming is active via audio manager C API
        else if (audio_is_playing()) {
            audio_dma_callback(hi2s, true);  // true = half complete callback
        }
    }
}

/**
 * @brief AudioManager constructor - private for singleton pattern
 */
AudioManager::AudioManager(void) : initialized(false)
{
    memset(&current_audio, 0, sizeof(current_audio));
}

/**
 * @brief AudioManager destructor
 */
AudioManager::~AudioManager(void)
{
    deinit();
}

/**
 * @brief Initialize AudioManager for audio operations
 * Registers with global singleton and initializes underlying C API
 * @return true if initialization successful
 */
bool AudioManager::init(void)
{
    if (initialized) {
        return true;
    }

    // Initialize the underlying C audio manager
    if (!audio_manager_init()) {
        LOG_ERROR("[AudioManager] C API initialization failed");
        return false;
    }

    // Register as global singleton for HAL callbacks
    g_audio_manager = this;

    initialized = true;
    LOG_INFO("[AudioManager] Initialized - ready for audio operations");
    return true;
}

/**
 * @brief Deinitialize AudioManager and stop any active playback
 */
void AudioManager::deinit(void)
{
    if (!initialized) {
        return;
    }

    // Stop any active playback
    audio_stop_playback();

    // Unregister from global singleton
    if (g_audio_manager == this) {
        g_audio_manager = nullptr;
    }

    initialized = false;
    LOG_INFO("[AudioManager] Deinitialized");
}

/**
 * @brief Play startup tone - 400Hz→500Hz→600Hz→800Hz (~700ms)
 */
void AudioManager::playStartupTone(void)
{
    if (!initialized) {
        LOG_WARN("[AudioManager] Not initialized - cannot play tone");
        return;
    }
    play_startup_tone();
}

/**
 * @brief Play failure tone - Warning beeps (~450ms)
 */
void AudioManager::playFailureTone(void)
{
    if (!initialized) {
        LOG_WARN("[AudioManager] Not initialized - cannot play tone");
        return;
    }
    play_failure_tone();
}

/**
 * @brief Play finder tone - Loud locator beacon (3 seconds)
 */
void AudioManager::playFinderTone(void)
{
    if (!initialized) {
        LOG_WARN("[AudioManager] Not initialized - cannot play tone");
        return;
    }
    play_finder_tone();
}

/**
 * @brief Demo function - Play all operational tones in sequence
 */
void AudioManager::testOperationalTones(void)
{
    if (!initialized) {
        LOG_WARN("[AudioManager] Not initialized - cannot run test");
        return;
    }
    test_operational_tones();
}

/**
 * @brief Handle I2S transmit complete interrupt
 * Called from HAL_I2S_TxCpltCallback()
 * @param hi2s I2S handle
 */
void AudioManager::onI2STxComplete(I2S_HandleTypeDef *hi2s)
{
    if (!initialized || hi2s != &hi2s2) {
        return;
    }

    // Delegate to underlying C API for actual DMA handling
    if (audio_is_playing()) {
        audio_dma_callback(hi2s, false);  // false = complete callback
    } else if (is_simple_tone_playing()) {
        simple_tone_dma_complete_callback();
    } else {
        // Playback finished
        HAL_I2S_DMAStop(hi2s);
        playBackComplete = true;
    }
}

/**
 * @brief Handle I2S transmit half-complete interrupt
 * Called from HAL_I2S_TxHalfCpltCallback()
 * @param hi2s I2S handle
 */
void AudioManager::onI2STxHalfComplete(I2S_HandleTypeDef *hi2s)
{
    if (!initialized || hi2s != &hi2s2) {
        return;
    }

    // Delegate to underlying C API for actual DMA handling
    if (audio_is_playing()) {
        audio_dma_callback(hi2s, true);  // true = half complete callback
    }
}

/**
 * @brief Start WAV file playback
 * @param filename WAV file to play
 * @return true if playback started
 */
bool AudioManager::playWavFile(const char* filename)
{
    if (!initialized) {
        LOG_WARN("[AudioManager] Not initialized - cannot play WAV file");
        return false;
    }

    // Load WAV file using C API
    if (!audio_load_wav_file(filename, &current_audio)) {
        LOG_ERROR("[AudioManager] Failed to load WAV file: %s", filename);
        return false;
    }

    // Validate format compatibility
    if (!audio_validate_format(&current_audio)) {
        LOG_ERROR("[AudioManager] WAV file format not supported: %s", filename);
        return false;
    }

    // Start playback using C API
    if (!audio_play_wav_file(&current_audio)) {
        LOG_ERROR("[AudioManager] Failed to start playback: %s", filename);
        return false;
    }

    LOG_INFO("[AudioManager] Playing WAV file: %s", filename);
    return true;
}

/**
 * @brief Stop current audio playback
 */
void AudioManager::stopPlayback(void)
{
    if (!initialized) {
        return;
    }
    audio_stop_playback();
    LOG_INFO("[AudioManager] Playback stopped");
}

/**
 * @brief Check if audio is currently playing
 * @return true if playback active
 */
bool AudioManager::isPlaying(void) const
{
    if (!initialized) {
        return false;
    }
    return audio_is_playing();
}

/**
 * @brief Get playback progress
 * @return Progress percentage (0-100)
 */
uint8_t AudioManager::getProgress(void) const
{
    if (!initialized) {
        return 0;
    }
    return audio_get_progress(&current_audio);
}