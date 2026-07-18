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
 * @file    rtc.cpp
 * @brief   RTC Driver Implementation
 */
#include "rtc.hpp"
#include "status.hpp"
#include <cstdio>
#include <cstring>
#include "main.h"
#include "log.hpp"


extern RTC_HandleTypeDef hrtc;
RtcDriver rtc;


RtcDriver::RtcDriver() {}

RtcDriver::~RtcDriver() {
    // No-op destructor - RTC singleton managed by main app
}



bool RtcDriver::init() {
    RTC_TimeTypeDef tempTime;
    RTC_DateTypeDef tempDate;
    HAL_RTC_GetTime(&hrtc, &tempTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &tempDate, RTC_FORMAT_BIN);

    // If RTC already holds valid time (non-zero), skip re-init
    if (tempDate.Year > 0 || tempDate.Month > 0 || tempDate.Date > 0) {
        return true;
    }

    return (HAL_RTC_Init(&hrtc) == HAL_OK);
}

void RtcDriver::deinit() {
    // Reset internal time/date structures
    memset(&currentTime, 0, sizeof(currentTime));
    memset(&currentDate, 0, sizeof(currentDate));

    LOG_SYSSTATUS("[RTC] RTC driver cleanup complete");
}

void RtcDriver::update() {
    HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BIN);  
}

const char* RtcDriver::get_formatted_time() {
    update();
    static char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             2000 + currentDate.Year, currentDate.Month, currentDate.Date,
             currentTime.Hours, currentTime.Minutes, currentTime.Seconds);
    return buf;
}


const char* RtcDriver::rtc_get_date() {
    update();
    static char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             2000 + currentDate.Year, currentDate.Month, currentDate.Date);
    return buf;
}


void RtcDriver::set_time(uint8_t hour, uint8_t minute, uint8_t second) {
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = hour;
    sTime.Minutes = minute;
    sTime.Seconds = second;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
}

void RtcDriver::set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year) {
    RTC_DateTypeDef sDate = {0};
    sDate.WeekDay = weekday;
    sDate.Month = month;
    sDate.Date = date;
    sDate.Year = year;
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}



const char* rtc_get_time_str() {
    return rtc.get_formatted_time();
}

const char* rtc_get_date_str() {
    return rtc.rtc_get_date();
}


void rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second) {
    rtc.set_time(hour, minute, second);
}


void rtc_set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year) {
    rtc.set_date(weekday, month, date, year);
}

void rtc_update_status(void) {
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    g_status.timeHours   = time.Hours;
    g_status.timeMinutes = time.Minutes;
    g_status.timeSeconds = time.Seconds;
    g_status.timeDays    = date.Date;
}