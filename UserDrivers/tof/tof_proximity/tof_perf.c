#include "tof_perf.h"

#if TOF_PERF_MONITOR

#include "log.hpp"
#include "stm32f7xx_hal.h"

#define TOF_PERF_SENSOR_COUNT 6U
#define TOF_PERF_REPORT_PERIOD_MS 1000U
#define TOF_PERF_FALLBACK_CORE_HZ 216000000UL
#define TOF_PERF_DWT_UNLOCK_KEY 0xC5ACCE55UL

typedef struct {
    volatile uint32_t irq_ts_ticks;
    volatile uint32_t irq_count;
    volatile uint32_t read_success_count;
    volatile uint32_t read_attempt_count;
    volatile uint32_t snapshot_count;

    volatile uint32_t irq2read_sum_us;
    volatile uint32_t irq2read_samples;
    volatile uint32_t read_sum_us;
    volatile uint32_t read_samples;
    volatile uint32_t irq2snap_sum_us;
    volatile uint32_t irq2snap_samples;

    uint32_t last_irq_count;
    uint32_t last_read_success_count;
} tof_perf_sensor_t;

static tof_perf_sensor_t s_perf[TOF_PERF_SENSOR_COUNT];
static uint8_t s_dwt_ready = 0;
static uint8_t s_dwt_usable = 0;
static uint8_t s_dwt_warning_reported = 0;
static uint32_t s_last_report_ms = 0;
static uint32_t s_seen_mask = 0;

extern uint32_t SystemCoreClock;

static void tof_perf_report_if_due(uint32_t present_mask);

static void tof_perf_init_once(void)
{
    if (s_dwt_ready) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT)
    DWT->LAR = TOF_PERF_DWT_UNLOCK_KEY;
#endif
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t first = DWT->CYCCNT;
    for (volatile uint32_t i = 0; i < 128U; i++) {
        __NOP();
    }
    uint32_t second = DWT->CYCCNT;

    s_dwt_usable = ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) && (second != first);
    s_last_report_ms = HAL_GetTick();
    s_dwt_ready = 1;
}

static uint32_t tof_perf_cycles_to_us(uint32_t cycles)
{
    uint32_t hz = SystemCoreClock;
    if (hz == 0U) {
        hz = TOF_PERF_FALLBACK_CORE_HZ;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / hz);
}

static uint32_t tof_perf_now_ticks(void)
{
    tof_perf_init_once();

    if (s_dwt_usable) {
        return DWT->CYCCNT;
    }

    return HAL_GetTick() * 1000U;
}

static uint32_t tof_perf_elapsed_us(uint32_t start_ticks, uint32_t end_ticks)
{
    uint32_t elapsed_ticks = end_ticks - start_ticks;
    if (s_dwt_usable) {
        return tof_perf_cycles_to_us(elapsed_ticks);
    }

    return elapsed_ticks;
}

static uint32_t tof_perf_mean_and_reset(volatile uint32_t *sum, volatile uint32_t *samples)
{
    uint32_t local_sum = *sum;
    uint32_t local_samples = *samples;

    *sum = 0;
    *samples = 0;

    if (local_samples == 0U) {
        return 0;
    }

    return local_sum / local_samples;
}

void tof_perf_mark_irq(uint8_t sensor_index)
{
    if (sensor_index >= TOF_PERF_SENSOR_COUNT) {
        return;
    }

    uint32_t now_ticks = tof_perf_now_ticks();
    s_perf[sensor_index].irq_ts_ticks = now_ticks;
    s_perf[sensor_index].irq_count++;
    s_seen_mask |= (1UL << sensor_index);
}

