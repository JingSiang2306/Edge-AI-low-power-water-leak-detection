#ifndef DETECTION_LOGIC_H
#define DETECTION_LOGIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* 30 s settings                                                              */
/* -------------------------------------------------------------------------- */
#ifndef DETECTION_30S_CHUNK_COUNT
#define DETECTION_30S_CHUNK_COUNT 10U   /* 10 x 3 s = 30 s */
#endif

/* -------------------------------------------------------------------------- */
/* Shared result struct for one 3 s chunk                                     */
/* -------------------------------------------------------------------------- */
/*
 * Convention:
 *  - predicted_class_id: 1 = Leak, 2 = Background, 0 = Unknown
 */
typedef struct {
    uint16_t predicted_class_id;               /* 1=Leak, 2=Background, 0=Unknown */
    uint8_t  predicted_is_leak;                /* 1 if leak, else 0 */
    uint8_t  slot_average_logic_triggered;     /* SlotAvg triggered */
    uint8_t  strong_fraction_logic_triggered;  /* StrongLeakFrac triggered */

    uint32_t slot_count;                       /* e.g. 6 for 3 s with 0.5 s slots */
    uint32_t leaky_slot_count;                 /* slots with avg p(leak) >= threshold */

    uint32_t window_count;                     /* NEAI windows used */
    uint32_t leak_window_count;                /* windows where class_id==Leak */
    uint32_t strong_leak_window_count;         /* windows where p(leak)>=strong threshold */

    float    average_leak_confidence;          /* avg p(leak) across window_count */
    float    average_background_confidence;    /* avg p(bg)   across window_count */
    float    strong_leak_fraction;             /* strong_leak_window_count / window_count */

    /* For accumulation across multiple chunks (30 s etc.) */
    float    sum_leak_confidence;              /* sum of p(leak) across window_count */
    float    sum_background_confidence;        /* sum of p(bg)   across window_count */
} det_3s_result_t;

/* -------------------------------------------------------------------------- */
/* LoRa payload / shared verdict struct                                       */
/* -------------------------------------------------------------------------- */
typedef struct {
    /* Start date/time of the whole 30 s recording */
    char     record_date_string[16];
    char     start_time_string[16];
    char     end_time_string[16];

    /* Capture configuration */
    uint32_t chunk_duration_ms;
    uint32_t chunk_count;

    /* Overall averages across ALL windows in the 30 s recording (both sensors) */
    float    average_leak_confidence_all;
    float    average_background_confidence_all;

    /* Fusion outputs */
    float    final_fusion_score;
    uint8_t  final_prediction_is_leak;

    /* Per-chunk average leak confidence */
    float    audio_chunk_average_leak_confidence[DETECTION_30S_CHUNK_COUNT];
    float    vibration_chunk_average_leak_confidence[DETECTION_30S_CHUNK_COUNT];
} lora_verdict;

/* Filled by detection_logic_run_30s_latefusion() */
extern lora_verdict g_lora_verdict;

/* Public entrypoint: run one 30 s (10 x 3 s) late-fusion detection pass */
int detection_logic_run_30s_latefusion(uint32_t chunk_duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* DETECTION_LOGIC_H */
