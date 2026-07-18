/**
 * @file    test_spi.h
 * @brief   Simple SPI4 Test Header
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

#ifndef TEST_SPI_H
#define TEST_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Test result types */
typedef enum {
    SPI_TEST_PASSED = 0,
    SPI_TEST_FAILED = 1,
    SPI_TEST_ERROR = 2
} SpiTestResult_t;

/* Test functions */

/**
 * @brief Run basic SPI4 counter test
 * @details Sends incrementing counter values on SPI4 to verify clock/data signals
 * @param count Number of bytes to send (default: 10)
 * @return SpiTestResult_t
 */
SpiTestResult_t test_spi4_counter(uint32_t count);

/**
 * @brief Run continuous SPI4 test with pattern
 * @details Sends repeating test pattern for oscilloscope analysis
 * @param pattern Test pattern to send (e.g., 0xAA, 0x55)
 * @param duration_ms How long to run test in milliseconds
 * @return SpiTestResult_t
 */
SpiTestResult_t test_spi4_pattern(uint8_t pattern, uint32_t duration_ms);

/**
 * @brief Test SPI4 with Wi-Fi CS control
 * @details Tests SPI with proper Wi-Fi chip select timing
 * @return SpiTestResult_t
 */
SpiTestResult_t test_spi4_with_cs(void);

/**
 * @brief Run all basic SPI4 tests
 * @return SpiTestResult_t
 */
SpiTestResult_t test_spi4_all(void);

/* C interface for integration with test framework */
extern SpiTestResult_t test_spi_basic(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SPI_H */