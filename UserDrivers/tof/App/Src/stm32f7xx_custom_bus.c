/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : stm32f4xx_nucleo_bus.c
  * @brief          : source file for the BSP BUS IO driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
*/
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_custom_bus.h"
#include <main.h>
#include "tof_speed_opts.h"

#if TOF_OPT_I2C_ASYNC_MODE
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

#define USE_CUBEMX_BSP_V2 0

#define USE_STM32F7_I2C_HAL_INIT

uint8_t I2C1_INIT_DONE = 0;


__weak HAL_StatusTypeDef BSP_MX_I2C1_Init(I2C_HandleTypeDef* hi2c);

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F4XX_NUCLEO
  * @{
  */

/** @defgroup STM32F4XX_NUCLEO_BUS STM32F4XX_NUCLEO BUS
  * @{
  */

/** @defgroup STM32F4XX_NUCLEO_BUS_Exported_Variables BUS Exported Variables
  * @{
  */
#ifndef USE_STM32F7_I2C_HAL_INIT
I2C_HandleTypeDef hi2c1;
#endif
/**
  * @}
  */

/** @defgroup STM32F4XX_NUCLEO_BUS_Private_Variables BUS Private Variables
  * @{
  */

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
static uint32_t IsI2C1MspCbValid = 0;
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
static uint32_t I2C1InitCounter = 0;

#if TOF_OPT_I2C_ASYNC_MODE
#if TOF_OPT_I2C_DMA_MODE
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;
static uint8_t i2c1_dma_ready = 0;
#endif

static SemaphoreHandle_t i2c1_it_done = NULL;
static StaticSemaphore_t i2c1_it_done_buf;
static volatile HAL_StatusTypeDef i2c1_it_status = HAL_OK;
static volatile uint32_t i2c1_it_error = HAL_I2C_ERROR_NONE;
static volatile uint16_t i2c1_it_dev_addr = 0;

static int32_t BSP_I2C1_AsyncInit(void)
{
  if (i2c1_it_done == NULL)
  {
    i2c1_it_done = xSemaphoreCreateBinaryStatic(&i2c1_it_done_buf);
    if (i2c1_it_done == NULL)
    {
      return BSP_ERROR_PERIPH_FAILURE;
    }
  }

  while (xSemaphoreTake(i2c1_it_done, 0) == pdTRUE)
  {
  }

  i2c1_it_status = HAL_OK;
  i2c1_it_error = HAL_I2C_ERROR_NONE;
#if TOF_OPT_I2C_DMA_MODE
  if (!i2c1_dma_ready)
  {
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_i2c1_rx.Instance = DMA1_Stream0;
    hdma_i2c1_rx.Init.Channel = DMA_CHANNEL_1;
    hdma_i2c1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2c1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c1_rx.Init.Mode = DMA_NORMAL;
    hdma_i2c1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_i2c1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_i2c1_rx) != HAL_OK)
    {
      return BSP_ERROR_PERIPH_FAILURE;
    }
    __HAL_LINKDMA(&hi2c1, hdmarx, hdma_i2c1_rx);

    hdma_i2c1_tx.Instance = DMA1_Stream6;
    hdma_i2c1_tx.Init.Channel = DMA_CHANNEL_1;
    hdma_i2c1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2c1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c1_tx.Init.Mode = DMA_NORMAL;
    hdma_i2c1_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_i2c1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_i2c1_tx) != HAL_OK)
    {
      return BSP_ERROR_PERIPH_FAILURE;
    }
    __HAL_LINKDMA(&hi2c1, hdmatx, hdma_i2c1_tx);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    i2c1_dma_ready = 1;
  }
#endif

  return BSP_ERROR_NONE;
}

static int32_t BSP_I2C1_MapError(uint32_t error)
{
  return (error == HAL_I2C_ERROR_AF) ? BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE
                                     : BSP_ERROR_PERIPH_FAILURE;
}

static int32_t BSP_I2C1_WaitIT(void)
{
  if (xSemaphoreTake(i2c1_it_done, pdMS_TO_TICKS(BUS_I2C1_POLL_TIMEOUT)) != pdTRUE)
  {
    (void)HAL_I2C_Master_Abort_IT(&hi2c1, i2c1_it_dev_addr);
    return BSP_ERROR_PERIPH_FAILURE;
  }

  if (i2c1_it_status != HAL_OK)
  {
    return BSP_I2C1_MapError(i2c1_it_error);
  }

  return BSP_ERROR_NONE;
}

