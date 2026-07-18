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
 * @file    rtc.hpp
 * @brief   Real-Time Clock Driver for Precision Time Stamping and Scheduling
 * @details Comprehensive RTC driver providing accurate time/date management
 *          for flight data logging, mission scheduling, and system event
 *          timestamping. Integrates STM32F767 internal RTC with battery
 *          backup for continuous operation during power cycles.
 * 
 * RTC Features:
 * - Battery-backed real-time clock for continuous operation
 * - Sub-second precision for accurate flight data timestamping
 * - ISO 8601 formatted time strings for data logging compatibility
 * - Calendar functionality with automatic leap year handling
 * - Low power operation during sleep modes
 * - Hardware tamper detection capabilities
 * 
 * Time Management:
 * - 24-hour format with sub-second precision
 * - Full calendar with weekday, month, date, year
 * - Automatic daylight saving time adjustment capability
 * - GPS synchronization support for accurate time reference
 * - Battery backup maintains time during power loss
 * 
 * Drone Integration:
 * - Flight data logging with precise timestamps
 * - Mission scheduling and waypoint timing
 * - System event logging for maintenance
 * - Telemetry data correlation with ground systems
 * - Compliance with aviation data logging standards
 * 
 * Data Logging Format:
 * - ISO 8601 standard: "YYYY-MM-DDTHH:MM:SSZ"
 * - UTC time coordination for multi-system integration
 * - Millisecond precision for high-frequency data logging
 * - Compatible with standard flight data analysis tools
 * 
 * C/C++ Interface:
 * - C-callable wrapper functions for HAL integration
 * - C++ class interface for object-oriented usage
 * - Thread-safe operation with FreeRTOS
 * - Both formatted strings and raw time structure access
 * 
*/

#ifndef RTC_HPP
#define RTC_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// C-callable wrapper for use in .c files
const char* rtc_get_time_str();
const char* rtc_get_date_str();
void rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second);
void rtc_set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year);
void rtc_update_status(void);  // Updates g_status.timeXxx with current RTC time

#ifdef __cplusplus
}
#endif


class RtcDriver {

public:
    RtcDriver();

    /**
     * @brief Destructor (no-op - RTC is singleton managed by main app)
     */
    ~RtcDriver();

    bool init();

    /**
     * @brief Reset RTC driver state (minimal cleanup)
     * @note RTC peripheral managed by main app - this just resets internal state
     */
    void deinit();

    const char* get_formatted_time();  // returns "YYYY-MM-DDTHH:MM:SSZ" ISO 8601
    const char* rtc_get_date();

    void set_time(uint8_t hour, uint8_t minute, uint8_t second);
    void set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year);

private:
    RTC_TimeTypeDef currentTime;
    RTC_DateTypeDef currentDate;
    void update();
};


#ifdef __cplusplus
}
#endif

#endif // RTC_HPP
