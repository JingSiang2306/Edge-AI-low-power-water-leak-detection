/*
 * audio_processing.c
 *
 *  Created on: Nov 2, 2025
 *      Author: FBBC
 */

#include "audio_processing.h"
#include "peripheral_Initialize.h"
#include "vibration_processing.h"
#include "main.h"

// ===== External UART used by printf (adjust if not USARTx in main) =====
extern UART_HandleTypeDef huart1; // or huart3 — use the one you printf through

// ===== DMA working buffer (filled by BSP) =====
static int16_t s_dma_pcm[AP_DMA_SAMPLES]; // double-buffer: [0..FRAME-1][FRAME..2*FRAME-1]

// ===== Capture buffer (final recording) =====
static int16_t s_capture[AP_MAX_SAMPLES]; // linear capture
volatile uint32_t g_ap_rec_samples = 0;

// ===== WAV assembly buffer (header + pcm). Size: 44-byte header + samples*2
// allocate max required (10s worth) + header
static uint8_t s_wav[44 + AP_MAX_SAMPLES * 2U];
static uint32_t s_wav_size = 0;
volatile uint8_t g_ap_has_wav = 0;

// ===== CSV staging — we stream CSV directly; optional small scratch =====
volatile uint8_t g_ap_has_csv = 0;

// ===== Base64 encoder for WAV streaming =====
static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void uart_write(const uint8_t *buf, size_t len) {
    // Blocking send over the same UART as printf; chunk to avoid long blocking bursts
    while (len) {
        uint16_t n = (len > 512) ? 512 : (uint16_t)len;
        (void)HAL_UART_Transmit(&huart1, (uint8_t*)buf, n, HAL_MAX_DELAY);
        buf += n; len -= n;
    }
}

static void uart_print(const char *s) {
    uart_write((const uint8_t*)s, strlen(s));
}

static void b64_write_block(const uint8_t *in, size_t n, char out[4]) {
    uint32_t v = 0;
    v |= (uint32_t)(n > 0 ? in[0] : 0) << 16;
    v |= (uint32_t)(n > 1 ? in[1] : 0) << 8;
    v |= (uint32_t)(n > 2 ? in[2] : 0);

    out[0] = b64tab[(v >> 18) & 0x3F];
    out[1] = b64tab[(v >> 12) & 0x3F];
    out[2] = (n > 1) ? b64tab[(v >> 6) & 0x3F] : '=';
    out[3] = (n > 2) ? b64tab[(v >> 0) & 0x3F] : '=';
}

static void b64_stream(const uint8_t *data, size_t len) {
    char out[4];
    size_t i = 0;
    while (i + 3 <= len) {
        b64_write_block(&data[i], 3, out);
        uart_write((uint8_t*)out, 4);
        i += 3;
    }
    if (i < len) {
        b64_write_block(&data[i], len - i, out);
        uart_write((uint8_t*)out, 4);
    }
}

// ===== BSP callbacks are in peripheral_Initialize.c =====
extern volatile uint32_t RecHalfBuffCplt;  // ++ per half
extern volatile uint32_t RecBuffCplt;      // ++ per full

// ===== init mic (no recording) =====
int audio_init(void)
{
    BSP_AUDIO_Init_t cfg = {
        .Device        = AP_DEVICE,
        .SampleRate    = AP_SAMPLE_RATE_HZ,
        .BitsPerSample = AP_BITS_PER_SAMPLE,
        .ChannelsNbr   = AP_CHANNELS,
        .Volume        = 100U,
    };
    int32_t st = BSP_AUDIO_IN_Init(AP_AUDIO_INSTANCE, &cfg);
    if (st != BSP_ERROR_NONE) {
        printf("Audio init failed, state=%ld\r\n", (long)st);
        return -1;
    }
    printf("Audio init OK: %lu Hz, %u ch, 16-bit\r\n",
           (unsigned long)AP_SAMPLE_RATE_HZ, AP_CHANNELS);
    return 0;
}

