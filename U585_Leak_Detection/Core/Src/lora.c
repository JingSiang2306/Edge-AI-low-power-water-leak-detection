#include "lora.h"
#include "detection_logic.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------------- Configuration ---------------- */
#define LORA_RX_CHUNK_SZ   128
#define LORA_LINE_MAX      320
#define LORA_INTERLINE_DELAY_MS 200U

/* ---------------- State ---------------- */
static UART_HandleTypeDef *s_huart = NULL;

static uint8_t  s_rx_chunk[LORA_RX_CHUNK_SZ];
static char     s_line_buf[LORA_LINE_MAX];
static size_t   s_line_len = 0;

static volatile bool s_line_ready = false;
static char     s_last_line[LORA_LINE_MAX];

/* Simple queue (single-slot). */
static void store_line(const char *line)
{
    strncpy(s_last_line, line, sizeof(s_last_line) - 1);
    s_last_line[sizeof(s_last_line) - 1] = '\0';
    s_line_ready = true;
}

/* Restart Receive-to-Idle */
static void rx_start(void)
{
    if (!s_huart) return;
    (void)HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_chunk, sizeof(s_rx_chunk));
}

/* Called by HAL when new bytes arrive (ReceiveToIdle) */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (!s_huart || huart != s_huart) return;

    for (uint16_t i = 0; i < Size; i++) {
        char c = (char)s_rx_chunk[i];

        if (c == '\r') continue;

        if (c == '\n') {
            if (s_line_len > 0) {
                s_line_buf[s_line_len] = '\0';
                store_line(s_line_buf);
                s_line_len = 0;
            }
            continue;
        }

        if (s_line_len < (sizeof(s_line_buf) - 1)) {
            s_line_buf[s_line_len++] = c;
        } else {
            /* overflow → reset line */
            s_line_len = 0;
        }
    }

    rx_start();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (!s_huart || huart != s_huart) return;
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    rx_start();
}

/* ---------------- Public API ---------------- */

void lora_initialize(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_line_len = 0;
    s_line_ready = false;
    memset(s_last_line, 0, sizeof(s_last_line));
    rx_start();
}

void lora_task(void)
{
    /* optional */
}

bool lora_pop_line(char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return false;
    if (!s_line_ready) return false;

    s_line_ready = false;
    strncpy(out, s_last_line, out_sz - 1);
    out[out_sz - 1] = '\0';
    return true;
}

/* Send raw UART string */
static bool uart_send_str(const char *s)
{
    if (!s_huart || !s) return false;
    return (HAL_UART_Transmit(s_huart, (uint8_t*)s, (uint16_t)strlen(s), 200) == HAL_OK);
}

/* Send one line to NODE (ends with '\n'). NODE firmware parses "SEND:" and "PING:" */
static bool lora_send_raw_line(const char *line)
{
    if (!line) return false;

    char buf[320];
    int n = snprintf(buf, sizeof(buf), "%s\n", line);
    if (n <= 0 || n >= (int)sizeof(buf)) return false;

    return uart_send_str(buf);
}

bool lora_send_text(const char *payload)
{
    if (!payload) return false;

    char line[320];
    int n = snprintf(line, sizeof(line), "SEND:%s", payload);
    if (n <= 0 || n >= (int)sizeof(line)) return false;

    return lora_send_raw_line(line);
}

bool lora_sendf(const char *fmt, ...)
{
    if (!fmt) return false;

    char payload[LORA_PAYLOAD_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(payload, sizeof(payload), fmt, ap);
    va_end(ap);

    if (n < 0 || n >= (int)sizeof(payload)) return false;
    return lora_send_text(payload);
}

/* Handshake:
   STM32 sends:  PING:<nonce>\n
   NODE prints:  TX:PING:<nonce>
   BASE replies: PONG:<nonce>
   NODE prints:  RX:PONG:<nonce>
*/
bool lora_handshake_ping(uint32_t timeout_ms)
{
    uint32_t nonce = (HAL_GetTick() ^ 0xA5A5U) & 0xFFFFU;

    char ping[64];
    snprintf(ping, sizeof(ping), "PING:%lu", (unsigned long)nonce);

    if (!lora_send_raw_line(ping)) return false;

    char expect[80];
    snprintf(expect, sizeof(expect), "RX:PONG:%lu", (unsigned long)nonce);

    uint32_t t0 = HAL_GetTick();
    char line[LORA_LINE_MAX];

    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (lora_pop_line(line, sizeof(line))) {
            /* strict match */
            if (strstr(line, expect) != NULL) {
                return true;
            }
        }
    }
    return false;
}

