#include "servo.h"
#include "config.h"

/* Map 0–180° linearly to CCR1 1000–2000 µs */
static uint32_t angle_to_ccr(uint8_t deg)
{
    if (deg > 180U) deg = 180U;
    return SERVO_CCR_0DEG + ((uint32_t)deg * (SERVO_CCR_180DEG - SERVO_CCR_0DEG)) / 180U;
}

void Servo_Init(TIM_HandleTypeDef *htim)
{
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    Servo_SetAngle(htim, 0);
}

void Servo_SetAngle(TIM_HandleTypeDef *htim, uint8_t degrees)
{
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, angle_to_ccr(degrees));
}

/* Instant position jump — use in state transitions, not animation */
void Servo_Sweep(TIM_HandleTypeDef *htim, uint8_t from_deg, uint8_t to_deg)
{
    if (from_deg == to_deg) return;
    int8_t step = (to_deg > from_deg) ? 1 : -1;
    int16_t pos = from_deg;
    while (pos != (int16_t)to_deg) {
        pos += step;
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1,
                              angle_to_ccr((uint8_t)pos));
        /* Pace the sweep — HAL_Delay only safe to use outside main-loop context */
        HAL_Delay(SERVO_SWEEP_PERIOD_MS);
    }
}
