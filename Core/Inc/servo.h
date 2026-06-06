#ifndef SERVO_H
#define SERVO_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* Servo on TIM3 CH1 (PA6), 50 Hz, CCR = pulse width in microseconds. */

void Servo_Init(TIM_HandleTypeDef *htim);

/* Direct pulse in microseconds, clamped to [SERVO_PULSE_MIN_US, MAX]. */
void Servo_SetPulseUs(TIM_HandleTypeDef *htim, uint16_t pulse_us);

/* Absolute angle 0..180 deg -> pulse. */
void Servo_SetAngle(TIM_HandleTypeDef *htim, uint8_t degrees);

/* Point toward a target given the bearing RELATIVE to current heading
 * (degrees, any range). Clamped to +/-SERVO_BEARING_CLAMP_DEG; centre
 * (1500 us) means "straight ahead / on target". */
void Servo_PointBearing(TIM_HandleTypeDef *htim, float relative_bearing_deg);

#endif /* SERVO_H */
