#ifndef SERVO_H
#define SERVO_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

void Servo_Init(TIM_HandleTypeDef *htim);
void Servo_SetAngle(TIM_HandleTypeDef *htim, uint8_t degrees);
void Servo_Sweep(TIM_HandleTypeDef *htim, uint8_t from_deg, uint8_t to_deg);

#endif /* SERVO_H */
