/**
 * @file    st7789_opts.h
 * @brief   Compile-time switches for ST7789 driver optimizations.
 * @details Each option can be disabled independently (set to 0) to fall back
 *          to the original behavior during testing.
 */

#ifndef ST7789_OPTS_H
#define ST7789_OPTS_H

// Send pixel data as chunked bulk SPI transfers instead of one 2-byte HAL
// transaction per pixel (~500x fewer HAL calls per frame flush). Also makes
// the per-call timeout budget robust against preemption by higher-priority
// tasks, which caused phantom "buffer tx failed ... err=0x00000020" errors.
#define ST7789_OPT_BATCH_TX        1

// Pixels per chunk; the static byte-swap buffer is 2x this size.
// 512 px = 1KB buffer, ~113 chunks per full 240x240 frame.
#define ST7789_BATCH_CHUNK_PIXELS  512

#endif // ST7789_OPTS_H
