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
 * @file    nvram.hpp
 * @brief   NVRAM (Non-Volatile RAM) Driver Interface - Instant Write Persistent Memory
 * @details Driver for Cypress CY14B101Q2-LHXI 128KB NVRAM with SPI interface.
 *          Provides instant write, unlimited endurance persistent storage for:
 *          - Flight configuration parameters
 *          - Critical system state variables
 *          - Real-time flight data logging
 *          - Emergency recovery information
 * 
 * Key Features:
 * - No write delays (instant persistence vs flash)
 * - Unlimited write endurance (vs 100K cycles for flash)
 * - Autostore backup to ensure data integrity
 * - SPI interface up to 40MHz
 * - Industrial temperature range (-40°C to +85°C)
 * 
 * Memory Layout:
 * | Address Range    | Size  | Purpose                    |
 * |------------------|-------|----------------------------|
 * | 0x00000-0x0FFFF  | 64KB  | Flight parameters          |
 * | 0x10000-0x1DFFF  | 56KB  | System configuration       |
 * | 0x1E000-0x1FFFF  | 8KB   | Emergency/recovery data    |
 * 
 * Performance Characteristics:
 * | Operation        | Time      | Endurance     |
 * |------------------|-----------|---------------|
 * | Write (any size) | <1μs      | Unlimited     |
 * | Read (any size)  | <40ns/bit | N/A           |
 * | Autostore        | <8ms      | 1M cycles     |
 * 
 */

#ifndef NVRAM_HPP
#define NVRAM_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

#define NVRAM_PART_NUMBER   "CY14B101Q2-LHXI"
#define NVRAM_CAPACITY_KB   128
#define NVRAM_HAS_AUTOSTORE 1

struct NVRAM_StatusRegister {
    uint8_t RDY  : 1;
    uint8_t WEN  : 1;
    uint8_t BP0  : 1;
    uint8_t BP1  : 1;
    uint8_t RSVD1: 1;
    uint8_t RSVD2: 1;
    uint8_t SNL  : 1;
    uint8_t WPEN : 1;
};

typedef struct {
    uint8_t customerID[2];
    uint8_t uniqueSerialNumber[5];
    uint8_t crcChecksum;
    uint8_t readBackCrcChecksum;
} SerialNumber;

struct NvramDeviceInfo {
    const char* part_number;
    uint32_t capacity_kbit;
    uint32_t capacity_kbyte;
    uint32_t max_address;
    bool has_autostore;
};


class NVRAM {
public:
    NVRAM(SPI_HandleTypeDef* spiHandle, GPIO_TypeDef* csPort, uint16_t csPin, GPIO_TypeDef* holdPort, uint16_t holdPin, GPIO_TypeDef* wpPort, uint16_t wpPin);
    ~NVRAM();

    void deinit();
    void writeByte(uint32_t address, uint8_t data);
    uint8_t readByte(uint32_t address);
    void writeArray(uint32_t address, uint8_t* data, uint16_t size);
    void readArray(uint32_t address, uint8_t* buffer, uint16_t size);

    void softwareStore();
    void softwareRecall();
    void enableAutoStore();
    void disableAutoStore();

    void writeSerialNumber(SerialNumber* snInfo);
    SerialNumber readSerialNumber();

    uint32_t readDeviceID();
    NVRAM_StatusRegister readStatusRegister();

    void writeEnable();
    void writeDisable();

    void holdEnable();
    void holdDisable();

    void wpEnable();
    void wpDisable();

    uint32_t get_NVRAM_size();
    uint32_t get_DeviceID();

    uint8_t CalculateCRC8(uint8_t* buffer, long length);

    NvramDeviceInfo getDeviceInfo();
    

private:
    void select();
    void deselect();
    void sendCommand(uint8_t cmd);

    SPI_HandleTypeDef* spiHandle;
    GPIO_TypeDef* csPort;
    uint16_t csPin;
    GPIO_TypeDef* holdPort;
    uint16_t holdPin;
    GPIO_TypeDef* wpPort;
    uint16_t wpPin;

    static constexpr uint8_t CMD_WRITE = 0x02;
    static constexpr uint8_t CMD_READ = 0x03;
    static constexpr uint8_t CMD_WREN = 0x06;
    static constexpr uint8_t CMD_WRDI = 0x04;
    static constexpr uint8_t CMD_RDSR = 0x05;
    static constexpr uint8_t CMD_STORE = 0x3C;
    static constexpr uint8_t CMD_RECALL = 0x60;
    static constexpr uint8_t CMD_AUTO_STORE_EN = 0x59;
    static constexpr uint8_t CMD_AUTO_STORE_DIS = 0x19;
    static constexpr uint8_t CMD_WRSN = 0xC2;
    static constexpr uint8_t CMD_RDSN = 0xC3;
    static constexpr uint8_t CMD_RDID = 0x9F;

    static constexpr uint32_t NVRAM_SIZE = 0x1FFFF;
    static constexpr uint32_t NVRAM_ID = 0x06818820;
    static constexpr uint32_t CRC_DATA_SIZE = 7U;
    static constexpr uint32_t SN_DATA_OFFSET = 1U;
};

#ifdef __cplusplus
}
#endif

#endif // NVRAM_HPP
