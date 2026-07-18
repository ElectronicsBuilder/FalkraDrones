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
 * @file    wifi_bootloader.h
 * @brief   C Interface for Wi-Fi Module Bootloader Control
 * @details C-compatible interface for controlling ST67W611M1 bootloader mode
 *          from C code such as the command line interface. The ST67W611M1 WiFi
 *          module supports bootloader mode for firmware updates via UART.
 *
 * Hardware Interface:
 * - WIFI_EN (PH2): Chip enable signal (active high)
 * - WIFI_RESET (PH3): Hardware reset signal (active low)
 * - WIFI_BOOT (PF4): Bootloader mode control (alias for WIFI_WAKE)
 * - UART: Communication interface for bootloader commands
 *
 * Bootloader Entry Sequence:
 * 1. Assert WIFI_BOOT high (bootloader mode request)
 * 2. Perform hardware reset via WIFI_RESET
 * 3. Enable chip via WIFI_EN
 * 4. Module enters bootloader mode, ready for UART communication
 *
 * Normal Operation Sequence:
 * 1. Deassert WIFI_BOOT low (normal operation mode)
 * 2. Perform hardware reset via WIFI_RESET
 * 3. Enable chip via WIFI_EN
 * 4. Module boots into application firmware
 *
 * =============================================================================
 * FIRMWARE UPDATE PROCEDURE - ST67W611M1 WiFi Module (Mission Mode)
 * =============================================================================
 *
 * This procedure updates the ST67W611M1 WiFi module with mission mode firmware
 * using the STMicroelectronics QConn_Flash utility.
 *
 * Prerequisites:
 * --------------
 * 1. STM32CubeIDE installed with ST67W6X utilities
 * 2. USB-to-UART adapter connected to WiFi module UART pins
 * 3. QConn_Flash utility available in ST67W6X_Utilities/Binaries
 * 4. Mission mode firmware binary (st67w611m_mission_t01_v2.0.75.bin)
 * 5. eFuse data binary (efusedata.bin)
 *
 * Step-by-Step Update Procedure:
 * --------------------------------
 *
 * STEP 1: Enter WiFi Bootloader Mode
 *   Via CLI (recommended):
 *     - Connect to FalkraDrones serial console
 *     - Send command: "enterWifiBootloader" or equivalent CLI command
 *     - Wait for confirmation message
 *
 *   Programmatically (from application code):
 *     wifi_enter_bootloader_mode();
 *     // or with UART configuration
 *     wifi_enter_bootloader_mode_with_uart(&huart_wifi);
 *
 * STEP 2: Navigate to ST67W6X Utilities Directory
 *   Windows Command Prompt:
 *     cd C:\..\..\STM32CubeIDE\st67w611m1_base\xcube\Projects\ST67W6X_Utilities\Binaries
 *
 *   PowerShell:
 *     Set-Location "C:\..\..\STM32CubeIDE\st67w611m1_base\xcube\Projects\ST67W6X_Utilities\Binaries"
 *
 *   Linux/MacOS:
 *     cd ~/STM32CubeIDE/st67w611m1_base/xcube/Projects/ST67W6X_Utilities/Binaries
 *
 * STEP 3: Flash Mission Mode Firmware
 *   Command syntax:
 *     QConn_Flash\QConn_Flash_Cmd.exe --port <COMx> --config <firmware.bin> --efuse=<efusedata.bin>
 *
 *   Windows example (replace COM9 with your actual port):
 *     QConn_Flash\QConn_Flash_Cmd.exe --port COM9 --config NCP_Binaries\st67w611m_mission_t01_v2.0.75.bin --efuse=NCP_Binaries\efusedata.bin
 *
 *   Linux/MacOS example (replace /dev/ttyUSB0 with your actual port):
 *     ./QConn_Flash/QConn_Flash_Cmd --port /dev/ttyUSB0 --config NCP_Binaries/st67w611m_mission_t01_v2.0.75.bin --efuse=NCP_Binaries/efusedata.bin
 *
 *   Expected output:
 *   [05:29:26.821] - Load efuse 0
 *   [05:29:26.846] - Load efuse 1
 *   [05:29:26.878] - Load efuse remainder
 *   [05:29:26.910] - Finished
 *   [05:29:26.910] - All time cost(ms): 2630.314453125
 *   [05:29:27.115] - close interface
 *   [05:29:27.115] - [All Success]
 *
 *   Note: Flashing process takes approximately 30-60 seconds
 *
 * STEP 4: Exit Bootloader Mode
 *   Via CLI (recommended):
 *     - Send command: "exitWifiBootloader" or equivalent CLI command
 *     - Wait for WiFi module to restart in normal mode
 *
 *   Programmatically (from application code):
 *     wifi_exit_bootloader_mode();
 *
 * STEP 5: Verify Firmware Update
 *   - WiFi module should boot into mission mode
 *   - Check WiFi connectivity and functionality
 *   - Verify firmware version via WiFi AT commands or status query
 *
 * Troubleshooting:
 * -----------------
 * - "Port not found": Check COM port number (Windows Device Manager or Linux dmesg)
 * - "Bootloader not responding": Verify bootloader mode entry (check WIFI_BOOT pin)
 * - "Flash verification failed": Retry firmware update, check binary file integrity
 * - "eFuse programming error": Ensure efusedata.bin is correct for your hardware revision
 * - "Timeout error": Increase UART timeout or check physical connections
 *
 * Important Notes:
 * -----------------
 * - DO NOT power off device during firmware update (risk of bricking module)
 * - Ensure stable power supply (battery charged or USB connected)
 * - Keep backup of working firmware binaries
 * - eFuse data is ONE-TIME programmable - verify before flashing
 * - Use correct firmware variant for ST67W611M1 hardware revision
 *
 * Firmware Variants:
 * -------------------
 * - Mission Mode (NCP): st67w611m_mission_t01_vX.X.XX.bin - Network Co-Processor mode
 * - Standalone Mode: st67w611m_standalone_vX.X.XX.bin - Full TCP/IP stack on module
 *
 * Part of FalkraDrones - STM32F767-based drone controller firmware
 * 
 * https://wiki.st.com/stm32mcu/wiki/Connectivity:Wi-Fi_MCU_Hardware_Setup#Flashing_the_ST67W611M1_using_QConn_Flash_with_an_STM32_host
 */

