/**
 * detection_logic.c
 *
 * 30 s late-fusion detection pipeline (10 x 3 s chunks)
 *
 * Flow:
 *   main.c  -> detection_logic_run_30s_latefusion()
 *            -> for each 3 s chunk:
 *                 audio_record_chunk_with_vib(3000)
 *                 audio_detection_run_sliding_from_pcm(..., &audio_result)
 *                 vibration_detection_run_sliding_from_z(..., &vibration_result)
 *            -> per-sensor 30 s decision:
 *                 1) Temporal Hysteresis / persistence
 *                 2) Strong Fraction
 *            -> late fusion decision
 *            -> print final summary
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "detection_logic.h"
#include "audio_processing.h"
#include "audio_detection.h"
#include "vibration_processing.h"
#include "vibration_detection.h"

/* These helpers currently live in main.c (no header). */
extern uint32_t get_time_ms_of_day(void);
extern const char *get_date_string(void);

extern double verdict_conf;

/* Filled for LoRa payload later */
lora_verdict g_lora_verdict;

/* -------------------------------------------------------------------------- */
/* Tunable thresholds                                                         */
/* -------------------------------------------------------------------------- */

/* 30 s temporal-hysteresis logic:
 * Count only 3 s chunks that were already classified as leak, and require
 * either enough persistent leak chunks overall or a short consecutive run.
 */
#define DETECTION_30S_PERSISTENT_CHUNK_CONFIDENCE_THRESHOLD   0.58f
#define DETECTION_30S_PERSISTENT_CHUNK_MIN_COUNT              5U
#define DETECTION_30S_CONSECUTIVE_LEAK_MIN_COUNT              3U

/* 30 s strong-fraction logic:
 * Count only 3 s chunks that were already classified as leak and also look
 * strongly leak-like at the chunk level.
 */
#define DETECTION_30S_STRONG_CHUNK_CONFIDENCE_THRESHOLD       0.75f
#define DETECTION_30S_STRONG_CHUNK_FRACTION_THRESHOLD         0.35f
#define DETECTION_30S_STRONG_CHUNK_MIN_COUNT                  3U

/* Per-sensor score weights */
#define DETECTION_SCORE_WEIGHT_3S_LEAK_RATIO                  0.25f
#define DETECTION_SCORE_WEIGHT_PERSISTENT_RATIO               0.35f
#define DETECTION_SCORE_WEIGHT_STRONG_RATIO                   0.20f
#define DETECTION_SCORE_WEIGHT_MEAN_LEAK_CONFIDENCE           0.20f
#define DETECTION_SCORE_TEMPORAL_BONUS                        0.05f
#define DETECTION_SCORE_STRONG_BONUS                          0.05f

/* Fusion */
#define DETECTION_FUSION_AUDIO_WEIGHT                         0.80f
#define DETECTION_FUSION_VIBRATION_WEIGHT                     0.20f
#define DETECTION_FUSION_BOTH_TEMPORAL_BONUS                  0.05f
#define DETECTION_FUSION_BOTH_STRONG_BONUS                    0.05f
#define DETECTION_FUSION_THRESHOLD                            0.58f

/* -------------------------------------------------------------------------- */
/* Small helpers                                                              */
/* -------------------------------------------------------------------------- */

static float divide_safely(float numerator, float denominator)
{
    return (denominator > 0.0f) ? (numerator / denominator) : 0.0f;
}

static void format_time_ms(uint32_t time_ms_of_day, char *output_buffer, size_t output_buffer_length)
{
    if (!output_buffer || output_buffer_length == 0U) {
        return;
    }

    uint32_t total_seconds = time_ms_of_day / 1000U;
    uint32_t milliseconds  = time_ms_of_day % 1000U;

    uint32_t hours   = (total_seconds / 3600U) % 24U;
    uint32_t minutes = (total_seconds / 60U) % 60U;
    uint32_t seconds = total_seconds % 60U;

    snprintf(output_buffer,
             output_buffer_length,
             "%02lu:%02lu:%02lu.%03lu",
             (unsigned long)hours,
             (unsigned long)minutes,
             (unsigned long)seconds,
             (unsigned long)milliseconds);
}

