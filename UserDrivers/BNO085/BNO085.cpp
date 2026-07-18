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
 * @file    BNO085.cpp
 * @brief   Implementation of BNO085 9-DOF IMU Sensor Driver
 * @details Provides hardware interface implementation for BNO085 IMU using
 *          SPI communication, with support for hardware reset, interrupt
 *          handling, and sensor event processing. Implements the SH2 HAL
 *          interface for integration with Hillcrest Labs sensor hub.
 */

#include "BNO085.hpp"
#include "stm32f7xx_hal.h"
#include <cstring>

#include "log.hpp"
static BNO085* g_bno_instance = nullptr;

// Global sensor value pointer accessed from interrupt context (sensorHandler)
static sh2_SensorValue_t *_sensor_value = NULL;
static bool _reset_occurred = false;

BNO085::BNO085() : _spi(nullptr), _resetOccurred(false) {}

BNO085::~BNO085() {
    deinit();
}

void BNO085::deinit() {
    // Close the sh2 interface
    sh2_close();

    // Reset state variables
    _spi = nullptr;
    _resetOccurred = false;
    memset(&prodIds, 0, sizeof(prodIds));
    memset(&_sensorValue, 0, sizeof(_sensorValue));

    // Clear global instance reference
    if (g_bno_instance == this) {
        g_bno_instance = nullptr;
    }

    LOG_SYSSTATUS("[BNO085] BNO085 cleanup complete");
}
 
bool BNO085::begin_SPI(SPI_HandleTypeDef* spiHandle,
                       GPIO_TypeDef* csPort, uint16_t csPin,
                       GPIO_TypeDef* intPort, uint16_t intPin,
                       GPIO_TypeDef* resetPort, uint16_t resetPin) {
    _spi = spiHandle;
    _csPort = csPort;
    _csPin = csPin;
    _intPort = intPort;
    _intPin = intPin;
    _resetPort = resetPort;
    _resetPin = resetPin;

    g_bno_instance = this;

    _HAL.open = BNO085::spihalOpenStatic;
    _HAL.close = BNO085::spihalCloseStatic;
    _HAL.read = BNO085::spihalReadStatic;
    _HAL.write = BNO085::spihalWriteStatic;
    _HAL.getTimeUs = hal_getTimeUs;

    return _init();
}

bool BNO085::_init() {
    hardwareReset();

    if (sh2_open(&_HAL, hal_callback, nullptr) != SH2_OK) {
        return false;
    }


    memset(&prodIds, 0, sizeof(prodIds));
    if (sh2_getProdIds(&prodIds) != SH2_OK) {
        return false;
    }

    sh2_setSensorCallback(sensorHandler, nullptr);

    return true;
}

void BNO085::hardwareReset() {
    if (_resetPort != nullptr) {
        HAL_GPIO_WritePin(_resetPort, _resetPin, GPIO_PIN_SET);
        HAL_Delay(10);
        HAL_GPIO_WritePin(_resetPort, _resetPin, GPIO_PIN_RESET);
        HAL_Delay(10);
        HAL_GPIO_WritePin(_resetPort, _resetPin, GPIO_PIN_SET);
        HAL_Delay(10);
    }
}

void BNO085::debugSpiDump(const char* label, const uint8_t* data, size_t len) {
    log_info("%s (%d bytes):", label, len);
    for (size_t i = 0; i < len; ++i) {
        log_info(" %02X", data[i]);
    }
}


bool BNO085::wasReset() {
    bool flag = _resetOccurred;
    _resetOccurred = false;
    return flag;
}

bool BNO085::enableReport(sh2_SensorId_t sensorId, uint32_t interval_us) {
    sh2_SensorConfig_t config = {};
    config.reportInterval_us = interval_us;
    config.changeSensitivityEnabled = false;
    config.wakeupEnabled = false;
    config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false;
    config.changeSensitivity = 0;
    config.batchInterval_us = 0;
    config.sensorSpecific = 0;


    int status = sh2_setSensorConfig(sensorId, &config);

    if (status != SH2_OK) {
      return false;
    }
  
    return true;
}

bool BNO085::getSensorEvent(sh2_SensorValue_t* value) {
    _sensorValue.timestamp = 0;
    _sensor_value = &_sensorValue;

    sh2_service();

    if (_sensorValue.timestamp == 0 && _sensorValue.sensorId != SH2_GYRO_INTEGRATED_RV) {
        return false;
    }

    *value = _sensorValue;         // **copy updated result back to caller**
    return true;
}



int BNO085::spihalOpen(sh2_Hal_t* self) {
    waitForInt();
    return 0;
}

void BNO085::spihalClose(sh2_Hal_t* self) {
    // No specific action needed
}

bool BNO085::waitForInt() {
    for (int i = 0; i < 500; i++) {
        if (HAL_GPIO_ReadPin(_intPort, _intPin) == GPIO_PIN_RESET)
            return true;
        HAL_Delay(1);
    }
    hardwareReset();
    return false;
}

int BNO085::spihalRead(sh2_Hal_t* self, uint8_t* buf, unsigned len, uint32_t* t_us) {
    uint16_t packet_size = 0;
    uint8_t header[4];

    if (!waitForInt()) return 0;

    // Read 4-byte header
    uint8_t tx_dummy[4] = { 0 };
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive(_spi, tx_dummy, header, 4, HAL_MAX_DELAY) != HAL_OK) {
        HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
        return 0;
    }
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);

    packet_size = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    packet_size &= ~0x8000;

    if (packet_size > len) {
        log_warn("[BNO085] Packet size too big: %u (max %u)", packet_size, len);
        return 0;
    }

    if (!waitForInt()) return 0;

    // Read payload
    uint8_t tx_dummy_payload[384] = { 0 };
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive(_spi, tx_dummy_payload, buf, packet_size, HAL_MAX_DELAY) != HAL_OK) {
        HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
        log_warn("[BNO085] SPI read payload failed");
        return 0;
    }
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);

    return packet_size;
}


int BNO085::spihalWrite(sh2_Hal_t* self, uint8_t* buf, unsigned len) {

    if (!waitForInt()) return 0;

    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(_spi, buf, len, HAL_MAX_DELAY) != HAL_OK) {
        HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
        log_warn("[BNO085] SPI write failed");
        return 0;
    }
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);

    return len;
}


int BNO085::spihalOpenStatic(sh2_Hal_t* self) {
    return g_bno_instance->spihalOpen(self);
}

void BNO085::spihalCloseStatic(sh2_Hal_t* self) {
    g_bno_instance->spihalClose(self);
}

int BNO085::spihalReadStatic(sh2_Hal_t* self, uint8_t* buf, unsigned len, uint32_t* t_us) {
    return g_bno_instance->spihalRead(self, buf, len, t_us);
}

int BNO085::spihalWriteStatic(sh2_Hal_t* self, uint8_t* buf, unsigned len) {
    return g_bno_instance->spihalWrite(self, buf, len);
}

extern "C" uint32_t hal_getTimeUs(sh2_Hal_t* self) {
    return HAL_GetTick() * 1000;
}

extern "C" void hal_callback(void* cookie, sh2_AsyncEvent_t* pEvent) {
    if (pEvent->eventId == SH2_RESET) {
        g_bno_instance->_resetOccurred = true;
    }
}

extern "C" void sensorHandler(void* cookie, sh2_SensorEvent_t* event) {
    int rc;

    rc = sh2_decodeSensorEvent(_sensor_value, event);
    if (rc != SH2_OK) {
    log_warn("[BNO085]  Error decoding sensor event");
      _sensor_value->timestamp = 0;
      return;
    }
}
