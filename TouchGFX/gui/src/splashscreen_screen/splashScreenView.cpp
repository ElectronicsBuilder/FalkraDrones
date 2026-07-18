#include <gui/splashscreen_screen/splashScreenView.hpp>
#include <stdio.h>
#include <string.h>
#include "rtc.hpp"

extern int tickCounter;
extern RTC_HandleTypeDef hrtc;

static uint8_t lastSeconds = 0xFF;

splashScreenView::splashScreenView()
{

}

void splashScreenView::setupScreen()
{
    splashScreenViewBase::setupScreen();
}

void splashScreenView::tearDownScreen()
{
    splashScreenViewBase::tearDownScreen();
}

void splashScreenView::handleTickEvent()
{
    tickCounter++;

    if (tickCounter % 2 == 0)
    {
        RTC_TimeTypeDef time;
        //RTC_DateTypeDef date;
        HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
        //HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

        if (time.Seconds != lastSeconds)
        {
            lastSeconds = time.Seconds;
            digitalClock.setTime24Hour(time.Hours, time.Minutes, time.Seconds);
            digitalClock.invalidate();
            tickCounter = 0;
        }
    }
}