typedef struct {
    uint32_t chunk_count;

    float    chunk_average_leak_confidence[DETECTION_30S_CHUNK_COUNT];
    float    chunk_average_background_confidence[DETECTION_30S_CHUNK_COUNT];
    float    chunk_strong_leak_fraction[DETECTION_30S_CHUNK_COUNT];
    uint8_t  chunk_predicted_is_leak[DETECTION_30S_CHUNK_COUNT];
    uint8_t  chunk_slot_average_logic_triggered[DETECTION_30S_CHUNK_COUNT];
    uint8_t  chunk_strong_fraction_logic_triggered[DETECTION_30S_CHUNK_COUNT];

    uint32_t total_window_count;
    float    total_leak_confidence_sum;
    float    total_background_confidence_sum;
} detection_30s_accumulator_t;

typedef struct {
    uint32_t three_second_leak_chunk_count;
    uint32_t persistent_leak_chunk_count;
    uint32_t strong_leak_chunk_count;
    uint32_t longest_consecutive_persistent_leak_run;

    float    average_leak_confidence_all_windows;
    float    average_background_confidence_all_windows;
    float    average_leak_confidence_on_detected_chunks;

    uint8_t  temporal_hysteresis_triggered;
    uint8_t  strong_fraction_triggered;
    float    sensor_score;
} detection_30s_sensor_summary_t;

static void add_chunk_result_to_accumulator(detection_30s_accumulator_t *accumulator,
                                            const det_3s_result_t *chunk_result)
{
    if (!accumulator || !chunk_result) {
        return;
    }

    uint32_t chunk_index = accumulator->chunk_count;
    if (chunk_index < DETECTION_30S_CHUNK_COUNT) {
        accumulator->chunk_average_leak_confidence[chunk_index] =
            chunk_result->average_leak_confidence;
        accumulator->chunk_average_background_confidence[chunk_index] =
            chunk_result->average_background_confidence;
        accumulator->chunk_strong_leak_fraction[chunk_index] =
            chunk_result->strong_leak_fraction;
        accumulator->chunk_predicted_is_leak[chunk_index] =
            chunk_result->predicted_is_leak;
        accumulator->chunk_slot_average_logic_triggered[chunk_index] =
            chunk_result->slot_average_logic_triggered;
        accumulator->chunk_strong_fraction_logic_triggered[chunk_index] =
            chunk_result->strong_fraction_logic_triggered;
    }

    accumulator->chunk_count += 1U;

    accumulator->total_window_count += chunk_result->window_count;
    accumulator->total_leak_confidence_sum += chunk_result->sum_leak_confidence;
    accumulator->total_background_confidence_sum += chunk_result->sum_background_confidence;
}

static uint32_t find_longest_run_of_ones(const uint8_t *values, uint32_t count)
{
    uint32_t longest_run = 0U;
    uint32_t current_run = 0U;

    if (!values) {
        return 0U;
    }

    for (uint32_t index = 0U; index < count; index++) {
        if (values[index] != 0U) {
            current_run++;
            if (current_run > longest_run) {
                longest_run = current_run;
            }
        } else {
            current_run = 0U;
        }
    }

    return longest_run;
}

