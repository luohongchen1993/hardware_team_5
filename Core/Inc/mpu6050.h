#ifndef MPU6050_H
#define MPU6050_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* Raw two's-complement register output (spec: int16_t). */
typedef struct {
    int16_t ax, ay, az;   /* accelerometer */
    int16_t temp;         /* on-die temperature (read, unused) */
    int16_t gx, gy, gz;   /* gyroscope */
} MPU6050_Raw;

/* Scaled SI output after sensitivity scaling + bias removal. */
typedef struct {
    float ax, ay, az;     /* m/s^2 */
    float gx, gy, gz;     /* deg/s */
} MPU6050_Scaled;

/* Gyro zero-rate bias (deg/s) measured at rest during calibration. */
typedef struct {
    float gx, gy, gz;
} MPU6050_Bias;

/* Initialize the device: verify WHO_AM_I, wake, set DLPF and ranges.
 * Returns HAL_OK on success, HAL_ERROR/HAL_TIMEOUT on I2C fault. */
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);

/* Burst-read the 14 data registers into raw int16 counts. Retries on
 * I2C error (up to I2C_RETRY_COUNT) with bus recovery between attempts. */
HAL_StatusTypeDef MPU6050_ReadRawCounts(I2C_HandleTypeDef *hi2c, MPU6050_Raw *out);

/* Read + scale to SI units; subtracts the supplied gyro bias (may be NULL). */
HAL_StatusTypeDef MPU6050_ReadScaled(I2C_HandleTypeDef *hi2c,
                                     const MPU6050_Bias *bias,
                                     MPU6050_Scaled *out);

/* Average CALIB_SAMPLES at-rest samples: fills the gyro bias and an
 * accelerometer reference (mean gravity vector) used to seed orientation.
 * The device MUST be held still during this call. */
HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c,
                                    MPU6050_Bias *bias_out,
                                    MPU6050_Scaled *accel_ref_out);

#endif /* MPU6050_H */