// ===== blocking record into s_capture =====
int audio_record_xs(uint32_t duration_ms)
{
    if (duration_ms == 0U || duration_ms > AP_MAX_RECORD_MS) {
        printf("MIC: Bad duration (max %lu ms)\r\n", (unsigned long)AP_MAX_RECORD_MS);
        return -1;
    }

    g_ap_rec_samples = 0;
    RecHalfBuffCplt  = 0;
    RecBuffCplt      = 0;

    // Start DMA into the working double-buffer
    int32_t st = BSP_AUDIO_IN_Record(AP_AUDIO_INSTANCE,
                                     (uint8_t*)s_dma_pcm,
                                     AP_DMA_SAMPLES * sizeof(int16_t));
    if (st != BSP_ERROR_NONE) {
        printf("MIC: Record start failed, state=%ld\r\n", (long)st);
        return -2;
    }
    // printf("Recording...\r\n");

    // ---- Dynamic target sample counts ----
    const uint32_t actual_target_samples =
        (AP_SAMPLE_RATE_HZ * duration_ms) / 1000U;   // e.g. 16000 for 1 s

    const uint32_t trim_margin   = 5U;               // samples to drop at start/end
    const uint32_t extra_samples = trim_margin * 2U; // 10 samples total

    // We try to record a bit more so we can trim ±5 around boundaries
    uint32_t desired_total = actual_target_samples + extra_samples;
    uint32_t target_samples =
        (desired_total <= AP_MAX_SAMPLES) ? desired_total : AP_MAX_SAMPLES;

    uint32_t next_half = 1U, next_full = 1U; // expect counters to become 1,2,3...

    uint32_t t0 = HAL_GetTick();
    while (g_ap_rec_samples < target_samples) {
        // copy on half-complete
        if (RecHalfBuffCplt >= next_half) {
            uint32_t n = AP_FRAME_SAMPLES;
            if ((g_ap_rec_samples + n) > target_samples) {
                n = target_samples - g_ap_rec_samples;
            }
            memcpy(&s_capture[g_ap_rec_samples], &s_dma_pcm[0],
                   n * sizeof(int16_t));
            g_ap_rec_samples += n;
            next_half++;
        }

        // copy on full-complete
        if (RecBuffCplt >= next_full) {
            uint32_t n = AP_FRAME_SAMPLES;
            if ((g_ap_rec_samples + n) > target_samples) {
                n = target_samples - g_ap_rec_samples;
            }
            memcpy(&s_capture[g_ap_rec_samples],
                   &s_dma_pcm[AP_FRAME_SAMPLES],
                   n * sizeof(int16_t));
            g_ap_rec_samples += n;
            next_full++;
        }

        // -------- drive vibration logger if active --------
        if (vib_running) {
            vib_log_sample_if_due();
        }
        // --------------------------------------------------

        // simple timeout guard
        if ((HAL_GetTick() - t0) > (duration_ms + 2000U)) {
            printf("MIC: Record timeout\r\n");
            break;
        }
    }

    /* === Boundary trimming ===
       We recorded a few extra samples and now remove ±trim_margin
       (normally 5 at start and 5 at end), keeping exactly
       actual_target_samples in the common case.
    */
    if (g_ap_rec_samples > extra_samples) {
        uint32_t keep;
        uint32_t shift = trim_margin;

        // Normal case: we got at least desired_total samples
        if (g_ap_rec_samples >= (actual_target_samples + extra_samples)) {
            keep = actual_target_samples;
        } else {
            // If we got fewer (e.g. clamped by AP_MAX_SAMPLES), keep what we can
            keep = g_ap_rec_samples - extra_samples;
        }

        // Safety: don’t read past the captured region
        if (keep + shift > g_ap_rec_samples) {
            if (g_ap_rec_samples > shift) {
                keep = g_ap_rec_samples - shift;
            } else {
                keep = 0U;
            }
        }

        for (uint32_t i = 0U; i < keep; ++i) {
            s_capture[i] = s_capture[i + shift];
        }
        g_ap_rec_samples = keep;
    }

    // Stop capture (no Pause right before Stop to avoid -4)
    st = BSP_AUDIO_IN_Stop(AP_AUDIO_INSTANCE);
    if (st != BSP_ERROR_NONE) {
        printf("MIC: Stop failed, state=%ld\r\n", (long)st);
        return -3;
    }

    // printf("Recording done: %lu samples\r\n", (unsigned long)g_ap_rec_samples);
    return 0;
}

