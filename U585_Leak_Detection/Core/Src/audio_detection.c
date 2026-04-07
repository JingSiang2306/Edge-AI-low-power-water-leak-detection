/**
 * audio_detection.c
 *
 * NanoEdge AI-based acoustic leak detection (sliding-window version)
 *
 * - New NEAI API (no knowledge.h):
 *      neai_classification_init(void)
 *      neai_classification(..., int *id_class)  // 0-based id
 *      neai_get_class_name(id)
 *
 * - We keep your existing convention for the rest of the firmware:
 *      predicted_class_id: 1 = Leak, 2 = Background, 0 = Unknown
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "audio_detection.h"
#include "audio_processing.h"
#include "vibration_processing.h"
#include "data_logger.h"
#include "lora.h"

/* IMPORTANT: include shim BEFORE NanoEdgeAI header so prototypes get remapped */
#include "NEAI_Audio/NEAI_audio_shim.h"
#include "NEAI_Audio/NanoEdgeAI_audio.h"

/* -------------------------------------------------------------------------- */
/* Compatibility macros                    */
/* -------------------------------------------------------------------------- */
#ifndef DATA_INPUT_USER
#define DATA_INPUT_USER   NEAI_INPUT_SIGNAL_LENGTH
#endif

#ifndef AXIS_NUMBER
#define AXIS_NUMBER       NEAI_INPUT_AXIS_NUMBER
#endif

#ifndef CLASS_NUMBER
#define CLASS_NUMBER      NEAI_NUMBER_OF_CLASSES
#endif

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#ifndef AD_SAMPLE_RATE_HZ
#define AD_SAMPLE_RATE_HZ   16000U
#endif

uint32_t g_neai_num_windows    = 30U;
uint8_t  g_neai_skip_first_window = 0U;

#define AUDIO_SLOT_DURATION_MS                   500U
#define AUDIO_MAX_SLOT_COUNT                     16U

#define AUDIO_STRONG_LEAK_CONFIDENCE_THRESHOLD   0.80f
#define AUDIO_SLOT_LEAK_CONFIDENCE_THRESHOLD     0.60f
#define AUDIO_SLOT_LEAK_RATIO_THRESHOLD          0.50f
#define AUDIO_STRONG_LEAK_FRACTION_THRESHOLD     0.30f

/* -------------------------------------------------------------------------- */
/* NEAI Buffers & Class Mapping                                               */
/* -------------------------------------------------------------------------- */

static float audio_neai_input_buffer[DATA_INPUT_USER * AXIS_NUMBER];
static float audio_neai_output_probabilities[CLASS_NUMBER];

/* Resolved 0-based NEAI class IDs for this audio model */
static int g_audio_neai_leak_class_id = 0;
static int g_audio_neai_background_class_id = 1;

static const char *audio_get_class_name_from_predicted_id(uint16_t predicted_class_id)
{
    if (predicted_class_id == 1U) {
        return "Leak";
    }
    if (predicted_class_id == 2U) {
        return "Background";
    }
    return "Unknown";
}

static const char *audio_get_class_name_from_neai_id(int neai_class_id)
{
    const char *class_name = neai_get_class_name(neai_class_id);
    return (class_name != NULL) ? class_name : "Unknown";
}

static float audio_get_max_probability(const float *class_probabilities)
{
    float highest_probability = 0.0f;

    if (class_probabilities == NULL || CLASS_NUMBER == 0U) {
        return 0.0f;
    }

    highest_probability = class_probabilities[0];
    for (uint32_t class_index = 1U; class_index < CLASS_NUMBER; class_index++) {
        if (class_probabilities[class_index] > highest_probability) {
            highest_probability = class_probabilities[class_index];
        }
    }

    return highest_probability;
}

