/* servo.c — SG90-class servo on TIM3 CH1 (PA6).
 * The timer ticks at 1 MHz (PSC=169) so a CCR value equals the pulse
 * width in microseconds directly; 1000..2000 us spans 0..180 deg. */

#include "servo.h"
#include "config.h"

static uint16_t clamp_us(uint16_t us)
{
    if (us < SERVO_PULSE_MIN_US) return SERVO_PULSE_MIN_US;
    if (us > SERVO_PULSE_MAX_US) return SERVO_PULSE_MAX_US;
    return us;
}

void Servo_Init(TIM_HandleTypeDef *htim)
{
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    Servo_SetPulseUs(htim, SERVO_PULSE_MID_US);   /* centre on boot */
}

void Servo_SetPulseUs(TIM_HandleTypeDef *htim, uint16_t pulse_us)
{
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, clamp_us(pulse_us));
}

void Servo_SetAngle(TIM_HandleTypeDef *htim, uint8_t degrees)
{
    if (degrees > 180U) degrees = 180U;
    uint16_t us = (uint16_t)(SERVO_PULSE_MIN_US +
        ((uint32_t)degrees * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / 180U);
    Servo_SetPulseUs(htim, us);
}

void Servo_PointBearing(TIM_HandleTypeDef *htim, float relative_bearing_deg)
{
    /* Optionally invert direction for the physical mounting. */
    float r = SERVO_BEARING_SIGN * relative_bearing_deg;

    /* Clamp to the servo's usable +/- range. A target directly behind the
     * user saturates the servo to a hard left/right "turn around" cue. */
    if (r >  SERVO_BEARING_CLAMP_DEG) r =  SERVO_BEARING_CLAMP_DEG;
    if (r < -SERVO_BEARING_CLAMP_DEG) r = -SERVO_BEARING_CLAMP_DEG;

    /* Map [-clamp, +clamp] -> [MID-span, MID+span] microseconds. */
    float span = (float)(SERVO_PULSE_MAX_US - SERVO_PULSE_MID_US);
    int32_t us = (int32_t)SERVO_PULSE_MID_US +
                 (int32_t)((r / SERVO_BEARING_CLAMP_DEG) * span);
    Servo_SetPulseUs(htim, (uint16_t)us);
}
