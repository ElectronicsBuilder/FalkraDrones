/**
 * @file    test_spi.c
 * @brief   Simple SPI4 Test Implementation
 * @details Basic SPI4 functionality test for debugging Wi-Fi communication issues
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

#include "test_spi.hpp"
#include "main.h"
#include "log.hpp"
#include <stdio.h>
#include <string.h>

/* External SPI handle */
extern SPI_HandleTypeDef hspi4;

SpiTestResult_t test_spi4_counter(uint32_t count)
{
    LOG_INFO("=== SPI4 Counter Test ===");
    LOG_INFO("Sending %lu bytes with incrementing counter...", count);
    
    if (count == 0 || count > 255) {
        count = 10;  // Default to 10 bytes
    }
    
    uint8_t tx_data[256];
    uint8_t rx_data[256];
    
    // Prepare incrementing counter data
    for (uint32_t i = 0; i < count; i++) {
        tx_data[i] = (uint8_t)(i & 0xFF);
    }
    
    LOG_INFO("SPI4: Activating Wi-Fi CS (low)...");
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    
    printf("SPI4: Transmitting %lu bytes: ", count);
    for (uint32_t i = 0; i < count; i++) {
        printf("0x%02X ", tx_data[i]);
    }
    printf("\n");
    
    // Send data via SPI4
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi4, tx_data, rx_data, count, 1000);
    
    HAL_Delay(1);
    LOG_INFO("SPI4: Deactivating Wi-Fi CS (high)...");
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_SET);
    
    if (status == HAL_OK) {
        LOG_INFO("SPI4: Transfer completed successfully!");
        printf("SPI4: Received data: ");
        for (uint32_t i = 0; i < count; i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\n");
        return SPI_TEST_PASSED;
    } else {
        LOG_ERROR("SPI4: Transfer FAILED! HAL_Status = %d", status);
        return SPI_TEST_FAILED;
    }
}

SpiTestResult_t test_spi4_pattern(uint8_t pattern, uint32_t duration_ms)
{
    LOG_INFO("=== SPI4 Pattern Test ===");
    LOG_INFO("Sending pattern 0x%02X for %lu ms...", pattern, duration_ms);
    
    uint8_t tx_data[16];
    uint8_t rx_data[16];
    
    // Fill buffer with test pattern
    memset(tx_data, pattern, sizeof(tx_data));
    
    uint32_t start_time = HAL_GetTick();
    uint32_t iterations = 0;
    
    LOG_INFO("SPI4: Starting continuous pattern transmission...");
    
    while ((HAL_GetTick() - start_time) < duration_ms) {
        // Toggle CS for each transmission
        HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);
        HAL_Delay(1);
        
        HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi4, tx_data, rx_data, sizeof(tx_data), 100);
        
        HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_SET);
        HAL_Delay(10);  // 10ms between transmissions
        
        iterations++;
        
        if (status != HAL_OK) {
            LOG_ERROR("SPI4: Pattern test FAILED at iteration %lu! HAL_Status = %d", iterations, status);
            return SPI_TEST_FAILED;
        }
    }
    
    LOG_INFO("SPI4: Pattern test completed! Sent %lu iterations", iterations);
    return SPI_TEST_PASSED;
}

SpiTestResult_t test_spi4_with_cs(void)
{
    LOG_INFO("=== SPI4 CS Control Test ===");
    
    // Test CS pin control
    LOG_INFO("SPI4: Testing CS pin control...");
    LOG_INFO("SPI4: CS High (inactive)");
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_SET);
    HAL_Delay(500);
    
    LOG_INFO("SPI4: CS Low (active)");
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);
    HAL_Delay(500);
    
    // Send some data with proper CS timing
    uint8_t test_cmd[] = {0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34, 0x56, 0x78};
    uint8_t rx_buffer[sizeof(test_cmd)];
    
    LOG_INFO("SPI4: Sending test command with CS control...");
    
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi4, test_cmd, rx_buffer, sizeof(test_cmd), 1000);
    
    HAL_Delay(1);
    LOG_INFO("SPI4: CS High (inactive)");
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_SET);
    
    if (status == HAL_OK) {
        LOG_INFO("SPI4: CS control test PASSED!");
        return SPI_TEST_PASSED;
    } else {
        LOG_ERROR("SPI4: CS control test FAILED! HAL_Status = %d", status);
        return SPI_TEST_FAILED;
    }
}

SpiTestResult_t test_spi4_all(void)
{
    LOG_INFO("====== COMPREHENSIVE SPI4 TEST SUITE ======");
    
    SpiTestResult_t result1 = test_spi4_counter(8);
    HAL_Delay(1000);
    
    SpiTestResult_t result2 = test_spi4_with_cs();
    HAL_Delay(1000);
    
    SpiTestResult_t result3 = test_spi4_pattern(0x55, 2000);  // 2 second pattern test
    HAL_Delay(1000);
    
    if (result1 == SPI_TEST_PASSED && result2 == SPI_TEST_PASSED && result3 == SPI_TEST_PASSED) {
        LOG_INFO("ALL SPI4 TESTS PASSED!");
        LOG_INFO("SPI4 hardware and configuration appear to be working correctly.");
        return SPI_TEST_PASSED;
    } else {
        LOG_ERROR("SPI4 TESTS FAILED!");
        LOG_INFO("Counter test: %s", result1 == SPI_TEST_PASSED ? "PASS" : "FAIL");
        LOG_INFO("CS control test: %s", result2 == SPI_TEST_PASSED ? "PASS" : "FAIL");
        LOG_INFO("Pattern test: %s", result3 == SPI_TEST_PASSED ? "PASS" : "FAIL");
        return SPI_TEST_FAILED;
    }
}

/* C interface function */
SpiTestResult_t test_spi_basic(void)
{
    return test_spi4_all();
}