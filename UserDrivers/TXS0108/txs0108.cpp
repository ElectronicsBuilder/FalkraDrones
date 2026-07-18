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
 * @file    txs0108.cpp
 * @brief   TXS0108 Level Shifter Driver Implementation
 */
#include "txs0108.hpp"

TXS0108::TXS0108(GPIO_TypeDef* oePort, uint16_t oePin)
    : _oePort(oePort), _oePin(oePin) {}

void TXS0108::enable() {
    HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_SET);
}

void TXS0108::disable() {
    HAL_GPIO_WritePin(_oePort, _oePin, GPIO_PIN_RESET);
}

bool TXS0108::isEnabled() const {
    return HAL_GPIO_ReadPin(_oePort, _oePin) == GPIO_PIN_SET;
}