static void evaluate_30s_sensor_logic(const detection_30s_accumulator_t *accumulator,
                                      detection_30s_sensor_summary_t *summary)
{
    if (!accumulator || !summary) {
        return;
    }

    memset(summary, 0, sizeof(*summary));

    const uint32_t chunk_count = accumulator->chunk_count;

    summary->average_leak_confidence_all_windows =
        divide_safely(accumulator->total_leak_confidence_sum,
                      (float)accumulator->total_window_count);

    summary->average_background_confidence_all_windows =
        divide_safely(accumulator->total_background_confidence_sum,
                      (float)accumulator->total_window_count);

    float detected_chunk_leak_confidence_sum = 0.0f;
    uint32_t detected_chunk_leak_confidence_count = 0U;

    uint8_t persistent_leak_mask[DETECTION_30S_CHUNK_COUNT] = {0U};

    for (uint32_t chunk_index = 0U; chunk_index < chunk_count; chunk_index++) {
        uint8_t chunk_predicted_is_leak =
            accumulator->chunk_predicted_is_leak[chunk_index];

        float chunk_average_leak_confidence =
            accumulator->chunk_average_leak_confidence[chunk_index];

        float chunk_strong_leak_fraction =
            accumulator->chunk_strong_leak_fraction[chunk_index];

        if (chunk_predicted_is_leak != 0U) {
            summary->three_second_leak_chunk_count++;
            detected_chunk_leak_confidence_sum += chunk_average_leak_confidence;
            detected_chunk_leak_confidence_count++;
        }

        uint8_t chunk_is_persistent_leak =
            (chunk_predicted_is_leak != 0U) &&
            (chunk_average_leak_confidence >= DETECTION_30S_PERSISTENT_CHUNK_CONFIDENCE_THRESHOLD);

        if (chunk_is_persistent_leak != 0U) {
            summary->persistent_leak_chunk_count++;
            persistent_leak_mask[chunk_index] = 1U;
        }

        uint8_t chunk_is_strong_leak =
            (chunk_predicted_is_leak != 0U) &&
            ((chunk_average_leak_confidence >= DETECTION_30S_STRONG_CHUNK_CONFIDENCE_THRESHOLD) ||
             (chunk_strong_leak_fraction >= DETECTION_30S_STRONG_CHUNK_FRACTION_THRESHOLD));

        if (chunk_is_strong_leak != 0U) {
            summary->strong_leak_chunk_count++;
        }
    }

    summary->longest_consecutive_persistent_leak_run =
        find_longest_run_of_ones(persistent_leak_mask, chunk_count);

    summary->average_leak_confidence_on_detected_chunks =
        (detected_chunk_leak_confidence_count > 0U)
            ? divide_safely(detected_chunk_leak_confidence_sum,
                            (float)detected_chunk_leak_confidence_count)
            : summary->average_leak_confidence_all_windows;

    summary->temporal_hysteresis_triggered =
        ((summary->persistent_leak_chunk_count >= DETECTION_30S_PERSISTENT_CHUNK_MIN_COUNT) ||
         (summary->longest_consecutive_persistent_leak_run >= DETECTION_30S_CONSECUTIVE_LEAK_MIN_COUNT))
            ? 1U : 0U;

    summary->strong_fraction_triggered =
        (summary->strong_leak_chunk_count >= DETECTION_30S_STRONG_CHUNK_MIN_COUNT)
            ? 1U : 0U;

    float three_second_leak_ratio =
        divide_safely((float)summary->three_second_leak_chunk_count, (float)chunk_count);

    float persistent_leak_ratio =
        divide_safely((float)summary->persistent_leak_chunk_count, (float)chunk_count);

    float strong_leak_ratio =
        divide_safely((float)summary->strong_leak_chunk_count, (float)chunk_count);

    float sensor_score =
        (DETECTION_SCORE_WEIGHT_3S_LEAK_RATIO * three_second_leak_ratio) +
        (DETECTION_SCORE_WEIGHT_PERSISTENT_RATIO * persistent_leak_ratio) +
        (DETECTION_SCORE_WEIGHT_STRONG_RATIO * strong_leak_ratio) +
        (DETECTION_SCORE_WEIGHT_MEAN_LEAK_CONFIDENCE * summary->average_leak_confidence_on_detected_chunks);

    if (summary->temporal_hysteresis_triggered != 0U) {
        sensor_score += DETECTION_SCORE_TEMPORAL_BONUS;
    }
    if (summary->strong_fraction_triggered != 0U) {
        sensor_score += DETECTION_SCORE_STRONG_BONUS;
    }

    if (sensor_score < 0.0f) {
        sensor_score = 0.0f;
    }
    if (sensor_score > 1.0f) {
        sensor_score = 1.0f;
    }

    summary->sensor_score = sensor_score;
}

