/*
 * vibration_processing.h
 *
 *  Created on: Nov 2, 2025
 *      Author: FBBC
 */

#ifndef INC_VIBRATION_PROCESSING_H_
#define INC_VIBRATION_PROCESSING_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- Config ----------
#define VIB_TARGET_HZ        6600U     // effective vibration sample rate
#define VIB_MAX_SECONDS      15U        // Maximum 15 second only
#define VIB_MAX_SAMPLES      (VIB_TARGET_HZ * VIB_MAX_SECONDS)  // = 99000

// ---------- State/Buffers ----------
extern volatile uint8_t  vib_running;
extern volatile uint8_t  vib_done;

extern int16_t vib_raw_z[VIB_MAX_SAMPLES];

extern uint32_t vib_count;     // number of valid samples captured
extern uint32_t vib_fs_hz;     // actual sampling rate used (Hz)
extern uint32_t vib_sample_period_us;

/* Timebase helpers for vibration logging (must match definitions in .c) */
void vib_timebase_init(void);
uint32_t vib_micros(void);

/* Non-blocking vibration logger helpers for combined audio+vib recording */
void vib_log_start(uint32_t duration_ms);
void vib_log_sample_if_due(void);
void vib_log_finish(void);
int vib_log_print_z(void);
static inline const int16_t* vib_get_z_buffer(void) { return vib_raw_z; }

// ---------- API ----------
/** Capture accelerometer for x_ms milliseconds (non-interrupt, simple timing).
 *  Returns 0 on success, <0 on error. Sets vib_running during capture.
 */
int vibrate_record_xs(uint32_t x_ms);
int vibrate_record_xs_print(uint32_t x_ms);

#ifdef __cplusplus
}
#endif

#endif /* INC_VIBRATION_PROCESSING_H_ */
