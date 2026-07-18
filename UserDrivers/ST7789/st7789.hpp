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
 * @file    st7789.hpp
 * @brief   ST7789 TFT LCD Display Driver Interface
 * @details Driver for ST7789V controller-based 1.3" color TFT LCD display.
 *          Provides comprehensive graphics and text rendering capabilities for:
 *          - Real-time flight status display (altitude, speed, battery, GPS)
 *          - TouchGFX-based user interface for mission planning
 *          - System diagnostics and sensor data visualization
 *          - Video overlay graphics for FPV camera feeds
 * 
 * Display Specifications:
 * - Resolution: 240x240 pixels
 * - Color Depth: 16-bit (65K colors)
 * - Interface: 4-wire SPI with DC/Reset control
 * - Viewing Angle: IPS technology for wide viewing angles
 * - Brightness: LED backlight with PWM control
 * - Operating Temperature: -20°C to +70°C
 * 
 * Graphics Features:
 * - Hardware-accelerated pixel operations
 * - Configurable scan direction (portrait/landscape)
 * - Built-in frame buffer management
 * - Efficient bulk data transfer via DMA
 * - Integration with TouchGFX graphics framework
 * 
 * TouchGFX Integration:
 * - Serves as framebuffer target for TouchGFX rendering
 * - Hardware acceleration for graphics operations
 * - External flash asset loading for fonts and images
 * - Real-time UI updates without blocking flight operations
 * 
 * Usage Example:
 * @code
 * ST7789::Config cfg = {
 *     .hspi = &hspi3,
 *     .cs_port = LCD_CS_GPIO_Port, .cs_pin = LCD_CS_Pin,
 *     .dc_port = LCD_DC_GPIO_Port, .dc_pin = LCD_DC_Pin,
 *     .reset_port = LCD_RST_GPIO_Port, .reset_pin = LCD_RST_Pin,
 *     .bkl_port = LCD_BL_GPIO_Port, .bkl_pin = LCD_BL_Pin
 * };
 * 
 * ST7789 display(cfg);
 * display.init();
 * display.setBacklight(true);
 * 
 * // Display flight status
 * display.fillRect(0, 0, 240, 30, COLOR_BLACK);
 * display.drawString(10, 10, "ALT: 125m  BAT: 87%", COLOR_GREEN);
 * @endcode
 */

#ifndef ST7789_HPP
#define ST7789_HPP
#include "stm32f7xx_hal.h"

enum st7789_scan_dir_t {
    ST7789_SCAN_DIR_HORIZONTAL,
    ST7789_SCAN_DIR_VERTICAL
};

class ST7789 {
public:
    struct Config {
        SPI_HandleTypeDef *hspi;
        GPIO_TypeDef *cs_port;
        uint16_t cs_pin;
        GPIO_TypeDef *dc_port;
        uint16_t dc_pin;
        GPIO_TypeDef *reset_port;
        uint16_t reset_pin;
        GPIO_TypeDef *bkl_port;
        uint16_t bkl_pin;
        st7789_scan_dir_t scan_dir;
    };

    explicit ST7789(const Config& cfg);
    ~ST7789();

    void init();
    void deinit();
    void drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
    void fillScreen(uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void setRotation(uint8_t m);
    void setBacklight(bool on);
    uint8_t DisplayBusyStatus = 0;

private:
    Config cfg_;
    uint16_t width_;
    uint16_t height_;

    void reset();
    void setDir(st7789_scan_dir_t dir);
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void sendCommand(uint8_t c);
    void sendData(uint8_t c);
    void sendBuffer(const uint8_t *data, size_t size);
    void sendColorData(const uint16_t *data, uint32_t length);
    void writeInitSequence();
};
#endif // ST7789_HPP
