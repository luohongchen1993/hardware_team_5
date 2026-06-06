#ifndef CONFIG_H
#define CONFIG_H

/* =========================================================
 * config.h — All thresholds, pin definitions, and timing
 *             constants for the Health Monitor firmware.
 *             Never put magic numbers in logic files.
 * ========================================================= */

/* ---- I2C ---- */
#define MPU6050_I2C_ADDR        (0x68 << 1)   /* AD0 low */
#define I2C_TIMEOUT_MS          100U

/* ---- MPU-6050 sample config ---- */
#define MPU6050_VIB_SAMPLES     32U            /* samples per RMS window */
#define MPU6050_SAMPLE_PERIOD_MS 8U            /* ~125 Hz */

/* ---- Vibration thresholds (g deviation from 1 g) ---- */
#define VIB_RMS_WARN_THRESHOLD      0.15f
#define VIB_RMS_CRITICAL_THRESHOLD  0.35f

/* ---- Temperature thresholds (°C) ---- */
#define TEMP_WARN_CELSIUS           45.0f
#define TEMP_CRITICAL_CELSIUS       65.0f

/* ---- Distance / guard thresholds (mm) ---- */
#define DIST_GUARD_MIN_MM           50U
#define DIST_GUARD_MAX_MM           200U

/* ---- State-machine timing ---- */
#define CLEAR_HOLD_MS               5000U      /* all-clear hold before NOMINAL */
#define HEALTH_UPDATE_PERIOD_MS     100U        /* state-machine tick */
#define SERVO_SWEEP_PERIOD_MS       20U         /* CRITICAL sweep step */

/* ---- SG90 servo PWM (TIM3 CH1) ---- */
/* PSC=169 → 1 MHz tick; ARR=19999 → 20 ms period */
#define SERVO_CCR_0DEG              1000U
#define SERVO_CCR_90DEG             1500U
#define SERVO_CCR_180DEG            2000U

/* ---- LED GPIO ports / pins ---- */
#define LED_GREEN_PORT              GPIOB
#define LED_GREEN_PIN               GPIO_PIN_13
#define LED_AMBER_PORT              GPIOB
#define LED_AMBER_PIN               GPIO_PIN_14
#define LED_RED_PORT                GPIOB
#define LED_RED_PIN                 GPIO_PIN_15

/* ---- HC-SR04 pins ---- */
#define HCSR04_TRIG_PORT            GPIOA
#define HCSR04_TRIG_PIN             GPIO_PIN_9
#define HCSR04_ECHO_PORT            GPIOA
#define HCSR04_ECHO_PIN             GPIO_PIN_8
#define HCSR04_TIMEOUT_US           30000U     /* ~5 m max range */

/* ---- Temperature sensor (DS18B20 / DHT) ---- */
#define TEMP_SENSOR_PORT            GPIOA
#define TEMP_SENSOR_PIN             GPIO_PIN_5
#define TEMP_SENSOR_TIMER           TIM2       /* 1-µs tick for 1-Wire timing */

/* ---- STEMMA Speaker (TIM4 CH1, PB6, AF2) ---- */
/* PSC=169 → 1 MHz tick; ARR set dynamically per tone frequency */
#define SPEAKER_GPIO_PORT           GPIOB
#define SPEAKER_GPIO_PIN            GPIO_PIN_6

/* ---- UART ---- */
#define LOG_UART_TIMEOUT_MS         10U

#endif /* CONFIG_H */
