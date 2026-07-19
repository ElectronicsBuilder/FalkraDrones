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
 * @file    driver_health.hpp
 * @brief   Driver health monitoring and diagnostics
 */
#ifndef __DRIVER_HEALTH_HPP
#define __DRIVER_HEALTH_HPP

#include <cstdint>
#include "driver_registry.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Health information for a single driver
 */
struct DriverHealth {
    DriverState state;              // Current initialization state
    bool initialized;               // Has init() been called successfully
    bool responding;                // Driver responding to queries
    uint32_t init_time_ms;         // Time taken to initialize
    uint32_t last_response_ms;      // Last successful operation timestamp
    uint32_t error_count;           // Cumulative error count
    uint32_t warning_count;         // Cumulative warning count
    uint8_t health_score;           // Overall health 0-100 (100 = perfect)
    char last_error[64];            // Last error message
};

/**
 * @brief Driver health monitoring and diagnostics
 *
 * Tracks driver health with error/warning reporting and system-wide diagnostics.
 */
class DriverHealthMonitor {
public:
    /**
     * @brief Get health information for a driver
     * @param id Driver ID
     * @return Const reference to DriverHealth
     */
    static const DriverHealth& getHealth(DriverId id);

    /**
     * @brief Get overall system health score (0-100)
     * Calculated from critical drivers' health scores
     * @return System health percentage
     */
    static uint8_t getSystemHealthScore(void);

    /**
     * @brief Report error for a driver
     * Increments error count and updates health score
     * @param id Driver ID
     * @param error_msg Error message (copied, max 64 chars)
     */
    static void reportError(DriverId id, const char* error_msg);

    /**
     * @brief Report warning for a driver
     * Increments warning count, non-critical
     * @param id Driver ID
     * @param warning_msg Warning message
     */
    static void reportWarning(DriverId id, const char* warning_msg);

    /**
     * @brief Mirror DriverManager state transitions into the health registry
     * @param id Driver ID
     * @param state New DriverManager state
     */
    static void setState(DriverId id, DriverState state);

    /**
     * @brief Check if all critical drivers are healthy
     * @return true if all required drivers in READY state with good health
     */
    static bool isSystemHealthy(void);

    /**
     * @brief Print detailed health report to log
     * Lists all drivers with their states and error counts
     */
    static void printHealthReport(void);

private:
    DriverHealthMonitor() = delete;
    ~DriverHealthMonitor() = delete;
};

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_HEALTH_HPP */
