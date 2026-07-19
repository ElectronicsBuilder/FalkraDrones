/**
 * @file    tof_speed_opts.h
 * @brief   Compile-time switches for the ToF proximity speed optimizations.
 * @details Each optimization can be disabled independently (set to 0) to fall
 *          back to the original behavior during testing. All flags 0 = legacy
 *          polling-era behavior.
 */

#ifndef TOF_SPEED_OPTS_H
#define TOF_SPEED_OPTS_H

#define TOF_PERF_MONITOR        0  // Per-sensor latency/rate instrumentation ([TOF_PERF] logs)
#define TOF_PERF_FRAME_MARKER   0  // Log one [TOF_FRAME] line per converted frame
#define TOF_PAYLOAD_PROBE       0  // One-shot payload/I2C timing log after ToF ranging starts
#define TOF_DETECTION_LOG       0  // Log detection state changes ([TOF_DET])
#define TOF_OPT_I2C_DMA_MODE    1  // DMA I2C1 transfers (falls back to IT mode at 0)
#define TOF_OPT_I2C_IT_MODE     1  // IRQ-driven I2C1 transfers (0 = blocking HAL reads)
#define TOF_OPT_I2C_ASYNC_MODE  (TOF_OPT_I2C_DMA_MODE || TOF_OPT_I2C_IT_MODE)

#if TOF_OPT_I2C_DMA_MODE
#define TOF_RANGING_FREQUENCY_HZ 60U  // Target six-sensor DMA rate
#define TOF_STALE_THRESHOLD_MS  250U  // Restore tighter health threshold for DMA
#elif TOF_OPT_I2C_IT_MODE
#define TOF_RANGING_FREQUENCY_HZ 45U  // Sustainable per-sensor rate with sleeping I2C transfers
#define TOF_STALE_THRESHOLD_MS  1000U // Health threshold for six-sensor IT operation
#else
#define TOF_RANGING_FREQUENCY_HZ 20U  // Safe blocking-read six-sensor rate
#define TOF_STALE_THRESHOLD_MS  3000U // Health threshold for reduced-rate operation
#endif

#define TOF_OPT_TASK_PRIORITY   1  // Raise ToF data task priority, but keep it cooperative
#define TOF_OPT_DIRECT_NOTIFY   1  // ISR wakes data task via direct task notification (skips timer daemon)
#define TOF_OPT_LEAN_PAYLOAD    1  // Strip unused ULD outputs to shrink the per-frame I2C read
#define TOF_OPT_I2C_FMPLUS      1  // I2C1 @ 1MHz Fast Mode Plus (0 = revert to 400kHz)
#define TOF_OPT_ASYNC_MODE      1  // RS_MODE_ASYNC_CONTINUOUS (no data-ready busy-poll per read)
#define TOF_OPT_MM_RESOLUTION   1  // Keep distances in mm end-to-end internally (g_status stays cm)
#define TOF_OPT_STATUS_FILTER   1  // Zero out zones whose target status is invalid

#endif // TOF_SPEED_OPTS_H
