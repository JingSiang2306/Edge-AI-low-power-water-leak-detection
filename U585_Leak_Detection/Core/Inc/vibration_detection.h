#ifndef VIBRATION_DETECTION_H
#define VIBRATION_DETECTION_H

#include <stdint.h>
#include "audio_detection.h"   /* for neai_det_status_t */
#include "detection_logic.h"   /* for det_3s_result_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Set to 1 once you have:
 *  - Inc/NEAI_Vibration/NanoEdgeAI.h
 *  - Inc/NEAI_Vibration/knowledge_vibration.h
 *  - Drivers/NanoEdgeAI_Vibration/lib/libneai_vibration_prefixed.a linked
 *
 * Keeping it 0 lets you compile Step 1 even before the vibration model is ready.
 */
#ifndef VIB_NEAI_ENABLE
#define VIB_NEAI_ENABLE 1
#endif

/* Optional tuning (mirrors audio side) */
extern uint32_t g_vneai_num_windows;          /* number of sliding windows (default 30) */
extern uint8_t  g_vneai_skip_first_window;    /* if 1, skip win 0 from fusion */

void vibration_detection_init(void);
uint8_t vibration_detection_is_enabled(void);

/*
 * Sliding-window with the same SlotAvg + StrongLeakFrac logic as audio.
 *
 * z_samples: pointer to Z-axis samples (int16)
 * num_samples: sample count (e.g. ~19800 for ~3 s)
 * sample_rate_hz: effective fs for this chunk (use vib_fs_hz)
 *
 * predicted_class_id: 1=Leak, 2=Background
 * out_res: filled with detailed stats (may be NULL)
 */
neai_det_status_t vibration_detection_run_sliding_from_z(const int16_t *z_samples,
                                                        uint32_t num_samples,
                                                        uint32_t sample_rate_hz,
                                                        uint16_t *predicted_class_id,
                                                        uint32_t record_time_ms_of_day,
                                                        const char *record_date_str,
                                                        det_3s_result_t *out_res);

#ifdef __cplusplus
}
#endif

#endif /* VIBRATION_DETECTION_H */
