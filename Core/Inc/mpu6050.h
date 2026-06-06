#ifndef MPU6050_H
#define MPU6050_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

typedef struct {
    float ax, ay, az;   /* g */
    float gx, gy, gz;   /* °/s */
} MPU6050_Data_t;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *out);
float             MPU6050_CalcVibRMS(I2C_HandleTypeDef *hi2c);

#endif /* MPU6050_H */
