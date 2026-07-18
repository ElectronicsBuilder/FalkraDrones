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
 * @file    qspi_flash.cpp
 * @brief   Quad-SPI Flash Memory Driver Implementation
 * @details High-speed QSPI flash driver with memory-mapped mode support for
 *          TouchGFX graphics assets. Provides optimized read/write operations
 *          with DMA support for maximum bandwidth utilization.
 */

#include "qspi_flash.hpp"
#include "log.hpp"
#include "w25q128.h"

#define QSPI_TIMEOUT_DEFAULT HAL_QPSI_TIMEOUT_DEFAULT_VALUE

// Global flag for QSPI DMA transfer completion
volatile bool qspi_dma_tx_done = false;

// Global QSPI Flash instance for backward compatibility with boot_fuse and other components
// This will be properly initialized through DriverManager in Phase 2+
extern QSPI_HandleTypeDef hqspi;
QspiFlash flash(&hqspi);


QspiFlash::QspiFlash(QSPI_HandleTypeDef* qspiHandle)
    : qspiHandle(qspiHandle) {}

QspiFlash::~QspiFlash() {
    deinit();
}

void QspiFlash::deinit() {
    // Reset internal state - do not disable QSPI peripheral
    // let main app manage hardware teardown
    LOG_SYSSTATUS("[QspiFlash] QspiFlash cleanup complete");
}

/**
  * @brief  Initialize the QSPI flash (reset device)
  */
void QspiFlash::init()
{
    reset();
}

/**
  * @brief  Reset the flash memory
  */
void QspiFlash::reset()
{
    QSPI_CommandTypeDef s_command = {0};

    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.Instruction = RESET_ENABLE_CMD;
    s_command.AddressMode = QSPI_ADDRESS_NONE;
    s_command.DataMode = QSPI_DATA_NONE;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    s_command.Instruction = RESET_MEMORY_CMD;
    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    autoPollingMemReady();
}

/**
  * @brief  Read Flash ID (JEDEC ID)
  * @param  idBuffer: Buffer to store ID (size 3 bytes)
  */
void QspiFlash::readID(uint8_t* idBuffer)
{
    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = READ_JEDEC_ID_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.NbData = 3;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Receive(qspiHandle, idBuffer, QSPI_TIMEOUT_DEFAULT);
}

/**
  * @brief  Read data from flash (normal 1-line read)
  */
void QspiFlash::readData(uint32_t address, uint8_t* buffer, size_t size)
{
    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = READ_DATA_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = address;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.DummyCycles = 0;
    s_command.NbData = size;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Receive(qspiHandle, buffer, QSPI_TIMEOUT_DEFAULT);
}

/**
  * @brief  Read data from flash using Quad mode
  */
void QspiFlash::readDataQuad(uint32_t address, uint8_t* buffer, size_t size)
{
    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = FAST_READ_QUAD_OUT_CMD; // 0x6B
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = address;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    s_command.DataMode = QSPI_DATA_4_LINES;
    s_command.DummyCycles = W25Q128J_DUMMY_CYCLES_READ; // 8 dummy cycles needed for 0x6B read
    s_command.NbData = size;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Receive(qspiHandle, buffer, QSPI_TIMEOUT_DEFAULT);
}

/**
  * @brief  Write data to flash (normal 1-line write)
  */
void QspiFlash::writeData(uint32_t address, const uint8_t* data, size_t size)
{
    uint32_t currentAddr = address;
    const uint8_t* currentData = data;
    uint32_t remaining = size;

    while (remaining > 0) {
        uint32_t chunk = 256 - (currentAddr % 256);
        if (chunk > remaining) chunk = remaining;

        inlineWriteEnable();

        QSPI_CommandTypeDef s_command = {0};

        s_command.Instruction = PAGE_PROG_CMD;
        s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        s_command.AddressMode = QSPI_ADDRESS_1_LINE;
        s_command.AddressSize = QSPI_ADDRESS_24_BITS;
        s_command.Address = currentAddr;
        s_command.DataMode = QSPI_DATA_1_LINE;
        s_command.NbData = chunk;

        HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
        HAL_QSPI_Transmit(qspiHandle, const_cast<uint8_t*>(currentData), QSPI_TIMEOUT_DEFAULT);

        autoPollingMemReady();

        currentAddr += chunk;
        currentData += chunk;
        remaining -= chunk;
    }
}

