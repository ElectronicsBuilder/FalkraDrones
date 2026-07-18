
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
 * @file   st7789_defs.h
*/
#ifndef ST7789_DEFS_H
#define ST7789_DEFS_H

// Display size
#define LCD_HEIGHT 320
#define LCD_WIDTH  170

// Color mode
#define ST7789_COLOR_MODE_16bit 0x55

#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define GRAY        0x8430
#define BRED        0xF81F
#define GRED        0xFFE0
#define GBLUE       0x07FF
#define BROWN       0xBC40
#define BRRED       0xFC07
#define DARKBLUE    0x01CF
#define LIGHTBLUE   0x7D7C
#define GRAYBLUE    0x5458
#define LIGHTGREEN  0x841F
#define LGRAY       0xC618
#define LGRAYBLUE   0xA651
#define LBBLUE      0x2B12

// Rotation
#define ST7789_ROTATION 2

// MADCTL bits
#define ST7789_MADCTL_MY   0x80
#define ST7789_MADCTL_MX   0x40
#define ST7789_MADCTL_MV   0x20
#define ST7789_MADCTL_ML   0x10
#define ST7789_MADCTL_RGB  0x00

// Command set
#define ST7789_NOP               0x00
#define ST7789_SWRESET           0x01
#define ST7789_RDDID             0x04
#define ST7789_RDDST             0x09
#define ST7789_SLPIN             0x10
#define ST7789_SLPOUT            0x11
#define ST7789_PTLON             0x12
#define ST7789_NORON             0x13
#define ST7789_INVOFF            0x20
#define ST7789_INVON             0x21
#define ST7789_DISPOFF           0x28
#define ST7789_DISPON            0x29
#define ST7789_CASET             0x2A
#define ST7789_PASET             0x2B
#define ST7789_RAMWR             0x2C
#define ST7789_RAMRD             0x2E
#define ST7789_PTLAR             0x30
#define ST7789_MADCTL            0x36
#define ST7789_COLMOD            0x3A

// Additional configuration commands
#define ST7789_PORCH_CTRL_CMD     0xB2
#define ST7789_GATE_CTRL_CMD      0xB7
#define ST7789_VCOM_CMD           0xBB
#define ST7789_PWR_CTRL1_CMD      0xC0
#define ST7789_PWR_CTRL2_CMD      0xC2
#define ST7789_VRH_VDV_CTRL_CMD   0xC3
#define ST7789_VDV_SETTING_CMD    0xC4
#define ST7789_FR_CTRL_CMD        0xC6
#define ST7789_PWR_CTRL3_CMD      0xD0
#define ST7789_GAMMA_POS_CMD      0xE0
#define ST7789_GAMMA_NEG_CMD      0xE1

// Corresponding data values
#define ST7789_GATE_CTRL_DATA     0x35
#define ST7789_VCOM_DATA          0x1A
#define ST7789_PWR1_DATA          0x2C
#define ST7789_PWR2_DATA          0x01
#define ST7789_VRH_VDV_DATA       0x0B
#define ST7789_VDV_SETTING_DATA   0x20
#define ST7789_FR_CTRL_DATA       0x0F
#define ST7789_PWR3_DATA1         0xA4
#define ST7789_PWR3_DATA2         0xA1

#endif // ST7789_DEFS_H
