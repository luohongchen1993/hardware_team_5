/* audio.c — Proximity audio via a PWM square wave on TIM2 CH1 (PA0).
 *
 * Frequency is set by the auto-reload (period); duty is held at ~50%.
 * As the target nears, both the pitch rises and the beep cadence speeds
 * up (Geiger-counter style), giving the user a strong "hotter/colder" cue. */

#include "audio.h"
#include "config.h"

/* Beep state machine (module-local; one speaker). */
static uint32_t s_beep_last_ms = 0;
static uint8_t  s_beep_on      = 0;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void Audio_SetTone(TIM_HandleTypeDef *htim, uint32_t freq_hz)
{
    if (freq_hz == 0U) {
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);   /* silence */
        return;
    }
    uint32_t arr = (AUDIO_TIMER_TICK_HZ / freq_hz);
    if (arr == 0U) arr = 1U;
    arr -= 1U;
    __HAL_TIM_SET_AUTORELOAD(htim, arr);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, (arr + 1U) / 2U);   /* 50% duty */
}

void Audio_Off(TIM_HandleTypeDef *htim)
{
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
    s_beep_on = 0;
}

void Audio_Init(TIM_HandleTypeDef *htim)
{
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    Audio_Off(htim);
}

AudioCue Audio_Update(TIM_HandleTypeDef *htim, float distance_m)
{
    /* proximity p in [0,1]: 0 at/over AUDIO_RANGE_M, 1 at the win radius. */
    float p = (AUDIO_RANGE_M - distance_m) / (AUDIO_RANGE_M - WIN_RADIUS_M);
    p = clampf(p, 0.0f, 1.0f);

    /* Pitch rises with proximity. */
    uint32_t freq = AUDIO_FREQ_FAR_HZ +
                    (uint32_t)(p * (float)(AUDIO_FREQ_NEAR_HZ - AUDIO_FREQ_FAR_HZ));

    /* Beep period shrinks with proximity. */
    uint32_t period = BEEP_PERIOD_FAR_MS -
                      (uint32_t)(p * (float)(BEEP_PERIOD_FAR_MS - BEEP_PERIOD_NEAR_MS));
    uint32_t on_ms = period / 2U;
    if (on_ms > BEEP_ON_MAX_MS) on_ms = BEEP_ON_MAX_MS;
    uint32_t off_ms = (period > on_ms) ? (period - on_ms) : 1U;

    /* Non-blocking on/off toggling. */
    uint32_t now = HAL_GetTick();
    if (s_beep_on) {
        if ((now - s_beep_last_ms) >= on_ms) {
            s_beep_on = 0;
            s_beep_last_ms = now;
            Audio_SetTone(htim, 0);
        }
    } else {
        if ((now - s_beep_last_ms) >= off_ms) {
            s_beep_on = 1;
            s_beep_last_ms = now;
            Audio_SetTone(htim, freq);
        }
    }

    if (p >= 0.85f) return AUDIO_CUE_VERY_NEAR;
    if (p >= 0.55f) return AUDIO_CUE_NEAR;
    if (p >= 0.25f) return AUDIO_CUE_MID;
    return AUDIO_CUE_FAR;
}

void Audio_WinJingle(TIM_HandleTypeDef *htim)
{
    /* Ascending C5-E5-G5-C6 arpeggio. Blocking is fine: gameplay has stopped. */
    static const uint32_t notes[] = { 523U, 659U, 784U, 1047U };
    for (uint32_t i = 0; i < (sizeof notes / sizeof notes[0]); i++) {
        Audio_SetTone(htim, notes[i]);
        HAL_Delay(150);
    }
    Audio_SetTone(htim, 1047U);
    HAL_Delay(300);
    Audio_Off(htim);
}

void Audio_BootTone(TIM_HandleTypeDef *htim)
{
    /* G4 -> C5 -> E5 startup signature: short, distinct from the win jingle. */
    static const uint32_t notes[] = { 392U, 523U, 659U };
    for (uint32_t i = 0; i < (sizeof notes / sizeof notes[0]); i++) {
        Audio_SetTone(htim, notes[i]);
        HAL_Delay(80);
        Audio_Off(htim);
        HAL_Delay(25);
    }
}
