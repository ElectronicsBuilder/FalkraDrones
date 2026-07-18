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
 * @file    w25q128.h
 * @brief   W25Q128 Flash Memory Driver
 */
#ifndef W25Q128_H
#define W25Q128_H

#include "stm32f7xx_hal.h"

#define W25Q128J_FLASH_SIZE                  0x1000000 /* 128 MBits => 16MBytes */
#define W25Q128J_BLOCK_SIZE                  0x10000   /* 256 blocks of 64KBytes */
#define W25Q128J_SUBBLOCK_SIZE               0x8000    /* 512 blocks of 32 KBytes */
#define W25Q128J_SECTOR_SIZE                 0x1000    /* 4096 sectors of 4kBytes */
#define W25Q128J_PAGE_SIZE                   0x100     /* 65536 pages of 256 bytes */

#define W25Q128J_DUMMY_CYCLES_READ           8
#define W25Q128J_DUMMY_CYCLES_READ_QUAD      4

#define W25Q128J_CHIP_ERASE_MAX_TIME         200000
#define W25Q128J_BLOCK_ERASE_MAX_TIME        2000
#define W25Q128J_SUBBLOCK_ERASE_MAX_TIME     1600
#define W25Q128J_SECTOR_ERASE_MAX_TIME       400

/** 
  * @brief  W25Q128J Commands  
  */  
/* Reset Operations */
#define RESET_ENABLE_CMD                     0x66
#define RESET_MEMORY_CMD                     0x99

/* Identification Operations */
#define READ_ID_CMD                          0x90
#define READ_ID_DUAL_CMD                     0x92
#define READ_ID_QUAD_CMD                     0x94

#define READ_JEDEC_ID_CMD                    0x9F
#define READ_UNIQUE_ID_CMD                   0x4B

/* Read Operations */
#define READ_DATA_CMD                        0x03
#define FAST_READ_CMD                        0x0B

#define FAST_READ_DUAL_OUT_CMD               0x3B
#define FAST_READ_DUAL_INOUT_CMD             0xBB

#define FAST_READ_QUAD_OUT_CMD               0x6B
#define FAST_READ_QUAD_INOUT_CMD             0xEB

/* Write Operations */
#define WRITE_ENABLE_CMD                     0x06
#define VOL_SR_WRITE_ENABLE_CMD              0x50
#define WRITE_DISABLE_CMD                    0x04

/* Register Operations */
#define READ_STATUS_REG_CMD                  0x05
#define READ_STATUS_REG_2_CMD                0x35
#define READ_STATUS_REG_3_CMD                0x15
#define WRITE_STATUS_REG_CMD                 0x01
#define WRITE_STATUS_REG_2_CMD               0x31
#define WRITE_STATUS_REG_3_CMD               0x11

#define READ_SFDP_REG_CMD                    0x5A
#define READ_BLOCK_SECTOR_LOCK               0x3D

#define ERASE_SECURITY_REG_CMD               0x44
#define PROG_SECURITY_REG_CMD                0x42
#define READ_SECURITY_REG_CMD                0x48

/* Program Operations */
#define PAGE_PROG_CMD                        0x02
#define QUAD_PAGE_PROG_CMD                   0x32

/* Erase Operations */
#define SECTOR_ERASE_CMD                     0x20
#define BLOCK_ERASE_32K_CMD                  0x52
#define BLOCK_ERASE_64K_CMD                  0xD8
#define CHIP_ERASE_CMD                       0xC7
#define CHIP_ERASE_CMD_1                     0x60

#define ERASE_PROG_SUSPEND_CMD               0x75
#define ERASE_PROG_RESUME_CMD                0x7A

/* Power-down operations */
#define POWER_DOWN_CMD                       0xB9
#define RELEASE_POWER_DOWN_CMD               0xAB

/* Other operations */
#define SET_BURST_WRAP_CMD                   0x77
#define INDIV_BLOCK_SECTOR_LOCK              0x36
#define INDIV_BLOCK_SECTOR_UNLOCK            0x39
#define GLOBAL_BLOCK_SECTOR_LOCK             0x7E
#define GLOBAL_BLOCK_SECTOR_UNLOCK           0x98

/** 
  * @brief  W25Q128J Registers  
  */ 
/* Status Register 1 */
#define W25Q128J_SR_BUSY                     ((uint8_t)0x01)    /*!< Erase/Write in progress */
#define W25Q128J_SR_WEL                      ((uint8_t)0x02)    /*!< Write enable latch */
#define W25Q128J_SR_BP                       ((uint8_t)0x1C)    /*!< Block protected bits */
#define W25Q128J_SR_TB                       ((uint8_t)0x20)    /*!< Top/Bottom protect */
#define W25Q128J_SR_SEC                      ((uint8_t)0x40)    /*!< Sector protect */

/* Status Register 2 */
#define W25Q128J_SR2_SRL                     ((uint8_t)0x01)    /*!< Status Register Lock */
#define W25Q128J_SR2_QE                      ((uint8_t)0x02)    /*!< Quad Enable */
#define W25Q128J_SR2_LB                      ((uint8_t)0x3C)    /*!< Security Register Lock bits */
#define W25Q128J_SR2_CMP                     ((uint8_t)0x40)    /*!< Complement protect */
#define W25Q128J_SR2_SUS                     ((uint8_t)0x80)    /*!< Suspend Status */

/* Status Register 3 */
#define W25Q128J_SR3_WPS                     ((uint8_t)0x04)    /*!< Write Protect Selection */
#define W25Q128J_SR3_DRV                     ((uint8_t)0x60)    /*!< Output Driver Strength */


#endif /* W25Q128_H */