static void audio_neai_resolve_class_ids(void)
{
    int number_of_classes = neai_get_number_of_classes();
    if (number_of_classes <= 0) {
        g_audio_neai_leak_class_id = 0;
        g_audio_neai_background_class_id = 1;
        printf("[NEAI] Audio class mapping: invalid class count, fallback to Leak=0, Background=1\r\n");
        return;
    }

    int resolved_leak_class_id = -1;
    int resolved_background_class_id = -1;

    for (int class_index = 0; class_index < number_of_classes; class_index++) {
        const char *class_name = neai_get_class_name(class_index);
        if (class_name == NULL) {
            continue;
        }

        if ((strcmp(class_name, "Leak") == 0) ||
            (strcmp(class_name, "LEAK") == 0) ||
            (strcmp(class_name, "leak") == 0)) {
            resolved_leak_class_id = class_index;
        }
        else if ((strcmp(class_name, "Background") == 0) ||
                 (strcmp(class_name, "BACKGROUND") == 0) ||
                 (strcmp(class_name, "background") == 0)) {
            resolved_background_class_id = class_index;
        }
    }

    if (resolved_leak_class_id < 0) {
        resolved_leak_class_id = 0;
    }
    if (resolved_background_class_id < 0) {
        resolved_background_class_id = (number_of_classes > 1) ? 1 : 0;
    }

    g_audio_neai_leak_class_id = resolved_leak_class_id;
    g_audio_neai_background_class_id = resolved_background_class_id;

    printf("[NEAI] Audio class mapping resolved:\r\n");
    for (int class_index = 0; class_index < number_of_classes; class_index++) {
        printf("[NEAI]   model_id=%d -> %s%s%s\r\n",
               class_index,
               audio_get_class_name_from_neai_id(class_index),
               (class_index == g_audio_neai_leak_class_id) ? " [used as Leak]" : "",
               (class_index == g_audio_neai_background_class_id) ? " [used as Background]" : "");
    }
}

/* -------------------------------------------------------------------------- */
/* Local helpers                                                              */
/* -------------------------------------------------------------------------- */

static void format_time_hms_ms(uint32_t ms_of_day, char *buf, size_t buflen)
{
    uint32_t total_ms = ms_of_day % (24U * 3600U * 1000U);
    uint32_t seconds  = total_ms / 1000U;
    uint32_t ms       = total_ms % 1000U;

    uint32_t hours   = seconds / 3600U;
    uint32_t minutes = (seconds % 3600U) / 60U;
    uint32_t secs    = seconds % 60U;

    uint32_t cs = ms / 10U;

    snprintf(buf, buflen, "%02lu:%02lu:%02lu.%02lu",
             (unsigned long)hours,
             (unsigned long)minutes,
             (unsigned long)secs,
             (unsigned long)cs);
}

