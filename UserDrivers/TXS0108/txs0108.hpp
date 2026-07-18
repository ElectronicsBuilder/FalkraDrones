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
 * @file    txs0108.hpp
 * @brief   TXS0108 Bidirectional Level Shifter for PWM and Sensor Interfaces
 * @details Driver for Texas Instruments TXS0108 8-channel bidirectional voltage
 *          level translator. Primary function is level-shifting ESC PWM signals
 *          from 3.3V STM32F767 to configurable VPWM voltage (3.3V or 5V) via
 *          TPS2115. Secondary use for mixed-voltage sensor and communication
 *          interfaces requiring bidirectional translation.
 *
 * Level Shifter Features:
 * - 8-channel bidirectional voltage translation
 * - Support for 1.2V to 3.6V (A-side) and 1.65V to 5.5V (B-side)
 * - Auto-direction sensing without external control signals
 * - High-speed translation up to 110 Mbps (suitable for PWM timing)
 * - Low power consumption with minimal propagation delay
 * - ESD protection on all I/O pins
 *
 * PWM Signal Level Shifting:
 * - A-Side (VCCA): 3.3V STM32F767 PWM signals (PWM[1..8])
 * - B-Side (VCCB): Configurable VPWM voltage from TPS2115 (3.3V or 5V)
 * - Automatic bidirectional direction detection
 * - No additional direction control pins required
 * - OE (VPWM_OE) pin enables/disables all 8 PWM channels simultaneously
 *
 * Voltage Translation Modes:
 * - VPWM = 5V: ESCs using 5V PWM input (common PWM standard)
 * - VPWM = 3.3V: ESCs using 3.3V PWM input (modern/logic-level ESCs)
 * - User-configurable via NVRAM (userconfig_system_t.pwm_voltage)
 * - TPS2115 D1 pin controls voltage selection
 *
 * Additional Interfaces:
 * - Mixed voltage sensor communication (I2C, SPI, UART)
 * - GPIO level translation for control signals
 * - Bus sharing support via high-Z state when disabled
 *
 * Control Interface:
 * - OE (VPWM_OE): Master enable/disable for all PWM channels
 * - High-Z state when disabled for bus unpowered ESCs
 * - PWM channels operational immediately when enabled
 * - No timing delays in PWM path
 */

#ifndef __TXS0108_HPP
#define __TXS0108_HPP

#include "stm32f7xx_hal.h"

class TXS0108 {
public:
    TXS0108(GPIO_TypeDef* oePort, uint16_t oePin);

    void enable();
    void disable();
    bool isEnabled() const;

private:
    GPIO_TypeDef* _oePort;
    uint16_t _oePin;
};

#endif