uint8_t tof_perf_read_sensor(uint8_t sensor_index, tof_perf_read_fn_t read_fn)
{
    if (read_fn == 0) {
        return 0;
    }

    if (sensor_index >= TOF_PERF_SENSOR_COUNT) {
        return read_fn(sensor_index);
    }

    uint32_t start_ticks = tof_perf_now_ticks();
    uint32_t irq_ts_ticks = s_perf[sensor_index].irq_ts_ticks;
    s_seen_mask |= (1UL << sensor_index);

    if (irq_ts_ticks != 0U) {
        s_perf[sensor_index].irq2read_sum_us += tof_perf_elapsed_us(irq_ts_ticks, start_ticks);
        s_perf[sensor_index].irq2read_samples++;
    }

    uint8_t ok = read_fn(sensor_index);
    uint32_t end_ticks = tof_perf_now_ticks();

    s_perf[sensor_index].read_attempt_count++;
    s_perf[sensor_index].read_sum_us += tof_perf_elapsed_us(start_ticks, end_ticks);
    s_perf[sensor_index].read_samples++;

    if (ok) {
        s_perf[sensor_index].read_success_count++;
#if TOF_PERF_FRAME_MARKER
        log_info("[TOF_FRAME] s=%u seq=%lu",
                 (unsigned int)sensor_index,
                 (unsigned long)s_perf[sensor_index].read_success_count);
#endif
    }

    tof_perf_report_if_due(0U);

    return ok;
}

void tof_perf_mark_snapshot_and_report(uint32_t sensor_flags, uint32_t present_mask)
{
    uint32_t now_ticks = tof_perf_now_ticks();
    s_seen_mask |= present_mask;

    for (uint8_t i = 0; i < TOF_PERF_SENSOR_COUNT; i++) {
        uint32_t bit = (1UL << i);
        if ((sensor_flags & bit) == 0U) {
            continue;
        }

        uint32_t irq_ts_ticks = s_perf[i].irq_ts_ticks;
        if (irq_ts_ticks != 0U) {
            s_perf[i].irq2snap_sum_us += tof_perf_elapsed_us(irq_ts_ticks, now_ticks);
            s_perf[i].irq2snap_samples++;
        }
        s_perf[i].snapshot_count++;
    }

    tof_perf_report_if_due(present_mask);
}

static void tof_perf_report_if_due(uint32_t present_mask)
{
    uint32_t now_ms = HAL_GetTick();
    uint32_t elapsed_ms = now_ms - s_last_report_ms;
    if (elapsed_ms < TOF_PERF_REPORT_PERIOD_MS) {
        return;
    }
    if (elapsed_ms == 0U) {
        elapsed_ms = 1U;
    }
    s_last_report_ms = now_ms;

    if (!s_dwt_usable && !s_dwt_warning_reported) {
        s_dwt_warning_reported = 1;
        log_info("[TOF_PERF] clock=DWT_UNAVAILABLE fallback=HAL_GetTick resolution_us=1000");
    }

    uint32_t report_mask = (present_mask != 0U) ? present_mask : s_seen_mask;

    for (uint8_t i = 0; i < TOF_PERF_SENSOR_COUNT; i++) {
        uint32_t bit = (1UL << i);
        if ((report_mask & bit) == 0U) {
            continue;
        }

        uint32_t irq_count = s_perf[i].irq_count;
        uint32_t read_count = s_perf[i].read_success_count;
        uint32_t irq_delta = irq_count - s_perf[i].last_irq_count;
        uint32_t read_delta = read_count - s_perf[i].last_read_success_count;
        uint32_t fps = (read_delta * 1000U + (elapsed_ms / 2U)) / elapsed_ms;
        uint32_t drops = (irq_delta > read_delta) ? (irq_delta - read_delta) : 0U;
        uint32_t irq2read_us = tof_perf_mean_and_reset(
            &s_perf[i].irq2read_sum_us,
            &s_perf[i].irq2read_samples);
        uint32_t read_us = tof_perf_mean_and_reset(
            &s_perf[i].read_sum_us,
            &s_perf[i].read_samples);
        uint32_t irq2snap_us = tof_perf_mean_and_reset(
            &s_perf[i].irq2snap_sum_us,
            &s_perf[i].irq2snap_samples);

        s_perf[i].last_irq_count = irq_count;
        s_perf[i].last_read_success_count = read_count;

        log_info("[TOF_PERF] s=%u fps=%lu irq2read_us=%lu read_us=%lu irq2snap_us=%lu drops=%lu",
                 (unsigned int)i,
                 (unsigned long)fps,
                 (unsigned long)irq2read_us,
                 (unsigned long)read_us,
                 (unsigned long)irq2snap_us,
                 (unsigned long)drops);
    }
}

#else

typedef int tof_perf_monitor_disabled_t;

#endif // TOF_PERF_MONITOR
