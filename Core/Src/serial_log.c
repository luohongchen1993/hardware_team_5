/* serial_log.c — UART telemetry over USART2 (ST-Link VCP), 115200 8N1.
 * snprintf into a stack buffer, then a single blocking HAL_UART_Transmit. */

#include "serial_log.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *s_huart;

static const char *state_name(GameState st)
{
    switch (st) {
        case STATE_INIT:      return "INIT";
        case STATE_CALIBRATE: return "CALIBRATE";
        case STATE_SEEKING:   return "SEEKING";
        case STATE_WIN:       return "WIN";
        case STATE_HALT:      return "HALT";
        case STATE_ERROR:     return "ERROR";
        default:              return "?";
    }
}

static const char *cue_name(AudioCue c)
{
    switch (c) {
        case AUDIO_CUE_FAR:       return "FAR";
        case AUDIO_CUE_MID:       return "MID";
        case AUDIO_CUE_NEAR:      return "NEAR";
        case AUDIO_CUE_VERY_NEAR: return "VERY_NEAR";
        case AUDIO_CUE_WIN:       return "WIN";
        default:                  return "?";
    }
}

void SerialLog_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

void SerialLog_Print(const char *msg)
{
    if (!s_huart || !msg) return;
    HAL_UART_Transmit(s_huart, (uint8_t *)msg, (uint16_t)strlen(msg), LOG_UART_TIMEOUT_MS);
}

void SerialLog_PrintNav(const NavState *n, GameState state, AudioCue cue)
{
    if (!s_huart || !n) return;

    char buf[160];
    int len = snprintf(buf, sizeof buf,
        "[%lu] %-9s | pos(%6.2f,%6.2f) | hdg %6.1f | dist %6.2f m | "
        "brg %6.1f rel %6.1f | %s\r\n",
        (unsigned long)HAL_GetTick(),
        state_name(state),
        (double)n->pos.x, (double)n->pos.y,
        (double)n->heading_deg,
        (double)n->distance_m,
        (double)n->bearing_deg,
        (double)(n->bearing_deg - n->heading_deg),
        cue_name(cue));

    if (len > 0) {
        HAL_UART_Transmit(s_huart, (uint8_t *)buf, (uint16_t)len, LOG_UART_TIMEOUT_MS);
    }
}