/* Pack + send detection payload */
bool lora_send_detection_payload(void)
{
    /* Uses g_lora_verdict filled by detection_logic_run_30s_latefusion(). */
    const lora_verdict *v = &g_lora_verdict;

    uint32_t chunk_ms   = v->chunk_duration_ms;
    uint32_t num_chunks = v->chunk_count;

    if (num_chunks == 0U) num_chunks = DETECTION_30S_CHUNK_COUNT;
    if (num_chunks > DETECTION_30S_CHUNK_COUNT) num_chunks = DETECTION_30S_CHUNK_COUNT;

    uint32_t chunk_s = (chunk_ms > 0U) ? (chunk_ms / 1000U) : 0U;
    uint32_t total_s = (chunk_ms > 0U) ? ((chunk_ms * num_chunks) / 1000U) : 0U;

    /* ---------------- Line 1: start date/time + recording info ---------------- */
    if (!lora_sendf("START: %s %s - Detect %lus (%lu*%lus chunks)",
                    v->record_date_string,
                    v->start_time_string,
                    (unsigned long)total_s,
                    (unsigned long)num_chunks,
                    (unsigned long)chunk_s)) {
        return false;
    }

    char payload[LORA_PAYLOAD_MAX];
    HAL_Delay(LORA_INTERLINE_DELAY_MS);

    /* ---------------- Line 2: acoustic per-chunk avg leak -------------------- */
    size_t off = 0U;
    int n = snprintf(payload + off, sizeof(payload) - off, "AUD_chunkAvgLeak: ");
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(payload) - off) {
        payload[sizeof(payload) - 1U] = '\0';
    } else {
        off += (size_t)n;
        for (uint32_t i = 0U; i < num_chunks; i++) {
            n = snprintf(payload + off, sizeof(payload) - off,
                         "%s%.3f", (i > 0U) ? ", " : "",
                         (double)v->audio_chunk_average_leak_confidence[i]);
            if (n < 0) n = 0;
            if ((size_t)n >= sizeof(payload) - off) {
                payload[sizeof(payload) - 1U] = '\0';
                break;
            }
            off += (size_t)n;
        }
    }
    if (!lora_send_text(payload)) return false;
    HAL_Delay(LORA_INTERLINE_DELAY_MS);

    /* ---------------- Line 3: vibration per-chunk avg leak ------------------- */
    off = 0U;
    n = snprintf(payload + off, sizeof(payload) - off, "VIB_chunkAvgLeak: ");
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(payload) - off) {
        payload[sizeof(payload) - 1U] = '\0';
    } else {
        off += (size_t)n;
        for (uint32_t i = 0U; i < num_chunks; i++) {
            n = snprintf(payload + off, sizeof(payload) - off,
                         "%s%.3f", (i > 0U) ? ", " : "",
                         (double)v->vibration_chunk_average_leak_confidence[i]);
            if (n < 0) n = 0;
            if ((size_t)n >= sizeof(payload) - off) {
                payload[sizeof(payload) - 1U] = '\0';
                break;
            }
            off += (size_t)n;
        }
    }
    if (!lora_send_text(payload)) return false;
    HAL_Delay(LORA_INTERLINE_DELAY_MS);

    /* ---------------- Line 4: fuse score + verdict + confidence -------------- */
    float conf = v->final_prediction_is_leak
               ? v->average_leak_confidence_all
               : v->average_background_confidence_all;

    if (!lora_sendf("Fuse Score: %.3f, Verdict: %s (%.3f)",
                    (double)v->final_fusion_score,
                    v->final_prediction_is_leak ? "Leak" : "Background",
                    (double)conf)) {
        return false;
    }
    HAL_Delay(LORA_INTERLINE_DELAY_MS);

    /* ---------------- Line 5: end time -------------------------------------- */
    if (!lora_sendf("END: %s", v->end_time_string)) {
        return false;
    }
    HAL_Delay(LORA_INTERLINE_DELAY_MS);

    return true;
}
