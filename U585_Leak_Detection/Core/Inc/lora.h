#pragma once

#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_PAYLOAD_MAX 240

typedef struct {
    char payload[256];     // null-terminated
} lora_rx_t;

void  lora_initialize(UART_HandleTypeDef *huart);
void  lora_task(void);

/* Basic UART/LoRa bridge helpers (to RA-08H NODE pingpong firmware) */
bool  lora_send_text(const char *payload);          // sends: "SEND:<payload>\n"
bool  lora_sendf(const char *fmt, ...);             // formats payload then SEND:<payload>

/* Receive handling */
bool  lora_pop_line(char *out, size_t out_sz);      // raw line from NODE (without CR/LF)

/* Handshake: STM32 sends PING:<nonce>, expects RX:PONG:<nonce> */
bool  lora_handshake_ping(uint32_t timeout_ms);

/* NEW: Build + send a standard detection payload */
bool  lora_send_detection_payload(void);

#ifdef __cplusplus
}
#endif
