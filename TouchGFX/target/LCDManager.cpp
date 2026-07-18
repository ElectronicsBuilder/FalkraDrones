
/**
 * @file    LCDManager.cpp
 * @brief   LCD Manager Implementation
 * @details Implements the LCD display manager for TouchGFX framework integration,
 *          handling frame buffer transfers and display updates
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

#include "LCDManager.hpp"
#include <st7789.hpp>
#include "display_driver.hpp"
#include "driver_manager.hpp"

extern void LCDManager_TransferComplete();


void DisplayDriver_TransferCompleteCallback();

static __IO uint8_t isTransmittingData = 0;

extern uint16_t DISPLAY_DONE_UPDATE;


uint32_t LCDManager_IsTransmittingData(void)
{
	return isTransmittingData;
}

#ifdef USE_SINGLE_BUFFER
void LCDManager_SendFrameBufferBlockWithPosition(uint16_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{

	isTransmittingData = 1;

	#if defined(STM32F7)
    SCB_CleanDCache_by_Addr((uint32_t *)pixels, w * h * 2);
	#endif

	// Get ST7789 display instance from DriverManager
	auto& dm = DriverManager::getInstance();
	ST7789* display = dm.getDisplay();
	if (display) {
		display->drawImage(x, y, w, h, pixels);
	}

	if (display->DisplayBusyStatus == 1) {
		isTransmittingData = 0;

		}

}

#endif

#ifdef USE_PARTIAL_BUFFER


void LCDManager_SendFrameBufferBlockWithPosition(uint8_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{

	isTransmittingData = 1;

	ST7789_DrawImage(x, y, w, h, (uint16_t *)pixels);

	isTransmittingData = 0;
		LCDManager_TransferComplete();

}


extern "C"
void touchgfxDisplayDriverTransmitBlock(const uint8_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	isTransmittingData = 1;

	ST7789_DrawImage(x, y, w, h, (uint8_t *)pixels);

	if(isTransmittingData == 0)
	{
		DisplayDriver_TransferCompleteCallback();
	}

}






extern "C"

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef * hspi1)
{

		 isTransmittingData = 0;

}



extern "C"
int touchgfxDisplayDriverTransmitActive()
{
	return LCDManager_IsTransmittingData();
}


#endif



