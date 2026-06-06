/* game.c — Navigation-game state machine.
 *
 * INIT -> CALIBRATE -> SEEKING -> WIN -> (replay) SEEKING
 *                      any state -> ERROR (halt + blink code)
 *
 * Owns the deterministic PRNG (xorshift32, seeded from the hardware RNG),
 * the virtual target, and per-cycle orchestration:
 *   read IMU -> fuse -> drive servo -> proximity audio -> win check.
 *
 * Complexity: O(1) time and O(1) space per SEEKING cycle; no heap. */

#include "game.h"
#include "config.h"
#include "mpu6050.h"
#include "navigation.h"
#include "servo.h"
#include "audio.h"
#include "led.h"
#include "serial_log.h"
#include <stdio.h>

/* ---- module state ---- */
static I2C_HandleTypeDef  *s_i2c;
static TIM_HandleTypeDef  *s_servo;
static TIM_HandleTypeDef  *s_audio;

static GameState     s_state = STATE_INIT;
static MPU6050_Bias  s_bias;
static NavState      s_nav;
static Coordinate    s_target;
static uint8_t       s_win_done;
static uint16_t      s_tele_ctr;

/* ---- forward declarations ---- */
static void error_halt(uint8_t blink_code) __attribute__((noreturn));
static void warn_blink(uint8_t blink_code);

/* ---- deterministic PRNG (xorshift32) ---- */
static uint32_t s_prng = 1u;

static void     prng_seed(uint32_t x)   { s_prng = x ? x : 1u; }
static uint32_t prng_next(void)
{
    uint32_t x = s_prng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return (s_prng = x);
}
static float prng_float01(void)         { return (float)(prng_next() >> 8) / 16777216.0f; }
static float prng_range(float lo, float hi) { return lo + prng_float01() * (hi - lo); }

/* ---- helpers ---- */

static void place_target(void)
{
    s_target.x = prng_range(-GRID_HALF_EXTENT_M, GRID_HALF_EXTENT_M);
    s_target.y = prng_range(-GRID_HALF_EXTENT_M, GRID_HALF_EXTENT_M);
    s_target.z = 0.0f;

    char buf[96];
    int n = snprintf(buf, sizeof buf,
                     "Target placed at (%.2f, %.2f) m. Go find it!\r\n",
                     (double)s_target.x, (double)s_target.y);
    if (n > 0) SerialLog_Print(buf);
}

static int button_pressed(void)
{
    if (HAL_GPIO_ReadPin(USER_BTN_PORT, USER_BTN_PIN) == USER_BTN_PRESSED_STATE) {
        HAL_Delay(20);   /* debounce */
        return (HAL_GPIO_ReadPin(USER_BTN_PORT, USER_BTN_PIN) == USER_BTN_PRESSED_STATE);
    }
    return 0;
}

/* Hardware self-test + startup show: LED chase, servo sweep, boot tone.
 * Proves every actuator is alive before calibration begins. ~2 s total. */
static void startup_demo(void)
{
    /* LED chase: green -> amber -> red -> all off -> all flash */
    LED_Set(LED_GREEN, 1); HAL_Delay(150);
    LED_Set(LED_AMBER, 1); HAL_Delay(150);
    LED_Set(LED_RED,   1); HAL_Delay(150);
    LED_Off(LED_ALL);      HAL_Delay(100);
    LED_Set(LED_ALL, 1);   HAL_Delay(100);
    LED_Off(LED_ALL);

    /* Servo sweep: centre -> hard left -> hard right -> centre */
    Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US); HAL_Delay(200);
    Servo_SetPulseUs(s_servo, SERVO_PULSE_MIN_US); HAL_Delay(350);
    Servo_SetPulseUs(s_servo, SERVO_PULSE_MAX_US); HAL_Delay(350);
    Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US); HAL_Delay(200);

    /* 3-note boot jingle */
    Audio_BootTone(s_audio);
}

/* Non-fatal warning: blink amber N times once, then continue. */
static void warn_blink(uint8_t blink_code)
{
    for (uint8_t i = 0; i < blink_code; i++) {
        LED_Set(LED_AMBER, 1); HAL_Delay(150);
        LED_Set(LED_AMBER, 0); HAL_Delay(150);
    }
}

/* Fatal fault: safe the actuators and blink the red LED forever.
 *   code 2 = MPU not found / init fail, 3 = I2C runtime fault. */
static void error_halt(uint8_t blink_code)
{
    s_state = STATE_ERROR;
    if (s_audio) Audio_Off(s_audio);
    if (s_servo) Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US);
    LED_Off(LED_ALL);
    SerialLog_Print("!!! FATAL: halting (see red LED blink code) !!!\r\n");

    for (;;) {
        for (uint8_t i = 0; i < blink_code; i++) {
            LED_Set(LED_RED, 1); HAL_Delay(200);
            LED_Set(LED_RED, 0); HAL_Delay(200);
        }
        HAL_Delay(1000);
    }
}

/* ---- state bodies ---- */

static void do_calibrate(void)
{
    LED_Off(LED_ALL);
    LED_Set(LED_AMBER, 1);
    SerialLog_Print("Calibrating IMU - hold the board still...\r\n");

    MPU6050_Scaled accel_ref;
    if (MPU6050_Calibrate(s_i2c, &s_bias, &accel_ref) != HAL_OK) {
        error_halt(3);   /* too many bus errors during calibration */
    }

    Nav_Init(&s_nav, &accel_ref);
    place_target();

    LED_Off(LED_ALL);
    LED_Set(LED_GREEN, 1);
    s_tele_ctr = 0;
    s_state = STATE_SEEKING;
}

