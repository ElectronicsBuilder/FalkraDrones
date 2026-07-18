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
 * @file    tps2115.hpp
 * @brief   TPS2115 Power Multiplexer Driver for PWM Voltage Selection
 * @details Driver for Texas Instruments TPS2115 power multiplexer IC for
 *          configurable PWM output voltage (VPWM). Selects between 3.3V and 5V
 *          to meet different ESC voltage requirements. Output voltage is
 *          level-shifted by TXS0108 to PWM[1..8] when VPWM_OE is enabled.
 *
 * PWM Voltage Management Features:
 * - Dual input voltage selection for VPWM output (3.3V or 5V)
 * - User-configurable PWM voltage via NVRAM (userconfig_system_t.pwm_voltage)
 * - Manual control via D1 pin (true=3.3V, false=5V)
 * - Real-time status monitoring via STAT pin (feedback only)
 * - Seamless switching without PWM signal interruption
 * - Low dropout voltage for maximum power efficiency
 *
 * Voltage Selection Configuration:
 * - VPWM at 3.3V: For 3.3V ESCs (D1=HIGH, STAT reads HIGH)
 * - VPWM at 5V: For 5V ESCs (D1=LOW, STAT reads LOW) - default
 * - User can change via console command or UI layer
 * - Setting persisted in NVRAM system configuration block
 *
 * Level Shifter Integration:
 * - TXS0108 receives VPWM on VCCB pin (level shifter reference)
 * - VPWM_OE signal enables level shifting of PWM[1..8] channels
 * - PWM channels level-shifted from 3.3V STM32 to VPWM voltage
 * - When VPWM_OE disabled, PWM pins remain high-impedance
 *
 * Control Interface:
 * - D1 Pin: PWM voltage selection (HIGH=3.3V, LOW=5V)
 * - STAT Pin: Active voltage feedback (HIGH=3.3V, LOW=5V, read-only)
 * - GPIO-based control compatible with STM32F767 pins
 *
*/

#ifndef __TPS2115_HPP
#define __TPS2115_HPP

#include "stm32f7xx_hal.h"

class TPS2115 {
public:
    enum class InputSource {
        IN1, // 3.3V
        IN2  // 5V
    };

    TPS2115(GPIO_TypeDef* statPort, uint16_t statPin,
            GPIO_TypeDef* d1Port, uint16_t d1Pin);

    void setD1(bool level);                 // true = 3.3V, false = 5V
    bool getD1State() const;               // read back D1 pin state
    InputSource getActiveInput();          // from STAT pin

private:
    GPIO_TypeDef* _statPort;
    uint16_t _statPin;
    GPIO_TypeDef* _d1Port;
    uint16_t _d1Pin;
};

#endif
