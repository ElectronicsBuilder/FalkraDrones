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
 * @file    audio_manager.hpp
 * @brief   Professional Audio Manager - C++ Class for Audio Control
 * @details Comprehensive audio management system providing:
 *          - Singleton C++ AudioManager class for DriverManager integration
 *          - WAV file streaming from Flash File System (FFS)
 *          - Operational tones (startup, failure, finder)
 *          - I2S DMA callbacks for seamless audio playback
 *          - Double-buffered streaming and dynamic sample rate reconfiguration
 *
 * Features:
 * - AudioManager C++ class integrated with DriverManager singleton
 * - Multi-format WAV support (8/16-bit, mono/stereo, 8-48kHz)
 * - Zero-gap streaming with double-buffered DMA architecture
 * - Operational tone feedback (startup, failure, locator beacon)
 * - Built-in I2S DMA callbacks (TxCplt, TxHalfCplt)
 * - Real-time playback progress monitoring
 * - FreeRTOS integration for background streaming
 * - HAL-compatible I2S interface
 *
 * Integration:
 * - Access via DriverManager::getInstance().getAudio()
 * - Automatic I2S DMA callback registration
 * - Graceful handling of audio device absence
 */

#ifndef __AUDIO_MANAGER_HPP
#define __AUDIO_MANAGER_HPP

#include <stdint.h>
#include <stdbool.h>
#include <stm32f7xx.h>

#ifdef __cplusplus
extern "C" {
#endif

// WAV File Format Structures
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8 bytes
    char wave[4];           // "WAVE"
} __attribute__((packed)) wav_riff_header_t;

typedef struct {
    char fmt[4];            // "fmt "
    uint32_t chunk_size;    // Size of fmt chunk (usually 16)
    uint16_t audio_format;  // 1 = PCM
    uint16_t num_channels;  // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;   // Sample rate in Hz
    uint32_t byte_rate;     // Bytes per second
    uint16_t block_align;   // Bytes per sample frame
    uint16_t bits_per_sample; // Bits per sample (8, 16, 24, 32)
} __attribute__((packed)) wav_fmt_chunk_t;

typedef struct {
    char data[4];           // "data"
    uint32_t data_size;     // Size of audio data
} __attribute__((packed)) wav_data_header_t;

typedef struct {
    // WAV file info
    bool valid;
    char filename[64];

    // Audio format
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t audio_data_size;
    uint32_t audio_data_offset;  // Offset in file where audio data starts

    // Playback state
    int file_id;
    uint32_t current_position;
    bool is_playing;
} audio_file_t;

// Audio Manager C API Functions (for backward compatibility)
/**
 * @brief Initialize audio manager
 */
bool audio_manager_init(void);

/**
 * @brief Load WAV file from FFS and parse header
 * @param filename WAV file to load
 * @param audio_file Output structure with parsed info
 * @return true if loaded and parsed successfully
 */
bool audio_load_wav_file(const char* filename, audio_file_t* audio_file);

/**
 * @brief Validate WAV file is compatible with hardware
 * @param audio_file WAV file info to validate
 * @return true if compatible
 */
bool audio_validate_format(const audio_file_t* audio_file);

/**
 * @brief Play WAV file using I2S DMA
 * @param audio_file WAV file to play
 * @return true if playback started successfully
 */
bool audio_play_wav_file(audio_file_t* audio_file);

/**
 * @brief Stop current audio playback
 */
void audio_stop_playback(void);

/**
 * @brief Check if audio is currently playing
 * @return true if playing
 */
bool audio_is_playing(void);

/**
 * @brief Get audio playback progress
 * @param audio_file Current audio file
 * @return Progress percentage (0-100)
 */
uint8_t audio_get_progress(const audio_file_t* audio_file);

/**
 * @brief List available WAV files in filesystem
 * @param wav_files Array to store filenames
 * @param max_files Maximum number of files to return
 * @return Number of WAV files found
 */
int audio_list_wav_files(char wav_files[][64], int max_files);

/**
 * @brief Reconfigure I2S sample rate to match WAV file
 * @param sample_rate Target sample rate in Hz
 * @return HAL_OK if successful, HAL_ERROR otherwise
 */
int audio_reconfigure_i2s_sample_rate(uint32_t sample_rate);

/**
 * @brief Stream next chunk of audio data (for large files)
 * @return true if more data available, false if end of file
 */
bool audio_stream_next_chunk(void);

