#include "health_monitor.h"
#include "config.h"
#include "mpu6050.h"
#include "distance_sensor.h"
#include "temp_sensor.h"
#include "servo.h"
#include "led.h"
#include "serial_log.h"
#include "speaker.h"

static I2C_HandleTypeDef   *s_hi2c;
static TIM_HandleTypeDef   *s_htim_servo;
static TIM_HandleTypeDef   *s_htim_us;
static UART_HandleTypeDef  *s_huart;

static HealthData_t  s_data;
static uint32_t      s_clear_ts;           /* tick when all-clear started */
static uint32_t      s_last_update_ts;
static uint32_t      s_flash_ts;           /* for CRITICAL LED flash */
static uint8_t       s_sweep_dir;          /* 0=up, 1=down for CRITICAL */
static uint8_t       s_sweep_pos;

/* ---- internal helpers ---- */

static void enter_nominal(void)
{
    s_data.state = STATE_NOMINAL;
    LED_Off(LED_ALL);
    LED_Set(LED_GREEN, 1);
    Servo_SetAngle(s_htim_servo, 0);
    Speaker_SetAlarm(STATE_NOMINAL);
}

static void enter_vibration_warn(void)
{
    s_data.state = STATE_VIBRATION_WARN;
    LED_Off(LED_ALL);
    LED_Set(LED_AMBER, 1);
    Servo_SetAngle(s_htim_servo, 45);
    Speaker_SetAlarm(STATE_VIBRATION_WARN);
}

static void enter_thermal_warn(void)
{
    s_data.state = STATE_THERMAL_WARN;
    LED_Off(LED_ALL);
    LED_Set(LED_AMBER, 1);
    Servo_SetAngle(s_htim_servo, 90);
    Speaker_SetAlarm(STATE_THERMAL_WARN);
}

static void enter_safety_breach(void)
{
    s_data.state = STATE_SAFETY_BREACH;
    LED_Off(LED_ALL);
    LED_Set(LED_RED, 1);
    Servo_SetAngle(s_htim_servo, 180);
    Speaker_SetAlarm(STATE_SAFETY_BREACH);
    SerialLog_Print("!!! SAFETY BREACH: GUARD REMOVED !!!\r\n");
}

static void enter_critical(void)
{
    s_data.state = STATE_CRITICAL;
    s_flash_ts = HAL_GetTick();
    s_sweep_pos = 0;
    s_sweep_dir = 0;
    Speaker_SetAlarm(STATE_CRITICAL);
    SerialLog_Print("!!! CRITICAL ALARM !!!\r\n");
}

static int guard_removed(uint32_t dist_mm)
{
    if (dist_mm == UINT32_MAX) return 0;     /* sensor error — don't trigger */
    return (dist_mm < DIST_GUARD_MIN_MM) || (dist_mm > DIST_GUARD_MAX_MM);
}

static void service_critical(void)
{
    /* Flash all LEDs at ~2 Hz */
    uint32_t now = HAL_GetTick();
    if ((now - s_flash_ts) >= 250U) {
        LED_Toggle(LED_ALL);
        s_flash_ts = now;
    }

    /* Sweep servo back and forth */
    if (s_sweep_dir == 0) {
        s_sweep_pos += 5;
        if (s_sweep_pos >= 180U) { s_sweep_pos = 180; s_sweep_dir = 1; }
    } else {
        if (s_sweep_pos >= 5) s_sweep_pos -= 5;
        else { s_sweep_pos = 0; s_sweep_dir = 0; }
    }
    Servo_SetAngle(s_htim_servo, s_sweep_pos);
}

/* ---- public API ---- */

void HealthMonitor_Init(I2C_HandleTypeDef *hi2c,
                        TIM_HandleTypeDef *htim_servo,
                        TIM_HandleTypeDef *htim_us,
                        TIM_HandleTypeDef *htim_spk,
                        UART_HandleTypeDef *huart)
{
    s_hi2c       = hi2c;
    s_htim_servo = htim_servo;
    s_htim_us    = htim_us;
    s_huart      = huart;

    s_data.state = STATE_NOMINAL;
    s_clear_ts   = 0;
    s_last_update_ts = 0;
    s_flash_ts   = 0;
    s_sweep_pos  = 0;
    s_sweep_dir  = 0;

    SerialLog_Init(huart);
    LED_Init();
    Servo_Init(htim_servo);
    Speaker_Init(htim_spk);
    enter_nominal();
}

void HealthMonitor_Update(void)
{
    uint32_t now = HAL_GetTick();
    if ((now - s_last_update_ts) < HEALTH_UPDATE_PERIOD_MS) return;
    s_last_update_ts = now;

    /* --- Read sensors --- */
    MPU6050_Data_t imu;
    if (MPU6050_ReadRaw(s_hi2c, &imu) == HAL_OK) {
        s_data.ax      = imu.ax;
        s_data.ay      = imu.ay;
        s_data.az      = imu.az;
        s_data.gx      = imu.gx;
        s_data.gy      = imu.gy;
        s_data.gz      = imu.gz;
    }
    s_data.vib_rms = MPU6050_CalcVibRMS(s_hi2c);
    s_data.dist_mm = Distance_ReadMM(s_htim_us);
    s_data.temp_c  = Temp_ReadCelsius(s_htim_us);

    /* --- Condition flags --- */
    int vib_warn  = (s_data.vib_rms >= VIB_RMS_WARN_THRESHOLD);
    int vib_crit  = (s_data.vib_rms >= VIB_RMS_CRITICAL_THRESHOLD);
    int temp_warn = (s_data.temp_c  >= TEMP_WARN_CELSIUS  && s_data.temp_c != -999.0f);
    int temp_crit = (s_data.temp_c  >= TEMP_CRITICAL_CELSIUS && s_data.temp_c != -999.0f);
    int guard     = guard_removed(s_data.dist_mm);

    int n_warn = (vib_warn ? 1 : 0) + (temp_warn ? 1 : 0) + (guard ? 1 : 0);
    int any_crit = vib_crit || temp_crit;

    /* --- State machine transitions --- */
    if (any_crit || n_warn >= 2) {
        if (s_data.state != STATE_CRITICAL) enter_critical();
        service_critical();
        s_clear_ts = now;   /* reset clear timer */
    } else if (guard) {
        if (s_data.state != STATE_SAFETY_BREACH) enter_safety_breach();
        s_clear_ts = now;
    } else if (temp_warn) {
        if (s_data.state != STATE_THERMAL_WARN) enter_thermal_warn();
        s_clear_ts = now;
    } else if (vib_warn) {
        if (s_data.state != STATE_VIBRATION_WARN) enter_vibration_warn();
        s_clear_ts = now;
    } else {
        /* All conditions clear — return to NOMINAL after hold */
        if (s_data.state != STATE_NOMINAL) {
            if (s_clear_ts == 0) s_clear_ts = now;
            if ((now - s_clear_ts) >= CLEAR_HOLD_MS) enter_nominal();
        }
    }

    /* --- Speaker pattern tick --- */
    Speaker_Update();

    /* --- Log --- */
    SerialLog_PrintStatus(&s_data);
}

HealthState_t HealthMonitor_GetState(void)
{
    return s_data.state;
}

HealthData_t *HealthMonitor_GetData(void)
{
    return &s_data;
}
