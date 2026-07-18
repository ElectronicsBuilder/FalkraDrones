/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : TouchGFXHAL.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.25.0. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include <TouchGFXHAL.hpp>

/* USER CODE BEGIN TouchGFXHAL.cpp */
extern uint16_t frameBuf[(170 * 320 * 2 + 3) / 4 ] ;

#include "LCDManager.hpp"

#include <stdio.h>
#include <string.h>
#include <touchgfx/hal/OSWrappers.hpp>
#include "LCDManager.hpp"
#include "st7789.hpp"
#include "st7789_defs.h"


using namespace touchgfx;

void TouchGFXHAL::initialize()
{
    // Calling parent implementation of initialize().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.
    // Please note, HAL::initialize() must be called to initialize the framework.

    TouchGFXGeneratedHAL::initialize();
}

/**
 * Gets the frame buffer address used by the TFT controller.
 *
 * @return The address of the frame buffer currently being displayed on the TFT.
 */
uint16_t* TouchGFXHAL::getTFTFrameBuffer() const
{
    // Calling parent implementation of getTFTFrameBuffer().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    return TouchGFXGeneratedHAL::getTFTFrameBuffer();
}

/**
 * Sets the frame buffer address used by the TFT controller.
 *
 * @param [in] address New frame buffer address.
 */
void TouchGFXHAL::setTFTFrameBuffer(uint16_t* address)
{
    // Calling parent implementation of setTFTFrameBuffer(uint16_t* address).
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::setTFTFrameBuffer(address);
}

/**
 * This function is called whenever the framework has performed a partial draw.
 *
 * @param rect The area of the screen that has been drawn, expressed in absolute coordinates.
 *
 * @see flushFrameBuffer().
 */
/*
void TouchGFXHAL::flushFrameBuffer(const touchgfx::Rect& rect)
{
    // Calling parent implementation of flushFrameBuffer(const touchgfx::Rect& rect).
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.
    // Please note, HAL::flushFrameBuffer(const touchgfx::Rect& rect) must
    // be called to notify the touchgfx framework that flush has been performed.
    // To calculate the start address of rect,
    // use advanceFrameBufferToRect(uint8_t* fbPtr, const touchgfx::Rect& rect)
    // defined in TouchGFXGeneratedHAL.cpp

    TouchGFXGeneratedHAL::flushFrameBuffer(rect);
}
*/

void TouchGFXHAL::flushFrameBuffer(const touchgfx::Rect& rect)
{
#ifdef USE_SINGLE_BUFFER
    HAL::getInstance()->lockFrameBuffer();

    if (!LCDManager_IsTransmittingData())
    {
        uint16_t* pixels = TouchGFXGeneratedHAL::getTFTFrameBuffer();
        
        // Optionally clip rect.x, rect.y, rect.width, rect.height here if needed
        LCDManager_SendFrameBufferBlockWithPosition(pixels, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    }

    // Notify TouchGFX that flush is complete
    TouchGFXGeneratedHAL::flushFrameBuffer(rect);

    HAL::getInstance()->unlockFrameBuffer();
#endif

#ifdef USE_PARTIAL_BUFFER
    HAL::flushFrameBuffer(rect);  // Notify TouchGFX core

    frameBufferAllocator->markBlockReadyForTransfer();

    if (!touchgfxDisplayDriverTransmitActive())
    {
        touchgfx::Rect r;
        const uint8_t* pixels = frameBufferAllocator->getBlockForTransfer(r);
        touchgfxDisplayDriverTransmitBlock((uint8_t*)pixels, r.x, r.y, r.width, r.height);
    }
#endif
}




bool TouchGFXHAL::blockCopy(void* RESTRICT dest, const void* RESTRICT src, uint32_t numBytes)
{
    return TouchGFXGeneratedHAL::blockCopy(dest, src, numBytes);
}

/**
 * Configures the interrupts relevant for TouchGFX. This primarily entails setting
 * the interrupt priorities for the DMA and LCD interrupts.
 */
void TouchGFXHAL::configureInterrupts()
{
    // Calling parent implementation of configureInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::configureInterrupts();
}

/**
 * Used for enabling interrupts set in configureInterrupts()
 */
void TouchGFXHAL::enableInterrupts()
{
    // Calling parent implementation of enableInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::enableInterrupts();
}

/**
 * Used for disabling interrupts set in configureInterrupts()
 */
void TouchGFXHAL::disableInterrupts()
{
    // Calling parent implementation of disableInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::disableInterrupts();
}

/**
 * Configure the LCD controller to fire interrupts at VSYNC. Called automatically
 * once TouchGFX initialization has completed.
 */
void TouchGFXHAL::enableLCDControllerInterrupt()
{
    // Calling parent implementation of enableLCDControllerInterrupt().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::enableLCDControllerInterrupt();
}

bool TouchGFXHAL::beginFrame()
{
    return TouchGFXGeneratedHAL::beginFrame();
}

void TouchGFXHAL::endFrame()
{
    TouchGFXGeneratedHAL::endFrame();
}


#ifdef USE_PARTIAL_BUFFER

	static volatile bool blockIsTransferred = false;

	void touchgfx:: FrameBufferAllocatorWaitOnTransfer()
	{
	  while(!blockIsTransferred);
	}


	void touchgfx:: FrameBufferAllocatorSignalBlockDrawn()
	{
		blockIsTransferred = false;
		return;
	}

#endif



extern "C" void touchgfx_signalVSyncTimer(void) { 
	HAL::getInstance()->vSync();
	touchgfx::OSWrappers::signalVSync();
}


void TouchGFXHAL::taskEntry() {
	enableLCDControllerInterrupt();
	enableInterrupts();

	OSWrappers::waitForVSync();
	backPorchExited();



	for (;;) {
		OSWrappers::waitForVSync();

		backPorchExited();
	}
}


/* USER CODE END TouchGFXHAL.cpp */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