/**
 * @brief Streaming task for background audio loading
 * @param pvParameters Task parameters (unused)
 */
void audio_streaming_task(void *pvParameters);

#ifdef __cplusplus
}

#include "max98357.hpp"

/**
 * @brief C++ AudioManager class for DriverManager integration
 *
 * Singleton class providing:
 * - Integration with DriverManager for dependency injection
 * - I2S DMA callback management
 * - Operational tone playback
 * - WAV file streaming control
 *
 * Usage:
 *   auto& dm = DriverManager::getInstance();
 *   MAX98357* audio = dm.getAudio();
 *   if (audio) {
 *       // Audio device available
 *   }
 */
class AudioManager {
public:
    /**
     * @brief Initialize audio manager
     * Registers I2S callbacks and prepares for audio operations
     * @return true if initialization successful
     */
    bool init(void);

    /**
     * @brief Deinitialize audio manager
     * Stops any active playback and cleanup
     */
    void deinit(void);

    /**
     * @brief Check if audio is initialized
     * @return true if ready for audio operations
     */
    bool isInitialized(void) const { return initialized; }

    // Operational Tone Methods
    /**
     * @brief Play startup tone (rising frequency: 400Hz→500Hz→600Hz→800Hz)
     * Duration: ~700ms
     */
    void playStartupTone(void);

    /**
     * @brief Play failure/error tone (warning beeps)
     * Duration: ~450ms
     */
    void playFailureTone(void);

    /**
     * @brief Play finder/locator tone (loud beacon for lost drone)
     * Duration: 3 seconds
     */
    void playFinderTone(void);

    /**
     * @brief Demo: Play all operational tones in sequence
     */
    void testOperationalTones(void);

    // I2S DMA Callback Methods
    /**
     * @brief Handle I2S transmit complete interrupt
     * Called from HAL_I2S_TxCpltCallback()
     * @param hi2s I2S handle
     */
    void onI2STxComplete(I2S_HandleTypeDef *hi2s);

    /**
     * @brief Handle I2S transmit half-complete interrupt
     * Called from HAL_I2S_TxHalfCpltCallback()
     * @param hi2s I2S handle
     */
    void onI2STxHalfComplete(I2S_HandleTypeDef *hi2s);

    // Audio Playback Methods
    /**
     * @brief Start WAV file playback
     * @param filename WAV file to play
     * @return true if playback started
     */
    bool playWavFile(const char* filename);

    /**
     * @brief Stop current audio playback
     */
    void stopPlayback(void);

    /**
     * @brief Check if audio is currently playing
     * @return true if playback active
     */
    bool isPlaying(void) const;

    /**
     * @brief Get playback progress
     * @return Progress percentage (0-100)
     */
    uint8_t getProgress(void) const;

    /**
     * @brief Constructor - for singleton pattern
     * @note Intentionally public to allow static instantiation in audio_manager.cpp
     */
    AudioManager(void);

    /**
     * @brief Destructor
     */
    ~AudioManager(void);

private:

    bool initialized = false;
    audio_file_t current_audio = {};  // C++11 value-initialization

    // Prevent copying
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    friend class DriverManager;
};

/**
 * @brief I2S DMA complete callback handler
 * @param hi2s I2S handle
 * @param is_half_complete true for half complete, false for full complete
 */
void audio_dma_callback(I2S_HandleTypeDef *hi2s, bool is_half_complete);

/**
 * @brief Play system startup tone (rising frequency sequence)
 * Plays: 400Hz → 500Hz → 600Hz → 800Hz
 * Duration: ~700ms total
 */
void play_startup_tone(void);

/**
 * @brief Play system failure/error tone (warning beeps)
 * Plays: 900Hz beep → gap → 900Hz beep → gap → 600Hz warning
 * Duration: ~450ms total
 */
void play_failure_tone(void);

/**
 * @brief Play system finder/locator tone (loud beacon)
 * Plays: Alternating 1200Hz/1500Hz beacon for 3 seconds
 * Duration: 3 seconds (high volume for locating drone)
 */
void play_finder_tone(void);

/**
 * @brief Demonstration of all operational tones
 * Plays startup, failure, and finder tones in sequence
 */
void test_operational_tones(void);

/**
 * @brief Get the global AudioManager singleton instance
 * @return Pointer to AudioManager, or nullptr if not yet initialized
 * @note This is only used internally by DriverManager
 */
AudioManager* getAudioManagerInstance(void);

#endif

#endif /* __AUDIO_MANAGER_HPP */
