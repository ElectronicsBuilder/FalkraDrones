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
 * @file    test_max98357.hpp
 * @brief   Comprehensive MAX98357A Audio System Testing Interface
*/

#ifndef __TEST_MAX98357_HPP
#define __TEST_MAX98357_HPP

#ifdef __cplusplus
extern "C" {
#endif


// Original test using hardcoded WAV data
void test_max98357();

// Enhanced tests using AudioManager with WAV file parsing
void test_max98357_wav_files();                    // Play all WAV files found
void test_max98357_play_file(const char* filename); // Play specific WAV file
void demo_audio_capabilities();                     // Comprehensive audio demo

// Operational tone functions
void play_startup_tone();                          // Pleasant startup sound
void play_failure_tone();                          // Warning/error sound  
void play_finder_tone();                           // Loud locator beacon
void test_operational_tones();                     // Demo all operational tones

#ifdef __cplusplus
}
#endif

#endif // __TEST_MAX98357_HPP
