/*
 * vibration_processing.c
 *
 *  Created on: Nov 2, 2025
 *      Author: FBBC
 */

#include "vibration_processing.h"
#include "main.h"   // for SystemCoreClock, HAL definitions

// --------- Internal helpers ---------
static inline int get_axes(BSP_MOTION_SENSOR_Axes_t *a) {
    // Your BSP uses xval/yval/zval (you mentioned this explicitly)
    return BSP_MOTION_SENSOR_GetAxes(0, MOTION_ACCELERO, a);
}

// convert mg (int32) to int16_t with saturation
static inline int16_t sat16(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

volatile uint8_t vib_running = 0;
volatile uint8_t vib_done    = 0;

int16_t vib_raw_z[VIB_MAX_SAMPLES];

uint32_t vib_count = 0;
uint32_t vib_fs_hz = VIB_TARGET_HZ;

static uint32_t vib_log_last_us = 0;
static uint32_t vib_log_start_us = 0;
static uint32_t vib_log_end_us   = 0;

/* -------------------------------------------------------------------------
 * Non-blocking vibration logger for combined audio+vib recording
 * ------------------------------------------------------------------------- */

/* Target vibration sampling rate during logging (you already use ~6.6 kHz) */
#define VIB_LOG_FS_HZ     6667U

// Target vibration rate (6.6 kHz)
uint32_t vib_sample_period_us = 1000000U / VIB_LOG_FS_HZ;

/**
 * @brief  Start vibration logging over a given duration (non-blocking).
 *         After this, you must call vib_log_sample_if_due() regularly.
 */
static uint32_t vib_debug_t0_us;
void vib_log_start(uint32_t duration_ms)
{
    vib_count   = 0;
    vib_running = 1;
    vib_done    = 0;

    vib_timebase_init();

    // Time bounded capture
    vib_log_start_us = vib_micros();
    vib_log_end_us   = vib_log_start_us + (duration_ms * 1000U);

    /* Period: round UP so we don't schedule faster than target */
    vib_sample_period_us = (1000000U + (VIB_LOG_FS_HZ - 1U)) / VIB_LOG_FS_HZ;  // ceil
    vib_log_last_us = vib_log_start_us;

    vib_debug_t0_us = vib_log_start_us;
}

void vib_log_sample_if_due(void)
{
    if (!vib_running) {
        return;
    }

    uint32_t now_us = vib_micros();

    /* Stop automatically when time is reached */
    if ((int32_t)(now_us - vib_log_end_us) >= 0) {
        vib_running = 0;
        vib_done    = 1;
        return;
    }

    /* Don't spend too long here: cap catch-up samples per call */
    uint32_t max_catchup = 16U;

    while (((now_us - vib_log_last_us) >= vib_sample_period_us) &&
           (vib_count < VIB_MAX_SAMPLES) &&
           (max_catchup--))
    {
        vib_log_last_us += vib_sample_period_us;

        uint32_t t0 = vib_micros();
        BSP_MOTION_SENSOR_Axes_t axes;
        if (get_axes(&axes) != BSP_ERROR_NONE) {
            return;
        }
        uint32_t t1 = vib_micros();

        static uint32_t worst = 0;
        uint32_t dt = t1 - t0;
        if (dt > worst) worst = dt;

        vib_raw_z[vib_count++] = sat16(axes.zval);

/*        if ((vib_count % 5000U) == 0U) {
            printf("[VIB ] get_axes dt_us=%lu worst=%lu\r\n",
                   (unsigned long)dt, (unsigned long)worst);
        }*/

        /* refresh 'now' so catch-up doesn't run too far on stale time */
        now_us = vib_micros();

        /* Stop check again (important if catch-up loop is long) */
        if ((int32_t)(now_us - vib_log_end_us) >= 0) {
            vib_running = 0;
            vib_done    = 1;
            return;
        }
    }
}

void vib_log_finish(void)
{
    vib_running = 0;
    vib_done = 1;

    uint32_t dt_us = vib_micros() - vib_debug_t0_us;

    if (dt_us > 0U) {
        vib_fs_hz = (uint32_t)(((uint64_t)vib_count * 1000000ULL) / (uint64_t)dt_us);
    } else {
        vib_fs_hz = VIB_LOG_FS_HZ;
    }

    float dur_s_raw = (float)dt_us / 1000000.0f;

    float dur_s = roundf(dur_s_raw);
    if (dur_s < 1.0f) dur_s = 1.0f;   // avoid divide-by-zero

    float expected = dur_s * 6667.0f;
    float pct_diff = (expected > 0.0f) ? (( (float)vib_count - expected) / expected) * 100.0f : 0.0f;

    printf("[VIB ] Elapsed = %luus (%.2f s)  vib_count = %lu (diff: %+0.2f%% expected_vib_count: %lu)  effective fs = %.1f Hz\r\n",
           (unsigned long)dt_us,
           (double)dur_s_raw,
           (unsigned long)vib_count,
           (double)pct_diff,
		   (unsigned long)expected,
           (dt_us > 0U) ? (1000000.0f * (float)vib_count / (float)dt_us) : 0.0f);

}

int vib_log_print_z(void)
{
    if (vib_count < 1U) {
        return -1;
    }

    int32_t baseline_z = (int32_t)vib_raw_z[0];

    for (uint32_t i = 0U; i < vib_count; i++) {
        int32_t dz = (int32_t)vib_raw_z[i] - baseline_z;
        printf("%.2f\r\n", (double)((float)dz));
    }

    return 0;
}

void vib_timebase_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t vib_micros(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000U));
}