/**
  * @brief  Write data to flash using Quad mode
  */
void QspiFlash::writeDataQuad(uint32_t address, const uint8_t* data, size_t size)
{
    uint32_t currentAddr = address;
    const uint8_t* currentData = data;
    uint32_t remaining = size;

    while (remaining > 0) {
        uint32_t chunk = 256 - (currentAddr % 256);
        if (chunk > remaining) chunk = remaining;

        inlineWriteEnable();

        QSPI_CommandTypeDef s_command = {0};

        s_command.Instruction = QUAD_PAGE_PROG_CMD; // 0x32
        s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        s_command.AddressMode = QSPI_ADDRESS_1_LINE;
        s_command.AddressSize = QSPI_ADDRESS_24_BITS;
        s_command.Address = currentAddr;
        s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
        s_command.DataMode = QSPI_DATA_4_LINES;
        s_command.DummyCycles = 0;
        s_command.NbData = chunk;
        s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
        s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
        s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

        HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
        HAL_QSPI_Transmit(qspiHandle, const_cast<uint8_t*>(currentData), QSPI_TIMEOUT_DEFAULT);

        autoPollingMemReady();

        currentAddr += chunk;
        currentData += chunk;
        remaining -= chunk;
    }
}

/**
  * @brief  Erase a sector at specified address
  */
void QspiFlash::eraseSector(uint32_t address)
{
    inlineWriteEnable();

    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = SECTOR_ERASE_CMD; // 0x20
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = address;
    s_command.DataMode = QSPI_DATA_NONE;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    autoPollingMemReady(W25Q128J_SECTOR_ERASE_MAX_TIME);
}

/**
  * @brief  Erase the entire chip
  */
void QspiFlash::eraseChip()
{
    inlineWriteEnable();

    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = CHIP_ERASE_CMD; // 0xC7
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.AddressMode = QSPI_ADDRESS_NONE;
    s_command.DataMode = QSPI_DATA_NONE;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    autoPollingMemReady(W25Q128J_CHIP_ERASE_MAX_TIME);
}

/**
  * @brief  Set the Quad Enable (QE) bit in the status register
  */
void QspiFlash::setQuadEnable()
{
    uint8_t sr2 = 0;
    QSPI_CommandTypeDef s_command = {0};

    // Read SR2
    s_command.Instruction = READ_STATUS_REG_2_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.AddressMode = QSPI_ADDRESS_NONE;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.NbData = 1;
    s_command.DummyCycles = 0;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Receive(qspiHandle, &sr2, QSPI_TIMEOUT_DEFAULT);

    // Enable Volatile SR Write
    s_command.Instruction = VOL_SR_WRITE_ENABLE_CMD;
    s_command.DataMode = QSPI_DATA_NONE;
    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    // Set QE bit and write back SR2
    sr2 |= W25Q128J_SR2_QE;
    s_command.Instruction = WRITE_STATUS_REG_2_CMD;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.NbData = 1;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Transmit(qspiHandle, &sr2, QSPI_TIMEOUT_DEFAULT);
}

/**
  * @brief  Enable memory-mapped mode (1-1-1 normal read)
  */
void QspiFlash::enableMemoryMappedMode()
{
    QSPI_CommandTypeDef s_command = {0};
    QSPI_MemoryMappedTypeDef mem_mapped_cfg = {0};

    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.Instruction = READ_DATA_CMD;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = 0;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.DummyCycles = 0;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    mem_mapped_cfg.TimeOutPeriod = 0;

    HAL_QSPI_MemoryMapped(qspiHandle, &s_command, &mem_mapped_cfg);
}

