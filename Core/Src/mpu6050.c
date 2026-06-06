#include "mpu6050.h"
#include "config.h"
#include <math.h>
#include <string.h>

/* MPU-6050 register map */
#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

#define ACCEL_SCALE_2G   16384.0f   /* LSB/g for ±2g range */
#define GYRO_SCALE_500   65.5f      /* LSB/(°/s) for ±500°/s range */

static HAL_StatusTypeDef write_reg(I2C_HandleTypeDef *hi2c,
                                   uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(hi2c, MPU6050_I2C_ADDR,
                                   buf, 2, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef read_regs(I2C_HandleTypeDef *hi2c,
                                   uint8_t reg, uint8_t *dst, uint16_t len)
{
    HAL_StatusTypeDef s;
    s = HAL_I2C_Master_Transmit(hi2c, MPU6050_I2C_ADDR,
                                 &reg, 1, I2C_TIMEOUT_MS);
    if (s != HAL_OK) return s;
    return HAL_I2C_Master_Receive(hi2c, MPU6050_I2C_ADDR,
                                  dst, len, I2C_TIMEOUT_MS);
}

static void i2c_recover(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_DeInit(hi2c);
    HAL_Delay(5);
    HAL_I2C_Init(hi2c);
}

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef s;
    uint8_t who;

    /* Verify device identity */
    s = read_regs(hi2c, REG_WHO_AM_I, &who, 1);
    if (s != HAL_OK || (who & 0x7E) != 0x68) return HAL_ERROR;

    /* Wake from sleep, use internal oscillator */
    s = write_reg(hi2c, REG_PWR_MGMT_1, 0x00);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }
    HAL_Delay(10);

    /* DLPF bandwidth ~44 Hz */
    s = write_reg(hi2c, REG_CONFIG, 0x03);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    /* Gyro: ±500 °/s */
    s = write_reg(hi2c, REG_GYRO_CONFIG, 0x08);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    /* Accel: ±2 g */
    s = write_reg(hi2c, REG_ACCEL_CONFIG, 0x00);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    /* Sample rate: 1 kHz / (1 + 7) = 125 Hz */
    s = write_reg(hi2c, REG_SMPLRT_DIV, 0x07);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c,
                                  MPU6050_Data_t *out)
{
    uint8_t raw[14];
    HAL_StatusTypeDef s = read_regs(hi2c, REG_ACCEL_XOUT_H, raw, 14);
    if (s != HAL_OK) {
        i2c_recover(hi2c);
        return s;
    }

    int16_t ax_raw = (int16_t)((raw[0]  << 8) | raw[1]);
    int16_t ay_raw = (int16_t)((raw[2]  << 8) | raw[3]);
    int16_t az_raw = (int16_t)((raw[4]  << 8) | raw[5]);
    /* raw[6..7] = temperature (unused here) */
    int16_t gx_raw = (int16_t)((raw[8]  << 8) | raw[9]);
    int16_t gy_raw = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz_raw = (int16_t)((raw[12] << 8) | raw[13]);

    out->ax = ax_raw / ACCEL_SCALE_2G;
    out->ay = ay_raw / ACCEL_SCALE_2G;
    out->az = az_raw / ACCEL_SCALE_2G;
    out->gx = gx_raw / GYRO_SCALE_500;
    out->gy = gy_raw / GYRO_SCALE_500;
    out->gz = gz_raw / GYRO_SCALE_500;

    return HAL_OK;
}

float MPU6050_CalcVibRMS(I2C_HandleTypeDef *hi2c)
{
    MPU6050_Data_t d;
    float sum_sq = 0.0f;
    uint32_t valid = 0;
    uint32_t t = HAL_GetTick();

    for (uint32_t i = 0; i < MPU6050_VIB_SAMPLES; i++) {
        /* Non-blocking sample pacing: wait for next 8 ms slot */
        while ((HAL_GetTick() - t) < (i * MPU6050_SAMPLE_PERIOD_MS)) {}

        if (MPU6050_ReadRaw(hi2c, &d) != HAL_OK) continue;

        float mag = sqrtf(d.ax * d.ax + d.ay * d.ay + d.az * d.az);
        float dev = mag - 1.0f;
        sum_sq += dev * dev;
        valid++;
    }

    if (valid == 0) return 0.0f;
    return sqrtf(sum_sq / (float)valid);
}
