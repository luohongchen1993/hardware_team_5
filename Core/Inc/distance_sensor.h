#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* Returns distance in mm, or UINT32_MAX on timeout/error */
uint32_t Distance_ReadMM(TIM_HandleTypeDef *htim);

#endif /* DISTANCE_SENSOR_H */
