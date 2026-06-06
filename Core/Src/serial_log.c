#include "serial_log.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *s_huart;

static const char *state_name(HealthState_t s)
{
    switch (s) {
        case STATE_NOMINAL:        return "NOMINAL";
        case STATE_VIBRATION_WARN: return "VIBRATION_WARN";
        case STATE_THERMAL_WARN:   return "THERMAL_WARN";
        case STATE_SAFETY_BREACH:  return "SAFETY_BREACH";
        case STATE_CRITICAL:       return "CRITICAL";
        default:                   return "UNKNOWN";
    }
}

void SerialLog_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

void SerialLog_PrintStatus(const HealthData_t *d)
{
    char buf[200];
    int len = snprintf(buf, sizeof(buf),
        "[%lu] %s | TEMP:%4.1fC | DIST:%4lumm | "
        "AX:%5.2f AY:%5.2f AZ:%5.2f | "
        "GX:%6.1f GY:%6.1f GZ:%6.1f | "
        "VIB_RMS:%5.3f | EVENT:%s\r\n",
        (unsigned long)HAL_GetTick(),
        state_name(d->state),
        (double)d->temp_c,
        (unsigned long)d->dist_mm,
        (double)d->ax, (double)d->ay, (double)d->az,
        (double)d->gx, (double)d->gy, (double)d->gz,
        (double)d->vib_rms,
        (d->state == STATE_SAFETY_BREACH) ? "GUARD_BREACH" :
        (d->state == STATE_CRITICAL)      ? "CRITICAL_ALARM" : "NONE");

    if (len > 0 && s_huart) {
        HAL_UART_Transmit(s_huart, (uint8_t *)buf,
                          (uint16_t)len, LOG_UART_TIMEOUT_MS);
    }
}

void SerialLog_Print(const char *msg)
{
    if (!s_huart || !msg) return;
    HAL_UART_Transmit(s_huart, (uint8_t *)msg,
                      (uint16_t)strlen(msg), LOG_UART_TIMEOUT_MS);
}
