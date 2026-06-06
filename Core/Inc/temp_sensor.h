#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "stm32g4xx_hal.h"

/* Returns temperature in °C, or -999.0f on error */
float Temp_ReadCelsius(TIM_HandleTypeDef *htim);

#endif /* TEMP_SENSOR_H */
