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
 * @file    bmp581.hpp
 * @brief   BMP581 Pressure Sensor Driver Interface
 * @details C++ wrapper interface for Bosch BMP581 barometric pressure sensor.
 *          Provides simplified interface over the Bosch BSD-licensed driver
 *          library, with STM32 HAL integration for altitude measurement and
 *          environmental monitoring on drone systems.
 */

#ifndef BMP581_HPP
#define BMP581_HPP

#include "bmp5.h"
#include "bmp5_defs.h"

#ifdef __cplusplus
extern "C" {
#endif


class BMP581 {
public:
    BMP581(bmp5_read_fptr_t read_func,
        bmp5_write_fptr_t write_func,
        bmp5_delay_us_fptr_t delay_func,
        void* intf_ptr,
        enum bmp5_intf interface_type = BMP5_I2C_INTF);
    ~BMP581();

    bool initialize();
    void deinit();
    bool softReset();
    bool getSensorData(float& temperature, float& pressure);
    bool configureSensor(const bmp5_osr_odr_press_config& cfg);
    bool configureIIR(const bmp5_iir_config& cfg);
    bool setPowerMode(enum bmp5_powermode mode);
    bool getPowerMode(enum bmp5_powermode& mode);
    bool getInterruptStatus(uint8_t& status);
    //bool enableInterrupts(const bmp5_int_source_select& cfg);
    bool setupInterruptPin(enum bmp5_intr_mode mode, enum bmp5_intr_polarity pol,
                           enum bmp5_intr_drive drive, enum bmp5_intr_en_dis enable);

    bmp5_dev* dev();  // expose for debug or raw access

private:
    bmp5_dev dev_;
};

#ifdef __cplusplus
}
#endif

#endif // BMP581_HPP