// ===== build WAV header into s_wav (header + PCM) =====
static void put_u32le(uint8_t *p, uint32_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }
static void put_u16le(uint8_t *p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }

int audio_wav(void)
{
    if (g_ap_rec_samples == 0) return -1;

    const uint32_t bytes_per_sample = 2U; // 16-bit
    const uint32_t data_bytes = g_ap_rec_samples * bytes_per_sample;
    const uint32_t riff_size  = 36U + data_bytes;

    uint8_t *h = s_wav;
    memcpy(h+0, "RIFF", 4);
    put_u32le(h+4, riff_size);
    memcpy(h+8, "WAVE", 4);
    memcpy(h+12,"fmt ", 4);
    put_u32le(h+16, 16);                 // PCM fmt chunk size
    put_u16le(h+20, 1);                  // PCM
    put_u16le(h+22, AP_CHANNELS);
    put_u32le(h+24, AP_SAMPLE_RATE_HZ);
    put_u32le(h+28, AP_SAMPLE_RATE_HZ * AP_CHANNELS * bytes_per_sample); // byte rate
    put_u16le(h+32, AP_CHANNELS * bytes_per_sample); // block align
    put_u16le(h+34, 16);                 // bits per sample
    memcpy(h+36,"data", 4);
    put_u32le(h+40, data_bytes);

    // append PCM
    memcpy(&s_wav[44], (uint8_t*)s_capture, data_bytes);
    s_wav_size = 44U + data_bytes;
    g_ap_has_wav = 1;
    return 0;
}

// ===== create CSV (streamed during output; here we just set a flag) =====
int audio_csv(void)
{
    if (g_ap_rec_samples == 0) return -1;
    g_ap_has_csv = 1;
    return 0;
}

// ===== output: CSV (text) then WAV (base64) over UART =====
int audio_output(void)
{
    if (!g_ap_has_csv && !g_ap_has_wav) return -1;

    // --- CSV ---
    if (g_ap_has_csv) {
        char line[64];
        uart_print("\r\n---BEGIN_CSV:name=audio.csv---\r\n");
        for (uint32_t i = 0; i < g_ap_rec_samples; ++i) {
            int n = snprintf(line, sizeof(line), "%d\r\n", (int)s_capture[i]);
            uart_write((uint8_t*)line, (size_t)n);
        }
        uart_print("---END_CSV---\r\n");
    }

    // --- WAV (base64) ---
    if (g_ap_has_wav) {
        char hdr[96];
        int n = snprintf(hdr, sizeof(hdr),
                         "---BEGIN_WAV_B64:name=audio.wav;size=%lu---\r\n",
                         (unsigned long)s_wav_size);
        uart_write((uint8_t*)hdr, (size_t)n);
        b64_stream(s_wav, s_wav_size);
        uart_print("\r\n---END_WAV_B64---\r\n");
    }

    return 0;
}

// ===== NEAI =====
const int16_t* audio_get_capture_buffer(void) {
    return s_capture;
}

// ===== orchestrator =====
int audio_record_now(uint32_t duration_ms)
{
    if (audio_record_xs(duration_ms) != 0) return -1;
    if (audio_wav() != 0) return -2;
    if (audio_csv() != 0) return -3;
    if (audio_output() != 0) return -4;
    return 0;
}

int audio_record_chunk_with_vib(uint32_t duration_ms)
{
    // Start vib logger first (so vib aligns with audio start)
    vib_log_start(duration_ms);

    int rc = audio_record_xs(duration_ms);

    vib_log_finish();

    return rc;
}