static void do_seek(void)
{
    MPU6050_Scaled s;
    if (MPU6050_ReadScaled(s_i2c, &s_bias, &s) != HAL_OK) {
        error_halt(3);
    }

    Nav_Update(&s_nav, &s, &s_target, LOOP_DT_S);

    /* Point the servo toward the target relative to current heading. */
    Servo_PointBearing(s_servo, Nav_RelativeBearing(&s_nav));

    /* Proximity audio cue. */
    AudioCue cue = Audio_Update(s_audio, s_nav.distance_m);

    /* LEDs: green = seeking, amber pulses when the target is near. */
    LED_Set(LED_GREEN, 1);
    LED_Set(LED_AMBER, (cue >= AUDIO_CUE_NEAR) ? 1 : 0);

    /* Throttled telemetry. */
    if (++s_tele_ctr >= TELEMETRY_EVERY_TICKS) {
        s_tele_ctr = 0;
        SerialLog_PrintNav(&s_nav, s_state, cue);
    }

    /* Win condition. */
    if (s_nav.distance_m < WIN_RADIUS_M) {
        s_state = STATE_WIN;
        s_win_done = 0;
    }
}

static void do_win(void)
{
    if (!s_win_done) {
        s_win_done = 1;
        Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US);   /* centre & hold */
        Audio_Off(s_audio);
        SerialLog_Print(">>> TARGET REACHED - YOU WIN! <<<\r\n");
        Audio_WinJingle(s_audio);

        /* Servo waggle: celebrate with 3 left-right sweeps. */
        for (int i = 0; i < 3; i++) {
            Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US - 400U); HAL_Delay(130);
            Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US + 400U); HAL_Delay(130);
        }
        Servo_SetPulseUs(s_servo, SERVO_PULSE_MID_US);

        for (int i = 0; i < 6; i++) { LED_Toggle(LED_ALL); HAL_Delay(120); }
        LED_Off(LED_ALL);
        LED_Set(LED_GREEN, 1);
        SerialLog_Print("Press B1 to play again.\r\n");
    }

    /* Replay on button press: new target, reset position estimate. */
    if (button_pressed()) {
        MPU6050_Scaled s;
        if (MPU6050_ReadScaled(s_i2c, &s_bias, &s) == HAL_OK) {
            Nav_Init(&s_nav, &s);
        } else {
            SerialLog_Print("WARN: IMU read failed at replay; resetting without accel seed.\r\n");
            Nav_Init(&s_nav, NULL);
        }
        place_target();
        LED_Off(LED_ALL);
        LED_Set(LED_GREEN, 1);
        s_tele_ctr = 0;
        s_state = STATE_SEEKING;
    }
}

/* ---- public API ---- */

void Game_Init(I2C_HandleTypeDef *hi2c,
               TIM_HandleTypeDef *htim_servo,
               TIM_HandleTypeDef *htim_audio,
               UART_HandleTypeDef *huart,
               RNG_HandleTypeDef *hrng)
{
    s_i2c   = hi2c;
    s_servo = htim_servo;
    s_audio = htim_audio;

    LED_Init();
    SerialLog_Init(huart);
    Servo_Init(htim_servo);
    Audio_Init(htim_audio);

    SerialLog_Print("\r\n=== Navigation Game (STM32G474RE) ===\r\n");

    /* Bring up the IMU (retry, then fatal). */
    HAL_StatusTypeDef s = HAL_ERROR;
    for (uint32_t i = 0; i < I2C_RETRY_COUNT; i++) {
        s = MPU6050_Init(hi2c);
        if (s == HAL_OK) break;
        HAL_Delay(50);
    }
    if (s != HAL_OK) error_halt(2);

    /* Seed the deterministic PRNG from the hardware RNG if available. */
    uint32_t seed = 0;
    if (hrng && HAL_RNG_GenerateRandomNumber(hrng, &seed) == HAL_OK && seed != 0) {
        prng_seed(seed);
        SerialLog_Print("RNG: hardware seed acquired.\r\n");
    } else {
        prng_seed(0x00C0FFEEu);
        SerialLog_Print("RNG: hardware unavailable - using fixed seed.\r\n");
        warn_blink(4);   /* non-fatal warning code */
    }

    /* Hardware self-test: sweep servo, chase LEDs, play boot tone. */
    startup_demo();

    s_state = STATE_CALIBRATE;
}

void Game_Tick(void)
{
    static uint32_t last_ms = 0;
    uint32_t now = HAL_GetTick();

    switch (s_state) {
    case STATE_CALIBRATE:
        do_calibrate();                 /* blocking ~2 s, one-shot */
        break;

    case STATE_SEEKING:
        if ((now - last_ms) < LOOP_PERIOD_MS) return;   /* pace to LOOP_HZ */
        last_ms = now;
        do_seek();
        break;

    case STATE_WIN:
        do_win();
        break;

    case STATE_INIT:
    case STATE_HALT:
    case STATE_ERROR:
    default:
        /* INIT is transient (set in Game_Init); ERROR never returns here. */
        break;
    }
}

GameState Game_GetState(void)
{
    return s_state;
}
