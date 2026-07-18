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
 * @file    tps2121.hpp
 * @brief   TPS2121 Dual Input Power Switch Driver for Automatic Power Source Selection
 * @details Driver for Texas Instruments TPS2121 dual input power switch providing
 *          intelligent automatic power source selection between main power and USB.
 *          Critical component for drone power management ensuring uninterrupted
 *          operation during power source transitions and development scenarios.
 * 
 * Power Switch Features:
 * - Automatic priority-based power source selection
 * - Seamless switchover without power interruption
 * - Built-in overvoltage and reverse current protection
 * - Low dropout voltage for maximum efficiency
 * - Input voltage monitoring and status reporting
 * - Thermal shutdown protection
 * 
 * Input Source Priority:
 * - MAIN Input: Primary power source (battery/external supply)
 * - USB Input: Secondary power source (development/charging)
 * - Automatic Selection: MAIN has priority when both sources present
 * - Failover Protection: Switches to USB if MAIN fails or disconnects
 * 
 * Drone Integration:
 * - Development Mode: USB power during programming/debugging
 * - Flight Mode: Automatic switch to battery power
 * - Emergency Backup: USB power if battery depletes during tethered operation
 * - Charging Support: Power system while charging battery
 * 
 * Status Monitoring:
 * - VSEL Pin: Indicates active power source selection
 * - Real-time power source identification for system monitoring
 * - Integration with power management and battery monitoring systems
*/

#ifndef __TPS2121_HPP
#define __TPS2121_HPP

#include "stm32f7xx_hal.h"

class TPS2121 {
public:
    TPS2121(GPIO_TypeDef* vselPort, uint16_t vselPin);

    enum class InputSource {
        MAIN,
        USB
    };

    InputSource getSelectedInput();

private:
    GPIO_TypeDef* _vselPort;
    uint16_t _vselPin;
};

#endif
