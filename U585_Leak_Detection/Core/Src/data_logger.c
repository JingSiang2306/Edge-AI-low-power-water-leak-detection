#include "data_logger.h"
#include "audio_processing.h"
#include "vibration_processing.h"
#include <stdio.h>

/* Max total logging duration and slice length (ms) */
#define DL_MAX_TOTAL_MS  60000U   // 60 s max, adjust if needed
#define DL_SLICE_MS      1000U    // 1 s per audio slice

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

/* -------------------------------------------------------------------------- */
/* 1) Sequential logger: audio then vib										  */
/* -------------------------------------------------------------------------- */

int logging_now(uint32_t duration_ms)
{
	// Safety: ensure vibration logger is OFF during sequential audio logging
	vib_running = 0U;
	vib_done    = 1U;

    if (duration_ms == 0U) {
        printf("[LOG ] ERROR: duration_ms = 0\r\n");
        return -1;
    }

    if (duration_ms > DL_MAX_TOTAL_MS) {
        printf("[LOG ] WARNING: duration_ms=%lu > DL_MAX_TOTAL_MS=%lu, clamping.\r\n",
               (unsigned long)duration_ms, (unsigned long)DL_MAX_TOTAL_MS);
        duration_ms = DL_MAX_TOTAL_MS;
    }

    /* number of slices of DL_SLICE_MS */
    uint32_t num_slices = duration_ms / DL_SLICE_MS;
    if ((duration_ms % DL_SLICE_MS) != 0U) {
        num_slices += 1U;
    }

    printf("---BEGIN_SAMPLE---\r\n");

    /******************** AUDIO PART (sequential) ************************/
    printf("---BEGIN_AUDIO_CSV---\r\n");

    uint32_t remaining_ms = duration_ms;

    for (uint32_t slice_idx = 0U; slice_idx < num_slices; slice_idx++) {

        uint32_t this_ms = min_u32(DL_SLICE_MS, remaining_ms);
        if (this_ms == 0U) {
            break;
        }

        if (this_ms > AP_MAX_RECORD_MS) {
            this_ms = AP_MAX_RECORD_MS;
        }

        int rc_a = audio_record_xs(this_ms);
        if (rc_a != 0) {
            printf("[LOG ] ERROR: audio_record_xs(ms=%lu) rc=%d\r\n",
                   (unsigned long)this_ms, rc_a);
            printf("---END_AUDIO_CSV---\r\n");
            printf("---BEGIN_VIB_CSV---\r\n");
            printf("---END_VIB_CSV---\r\n");
            printf("---END_SAMPLE---\r\n");
            return -2;
        }

        const int16_t *pcm = audio_get_capture_buffer();
        uint32_t n = g_ap_rec_samples;  // valid samples in this slice

        for (uint32_t i = 0U; i < n; i++) {
            printf("%d\r\n", (int)pcm[i]);
        }

        remaining_ms -= this_ms;
        if (remaining_ms == 0U) {
            break;
        }
    }

    printf("---END_AUDIO_CSV---\r\n");

    /****************** VIBRATION PART (sequential) **********************/
    printf("---BEGIN_VIB_CSV---\r\n");

    vib_done  = 0;
    vib_count = 0;

    int rv = vibrate_record_xs_print(duration_ms);
    if (rv != 0) {
        printf("[LOG ] ERROR: vibrate_record_xs_print(ms=%lu) rc=%d\r\n",
               (unsigned long)duration_ms, rv);
        printf("---END_VIB_CSV---\r\n");
        printf("---END_SAMPLE---\r\n");
        return -3;
    }

    // vibrate_record_xs_print() prints one value per line
    printf("---END_VIB_CSV---\r\n");
    printf("---END_SAMPLE---\r\n");

    return 0;
}


/* -------------------------------------------------------------------------- */
/* 2) Simultaneous multi-slice dataset logger                                 */
/* -------------------------------------------------------------------------- */
/*
 * Use this for dataset collection (e.g. 3 s, 30 s).
 * - Audio: streamed in 1 s slices (no huge RAM).
 * - Vibration: captured continuously over the full duration using vib_log_*,
 * - Output format matches DL_Logger_v4.py expectations.
 */
int logging_syn_dataset(uint32_t duration_ms)
{
    if (duration_ms == 0U) {
        printf("[LOG ] ERROR: duration_ms = 0\r\n");
        return -1;
    }

    if (duration_ms > DL_MAX_TOTAL_MS) {
        printf("[LOG ] WARNING: duration_ms=%lu > DL_MAX_TOTAL_MS=%lu, clamping.\r\n",
               (unsigned long)duration_ms, (unsigned long)DL_MAX_TOTAL_MS);
        duration_ms = DL_MAX_TOTAL_MS;
    }

    if (duration_ms > (VIB_MAX_SECONDS * 1000U)) {
        duration_ms = VIB_MAX_SECONDS * 1000U;
    }

    /* number of DL_SLICE_MS slices */
    uint32_t num_slices = duration_ms / DL_SLICE_MS;
    if ((duration_ms % DL_SLICE_MS) != 0U) {
        num_slices += 1U;
    }

    printf("---BEGIN_SAMPLE---\r\n");

    /******************** AUDIO PART (multi-slice) ************************/
    printf("---BEGIN_AUDIO_CSV---\r\n");

    uint32_t remaining_ms = duration_ms;

    /* Start continuous vibration logging over the whole window */
    vib_log_start(duration_ms);

    for (uint32_t slice_idx = 0U; slice_idx < num_slices; slice_idx++) {

        uint32_t this_ms = min_u32(DL_SLICE_MS, remaining_ms);
        if (this_ms == 0U) {
            break;
        }

        if (this_ms > AP_MAX_RECORD_MS) {
            this_ms = AP_MAX_RECORD_MS;
        }

        int rc_a = audio_record_xs(this_ms);
        if (rc_a != 0) {
            printf("[LOG ] ERROR: audio_record_xs(ms=%lu) rc=%d\r\n",
                   (unsigned long)this_ms, rc_a);
            printf("---END_AUDIO_CSV---\r\n");
            printf("---BEGIN_VIB_CSV---\r\n");
            printf("---END_VIB_CSV---\r\n");
            printf("---END_SAMPLE---\r\n");
            vib_log_finish();
            return -2;
        }

        const int16_t *pcm = audio_get_capture_buffer();
        uint32_t n = g_ap_rec_samples;

        for (uint32_t i = 0U; i < n; i++) {
            printf("%d\r\n", (int)pcm[i]);
        }

        remaining_ms -= this_ms;
        if (remaining_ms == 0U) {
            break;
        }
    }

    /* Stop vibration logging AFTER all slices */
    vib_log_finish();

    printf("---END_AUDIO_CSV---\r\n");

    /****************** VIBRATION PART ************/
    printf("---BEGIN_VIB_CSV---\r\n");

    int rv = vib_log_print_z();
    if (rv != 0) {
        printf("[LOG ] ERROR: vib_log_print_z() rc=%d\r\n", rv);
        printf("---END_VIB_CSV---\r\n");
        printf("---END_SAMPLE---\r\n");
        return -3;
    }

    printf("---END_VIB_CSV---\r\n");
    printf("---END_SAMPLE---\r\n");

    return 0;
}

