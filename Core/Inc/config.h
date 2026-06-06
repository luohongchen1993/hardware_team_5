#ifndef CONFIG_H
#define CONFIG_H

/* =========================================================
 * config.h — All tunables, thresholds, pin and peripheral
 *            definitions for the navigation-game firmware.
 *            Never put magic numbers in logic files.
 *
 * Target: STM32G474RE (Nucleo-G474RE), bare-metal, HAL,
 *         170 MHz, 100 Hz fixed-rate super-loop.
 * ========================================================= */

#include "stm32g4xx_hal.h"   /* so GPIOx / GPIO_PIN_x resolve below */

/* ---------------------------------------------------------
 * Main loop timing — 100 Hz fixed rate, 10 ms deadline.
 * Every per-cycle operation is O(1) time and O(1) space.
 * --------------------------------------------------------- */
#define LOOP_HZ                 100U
#define LOOP_PERIOD_MS          10U
#define LOOP_DT_S               (1.0f / (float)LOOP_HZ)   /* 0.01 s */

/* ---------------------------------------------------------
 * I2C1 — MPU-6050
 * --------------------------------------------------------- */
#define MPU6050_I2C_ADDR        (0x68 << 1)   /* 7-bit 0x68, AD0 low; HAL wants 8-bit */
#define I2C_TIMEOUT_MS          100U
#define I2C_RETRY_COUNT         3U            /* retries before declaring sensor fault */

/* ---------------------------------------------------------
 * MPU-6050 scaling.  Accel +/-2 g, Gyro +/-500 deg/s.
 * (+/-500 chosen over the spec's +/-250 so brisk handheld
 *  turns do not saturate; resolution is still ~0.015 deg/s.)
 * --------------------------------------------------------- */
#define ACCEL_LSB_PER_G         16384.0f      /* +/-2 g full scale */
#define GYRO_LSB_PER_DPS        65.5f         /* +/-500 deg/s full scale */
#define GRAVITY_MS2             9.80665f
#define ACCEL_MS2_PER_LSB       (GRAVITY_MS2 / ACCEL_LSB_PER_G)

/* ---------------------------------------------------------
 * Calibration (performed at rest on power-up).
 * --------------------------------------------------------- */
#define CALIB_SAMPLES           400U          /* averaged at-rest samples for gyro bias */
#define CALIB_SAMPLE_PERIOD_MS  5U            /* ~2 s total settle/average window */

/* ---------------------------------------------------------
 * Game world.  Grid spans ~20 ft (6.096 m) from origin in
 * each cardinal direction; win when within ~1 ft (0.3048 m).
 * --------------------------------------------------------- */
#define GRID_HALF_EXTENT_M      6.096f        /* +/-20 ft per axis (X,Y) */
#define WIN_RADIUS_M            0.1524f       /* ~6 in (15 cm) win threshold (configurable) */

/* ---------------------------------------------------------
 * Sensor fusion.
 * Heading: gyro-Z integration. Position: step dead-reckoning
 * (pedometer) — one stride per detected step along the heading.
 * Bounded per-step error, no acceleration-integration drift,
 * and tilt-independent (uses |accel| magnitude).
 * --------------------------------------------------------- */
#define STRIDE_M                  0.65f   /* metres advanced per detected step (tune to user) */
#define STEP_ACCEL_THRESH_MS2     1.5f    /* dynamic-accel peak to count a step (~0.15 g) */
#define STEP_REARM_MS2            0.6f    /* dynamic accel must fall below this to re-arm */
#define STEP_MIN_INTERVAL_SAMPLES 25U     /* refractory between steps (~250 ms @ 100 Hz) */
#define STEP_LP_ALPHA             0.02f   /* low-pass factor for the gravity baseline */

/* ---------------------------------------------------------
 * Servo — SG90 class on TIM3 CH1 (PA6).
 * PSC=169 -> 1 MHz tick; ARR=19999 -> 20 ms (50 Hz).
 * CCR value is directly the pulse width in microseconds.
 * --------------------------------------------------------- */
#define SERVO_PULSE_MIN_US      1000U         /* 0 deg   (hard left)  */
#define SERVO_PULSE_MID_US      1500U         /* 90 deg  (straight ahead / on target) */
#define SERVO_PULSE_MAX_US      2000U         /* 180 deg (hard right) */
#define SERVO_BEARING_CLAMP_DEG 90.0f         /* relative bearing mapped onto +/-this */
#define SERVO_BEARING_SIGN      (+1.0f)       /* flip to -1.0f if servo points the wrong way */

/* ---------------------------------------------------------
 * Audio — PWM square-wave tone on TIM2 CH1 (PA0).
 * PSC=169 -> 1 MHz tick.  freq = TICK/(ARR+1); duty 50%.
 * Pitch rises and beep cadence speeds up as the target nears.
 * --------------------------------------------------------- */
#define AUDIO_TIMER_TICK_HZ     1000000UL     /* 1 MHz timer tick */
#define AUDIO_FREQ_FAR_HZ       300U          /* tone when far */
#define AUDIO_FREQ_NEAR_HZ      2000U         /* tone when close */
#define AUDIO_RANGE_M           9.0f          /* distance mapped to "far"; beyond clamps */
#define BEEP_PERIOD_FAR_MS      800U          /* slow beeping when far */
#define BEEP_PERIOD_NEAR_MS     90U           /* fast beeping when close */
#define BEEP_ON_MAX_MS          60U           /* cap on per-beep on-time */

/* ---------------------------------------------------------
 * Status LEDs (reused from the prior board bring-up).
 * Green = seeking, Amber = proximity pulse, Red = win/error.
 * --------------------------------------------------------- */
#define LED_GREEN_PORT          GPIOB
#define LED_GREEN_PIN           GPIO_PIN_13
#define LED_AMBER_PORT          GPIOB
#define LED_AMBER_PIN           GPIO_PIN_14
#define LED_RED_PORT            GPIOB
#define LED_RED_PIN             GPIO_PIN_15

/* ---------------------------------------------------------
 * User button (Nucleo B1) — replay after a win.
 * On Nucleo-G474RE B1 is PC13; pressed reads LOW.
 * --------------------------------------------------------- */
#define USER_BTN_PORT           GPIOC
#define USER_BTN_PIN            GPIO_PIN_13
#define USER_BTN_PRESSED_STATE  GPIO_PIN_RESET

/* ---------------------------------------------------------
 * UART logging (USART2 -> ST-Link VCP, 115200 8N1).
 * --------------------------------------------------------- */
#define LOG_UART_TIMEOUT_MS     10U

/* ---------------------------------------------------------
 * Telemetry throttle — print one nav line every N ticks.
 * --------------------------------------------------------- */
#define TELEMETRY_EVERY_TICKS   10U           /* 100 Hz / 10 = 10 Hz logging */

#endif /* CONFIG_H */
