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
 * @file    max98357.hpp
 * @brief   MAX98357A Digital Audio Amplifier Control Driver
 * @details Hardware control driver for MAX98357A Class-D digital audio amplifier
 *          providing high-efficiency audio output for drone operational feedback.
 *          Manages power control, channel configuration, and operational modes
 *          for reliable audio amplification with minimal power consumption.
 * 
 * MAX98357A Features:
 * - Class-D digital amplifier with 3.2W output power
 * - Direct I2S digital audio input (no DAC required)
 * - Single-supply operation with built-in charge pump
 * - Filterless operation with spread-spectrum modulation
 * - Click-and-pop suppression for clean audio switching
 * - Thermal and overcurrent protection
 * 
 * Control Interface:
 * - EN Pin: Master power enable/disable control
 * - MODE Pin: Left/Right channel selection and configuration
 * - SD Pin: Shutdown control for power management
 * - GPIO-based control compatible with STM32F767
 * 
 * Channel Configuration:
 * - MODE High: Left channel amplification
 * - MODE Low: Right channel amplification  
 * - MODE Floating: Mono (L+R)/2 output
 * - Dynamic channel switching support
 * 
 * Power Management:
 * - Enable/disable control for power savings
 * - Shutdown mode for minimal power consumption
 * - Fast wake-up time for immediate audio response
 * - Integration with drone power management system
 * 
 * Drone Integration:
 * - Operational tone feedback (startup, failure, finder)
 * - Voice announcement amplification
 * - Emergency audio signal amplification
 * - Low-power standby during flight operations
 * - Reliable audio output in harsh environmental conditions
 */

#ifndef __MAX98357_HPP
#define __MAX98357_HPP

#include "stm32f7xx_hal.h"

class MAX98357 {
public:
    struct Config {
        GPIO_TypeDef* en_port;
        uint16_t en_pin;
        GPIO_TypeDef* mode_port;  
        uint16_t mode_pin;        
    };

    explicit MAX98357(const Config& cfg);

    /**
     * @brief Destructor - disables amplifier on cleanup
     */
    ~MAX98357();

    void init();
    void enable();
    void disable();
    void setLeftChannel();
    void shutdown();

    /**
     * @brief Reset driver state and disable amplifier
     */
    void deinit();

private:
    Config cfg_;
};

#endif // __MAX98357_HPP
