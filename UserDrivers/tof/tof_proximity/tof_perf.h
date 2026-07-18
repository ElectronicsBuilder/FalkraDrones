#ifndef TOF_PERF_H
#define TOF_PERF_H

#include "tof_speed_opts.h"

#if TOF_PERF_MONITOR

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*tof_perf_read_fn_t)(uint8_t sensor_index);

void tof_perf_mark_irq(uint8_t sensor_index);
uint8_t tof_perf_read_sensor(uint8_t sensor_index, tof_perf_read_fn_t read_fn);
void tof_perf_mark_snapshot_and_report(uint32_t sensor_flags, uint32_t present_mask);

#ifdef __cplusplus
}
#endif

#define TOF_PERF_MARK_IRQ(sensor_index) \
    tof_perf_mark_irq((sensor_index))

#define TOF_PERF_READ_SENSOR(sensor_index, read_fn) \
    tof_perf_read_sensor((sensor_index), (read_fn))

#define TOF_PERF_SNAPSHOT_AND_REPORT(sensor_flags, present_mask) \
    tof_perf_mark_snapshot_and_report((sensor_flags), (present_mask))

#else

#define TOF_PERF_MARK_IRQ(sensor_index) \
    ((void)0)

#define TOF_PERF_READ_SENSOR(sensor_index, read_fn) \
    (read_fn)(sensor_index)

#define TOF_PERF_SNAPSHOT_AND_REPORT(sensor_flags, present_mask) \
    ((void)0)

#endif // TOF_PERF_MONITOR

#endif // TOF_PERF_H