#ifndef WIFI_BOOTLOADER_H
#define WIFI_BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "stm32f7xx_hal.h"

/**
 * @brief Enter Wi-Fi module bootloader mode
 * @details Puts the ST67W611M1 module into bootloader mode for firmware flashing.
 *          Follows the sequence: WAKE high -> Reset -> Enable
 * @return true if successful, false otherwise
 */
bool wifi_enter_bootloader_mode(void);

/**
 * @brief Enter Wi-Fi module bootloader mode with UART configuration
 * @details Puts the ST67W611M1 module into bootloader mode and configures UART
 *          for direct communication at 2Mbps baud rate
 * @param uart_handle Pointer to UART handle to configure for Wi-Fi communication
 * @return true if successful, false otherwise
 */
bool wifi_enter_bootloader_mode_with_uart(UART_HandleTypeDef* uart_handle);

/**
 * @brief Exit Wi-Fi module bootloader mode
 * @details Resets the Wi-Fi module and returns it to normal operation mode
 * @return true if successful, false otherwise
 */
bool wifi_exit_bootloader_mode(void);

/**
 * @brief Enable Wi-Fi module (normal operation mode)
 * @details Sets chip enable (WIFI_EN) high and deasserts BOOT for normal operation
 * @return true if successful, false otherwise
 */
bool wifi_enable(void);

/**
 * @brief Disable Wi-Fi module (power down)
 * @details Powers down the WiFi module by deasserting chip enable
 * @return true if successful, false otherwise
 */
bool wifi_disable(void);

/**
 * @brief Perform hardware reset of Wi-Fi module
 * @details Toggles the WIFI_RESET pin to perform a hardware reset
 * @return true if successful, false otherwise
 */
bool wifi_hardware_reset(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_BOOTLOADER_H
