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
 * @file    test_ppm.cpp
 * @brief   PPM Decoder Test Implementation
 * @details Test suite for validating PPM signal capture and channel decoding
 */

#include "test_ppm.hpp"
#include "ppm.hpp"
#include "log.hpp"
#include "main.h"
#include "cmsis_os.h"
#include "task.h"
#include "driver_manager.hpp"

// External timer handle - should be defined in main.c or tim.c
extern TIM_HandleTypeDef htim2;

// Global PPM decoder instance for interrupt callback access
static PPMDecoder* g_ppmDecoder = nullptr;

void test_ppm(uint32_t timeout)
{
    if(timeout == 0) {
        timeout = 10000; // Default to 10 seconds
    }
    LOG_INFO("[PPM] Test Started!");
    LOG_INFO("[PPM] Signal detection timeout: %dms", timeout);
    LOG_INFO("[PPM] Initializing PPM decoder via DriverManager...");

    // Get PPM decoder from DriverManager singleton
    auto& dm = DriverManager::getInstance();
    PPMDecoder* ppm = dm.getPPMDecoder();

    if (!ppm) {
        LOG_ERROR("[PPM] PPMDecoder not available via DriverManager");
        return;
    }

    g_ppmDecoder = ppm;

    LOG_INFO("[PPM] Decoder initialized successfully via DriverManager");
    LOG_INFO("[PPM] Waiting for PPM signal from receiver...");

    // Wait for signal detection within timeout
    uint32_t startTime = HAL_GetTick();
    bool signalDetected = false;
    uint8_t lastChannelCount = 0;
    uint32_t lastInterruptCount = 0;

    while ((HAL_GetTick() - startTime) < timeout) {
        // Check if we're receiving any data (even incomplete frames)
        uint8_t currentChannelCount = ppm->getChannelCount();
        uint32_t currentInterruptCount = ppm->getInterruptCount();

        if (ppm->isSignalValid()) {
            signalDetected = true;
            break;
        }

        // Check if interrupts are firing (indicates timer is working)
        if (currentInterruptCount != lastInterruptCount) {
            LOG_INFO("[PPM] Interrupts detected: %d total", currentInterruptCount);
            lastInterruptCount = currentInterruptCount;
        }

        // Also check if channel count is changing (indicates signal activity)
        if (currentChannelCount > 0 && currentChannelCount != lastChannelCount) {
            LOG_INFO("[PPM] Signal activity detected: %d channels", currentChannelCount);
            lastChannelCount = currentChannelCount;
        }

        osDelay(100);
    }

    if (!signalDetected) {
        uint32_t finalInterruptCount = ppm->getInterruptCount();
        LOG_WARN("[PPM] No valid signal detected within timeout period");
        LOG_WARN("[PPM] Interrupt count: %d", finalInterruptCount);
        LOG_WARN("[PPM] Last channel count: %d", ppm->getChannelCount());
        LOG_WARN("[PPM] Time since last frame: %dms", ppm->getTimeSinceLastFrame());

        if (finalInterruptCount == 0) {
            LOG_ERROR("[PPM] CRITICAL: No timer interrupts detected!");

        } else {
            LOG_WARN("[PPM] Timer interrupts working (%d detected)", finalInterruptCount);
            LOG_ERROR("[PPM]   - No PPM signal from receiver (check power/binding)");
        }

        g_ppmDecoder = nullptr;
        return;
    }

    LOG_INFO("[PPM] Signal detected! Channel count: %d", ppm->getChannelCount());

    // Display channel values for 5 seconds
    LOG_INFO("[PPM] Reading channel values (5 seconds)...");
    startTime = HAL_GetTick();

    while ((HAL_GetTick() - startTime) < timeout) {
        if (ppm->isSignalValid()) {
            // Blink activity LED
            HAL_GPIO_TogglePin(LED_ACTY_GPIO_Port, LED_ACTY_Pin);

            // Read and display all channels
            LOG_INFO("[PPM] Ch1=%4dus Ch2=%4dus Ch3=%4dus Ch4=%4dus Ch5=%4dus Ch6=%4dus Ch7=%4dus Ch8=%4dus",
                     ppm->getChannel(0), ppm->getChannel(1), ppm->getChannel(2), ppm->getChannel(3),
                     ppm->getChannel(4), ppm->getChannel(5), ppm->getChannel(6), ppm->getChannel(7));

            // Display normalized values for primary controls
            LOG_INFO("[PPM] Normalized: Roll=%.2f Pitch=%.2f Throttle=%.2f Yaw=%.2f",
                     ppm->getChannelNormalized(0), ppm->getChannelNormalized(1),
                     ppm->getChannelNormalized(2), ppm->getChannelNormalized(3));

        } else {
            LOG_WARN("[PPM] Signal lost! Time since last frame: %dms", ppm->getTimeSinceLastFrame());
        }

        osDelay(500);
    }

    // Test signal statistics
    if (ppm->isSignalValid()) {
        LOG_INFO("[PPM] Final signal status: VALID");
        LOG_INFO("[PPM] Channel count: %d", ppm->getChannelCount());
        LOG_INFO("[PPM] Time since last frame: %dms", ppm->getTimeSinceLastFrame());
        LOG_INFO("[PPM] Test PASSED");
    } else {
        LOG_WARN("[PPM] Final signal status: INVALID");
        LOG_WARN("[PPM] Test COMPLETED with warnings");
    }

    // DriverManager manages cleanup
    g_ppmDecoder = nullptr;

    HAL_GPIO_WritePin(LED_ACTY_GPIO_Port, LED_ACTY_Pin, GPIO_PIN_RESET);
    LOG_INFO("[PPM] Test Completed!");
}

// Interrupt callback handled by DriverManager (see driver_manager.cpp)
// HAL_TIM_IC_CaptureCallback is implemented in driver_manager.cpp and routes
// TIM2 interrupts to the DriverManager-allocated PPMDecoder instance
