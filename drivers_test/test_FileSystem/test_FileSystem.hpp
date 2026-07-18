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
 * @file    test_FileSystem.hpp
 * @brief   File System Validation Test Suite
 */

#ifndef __TEST_FILESYSTEM_HPP
#define __TEST_FILESYSTEM_HPP

#ifdef __cplusplus
extern "C" {
#endif

void test_filesystem();

#define TEST_READ_WRITE_SUITE         1U
#define TEST_BINARY_SUITE             1U
#define TEST_PERSISTENT_LOG           0U
#define TEST_PERSISTENT_BINARY        0U

/* FFS fix-plan regression tests (see .claude/ffs_review.md).
   TEST_STRADDLE: single write larger than tail-block free space (bug A3) —
     expected to FAIL until Stage 3 is applied.
   TEST_PERSISTENCE: two-phase across a power cycle; erase-count half is
     expected to FAIL until Stage 1 is applied (48KB malloc bug A1). */
#define TEST_STRADDLE                 1U
#define TEST_PERSISTENCE              1U

#ifdef __cplusplus
}
#endif

#endif // __TEST_FILESYSTEM_HPP