/**
  * @brief  Enable memory-mapped mode (1-4-4 quad read)
  */
void QspiFlash::enableQuadMemoryMappedMode()
{
    QSPI_CommandTypeDef s_command = {0};
    QSPI_MemoryMappedTypeDef mem_mapped_cfg = {0};

    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.Instruction = FAST_READ_QUAD_INOUT_CMD; // 0xEB
    s_command.AddressMode = QSPI_ADDRESS_4_LINES;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = 0;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES;
    s_command.AlternateBytes = 0xFF;
    s_command.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    s_command.DataMode = QSPI_DATA_4_LINES;
    s_command.DummyCycles = W25Q128J_DUMMY_CYCLES_READ_QUAD;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    mem_mapped_cfg.TimeOutPeriod = 0;

    if (HAL_QSPI_MemoryMapped(qspiHandle, &s_command, &mem_mapped_cfg) != HAL_OK) {
        LOG_ERROR("[ERROR] HAL_QSPI_MemoryMapped failed (Quad Mode)");
    }
}

/**
  * @brief  Enable memory-mapped mode (1-2-2 dual read)
  */
void QspiFlash::enableDualMemoryMappedMode()
{
    QSPI_CommandTypeDef s_command = {0};
    QSPI_MemoryMappedTypeDef mem_mapped_cfg = {0};

    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.Instruction = FAST_READ_DUAL_OUT_CMD;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.Address = 0;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    s_command.DataMode = QSPI_DATA_2_LINES;
    s_command.DummyCycles = 4;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    mem_mapped_cfg.TimeOutPeriod = 0;

    if (HAL_QSPI_MemoryMapped(qspiHandle, &s_command, &mem_mapped_cfg) != HAL_OK) {
        LOG_ERROR("[ERROR] HAL_QSPI_MemoryMapped failed (Dual Mode)");
    }
}

/**
  * @brief  Write data to flash using Quad mode with DMA
  * @param  address: Flash start address
  * @param  data: Pointer to input buffer
  * @param  size: Number of bytes to write
  * @retval true if started OK, false if error
  */
 bool QspiFlash::writeDataQuadDMA(uint32_t address, const uint8_t* data, size_t size)
 {
     if (size == 0 || data == nullptr) {
         return false;
     }
 
     inlineWriteEnable();
 
     QSPI_CommandTypeDef s_command = {0};
 
     s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
     s_command.Instruction = QUAD_PAGE_PROG_CMD; // 0x32
     s_command.AddressMode = QSPI_ADDRESS_1_LINE;
     s_command.AddressSize = QSPI_ADDRESS_24_BITS;
     s_command.Address = address;
     s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
     s_command.DataMode = QSPI_DATA_4_LINES;
     s_command.DummyCycles = 0;
     s_command.NbData = size;
     s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
     s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
     s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
     if (HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT) != HAL_OK) {
         LOG_ERROR("[ERROR] HAL_QSPI_Command failed before DMA transmit");
         return false;
     }
 
     /* Start DMA transmit */
     if (HAL_QSPI_Transmit_DMA(qspiHandle, const_cast<uint8_t*>(data)) != HAL_OK) {
         LOG_ERROR("[ERROR] HAL_QSPI_Transmit_DMA failed to start");
         return false;
     }
 
     /* Note: You must wait for completion in your app! */
     return true;
 }
 
 /**
  * @brief  Wait until DMA operation completes or timeout occurs
  * @param  timeout_ms: Timeout in milliseconds
  * @retval true if DMA completed, false if timeout
  */
bool QspiFlash::waitDmaComplete(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (!qspi_dma_tx_done)
    {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            LOG_ERROR("[ERROR] waitDmaComplete() timed out after %lu ms", timeout_ms);
            return false;
        }
    }

    qspi_dma_tx_done = false; // Reset for next operation
    return true;
}


