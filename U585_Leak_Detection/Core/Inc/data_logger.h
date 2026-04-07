#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>

/**
 * 1) Sequential recording:
 *    - Audio and vibration are recorded SEQUENTIALLY.
 *    - Audio is recorded (possibly in 1s slices) and streamed as CSV.
 *    - Vibration is then recorded at 6.6 kHz using
 *      vibrate_record_xs_print(), which prints one value per line.
 *    - Output format is compatible with DL_Logger_v4.py:
 *
 *      ---BEGIN_SAMPLE---
 *      ---BEGIN_AUDIO_CSV---
 *      <audio lines>
 *      ---END_AUDIO_CSV---
 *      ---BEGIN_VIB_CSV---
 *      <upsampled vib lines>
 *      ---END_VIB_CSV---
 *      ---END_SAMPLE---
 */
int logging_now(uint32_t duration_ms);

/**
 * 2) Simultaneous multi-slice dataset logging:
 *    - Designed for dataset collection (e.g. 3 s or 30 s).
 *    - Uses vib_log_start(total_duration_ms) so that vibration is captured
 *      continuously over the entire window.
 *    - Audio is recorded in 1s slices using audio_record_xs(1000) and streamed
 *      immediately, so no huge RAM usage.
 *    - After all slices, vibrate_record_xs_print(total_duration_ms) is called
 *      to print the vibration for the whole window.
 *    - Output format is the SAME as logging_now() and works with DL_Logger_v4.py.
 */
int logging_syn_dataset(uint32_t duration_ms);

#endif /* DATA_LOGGER_H */