static void BSP_I2C1_CompleteFromISR(I2C_HandleTypeDef *hi2c, HAL_StatusTypeDef status)
{
  if (hi2c->Instance != I2C1 || i2c1_it_done == NULL)
  {
    return;
  }

  i2c1_it_status = status;
  i2c1_it_error = HAL_I2C_GetError(hi2c);

  BaseType_t higher_priority_task_woken = pdFALSE;
  xSemaphoreGiveFromISR(i2c1_it_done, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static int32_t BSP_I2C1_MemWrite(uint16_t DevAddr,
                                 uint16_t Reg,
                                 uint16_t MemAddSize,
                                 uint8_t *pData,
                                 uint16_t Length)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
  {
    if (HAL_I2C_Mem_Write(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
    {
      return BSP_I2C1_MapError(HAL_I2C_GetError(&hi2c1));
    }
    return BSP_ERROR_NONE;
  }

  if (BSP_I2C1_AsyncInit() != BSP_ERROR_NONE)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  i2c1_it_dev_addr = DevAddr;
#if TOF_OPT_I2C_DMA_MODE
  if (HAL_I2C_Mem_Write_DMA(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length) != HAL_OK)
#else
  if (HAL_I2C_Mem_Write_IT(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length) != HAL_OK)
#endif
  {
    return BSP_I2C1_MapError(HAL_I2C_GetError(&hi2c1));
  }

  return BSP_I2C1_WaitIT();
}

static int32_t BSP_I2C1_MemRead(uint16_t DevAddr,
                                uint16_t Reg,
                                uint16_t MemAddSize,
                                uint8_t *pData,
                                uint16_t Length)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
  {
    if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
    {
      return BSP_I2C1_MapError(HAL_I2C_GetError(&hi2c1));
    }
    return BSP_ERROR_NONE;
  }

  if (BSP_I2C1_AsyncInit() != BSP_ERROR_NONE)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  i2c1_it_dev_addr = DevAddr;
#if TOF_OPT_I2C_DMA_MODE
  if (HAL_I2C_Mem_Read_DMA(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length) != HAL_OK)
#else
  if (HAL_I2C_Mem_Read_IT(&hi2c1, DevAddr, Reg, MemAddSize, pData, Length) != HAL_OK)
#endif
  {
    return BSP_I2C1_MapError(HAL_I2C_GetError(&hi2c1));
  }

  return BSP_I2C1_WaitIT();
}
#endif

/**
  * @}
  */

/** @defgroup STM32F4XX_NUCLEO_BUS_Private_FunctionPrototypes  BUS Private Function
  * @{
  */

static void I2C1_MspInit(I2C_HandleTypeDef* hI2c);
static void I2C1_MspDeInit(I2C_HandleTypeDef* hI2c);
#if (USE_CUBEMX_BSP_V2 == 1)
static uint32_t I2C_GetTiming(uint32_t clock_src_hz, uint32_t i2cfreq_hz);
static void Compute_PRESC_SCLDEL_SDADEL(uint32_t clock_src_freq, uint32_t I2C_Speed);
static uint32_t Compute_SCLL_SCLH (uint32_t clock_src_freq, uint32_t I2C_speed);
#endif

/**
  * @}
  */

/** @defgroup STM32F4XX_NUCLEO_LOW_LEVEL_Private_Functions STM32F4XX_NUCLEO LOW LEVEL Private Functions
  * @{
  */

/** @defgroup STM32F4XX_NUCLEO_BUS_Exported_Functions STM32F4XX_NUCLEO_BUS Exported Functions
  * @{
  */

/* BUS IO driver over I2C Peripheral */
/*******************************************************************************
                            BUS OPERATIONS OVER I2C
*******************************************************************************/
/**
  * @brief  Initialize I2C HAL
  * @retval BSP status
  */
int32_t BSP_I2C1_Init(void)
{
#ifndef USE_STM32F7_I2C_HAL_INIT

	 int32_t ret = BSP_ERROR_NONE;

	  hi2c1.Instance  = I2C1;

	  if(I2C1InitCounter++ == 0)
	  {
	    if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_RESET)
	    {
	    #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
	      /* Init the I2C Msp */
	      I2C1_MspInit(&hi2c1);
	    #else
	      if(IsI2C1MspCbValid == 0U)
	      {
	        if(BSP_I2C1_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
	        {
	          return BSP_ERROR_MSP_FAILURE;
	        }
	      }
	    #endif
	      if(ret == BSP_ERROR_NONE)
	      {
	        /* Init the I2C */
	        if(BSP_MX_I2C1_Init(&hi2c1) != HAL_OK)
	        {
	          ret = BSP_ERROR_BUS_FAILURE;
	        }
	        else
	        {
	          ret = BSP_ERROR_NONE;
	          I2C1_INIT_DONE = 1;

	        }
	      }
	    }
	  }
	  return ret;

#endif

	  MX_I2C1_Init();

	  I2C1_INIT_DONE = 1;
    return HAL_OK;

}

/**
  * @brief  DeInitialize I2C HAL.
  * @retval BSP status
  */
int32_t BSP_I2C1_DeInit(void)
{
  int32_t ret = BSP_ERROR_NONE;

  if (I2C1InitCounter > 0)
  {
    if (--I2C1InitCounter == 0)
    {
  #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* DeInit the I2C */
      I2C1_MspDeInit(&hi2c1);
  #endif
      /* DeInit the I2C */
      if (HAL_I2C_DeInit(&hi2c1) != HAL_OK)
      {
        ret = BSP_ERROR_BUS_FAILURE;
      }
    }
  }
  return ret;
}

/**
  * @brief  Check whether the I2C bus is ready.
  * @param DevAddr : I2C device address
  * @param Trials : Check trials number
  * @retval BSP status
  */
int32_t BSP_I2C1_IsReady(uint16_t DevAddr, uint32_t Trials)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_IsDeviceReady(&hi2c1, DevAddr, Trials, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    ret = BSP_ERROR_BUSY;
  }

  return ret;
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write
  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP status
  */

int32_t BSP_I2C1_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

#if TOF_OPT_I2C_ASYNC_MODE
  ret = BSP_I2C1_MemWrite(DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length);
#else
  if (HAL_I2C_Mem_Write(&hi2c1, DevAddr,Reg, I2C_MEMADD_SIZE_8BIT,pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
#endif
  return ret;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to read
  * @param  pData  Pointer to data buffer to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C1_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

#if TOF_OPT_I2C_ASYNC_MODE
  ret = BSP_I2C1_MemRead(DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length);
#else
  if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
#endif
  return ret;
}

/**

  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write

  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP statu
  */




int32_t BSP_I2C1_WriteReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

#if TOF_OPT_I2C_ASYNC_MODE
  ret = BSP_I2C1_MemWrite(DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length);
#else
  if (HAL_I2C_Mem_Write(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
//	  if (HAL_I2C_Mem_Write_IT(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
#endif
  return ret;
}

/**
  * @brief  Read registers through a bus (16 bits)
  * @param  DevAddr: Device address on BUS
  * @param  Reg: The target register address to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C1_ReadReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

#if TOF_OPT_I2C_ASYNC_MODE
  ret = BSP_I2C1_MemRead(DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length);
#else
  if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
//	  if (HAL_I2C_Mem_Read_IT(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret =  BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
#endif
  return ret;
}

#if TOF_OPT_I2C_ASYNC_MODE
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  BSP_I2C1_CompleteFromISR(hi2c, HAL_OK);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  BSP_I2C1_CompleteFromISR(hi2c, HAL_OK);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  BSP_I2C1_CompleteFromISR(hi2c, HAL_ERROR);
}
#endif

/**
  * @brief  Send an amount width data through bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C1_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Transmit(&hi2c1, DevAddr, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Receive an amount of data through a bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C1_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Receive(&hi2c1, DevAddr, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
/**
  * @brief Register Default BSP I2C1 Bus Msp Callbacks
  * @retval BSP status
  */
int32_t BSP_I2C1_RegisterDefaultMspCallbacks (void)
{

  __HAL_I2C_RESET_HANDLE_STATE(&hi2c1);

  /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPINIT_CB_ID, I2C1_MspInit)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPDEINIT_CB_ID, I2C1_MspDeInit) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }
  IsI2C1MspCbValid = 1;

  return BSP_ERROR_NONE;
}

/**
  * @brief BSP I2C1 Bus Msp Callback registering
  * @param Callbacks     pointer to I2C1 MspInit/MspDeInit callback functions
  * @retval BSP status
  */
int32_t BSP_I2C1_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks)
{
  /* Prevent unused argument(s) compilation warning */
  __HAL_I2C_RESET_HANDLE_STATE(&hi2c1);

   /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPINIT_CB_ID, Callbacks->pMspInitCb)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPDEINIT_CB_ID, Callbacks->pMspDeInitCb) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  IsI2C1MspCbValid = 1;

  return BSP_ERROR_NONE;
}
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */

