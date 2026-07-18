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
 * @file    radio_receiver.cpp
 * @brief   Radio Receiver Driver Implementation
 */
#include "radio_receiver.hpp"

RadioReceiver::RadioReceiver(GPIO_TypeDef* ppmOePort, uint16_t ppmOePin,
                             GPIO_TypeDef* sbusOePort, uint16_t sbusOePin)
    : _ppmOePort(ppmOePort), _ppmOePin(ppmOePin),
      _sbusOePort(sbusOePort), _sbusOePin(sbusOePin) {}

void RadioReceiver::enablePPM(bool enable)
{
    HAL_GPIO_WritePin(_ppmOePort, _ppmOePin, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void RadioReceiver::enableSBUS(bool enable)
{
    HAL_GPIO_WritePin(_sbusOePort, _sbusOePin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool RadioReceiver::isPPMEnabled() const
{
    return HAL_GPIO_ReadPin(_ppmOePort, _ppmOePin) == GPIO_PIN_RESET; // active Low signal
}

bool RadioReceiver::isSBUSEnabled() const
{
    return HAL_GPIO_ReadPin(_sbusOePort, _sbusOePin) == GPIO_PIN_SET;
}