static void print_chunk_leak_confidence_list(const char *tag,
                                             const detection_30s_accumulator_t *accumulator)
{
    if (!tag || !accumulator) {
        return;
    }

    printf("[%s] Chunk average leak confidence = {", tag);
    for (uint32_t chunk_index = 0U; chunk_index < accumulator->chunk_count; chunk_index++) {
        if (chunk_index > 0U) {
            printf(", ");
        }
        printf("%.3f",
               (double)accumulator->chunk_average_leak_confidence[chunk_index]);
    }
    printf("}\r\n");
}

static void print_chunk_leak_flags(const char *tag,
                                   const detection_30s_accumulator_t *accumulator)
{
    if (!tag || !accumulator) {
        return;
    }

    printf("[%s] 3s leak verdicts = {", tag);
    for (uint32_t chunk_index = 0U; chunk_index < accumulator->chunk_count; chunk_index++) {
        if (chunk_index > 0U) {
            printf(", ");
        }
        printf("%u", (unsigned)accumulator->chunk_predicted_is_leak[chunk_index]);
    }
    printf("}\r\n");
}

static void print_30s_sensor_summary(const char *tag,
                                     const detection_30s_accumulator_t *accumulator,
                                     const detection_30s_sensor_summary_t *summary)
{
    if (!tag || !accumulator || !summary) {
        return;
    }

    print_chunk_leak_confidence_list(tag, accumulator);
    print_chunk_leak_flags(tag, accumulator);

    printf("[%s] 3s leak chunks = %lu/%lu  persistent leak chunks = %lu/%lu  strong leak chunks = %lu/%lu\r\n",
           tag,
           (unsigned long)summary->three_second_leak_chunk_count,
           (unsigned long)accumulator->chunk_count,
           (unsigned long)summary->persistent_leak_chunk_count,
           (unsigned long)accumulator->chunk_count,
           (unsigned long)summary->strong_leak_chunk_count,
           (unsigned long)accumulator->chunk_count);

    printf("[%s] longest consecutive persistent leak run = %lu\r\n",
           tag,
           (unsigned long)summary->longest_consecutive_persistent_leak_run);

    printf("[%s] average leak confidence = %.3f  average background confidence = %.3f\r\n",
           tag,
           (double)summary->average_leak_confidence_all_windows,
           (double)summary->average_background_confidence_all_windows);

    printf("[%s] temporal hysteresis = %s  strong fraction = %s  sensor score = %.3f\r\n",
           tag,
           summary->temporal_hysteresis_triggered ? "triggered" : "not triggered",
           summary->strong_fraction_triggered ? "triggered" : "not triggered",
           (double)summary->sensor_score);
}

/* -------------------------------------------------------------------------- */
/* Public entrypoint                                                          */
/* -------------------------------------------------------------------------- */