/**
  * @brief  Return system tick in ms
  * @retval Current HAL time base time stamp
  */
int32_t BSP_GetTick(void) {
  return HAL_GetTick();
}

/* I2C1 init function */

__weak HAL_StatusTypeDef BSP_MX_I2C1_Init(I2C_HandleTypeDef* hi2c)
{
  HAL_StatusTypeDef ret = HAL_OK;

  hi2c->Instance = I2C1;
  //hi2c->Init.ClockSpeed = 400000;
  //hi2c->Init.DutyCycle = I2C_DUTYCYCLE_2;


  hi2c->Init.Timing = 0x6000030D;
  //hi2c->Init.Timing = 0x20404768;
  //  hi2c->Init.Timing = 0x40808ED0;


  hi2c->Init.OwnAddress1 = 0;
  hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c->Init.OwnAddress2 = 0;
  hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  return ret;
}

static void I2C1_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = BUS_I2C1_SCL_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = BUS_I2C1_SCL_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BUS_I2C1_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = BUS_I2C1_SDA_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C1_SDA_GPIO_PORT, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
}

static void I2C1_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(BUS_I2C1_SCL_GPIO_PORT, BUS_I2C1_SCL_GPIO_PIN);

    HAL_GPIO_DeInit(BUS_I2C1_SDA_GPIO_PORT, BUS_I2C1_SDA_GPIO_PIN);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
