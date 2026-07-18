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
 * @file    tps2115.cpp
 * @brief   TPS2115 Power Management IC Driver Implementation
 */
#include "tps2115.hpp"

TPS2115::TPS2115(GPIO_TypeDef* statPort, uint16_t statPin,
                 GPIO_TypeDef* d1Port, uint16_t d1Pin)
    : _statPort(statPort), _statPin(statPin),
      _d1Port(d1Port), _d1Pin(d1Pin) {}

void TPS2115::setD1(bool level)
{
    HAL_GPIO_WritePin(_d1Port, _d1Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool TPS2115::getD1State() const
{
    return HAL_GPIO_ReadPin(_d1Port, _d1Pin) == GPIO_PIN_SET;
}

TPS2115::InputSource TPS2115::getActiveInput()
{
    GPIO_PinState state = HAL_GPIO_ReadPin(_statPort, _statPin);
    return (state == GPIO_PIN_SET) ? InputSource::IN2 : InputSource::IN1;
}