static void audio_detection_print_window_with_time(uint16_t predicted_class_id,
                                                   uint32_t window_index,
                                                   uint32_t offset_samples,
                                                   const float *class_probabilities,
                                                   uint32_t record_time_ms_of_day)
{
    uint32_t center_samples = offset_samples + (uint32_t)(DATA_INPUT_USER / 2U);
    uint32_t window_ms      = (center_samples * 1000U) / AD_SAMPLE_RATE_HZ;
    uint32_t this_ms_of_day = record_time_ms_of_day + window_ms;

    char time_buf[16];
    format_time_hms_ms(this_ms_of_day, time_buf, sizeof(time_buf));

    printf("[NEAI] win=%lu \t\t time: %s \t\t offset=%lu \t\t\t class_id=%u (%s)\t\t\t\t probs=[",
           (unsigned long)window_index,
           time_buf,
           (unsigned long)offset_samples,
           (unsigned)predicted_class_id,
           audio_get_class_name_from_predicted_id(predicted_class_id));

    float highest_probability = audio_get_max_probability(class_probabilities);

    lora_sendf("Aud, %s, %.3f",
               audio_get_class_name_from_predicted_id(predicted_class_id),
               (double)highest_probability);
    HAL_Delay(200);

    for (uint32_t class_index = 0U; class_index < CLASS_NUMBER; class_index++) {
        printf("%.3f", (double)class_probabilities[class_index]);
        if (class_index + 1U < CLASS_NUMBER) {
            printf(", ");
        }
    }
    printf("]\r\n");
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void audio_detection_init(void)
{
    enum neai_state initialization_status = neai_classification_init();
    if (initialization_status != NEAI_OK) {
        printf("[NEAI] Audio detection initialize - error: %d\r\n", (int)initialization_status);
    } else {
        audio_neai_resolve_class_ids();
        printf("[NEAI] Audio detection initialize - OK\r\n");
    }
}

neai_det_status_t audio_detection_run_from_pcm(const int16_t *pcm,
                                               uint32_t pcm_samples,
                                               uint16_t *out_class_id)
{
    if (pcm == NULL || pcm_samples < DATA_INPUT_USER) {
        return NEAI_DET_ERR_ARGS;
    }

    for (uint32_t i = 0; i < DATA_INPUT_USER; i++) {
        float v = 0.0f;
        if (i < pcm_samples) {
            v = (float)pcm[i];
        }
        audio_neai_input_buffer[i] = v;
    }

    int neai_id0 = 0;
    enum neai_state err = neai_classification(audio_neai_input_buffer,
    		audio_neai_output_probabilities,
                                              &neai_id0);
    if (err != NEAI_OK) {
        printf("[NEAI] ERR: classification failed, err=%d\r\n", (int)err);
        return NEAI_DET_ERR_RUNTIME;
    }

    uint16_t id_class_1b = 0U;
    if (neai_id0 == g_audio_neai_leak_class_id) {
        id_class_1b = 1U;
    } else if (neai_id0 == g_audio_neai_background_class_id) {
        id_class_1b = 2U;
    } else {
        id_class_1b = 0U;
    }

    if (out_class_id != NULL) {
        *out_class_id = id_class_1b;
    }

    audio_detection_print_window_with_time(id_class_1b,
                                           0U,
                                           0U,
										   audio_neai_output_probabilities,
                                           0U);

    return NEAI_DET_OK;
}

neai_det_status_t audio_detection_run_sliding_from_pcm(const int16_t *pcm,
                                                       uint32_t pcm_samples,
                                                       uint16_t *predicted_class_id,
                                                       uint32_t record_time_ms_of_day,
                                                       const char *record_date_str,
                                                       det_3s_result_t *out_res)
{
    if (out_res) {
        memset(out_res, 0, sizeof(*out_res));
    }

    if (pcm == NULL || pcm_samples < DATA_INPUT_USER) {
        return NEAI_DET_ERR_ARGS;
    }

    neai_det_status_t overall_status = NEAI_DET_OK;

    char start_time_buf[16];
    format_time_hms_ms(record_time_ms_of_day, start_time_buf, sizeof(start_time_buf));
    if (record_date_str && record_date_str[0] != '\0') {
        printf("[NEAI] %s    %s\r\n", record_date_str, start_time_buf);
    } else {
        printf("[NEAI] N/A    %s\r\n", start_time_buf);
    }

    uint32_t max_possible_windows = 1U;
    if (pcm_samples > DATA_INPUT_USER) {
        max_possible_windows = (pcm_samples - DATA_INPUT_USER) + 1U;
    }

    uint32_t num_windows = g_neai_num_windows;
    if (num_windows < 1U) {
        num_windows = 1U;
    }
    if (num_windows > max_possible_windows) {
        num_windows = max_possible_windows;
    }

    uint32_t hop = 0U;
    if (num_windows > 1U) {
        hop = (pcm_samples - DATA_INPUT_USER) / (num_windows - 1U);
    }

    uint32_t total_ms = (pcm_samples * 1000U + (AD_SAMPLE_RATE_HZ - 1U)) / AD_SAMPLE_RATE_HZ;

    uint32_t slot_count = (total_ms + (AUDIO_SLOT_DURATION_MS - 1U)) / AUDIO_SLOT_DURATION_MS;
    if (slot_count == 0U) {
        slot_count = 1U;
    }
    if (slot_count > AUDIO_MAX_SLOT_COUNT) {
        slot_count = AUDIO_MAX_SLOT_COUNT;
    }

    float    slot_leak_sum[AUDIO_MAX_SLOT_COUNT]       = {0.0f};
    float    slot_bg_sum[AUDIO_MAX_SLOT_COUNT]         = {0.0f};
    uint32_t slot_counts[AUDIO_MAX_SLOT_COUNT]         = {0U};
    uint32_t slot_strong_leak_window_count[AUDIO_MAX_SLOT_COUNT] = {0U};

    float    sum_leak_confidence        = 0.0f;
    float    sum_background_confidence          = 0.0f;
    uint32_t window_count         = 0U;
    uint32_t total_strong_leak_win = 0U;
    uint32_t total_leak_window_count    = 0U;

    for (uint32_t w = 0U; w < num_windows; w++) {
        uint32_t offset = w * hop;
        if (offset + DATA_INPUT_USER > pcm_samples) {
            offset = pcm_samples - DATA_INPUT_USER;
        }

        for (uint32_t i = 0U; i < DATA_INPUT_USER; i++) {
        	audio_neai_input_buffer[i] = (float)pcm[offset + i];
        }

        int neai_id0 = 0;
        enum neai_state err = neai_classification(audio_neai_input_buffer,
        		audio_neai_output_probabilities,
                                                  &neai_id0);
        if (err != NEAI_OK) {
            printf("[NEAI] ERR(slide): classification failed on window %lu, err=%d\r\n",
                   (unsigned long)w, (int)err);
            overall_status = NEAI_DET_ERR_RUNTIME;
            continue;
        }

        uint16_t id_class_1b = 0U;
        if (neai_id0 == g_audio_neai_leak_class_id) {
            id_class_1b = 1U;
        } else if (neai_id0 == g_audio_neai_background_class_id) {
            id_class_1b = 2U;
        } else {
            id_class_1b = 0U;
        }

        uint8_t  id_valid    = (id_class_1b >= 1U && id_class_1b <= (uint16_t)CLASS_NUMBER);
        uint8_t  is_leak_id  = (neai_id0 == g_audio_neai_leak_class_id) ? 1U : 0U;

        audio_detection_print_window_with_time(id_class_1b,
                                               w,
                                               offset,
											   audio_neai_output_probabilities,
                                               record_time_ms_of_day);

        if (g_neai_skip_first_window && (w == 0U)) {
            continue;
        }

        int leak_idx = (g_audio_neai_leak_class_id >= 0 && g_audio_neai_leak_class_id < (int)CLASS_NUMBER) ? g_audio_neai_leak_class_id : 0;
        float p_leak = audio_neai_output_probabilities[leak_idx];

        float p_bg = 0.0f;
        if (CLASS_NUMBER > 1) {
            int bg_idx = (g_audio_neai_background_class_id >= 0 && g_audio_neai_background_class_id < (int)CLASS_NUMBER) ? g_audio_neai_background_class_id : 1;
            p_bg = audio_neai_output_probabilities[bg_idx];
        } else {
            p_bg = 1.0f - p_leak;
        }

        uint32_t center_samples = offset + (uint32_t)(DATA_INPUT_USER / 2U);
        uint32_t window_ms      = (center_samples * 1000U) / AD_SAMPLE_RATE_HZ;

        uint32_t slot_idx = window_ms / AUDIO_SLOT_DURATION_MS;
        if (slot_idx >= slot_count) {
            slot_idx = slot_count - 1U;
        }

        window_count++;
        sum_leak_confidence += p_leak;
        sum_background_confidence   += p_bg;

        if (id_valid && is_leak_id) {
            total_leak_window_count++;
        }

        if (slot_idx < AUDIO_MAX_SLOT_COUNT) {
            slot_leak_sum[slot_idx] += p_leak;
            slot_bg_sum[slot_idx]   += p_bg;
            slot_counts[slot_idx]++;
        }

        if (p_leak >= AUDIO_STRONG_LEAK_CONFIDENCE_THRESHOLD) {
            total_strong_leak_win++;
            if (slot_idx < AUDIO_MAX_SLOT_COUNT) {
                slot_strong_leak_window_count[slot_idx]++;
            }
        }
    }

    float average_leak_confidence = 0.0f;
    float average_background_confidence   = 0.0f;
    if (window_count > 0U) {
        average_leak_confidence = sum_leak_confidence / (float)window_count;
        average_background_confidence   = sum_background_confidence   / (float)window_count;
    }

    uint32_t leaky_slot_count = 0U;
    for (uint32_t s = 0U; s < slot_count; s++) {
        if (slot_counts[s] == 0U) {
            continue;
        }
        float slot_average_leak_confidence = slot_leak_sum[s] / (float)slot_counts[s];
        if (slot_average_leak_confidence >= AUDIO_SLOT_LEAK_CONFIDENCE_THRESHOLD) {
            leaky_slot_count++;
        }
    }

    uint32_t logic1_leaky_slot_count_thresh = (uint32_t)((float)slot_count * AUDIO_SLOT_LEAK_RATIO_THRESHOLD);
    uint8_t slot_average_logic_triggered = (leaky_slot_count > logic1_leaky_slot_count_thresh) ? 1U : 0U;

    float strong_leak_fraction = 0.0f;
    if (window_count > 0U) {
        strong_leak_fraction = (float)total_strong_leak_win / (float)window_count;
    }
    uint8_t strong_fraction_logic_triggered = (strong_leak_fraction >= AUDIO_STRONG_LEAK_FRACTION_THRESHOLD) ? 1U : 0U;

    uint8_t predicted_is_leak = (slot_average_logic_triggered || strong_fraction_logic_triggered) ? 1U : 0U;
    uint16_t final_id     = predicted_is_leak ? 1U : 2U;

    if (predicted_class_id != NULL) {
        *predicted_class_id = final_id;
    }

    char logic_str[64];
    if (slot_average_logic_triggered && strong_fraction_logic_triggered) {
        snprintf(logic_str, sizeof(logic_str), "1 + 2 (SlotAvg + StrongFrac)");
    } else if (slot_average_logic_triggered) {
        snprintf(logic_str, sizeof(logic_str), "1 (SlotAvg)");
    } else if (strong_fraction_logic_triggered) {
        snprintf(logic_str, sizeof(logic_str), "2 (StrongFrac)");
    } else {
        snprintf(logic_str, sizeof(logic_str), "none");
    }

    const char *verdict_str = predicted_is_leak ? "leak" : "background";

    printf("[NEAI] Summary: 3s acoustic chunk verdict = %s  decision logic = %s \r\n"
           "[NEAI] Leak windows = %lu/%lu  "
           "Leak slots = %lu/%lu  "
           "Leak slots (high confidence) = %lu/%lu\r\n"
           "[NEAI] Leak Average = %.3f  Background Average = %.3f\r\n",
           verdict_str,
           logic_str,
           (unsigned long)leaky_slot_count,
           (unsigned long)slot_count,
           (unsigned long)total_leak_window_count,
           (unsigned long)window_count,
           (unsigned long)total_strong_leak_win,
           (unsigned long)window_count,
           (double)average_leak_confidence,
           (double)average_background_confidence);

    // Detection for confusion matrix -----------------
    float verdict_prob = 0;
    if ((strcmp(verdict_str, "leak") == 0)){
    	verdict_prob = average_leak_confidence;
    }
    else{
    	verdict_prob = average_background_confidence;
    }

    lora_sendf("3s-Aud, %s, %.3f",verdict_str, (double)verdict_prob);
    HAL_Delay(200);
    // -----------------------------------------------

    if (out_res) {
        *out_res = (det_3s_result_t){
            .predicted_class_id      = final_id,
            .predicted_is_leak       = predicted_is_leak,
            .slot_average_logic_triggered      = slot_average_logic_triggered,
            .strong_fraction_logic_triggered      = strong_fraction_logic_triggered,
            .slot_count           = slot_count,
            .leaky_slot_count         = leaky_slot_count,
            .window_count       = window_count,
            .leak_window_count        = total_leak_window_count,
            .strong_leak_window_count = total_strong_leak_win,
            .average_leak_confidence            = average_leak_confidence,
            .average_background_confidence              = average_background_confidence,
            .strong_leak_fraction         = strong_leak_fraction,
            .sum_leak_confidence      = sum_leak_confidence,
            .sum_background_confidence        = sum_background_confidence,
        };
    }

    return overall_status;
}
