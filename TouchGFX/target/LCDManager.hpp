/**
 * @file    LCDManager.hpp
 * @brief   LCD Manager Interface Header
 * @details Defines the interface for LCD display management with TouchGFX framework,
 *          including frame buffer transfer and display update functions
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

#ifndef LCDMANAGER_H
#define LCDMANAGER_H

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define USE_SINGLE_BUFFER

void LCDManager_SendFrameBufferBlockWithPosition(uint16_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h);


#ifdef USE_PARTIAL_BUFFER
void touchgfxDisplayDriverTransmitBlock(const uint8_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
int  touchgfxDisplayDriverTransmitActive(void);
#endif

uint32_t LCDManager_IsTransmittingData(void);
void     LCDManager_TransferComplete(void);

#ifdef __cplusplus
}
#endif

#endif // LCDMANAGER_H
