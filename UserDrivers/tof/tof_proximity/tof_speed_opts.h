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
#define TOF_OPT_TASK_PRIORITY   1  // Raise ToF data/detection task priorities
#define TOF_OPT_DIRECT_NOTIFY   1  // ISR wakes data task via direct task notification (skips timer daemon)
#define TOF_OPT_LEAN_PAYLOAD    1  // Strip unused ULD outputs to shrink the per-frame I2C read
#define TOF_OPT_I2C_FMPLUS      1  // I2C1 @ 1MHz Fast Mode Plus (0 = revert to 400kHz)
#define TOF_OPT_ASYNC_MODE      1  // RS_MODE_ASYNC_CONTINUOUS (no data-ready busy-poll per read)
#define TOF_OPT_MM_RESOLUTION   1  // Keep distances in mm end-to-end internally (g_status stays cm)
#define TOF_OPT_STATUS_FILTER   1  // Zero out zones whose target status is invalid

#endif // TOF_SPEED_OPTS_H
