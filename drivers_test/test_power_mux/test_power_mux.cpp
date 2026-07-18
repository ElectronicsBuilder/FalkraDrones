/**
 * @file    test_power_mux.cpp
 * @brief   Power Multiplexer Test Implementation
 * @details Implementation of power multiplexer and level shifter testing
 *          for power source selection and voltage level conversion
 * 
 * Part of FalkraController - STM32F767-based drone controller firmware
 * 
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
 */

#include "test_power_mux.hpp"
#include "log.hpp"
#include "main.h"
#include "driver_manager.hpp"
#include "tps2115.hpp"
#include "tps2121.hpp"
#include "txs0108.hpp"

extern TIM_HandleTypeDef htim1;

void test_power_mux(VoutSelect selection)
{
    LOG_INFO("[TEST] Power Mux + Level Shifter Test Begin");

    LOG_INFO("[TEST] ESC PWM Output Init");
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
    LOG_INFO("[PWM TEST] TIM1 CH1 started @ 50Hz, pulse = 1.5 ms");

    // Get power management components from DriverManager
    auto& dm = DriverManager::getInstance();
    TPS2115* powerSelector = dm.getPowerMux();
    TPS2121* usbMux = dm.getPowerSwitch();
    TXS0108* levelShifter = dm.getLevelShifter();

    if (!powerSelector) {
        LOG_ERROR("[TEST] TPS2115 power mux not available from DriverManager!");
        return;
    }

    if (!usbMux) {
        LOG_ERROR("[TEST] TPS2121 USB mux not available from DriverManager!");
        return;
    }

    if (!levelShifter) {
        LOG_WARN("[TEST] TXS0108 level shifter not available (may not be configured yet)");
    }

    // Apply voltage selection
    bool d1Value = (selection == VOUT_3V3);
    powerSelector->setD1(d1Value);

    HAL_Delay(50);

    const char* targetV = d1Value ? "3.3V" : "5V";
    LOG_INFO("[TPS2115] Forcing output to %s (D1 = %d)", targetV, d1Value ? 1 : 0);
    LOG_INFO("[TPS2115] D1 State: %s", powerSelector->getD1State() ? "HIGH (3.3V)" : "LOW (5V)");

    auto source = powerSelector->getActiveInput();
    LOG_INFO("[TPS2115] Output Source: %s", source == TPS2115::InputSource::IN1 ? "IN1 (3.3V)" : "IN2 (5V)");

    auto selected = usbMux->getSelectedInput();
    LOG_INFO("[TPS2121] Output Connected To: %s", selected == TPS2121::InputSource::MAIN ? "3V3_MAIN" : "3V3_USB");

    if (levelShifter) {
        LOG_INFO("[TXS0108] OE: %s", levelShifter->isEnabled() ? "ENABLED" : "DISABLED");

        levelShifter->disable();
        HAL_Delay(10);
        LOG_INFO("[TXS0108] Disabled");

        levelShifter->enable();
        HAL_Delay(10);
        LOG_INFO("[TXS0108] Enabled");
    }

    LOG_INFO("[TEST] Power Mux + Level Shifter Test Complete");
}
