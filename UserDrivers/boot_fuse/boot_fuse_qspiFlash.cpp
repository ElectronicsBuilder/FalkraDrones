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
 * @file    boot_fuse_qspiFlash.cpp
 * @brief   Boot fuse QSPI flash implementation
 */
#include "boot_fuse_qspiFlash.hpp"
#include "app_defs.hpp"
#include "boot_fuse.hpp"
#include "qspi_flash.hpp"
#include <cstring>


extern QspiFlash flash;

static const uint8_t boot_fuse_expected[BOOT_FUSE_SIZE] = {
    BOOT_FUSE_SET_BYTE1,
    BOOT_FUSE_SET_BYTE2,
    BOOT_FUSE_SET_BYTE3
};

static const uint8_t boot_fuse_cleared[BOOT_FUSE_SIZE] = {
    BOOT_FUSE_CLEAR_BYTE1,
    BOOT_FUSE_CLEAR_BYTE2,
    BOOT_FUSE_CLEAR_BYTE3
};

static void qspi_init()
{
    flash.init(); 
}


 bool qspiFlash_set_fuse()
{
    flash.disableMemoryMappedMode();

    boot_fuse_metadata_t meta = {
        .magic = BOOT_FUSE_MAGIC,
        .version = BOOT_FUSE_VERSION,
        .len = BOOT_FUSE_DATA_LEN,
    };
    memcpy(meta.fuse_data, boot_fuse_expected, BOOT_FUSE_SIZE);
    meta.crc8 = app_crc8(reinterpret_cast<uint8_t*>(&meta), offsetof(boot_fuse_metadata_t, crc8));

            flash.eraseSector(QSPI_FUSE_ADDR);
            flash.writeDataQuad(QSPI_FUSE_ADDR, reinterpret_cast<uint8_t*>(&meta), sizeof(meta));

            return true;

}

