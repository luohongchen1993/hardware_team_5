#include "speaker.h"
#include "config.h"

/* TIM4 CH1 (PB6, AF2) — square-wave tone generator for STEMMA Speaker.
 *
 * Timer clock: 170 MHz / (PSC+1=170) = 1 MHz.
 * Frequency:   ARR = (1 000 000 / freq_hz) - 1
 * Duty cycle:  CCR1 = (ARR + 1) / 2  (50% → clean fundamental)
 *
 * The STEMMA Class D amp is AC-coupled internally, so the PWM DC
 * offset is rejected — no external capacitor needed.
 */

/* ---- tone step descriptor ---- */
typedef struct {
    uint16_t freq_hz;       /* 0 = silence */
    uint16_t duration_ms;
} Step_t;

/* ---- alarm patterns ---- */

static const Step_t PAT_NOMINAL[] = {
    {0, 1000}
};

static const Step_t PAT_VIB_WARN[] = {
    {880, 200},
    {0,   800}
};

static const Step_t PAT_THERMAL_WARN[] = {
    {660, 150},
    {0,   100},
    {660, 150},
    {0,   700}
};

static const Step_t PAT_SAFETY_BREACH[] = {
    {1400, 100},
    {0,    100}
};

/* Siren: 11 steps, 800→1600 Hz in 80 Hz increments × ~45 ms each */
static const Step_t PAT_CRITICAL[] = {
    { 800, 45}, { 880, 45}, { 960, 45}, {1040, 45}, {1120, 45},
    {1200, 45}, {1280, 45}, {1360, 45}, {1440, 45}, {1520, 45},
    {1600, 45}
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* ---- driver state ---- */

static TIM_HandleTypeDef *s_htim;
static const Step_t      *s_pat;
static uint32_t           s_pat_len;
static uint32_t           s_step;
static uint32_t           s_step_ts;

/* ---- helpers ---- */

static void play_freq(uint16_t freq_hz)
{
    if (freq_hz == 0) {
        __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, 0);
        return;
    }
    uint32_t arr = (1000000UL / freq_hz) - 1UL;
    __HAL_TIM_SET_AUTORELOAD(s_htim, arr);
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, (arr + 1U) / 2U);
}

static void load_step(uint32_t idx)
{
    s_step    = idx;
    s_step_ts = HAL_GetTick();
    play_freq(s_pat[idx].freq_hz);
}

/* ---- public API ---- */

void Speaker_Init(TIM_HandleTypeDef *htim)
{
    s_htim    = htim;
    s_pat     = PAT_NOMINAL;
    s_pat_len = ARRAY_LEN(PAT_NOMINAL);
    s_step    = 0;
    s_step_ts = 0;

    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    play_freq(0);
}

void Speaker_SetAlarm(HealthState_t state)
{
    const Step_t *new_pat;
    uint32_t      new_len;

    switch (state) {
        case STATE_VIBRATION_WARN:
            new_pat = PAT_VIB_WARN;
            new_len = ARRAY_LEN(PAT_VIB_WARN);
            break;
        case STATE_THERMAL_WARN:
            new_pat = PAT_THERMAL_WARN;
            new_len = ARRAY_LEN(PAT_THERMAL_WARN);
            break;
        case STATE_SAFETY_BREACH:
            new_pat = PAT_SAFETY_BREACH;
            new_len = ARRAY_LEN(PAT_SAFETY_BREACH);
            break;
        case STATE_CRITICAL:
            new_pat = PAT_CRITICAL;
            new_len = ARRAY_LEN(PAT_CRITICAL);
            break;
        default:
            new_pat = PAT_NOMINAL;
            new_len = ARRAY_LEN(PAT_NOMINAL);
            break;
    }

    if (new_pat != s_pat) {
        s_pat     = new_pat;
        s_pat_len = new_len;
        load_step(0);
    }
}

void Speaker_Update(void)
{
    if (!s_htim || !s_pat) return;

    uint32_t now = HAL_GetTick();
    if ((now - s_step_ts) >= s_pat[s_step].duration_ms) {
        uint32_t next = (s_step + 1U) % s_pat_len;
        load_step(next);
    }
}

void Speaker_Stop(void)
{
    play_freq(0);
    s_pat     = PAT_NOMINAL;
    s_pat_len = ARRAY_LEN(PAT_NOMINAL);
    s_step    = 0;
}
