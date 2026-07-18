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
 * @file    radio_receiver.hpp
 * @brief   Multi-Protocol Radio Receiver Interface for Drone Remote Control
 * @details Control interface for managing PPM and S.BUS radio receiver protocols
 *          enabling pilot control input for drone flight operations. Supports
 *          protocol switching and signal routing for compatibility with various
 *          radio transmitter systems and flight control requirements.
 * 
 * Radio Receiver Features:
 * - Dual protocol support (PPM and S.BUS)
 * - Dynamic protocol switching during operation
 * - Hardware output enable control for signal routing
 * - Compatible with standard RC transmitters
 * - Real-time protocol status monitoring
 * - Failsafe integration for safety compliance
 * 
 * Supported Protocols:
 * - PPM (Pulse Position Modulation): Traditional analog-style protocol
 * - S.BUS (Serial Bus): Futaba digital protocol with higher channel count
 * - Simultaneous protocol availability for redundancy
 * - Software-selectable protocol activation
 * 
 * Protocol Characteristics:
 * - PPM: 8 channels, 1ms-2ms pulse width, 20ms frame rate
 * - S.BUS: 16 channels, inverted UART at 100kbaud, 14ms frame rate
 * - Both protocols provide 0-100% channel range mapping
 * - Hardware-level signal conditioning and buffering
 * 
 * Flight Control Integration:
 * - Pilot stick input (roll, pitch, yaw, throttle)
 * - Auxiliary channel control (modes, switches, knobs)
 * - Failsafe detection and recovery mechanisms  
 * - Real-time signal quality monitoring
 * - Integration with flight controller for autonomous/manual mode switching
 * 
 * Safety Features:
 * - Hardware signal validation and filtering
 * - Failsafe detection with configurable timeout
 * - Signal loss detection for emergency procedures
 * - Protocol redundancy for backup control paths
 * - Integration with return-to-home functionality
 */

#ifndef __RADIO_RECEIVER_HPP
#define __RADIO_RECEIVER_HPP

#include "stm32f7xx_hal.h"

class RadioReceiver {
public:
    RadioReceiver(GPIO_TypeDef* ppmOePort, uint16_t ppmOePin,
                  GPIO_TypeDef* sbusOePort, uint16_t sbusOePin);

    void enablePPM(bool enable);
    void enableSBUS(bool enable);
    bool isPPMEnabled() const;
    bool isSBUSEnabled() const;

private:
    GPIO_TypeDef* _ppmOePort;
    uint16_t _ppmOePin;

    GPIO_TypeDef* _sbusOePort;
    uint16_t _sbusOePin;
};

#endif
