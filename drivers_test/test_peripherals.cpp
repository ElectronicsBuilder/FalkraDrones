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
 * @file    test_peripherals.cpp
 * @brief   Comprehensive Peripheral Test Suite for FalkraController
 * @details Central test orchestrator that coordinates testing of all hardware
 *          peripherals and sensors on the STM32F767-based drone controller.
 *          Includes tests for sensors, flash memory, displays, audio, and
 *          communication interfaces to validate system functionality.
 */

#include "uart.hpp"
#include "log.hpp"
#include "cmsis_os.h"
#include "task.h"
#include "main.h"
#include "test_peripherals.hpp"
#include "test_nvram.hpp"
#include "test_uart.hpp"
#include "test_spi_flash.hpp"
#include "test_qspi_flash.hpp"
#include "test_SHT4x.hpp"
#include "test_bmp581.hpp"
#include "test_i2c.hpp"
#include "test_spi.hpp"
#include "test_bno085.hpp"
#include "test_bq27441.hpp"
#include "test_power_mux.hpp"
#include "test_radio_receiverHW.hpp"
#include "test_st7789.hpp"
#include "test_vl53l5cx.hpp"
#include "test_max98357.hpp"
#include "test_FileSystem.hpp"
#include "test_RTC.hpp"
#include "test_ppm.hpp"
//#include "test_wifi.hpp"
#include "main_app.h"

bool PeripheralsTestComplete = false;
extern TaskHandle_t test_peripheralsTask_TaskHandle;

extern bool log_initialized;

static TestConfig testConfig = {
    .test_uart = false,
    .test_nvram = false,
    .test_spi_flash = false,
    .test_qspi_flash = false,
    .test_SHT4x = false,
    .test_BMP581 = false,
    .test_i2c2 = false,
    .test_spi4 = false,
    .test_BNO085 = false,
    .test_BQ27441 = false,
    .test_power_mux = false,
    .test_radio_receiverHW = false,
    .test_continous_data = false,
    .test_st7789 = false,
    .test_vl53l5cx = false,
    .test_max98357 = false,
    .test_fileSystem = false,   // FFS regression suite disabled after SPI1 lock verification
    .test_rtc = false,
    .test_ppm = false,
    .test_wifi = false
};


void set_test_config(const TestConfig* cfg)
{
    if (cfg) {
        testConfig = *cfg;
    }
}

void test_peripheralsTask(void *argument)
{
    (void)argument; // Mark argument as unused

    do
    {
        osDelay(100);
    }while(log_initialized != true);

    HAL_GPIO_WritePin(LED_ACTY_GPIO_Port, LED_ACTY_Pin, GPIO_PIN_RESET);  // ACTIVE
    LOG_INFO("[DRIVER TEST] Peripherals Test Started");

    if (testConfig.test_uart) test_uart_W();
    if (testConfig.test_nvram) test_nvram_class_driver();
    if (testConfig.test_spi_flash) test_spi_flash_rw();
    if (testConfig.test_qspi_flash) qspi_flash_self_test();
    if (testConfig.test_i2c2) test_i2c2_scan();
    if (testConfig.test_spi4) test_spi4_all();
    if (testConfig.test_SHT4x) test_sht4x();
    if (testConfig.test_BMP581) test_bmp581();
    if (testConfig.test_BNO085) test_bno085();
    if (testConfig.test_BQ27441) test_bq27441(2200);
    if (testConfig.test_power_mux) test_power_mux(VOUT_3V3);
    if (testConfig.test_radio_receiverHW) test_radio_receiverHW();
    if (testConfig.test_ppm) test_ppm(5000); // 5 second timeout
    if (testConfig.test_st7789) test_st7789_sequence();
    if (testConfig.test_vl53l5cx) test_vl53l5cx();
    if (testConfig.test_max98357) test_operational_tones(); //test_max98357(); //test_max98357_wav_files();
    if (testConfig.test_fileSystem) test_filesystem(); //test_format_both_disks (); 
    if (testConfig.test_rtc) test_rtc();
    if(testConfig.test_wifi) wifi_test(); //test_wifi_ap(); 

    LOG_INFO("[DRIVER TEST] Peripherals Test Ended");
    LOG_SYSSTATUS ("[DRIVER TEST] Peripherals Test Ended"); 
    HAL_GPIO_WritePin(LED_ACTY_GPIO_Port, LED_ACTY_Pin, GPIO_PIN_SET);  // Done
    PeripheralsTestComplete = true;
   // play_startup_tone();
    osThreadExit();

}