/**
  * @brief  Read data from flash using Quad mode with DMA
  * @param  address: Flash start address
  * @param  buffer: Pointer to receive buffer
  * @param  size: Number of bytes to read
  * @retval true if started OK, false if error
  */
 bool QspiFlash::readDataQuadDMA(uint32_t address, uint8_t* buffer, size_t size)
 {
     if (size == 0 || buffer == nullptr) {
         return false;
     }
 
     QSPI_CommandTypeDef s_command = {0};
 
     /* Initialize Quad Output Fast Read command */
     s_command.Instruction = FAST_READ_QUAD_OUT_CMD; // 0x6B
     s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
     s_command.AddressMode = QSPI_ADDRESS_1_LINE;
     s_command.AddressSize = QSPI_ADDRESS_24_BITS;
     s_command.Address = address;
     s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
     s_command.DataMode = QSPI_DATA_4_LINES;
     s_command.DummyCycles = 8; // For 0x6B, quad output fast read
     s_command.NbData = size;
     s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
     s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
     s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
 
     /* Send the command */
     if (HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT) != HAL_OK) {
         LOG_ERROR("[ERROR] HAL_QSPI_Command failed before DMA receive");
         return false;
     }
 
     /* Start DMA receive */
     if (HAL_QSPI_Receive_DMA(qspiHandle, buffer) != HAL_OK) {
         LOG_ERROR("[ERROR] HAL_QSPI_Receive_DMA failed to start");
         return false;
     }
 
     /* Note: You must wait for completion separately */
     return true;
 }

 

/**
  * @brief  Get the flash device information
  */
QFlashDeviceInfo QspiFlash::getDeviceInfo()
{
    return {
        "W25Q128JVEIQ",
        128,
        256,
        4096,
        true
    };
}

// --- Private helpers ---

void QspiFlash::inlineWriteEnable()
{
    QSPI_CommandTypeDef s_command = {0};

    s_command.Instruction = WRITE_ENABLE_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.DataMode = QSPI_DATA_NONE;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);

    uint8_t status = getStatus();
    if (!(status & W25Q128J_SR_WEL)) {
        LOG_ERROR("[ERROR] WEL not set after Write Enable");
    }
}

void QspiFlash::autoPollingMemReady(uint32_t timeout)
{
    QSPI_CommandTypeDef s_command = {0};
    QSPI_AutoPollingTypeDef config = {0};

    s_command.Instruction = READ_STATUS_REG_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.DataMode = QSPI_DATA_1_LINE;

    config.Match = 0x00;
    config.Mask = 0x01;
    config.MatchMode = QSPI_MATCH_MODE_AND;
    config.StatusBytesSize = 1;
    config.Interval = 0x10;
    config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    HAL_QSPI_AutoPolling(qspiHandle, &s_command, &config, timeout);
}

uint8_t QspiFlash::getStatus()
{
    QSPI_CommandTypeDef s_command = {0};
    uint8_t reg = 0;

    s_command.Instruction = READ_STATUS_REG_CMD;
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.NbData = 1;

    HAL_QSPI_Command(qspiHandle, &s_command, QSPI_TIMEOUT_DEFAULT);
    HAL_QSPI_Receive(qspiHandle, &reg, QSPI_TIMEOUT_DEFAULT);

    return reg;
}

bool QspiFlash::disableMemoryMappedMode() {
    if (!qspiHandle) return false;

    if (HAL_QSPI_Abort(qspiHandle) != HAL_OK) {
        LOG_ERROR("[QSPI] HAL_QSPI_Abort failed");
        return false;
    }

    qspiHandle->Instance->CCR &= ~(QUADSPI_CCR_FMODE);

    while (qspiHandle->Instance->SR & QUADSPI_SR_BUSY) {
        __NOP();
    }

    LOG_INFO("[QSPI] MMAP mode disabled and flash ready");
    return true;
}


void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    qspi_dma_tx_done = true; // A global or static volatile flag
}

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    qspi_dma_tx_done = true;
}