// --------- Recording at vib_fs_hz (e.g. 3 kHz) ---------
int vibrate_record_xs(uint32_t x_ms)
{
    if (x_ms == 0U) {
        return -1;
    }

    if (x_ms > (VIB_MAX_SECONDS * 1000U)) {
        return -2;
    }

    vib_timebase_init();
    vib_count = 0U;

    const uint32_t period_us = (1000000U + (VIB_TARGET_HZ - 1U)) / VIB_TARGET_HZ; // ceil
    uint32_t start_us = vib_micros();
    uint32_t end_us   = start_us + (x_ms * 1000U);
    uint32_t next_us  = start_us;

    while ((int32_t)(vib_micros() - end_us) < 0) {

        uint32_t now_us = vib_micros();
        if ((int32_t)(now_us - next_us) >= 0) {

            BSP_MOTION_SENSOR_Axes_t axes;
            if (get_axes(&axes) != 0) {
                return -3;
            }

            if (vib_count < VIB_MAX_SAMPLES) {
                vib_raw_z[vib_count++] = sat16(axes.zval);
            } else {
                break;
            }

            next_us += period_us;
        }
    }

    uint32_t elapsed_us = vib_micros() - start_us;
    if (elapsed_us > 0U) {
        vib_fs_hz = (uint32_t)(((uint64_t)vib_count * 1000000ULL) / (uint64_t)elapsed_us);
    } else {
        vib_fs_hz = VIB_TARGET_HZ;
    }

    return 0;
}

int vibrate_record_xs_print(uint32_t x_ms)
{
    /* Clear any old state */
    vib_done    = 0U;
    vib_running = 0U;
    vib_count   = 0U;

    /* 1) Do a fresh blocking capture at ~VIB_TARGET_HZ */
    int rc = vibrate_record_xs(x_ms);
    if (rc != 0) {
        return rc;
    }

    /* 2) Make sure we have something to print */
    if (vib_count < 1U) {
        return -4;
    }

    /* 3) Baseline = first sample */
    int32_t baseline_z = (int32_t)vib_raw_z[0];

    /* 4) Print one baseline-subtracted sample per line, as float with 2 dp */
    for (uint32_t i = 0U; i < vib_count; ++i) {
        int32_t dz = (int32_t)vib_raw_z[i] - baseline_z;
        float   v  = (float)dz;
        printf("%.2f\r\n", (double)v);
    }

    return 0;
}
