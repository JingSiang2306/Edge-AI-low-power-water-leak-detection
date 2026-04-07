/*
 * audio_processing.h
 *
 *  Created on: Nov 2, 2025
 *      Author: FBBC
 */

#ifndef INC_AUDIO_PROCESSING_H_
#define INC_AUDIO_PROCESSING_H_

#include "main.h"

// Flags your audio code should set/clear
extern volatile uint8_t audio_running;
extern volatile uint8_t audio_done;

// ===== User-configurable audio profile =====
#define AP_AUDIO_INSTANCE          0U
#define AP_SAMPLE_RATE_HZ          16000U
#define AP_BITS_PER_SAMPLE         AUDIO_RESOLUTION_16B
#define AP_CHANNELS                1U   // we run MIC1 only
#define AP_DEVICE                  AUDIO_IN_DEVICE_DIGITAL_MIC1

// DMA frame size (half-buffer) and the working DMA buffer
#define AP_FRAME_SAMPLES           512U
#define AP_DMA_SAMPLES             (AP_FRAME_SAMPLES * 2U)

// Max capture length in milliseconds (fits SRAM)
#define AP_MAX_RECORD_MS           10000U   // 1s @ 16k mono = 32 KB

// Derived
#define AP_MAX_SAMPLES             ((AP_SAMPLE_RATE_HZ * AP_MAX_RECORD_MS) / 1000U)

// Public API
int  audio_init(void);                       // init mic (no start)
int  audio_record_xs(uint32_t duration_ms);  // capture to RAM buffer
int  audio_wav(void);                        // build WAV in RAM (header + PCM)
int  audio_csv(void);                        // build CSV in RAM (NEAI-friendly)
int  audio_output(void);                     // stream WAV (b64) + CSV over UART
int  audio_record_now(uint32_t duration_ms); // 1→4 pipeline
int audio_record_chunk_with_vib(uint32_t duration_ms);

// Expose last capture stats
extern volatile uint32_t g_ap_rec_samples;   // number of valid PCM samples captured
extern volatile uint8_t  g_ap_has_wav;       // 1 if WAV buffer is ready
extern volatile uint8_t  g_ap_has_csv;       // 1 if CSV buffer is ready
const int16_t* audio_get_capture_buffer(void);

#endif /* INC_AUDIO_PROCESSING_H_ */