int detection_logic_run_30s_latefusion(uint32_t chunk_duration_ms)
{
    printf("[DL  ] %lus late-fusion detection start: %lu chunks x %lu s\r\n",
           (unsigned long)chunk_duration_ms / 1000U,
           (unsigned long)DETECTION_30S_CHUNK_COUNT,
           (unsigned long)chunk_duration_ms / 1000U);

    memset(&g_lora_verdict, 0, sizeof(g_lora_verdict));
    g_lora_verdict.chunk_duration_ms = chunk_duration_ms;
    g_lora_verdict.chunk_count = DETECTION_30S_CHUNK_COUNT;

    {
        uint32_t start_time_ms_of_day = get_time_ms_of_day();
        const char *record_date_string = get_date_string();

        strncpy(g_lora_verdict.record_date_string,
                record_date_string,
                sizeof(g_lora_verdict.record_date_string) - 1U);
        g_lora_verdict.record_date_string[sizeof(g_lora_verdict.record_date_string) - 1U] = '\0';

        format_time_ms(start_time_ms_of_day,
                       g_lora_verdict.start_time_string,
                       sizeof(g_lora_verdict.start_time_string));
    }

    detection_30s_accumulator_t audio_accumulator = {0};
    detection_30s_accumulator_t vibration_accumulator = {0};

    for (uint32_t chunk_index = 0U; chunk_index < DETECTION_30S_CHUNK_COUNT; chunk_index++) {
        uint32_t record_time_ms_of_day = get_time_ms_of_day();
        const char *record_date_string = get_date_string();

        printf("[DL  ] Chunk %lu/%lu: recording %lu s...\r\n",
               (unsigned long)(chunk_index + 1U),
               (unsigned long)DETECTION_30S_CHUNK_COUNT,
               (unsigned long)chunk_duration_ms / 1000U);

        int capture_status = audio_record_chunk_with_vib(chunk_duration_ms);

        printf("[DL  ] Chunk %lu capture done. rc = %d  audio = %lu  vib = %lu (effective fs = %lu Hz)\r\n",
               (unsigned long)(chunk_index + 1U),
               capture_status,
               (unsigned long)g_ap_rec_samples,
               (unsigned long)vib_count,
               (unsigned long)vib_fs_hz);

        if (capture_status != 0) {
            printf("[DL  ] ERROR: capture failed on chunk %lu, rc=%d\r\n",
                   (unsigned long)(chunk_index + 1U),
                   capture_status);
            return -1;
        }

        const int16_t *audio_pcm_buffer = audio_get_capture_buffer();
        uint16_t audio_predicted_class_id = 0U;
        det_3s_result_t audio_chunk_result;

        neai_det_status_t audio_status =
            audio_detection_run_sliding_from_pcm(audio_pcm_buffer,
                                                 g_ap_rec_samples,
                                                 &audio_predicted_class_id,
                                                 record_time_ms_of_day,
                                                 record_date_string,
                                                 &audio_chunk_result);

        if (audio_status != NEAI_DET_OK) {
            printf("[DL  ] WARN: audio detection status=%d on chunk %lu\r\n",
                   (int)audio_status,
                   (unsigned long)(chunk_index + 1U));
        }
        add_chunk_result_to_accumulator(&audio_accumulator, &audio_chunk_result);

        uint16_t vibration_predicted_class_id = 0U;
        det_3s_result_t vibration_chunk_result;

        neai_det_status_t vibration_status =
            vibration_detection_run_sliding_from_z(vib_get_z_buffer(),
                                                   vib_count,
                                                   vib_fs_hz,
                                                   &vibration_predicted_class_id,
                                                   record_time_ms_of_day,
                                                   record_date_string,
                                                   &vibration_chunk_result);

        if (vibration_status != NEAI_DET_OK) {
            printf("[DL  ] WARN: vibration detection status=%d on chunk %lu\r\n",
                   (int)vibration_status,
                   (unsigned long)(chunk_index + 1U));
        }
        add_chunk_result_to_accumulator(&vibration_accumulator, &vibration_chunk_result);

        printf("\r\n");
    }

    {
        uint32_t end_time_ms_of_day = get_time_ms_of_day();
        format_time_ms(end_time_ms_of_day,
                       g_lora_verdict.end_time_string,
                       sizeof(g_lora_verdict.end_time_string));
    }

    detection_30s_sensor_summary_t audio_summary;
    detection_30s_sensor_summary_t vibration_summary;

    evaluate_30s_sensor_logic(&audio_accumulator, &audio_summary);
    evaluate_30s_sensor_logic(&vibration_accumulator, &vibration_summary);

    printf("[FUSE] ======================================== FUSE ========================================\r\n");
    print_30s_sensor_summary("AUD ", &audio_accumulator, &audio_summary);
    print_30s_sensor_summary("VIB ", &vibration_accumulator, &vibration_summary);

    memcpy(g_lora_verdict.audio_chunk_average_leak_confidence,
           audio_accumulator.chunk_average_leak_confidence,
           sizeof(g_lora_verdict.audio_chunk_average_leak_confidence));

    memcpy(g_lora_verdict.vibration_chunk_average_leak_confidence,
           vibration_accumulator.chunk_average_leak_confidence,
           sizeof(g_lora_verdict.vibration_chunk_average_leak_confidence));

    uint32_t total_window_count_all =
        audio_accumulator.total_window_count + vibration_accumulator.total_window_count;
    float total_leak_confidence_sum_all =
        audio_accumulator.total_leak_confidence_sum + vibration_accumulator.total_leak_confidence_sum;
    float total_background_confidence_sum_all =
        audio_accumulator.total_background_confidence_sum + vibration_accumulator.total_background_confidence_sum;

    float average_leak_confidence_all =
        divide_safely(total_leak_confidence_sum_all, (float)total_window_count_all);
    float average_background_confidence_all =
        divide_safely(total_background_confidence_sum_all, (float)total_window_count_all);

    uint8_t vibration_enabled = vibration_detection_is_enabled();

    float fusion_score = audio_summary.sensor_score;
    if (vibration_enabled != 0U) {
        fusion_score =
            (DETECTION_FUSION_AUDIO_WEIGHT * audio_summary.sensor_score) +
            (DETECTION_FUSION_VIBRATION_WEIGHT * vibration_summary.sensor_score);

        if ((audio_summary.temporal_hysteresis_triggered != 0U) &&
            (vibration_summary.temporal_hysteresis_triggered != 0U)) {
            fusion_score += DETECTION_FUSION_BOTH_TEMPORAL_BONUS;
        }

        if ((audio_summary.strong_fraction_triggered != 0U) &&
            (vibration_summary.strong_fraction_triggered != 0U)) {
            fusion_score += DETECTION_FUSION_BOTH_STRONG_BONUS;
        }
    }

    if (fusion_score < 0.0f) {
        fusion_score = 0.0f;
    }
    if (fusion_score > 1.0f) {
        fusion_score = 1.0f;
    }

    uint8_t final_prediction_is_leak =
        (fusion_score >= DETECTION_FUSION_THRESHOLD) ? 1U : 0U;

    if (final_prediction_is_leak != 0U) {
        verdict_conf = (double)average_leak_confidence_all;
    } else {
        verdict_conf = (double)average_background_confidence_all;
    }

    g_lora_verdict.average_leak_confidence_all = average_leak_confidence_all;
    g_lora_verdict.average_background_confidence_all = average_background_confidence_all;
    g_lora_verdict.final_fusion_score = fusion_score;
    g_lora_verdict.final_prediction_is_leak = final_prediction_is_leak;

    printf("[FUSE] Audio score = %.3f  Vibration score = %.3f\r\n",
           (double)audio_summary.sensor_score,
           (double)vibration_summary.sensor_score);

    printf("[FUSE] Audio weight = %.2f  Vibration weight = %.2f\r\n",
           (double)DETECTION_FUSION_AUDIO_WEIGHT,
           (double)DETECTION_FUSION_VIBRATION_WEIGHT);

    printf("[FUSE] Fusion score = %.3f %s %.2f (threshold) -> Final decision = %s\r\n",
           (double)fusion_score,
           final_prediction_is_leak ? ">=" : "<",
           (double)DETECTION_FUSION_THRESHOLD,
           final_prediction_is_leak ? "LEAK" : "BACKGROUND");

    printf("[FUSE] Leak average confidence = %.3f  Background average confidence = %.3f  Window count = %lu\r\n",
           (double)average_leak_confidence_all,
           (double)average_background_confidence_all,
           (unsigned long)total_window_count_all);

    return final_prediction_is_leak ? 1 : 0;
}
