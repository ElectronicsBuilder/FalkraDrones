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
 * @file    fs_time_sync.c
 * @brief   Time synchronization command handler implementation
 */

#include "fs_time_sync.h"
#include "app_reply.h"
#include "log.hpp"
#include <string.h>
#include <stdio.h>

// External RTC functions
extern const char* rtc_get_time_str();
extern void rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second);
extern void rtc_set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year);

void fs_cmd_handle_sync_time(const uint8_t* args, uint8_t len) {
    if (!args || len < 19) {  // "YYYY-MM-DDTHH:MM:SS" = 19 chars
        send_framed_response(RESP_TYPE_ERROR, "Invalid time format. Use YYYY-MM-DDTHH:MM:SS");
        return;
    }

    char time_str[32] = {0};
    size_t safe_len = (len < sizeof(time_str) - 1) ? len : (sizeof(time_str) - 1);
    memcpy(time_str, args, safe_len);
    time_str[safe_len] = '\0';

    // Parse time format: YYYY-MM-DDTHH:MM:SS
    int year, month, day, hour, minute, second;
    if (sscanf(time_str, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        send_framed_response(RESP_TYPE_ERROR, "Failed to parse time format");
        return;
    }

    // Validate ranges
    if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        send_framed_response(RESP_TYPE_ERROR, "Time values out of range");
        return;
    }

    LOG_INFO("[TIME_SYNC] Setting time to: %s", time_str);

    // Set the RTC
    rtc_set_date(1, month, day, year - 2000);  // Weekday=1 (Monday), adjust year to 2-digit
    rtc_set_time(hour, minute, second);

    // Verify the update
    const char* updated_time = rtc_get_time_str();
    
    char success_msg[96];
    snprintf(success_msg, sizeof(success_msg), "TIME UPDATED: %s", updated_time);
    send_framed_response(RESP_TYPE_SUCCESS, success_msg);
    
    LOG_INFO("[TIME_SYNC] Time synchronized successfully to %s", updated_time);
}