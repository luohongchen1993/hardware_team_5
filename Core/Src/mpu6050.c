/* mpu6050.c — I2C driver for the InvenSense MPU-6050 IMU.
 *
 * Every bus helper returns HAL_StatusTypeDef. Read paths retry on error
 * with a DeInit/Init bus recovery between attempts, then surface the
 * failure to the caller so the game can halt with a blink code.
 *
 * Configuration: accel +/-2 g, gyro +/-500 deg/s, DLPF ~44 Hz, 125 Hz ODR. */

#include "mpu6050.h"
#include "config.h"
#include <math.h>

/* ---- MPU-6050 register map (only the registers we touch) ---- */
#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B   /* burst start: 14 bytes -> accel(6)+temp(2)+gyro(6) */
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

/* ---- low-level bus helpers ---- */

static HAL_StatusTypeDef write_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(hi2c, MPU6050_I2C_ADDR, buf, 2, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef read_regs(I2C_HandleTypeDef *hi2c,
                                   uint8_t reg, uint8_t *dst, uint16_t len)
{
    HAL_StatusTypeDef s = HAL_I2C_Master_Transmit(hi2c, MPU6050_I2C_ADDR,
                                                  &reg, 1, I2C_TIMEOUT_MS);
    if (s != HAL_OK) return s;
    return HAL_I2C_Master_Receive(hi2c, MPU6050_I2C_ADDR, dst, len, I2C_TIMEOUT_MS);
}

/* Recover a wedged bus (e.g. after a NACK/timeout) by re-initialising it. */
static void i2c_recover(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_DeInit(hi2c);
    HAL_Delay(2);
    HAL_I2C_Init(hi2c);
}

/* ---- init ---- */

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef s;
    uint8_t who = 0;

    /* Identity check: WHO_AM_I returns 0x68 (ignore the lowest/AD0-ish bits). */
    s = read_regs(hi2c, REG_WHO_AM_I, &who, 1);
    if (s != HAL_OK || (who & 0x7E) != 0x68) return HAL_ERROR;

    /* Wake from sleep (internal 8 MHz oscillator). */
    s = write_reg(hi2c, REG_PWR_MGMT_1, 0x00);
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }
    HAL_Delay(10);

    s = write_reg(hi2c, REG_CONFIG, 0x03);        /* DLPF ~44 Hz */
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    s = write_reg(hi2c, REG_GYRO_CONFIG, 0x08);   /* +/-500 deg/s */
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    s = write_reg(hi2c, REG_ACCEL_CONFIG, 0x00);  /* +/-2 g */
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    s = write_reg(hi2c, REG_SMPLRT_DIV, 0x07);    /* 1 kHz / (1+7) = 125 Hz */
    if (s != HAL_OK) { i2c_recover(hi2c); return s; }

    return HAL_OK;
}

/* ---- reads ---- */

HAL_StatusTypeDef MPU6050_ReadRawCounts(I2C_HandleTypeDef *hi2c, MPU6050_Raw *out)
{
    uint8_t raw[14];
    HAL_StatusTypeDef s = HAL_ERROR;

    for (uint32_t attempt = 0; attempt < I2C_RETRY_COUNT; attempt++) {
        s = read_regs(hi2c, REG_ACCEL_XOUT_H, raw, sizeof raw);
        if (s == HAL_OK) break;
        i2c_recover(hi2c);   /* NACK/timeout -> recover, then retry */
    }
    if (s != HAL_OK) return s;

    out->ax   = (int16_t)((raw[0]  << 8) | raw[1]);
    out->ay   = (int16_t)((raw[2]  << 8) | raw[3]);
    out->az   = (int16_t)((raw[4]  << 8) | raw[5]);
    out->temp = (int16_t)((raw[6]  << 8) | raw[7]);
    out->gx   = (int16_t)((raw[8]  << 8) | raw[9]);
    out->gy   = (int16_t)((raw[10] << 8) | raw[11]);
    out->gz   = (int16_t)((raw[12] << 8) | raw[13]);
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadScaled(I2C_HandleTypeDef *hi2c,
                                     const MPU6050_Bias *bias,
                                     MPU6050_Scaled *out)
{
    MPU6050_Raw r;
    HAL_StatusTypeDef s = MPU6050_ReadRawCounts(hi2c, &r);
    if (s != HAL_OK) return s;

    /* Accel -> m/s^2 */
    out->ax = (float)r.ax * ACCEL_MS2_PER_LSB;
    out->ay = (float)r.ay * ACCEL_MS2_PER_LSB;
    out->az = (float)r.az * ACCEL_MS2_PER_LSB;

    /* Gyro -> deg/s, with zero-rate bias removed */
    float bgx = bias ? bias->gx : 0.0f;
    float bgy = bias ? bias->gy : 0.0f;
    float bgz = bias ? bias->gz : 0.0f;
    out->gx = (float)r.gx / GYRO_LSB_PER_DPS - bgx;
    out->gy = (float)r.gy / GYRO_LSB_PER_DPS - bgy;
    out->gz = (float)r.gz / GYRO_LSB_PER_DPS - bgz;
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c,
                                    MPU6050_Bias *bias_out,
                                    MPU6050_Scaled *accel_ref_out)
{
    double sgx = 0, sgy = 0, sgz = 0;   /* gyro counts -> deg/s accumulators */
    double sax = 0, say = 0, saz = 0;   /* accel m/s^2 accumulators */
    uint32_t valid = 0;
    uint32_t t0 = HAL_GetTick();

    for (uint32_t i = 0; i < CALIB_SAMPLES; i++) {
        /* Pace samples without a busy spin longer than necessary. */
        while ((HAL_GetTick() - t0) < (i * CALIB_SAMPLE_PERIOD_MS)) { }

        MPU6050_Raw r;
        if (MPU6050_ReadRawCounts(hi2c, &r) != HAL_OK) continue;

        sgx += (double)r.gx / GYRO_LSB_PER_DPS;
        sgy += (double)r.gy / GYRO_LSB_PER_DPS;
        sgz += (double)r.gz / GYRO_LSB_PER_DPS;
        sax += (double)r.ax * ACCEL_MS2_PER_LSB;
        say += (double)r.ay * ACCEL_MS2_PER_LSB;
        saz += (double)r.az * ACCEL_MS2_PER_LSB;
        valid++;
    }

    if (valid < (CALIB_SAMPLES / 2)) return HAL_ERROR;   /* too many bus errors */

    if (bias_out) {
        bias_out->gx = (float)(sgx / valid);
        bias_out->gy = (float)(sgy / valid);
        bias_out->gz = (float)(sgz / valid);
    }
    if (accel_ref_out) {
        accel_ref_out->ax = (float)(sax / valid);
        accel_ref_out->ay = (float)(say / valid);
        accel_ref_out->az = (float)(saz / valid);
        accel_ref_out->gx = accel_ref_out->gy = accel_ref_out->gz = 0.0f;
    }
    return HAL_OK;
}
