#ifndef AUDIO_DETECTION_H
#define AUDIO_DETECTION_H

#include <stdint.h>
#include "detection_logic.h"

typedef enum {
    NEAI_DET_OK          = 0,
    NEAI_DET_ERR_INIT    = -1,
    NEAI_DET_ERR_ARGS    = -2,
    NEAI_DET_ERR_RUNTIME = -3,
} neai_det_status_t;

/* Config globals (you already use these in main) */
extern uint32_t g_neai_num_windows;       /* number of sliding windows */
extern uint8_t  g_neai_skip_first_window; /* if 1, skip win 0 from fusion */

/* API */
void audio_detection_init(void);

const int16_t *audio_get_capture_buffer(void);
uint32_t audio_get_num_samples(void);

/* One-shot (mostly for debugging) */
neai_det_status_t audio_detection_run_from_pcm(const int16_t *pcm,
                                               uint32_t pcm_samples,
                                               uint16_t *out_class_id);

/* Sliding-window with temporal logic */
neai_det_status_t audio_detection_run_sliding_from_pcm(const int16_t *pcm,
                                                       uint32_t pcm_samples,
                                                       uint16_t *predicted_class_id,
                                                       uint32_t record_time_ms_of_day,
                                                       const char *record_date_str,
                                                       det_3s_result_t *out_res);

#endif /* AUDIO_DETECTION_H */
