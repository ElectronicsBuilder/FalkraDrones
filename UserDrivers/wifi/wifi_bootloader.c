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
 * @file    wifi_bootloader.c
 * @brief   Implementation of Wi-Fi Module Bootloader Control
 * @details Provides hardware control functions for entering/exiting bootloader mode
 *          on the ST67W611M1 WiFi module. The bootloader mode is used for firmware
 *          updates via UART at 2Mbps.
 *
 * Hardware Connections:
 * - WIFI_EN (PH2): Chip enable - active high
 * - WIFI_RESET (PH3): Hardware reset - active low
 * - WIFI_BOOT (PF4): Bootloader mode control (alias for WIFI_WAKE)
 *
 * Timing Requirements:
 * - Reset pulse width: Minimum 10ms
 * - Power stabilization: 50ms after enable
 * - Bootloader ready: 100ms after reset in bootloader mode
 *
 * Part of FalkraDrones - STM32F767-based drone controller firmware
 */

 

#include "wifi_bootloader.h"
#include "main.h"  // For GPIO pin definitions

// Timing constants (milliseconds)
#define WIFI_RESET_PULSE_MS         10
#define WIFI_POWER_STABLE_MS        50
#define WIFI_BOOTLOADER_READY_MS    100

// UART bootloader configuration
#define WIFI_BOOTLOADER_BAUD        2000000  // 2Mbps for bootloader

/**
 * @brief Delay wrapper using HAL
 * @param ms Milliseconds to delay
 */
static inline void wifi_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief Perform hardware reset of Wi-Fi module
 * @details Toggles the WIFI_RESET pin to perform a hardware reset
 * @return true if successful, false otherwise
 */
bool wifi_hardware_reset(void)
{
    // Assert reset (active low)
    HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, GPIO_PIN_RESET);
    wifi_delay_ms(WIFI_RESET_PULSE_MS);

    // Deassert reset
    HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, GPIO_PIN_SET);
    wifi_delay_ms(WIFI_POWER_STABLE_MS);

    return true;
}

/**
 * @brief Enable Wi-Fi module (normal operation mode)
 * @details Sets chip enable (WIFI_EN) high and deasserts BOOT for normal operation
 * @return true if successful, false otherwise
 */
bool wifi_enable(void)
{
    // Ensure BOOT is low for normal operation
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);

    // Enable chip
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_SET);

    // Wait for power stabilization
    wifi_delay_ms(WIFI_POWER_STABLE_MS);

    return true;
}

/**
 * @brief Disable Wi-Fi module (power down)
 * @details Powers down the WiFi module by deasserting chip enable
 * @return true if successful, false otherwise
 */
bool wifi_disable(void)
{
    // Disable chip
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_RESET);

    // Also deassert BOOT
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);

    return true;
}

/**
 * @brief Enter Wi-Fi module bootloader mode
 * @details Puts the ST67W611M1 module into bootloader mode for firmware flashing.
 *          Follows the sequence: BOOT high -> Reset -> Enable
 *
 * Bootloader Entry Procedure:
 * 1. Disable WiFi module
 * 2. Assert BOOT pin high (bootloader mode request)
 * 3. Perform hardware reset
 * 4. Enable chip
 * 5. Wait for bootloader to be ready
 *
 * @return true if successful, false otherwise
 */
bool wifi_enter_bootloader_mode(void)
{
    // Step 1: Ensure WiFi is disabled first
    wifi_disable();
    wifi_delay_ms(10);

    // Step 2: Assert BOOT pin high to request bootloader mode
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_SET);
    wifi_delay_ms(10);

    // Step 3: Enable chip enable
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_SET);
    wifi_delay_ms(WIFI_POWER_STABLE_MS);

    // Step 4: Perform hardware reset
    wifi_hardware_reset();

    // Step 5: Wait for bootloader to be ready
    wifi_delay_ms(WIFI_BOOTLOADER_READY_MS);

    return true;
}

/**
 * @brief Enter Wi-Fi module bootloader mode with UART configuration
 * @details Puts the ST67W611M1 module into bootloader mode and configures UART
 *          for direct communication at 2Mbps baud rate
 *
 * @param uart_handle Pointer to UART handle to configure for Wi-Fi communication
 * @return true if successful, false otherwise
 */
bool wifi_enter_bootloader_mode_with_uart(UART_HandleTypeDef* uart_handle)
{
    if (uart_handle == NULL) {
        return false;
    }

    // Enter bootloader mode first
    if (!wifi_enter_bootloader_mode()) {
        return false;
    }

    // Configure UART for bootloader communication (2Mbps)
    // Save original baud rate
    uint32_t original_baud = uart_handle->Init.BaudRate;

    // Deinitialize UART
    HAL_UART_DeInit(uart_handle);

    // Reconfigure for bootloader baud rate
    uart_handle->Init.BaudRate = WIFI_BOOTLOADER_BAUD;
    uart_handle->Init.WordLength = UART_WORDLENGTH_8B;
    uart_handle->Init.StopBits = UART_STOPBITS_1;
    uart_handle->Init.Parity = UART_PARITY_NONE;
    uart_handle->Init.Mode = UART_MODE_TX_RX;
    uart_handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart_handle->Init.OverSampling = UART_OVERSAMPLING_16;

    // Reinitialize UART
    if (HAL_UART_Init(uart_handle) != HAL_OK) {
        // Restore original baud rate on failure
        uart_handle->Init.BaudRate = original_baud;
        HAL_UART_Init(uart_handle);
        return false;
    }

    return true;
}

/**
 * @brief Exit Wi-Fi module bootloader mode
 * @details Resets the Wi-Fi module and returns it to normal operation mode
 *
 * Normal Operation Procedure:
 * 1. Disable WiFi module
 * 2. Deassert BOOT pin (normal operation mode)
 * 3. Enable chip
 * 4. Perform hardware reset
 * 5. Module boots into application firmware
 *
 * @return true if successful, false otherwise
 */
bool wifi_exit_bootloader_mode(void)
{
    // Step 1: Disable WiFi module
    wifi_disable();
    wifi_delay_ms(10);

    // Step 2: Deassert BOOT pin for normal operation
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);
    wifi_delay_ms(10);

    // Step 3: Enable chip
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_SET);
    wifi_delay_ms(WIFI_POWER_STABLE_MS);

    // Step 4: Perform hardware reset
    wifi_hardware_reset();

    // Step 5: Wait for normal application to start
    wifi_delay_ms(WIFI_POWER_STABLE_MS);

    return true;
}
