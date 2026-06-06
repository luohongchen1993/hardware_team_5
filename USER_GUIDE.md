# Navigation Game — User Guide

---

## What is this?

You hold the board and walk around the room. A hidden virtual target has been
placed somewhere within roughly a 40 ft × 40 ft area around where you powered on.

**The servo is your compass.** When it points straight forward, you are
heading directly toward the target. When it swings left or right, turn that
direction.

**The beeps are your radar.** Slow and low-pitched means far away. Fast and
high-pitched means close. Very fast near-continuous beeping means you are almost
on top of it.

**Reach the target and you win** — the board plays a victory jingle, the LEDs
celebrate, and you press the button to play again.

---

## Step-by-step: a full game

### 1. Power on

The board starts initialising. The amber LED turns on.

**Hold the board completely still** for about 2 seconds. The firmware is
measuring the sensor's resting noise floor so it can subtract it from every
future reading. If you move it during this window, the heading estimate will
be off for the whole game.

You will hear nothing and see only the amber LED during this phase.

### 2. Target placed — start seeking

The amber LED goes off. The **green LED turns on and stays on** for the rest
of the round. A message appears on the serial terminal (if connected):

```
Target placed at (3.24, -1.87) m. Go find it!
```

The servo moves to its starting position and the beeping begins.

### 3. Finding the target

Walk around normally, holding the board at about waist height and roughly flat
(face-up). The sensor assumes gravity is pointing down, so tilting it strongly
will confuse the position estimate.

**Follow the servo.** It points toward the target relative to whichever way you
are facing. If it is pointing left, turn left until it centres.

**Follow the beeps.** Use the table below to understand what you are hearing.

---

## Beep guide

All beeps are a single short tone. Pitch and speed both increase as you get closer.

| Zone | Distance | Pitch | Speed | Feel |
|---|---|---|---|---|
| Far | > 22 ft (6.8 m) | ~300 Hz (low) | 1 beep / 800 ms | Slow, low thumps |
| Mid | 14 – 22 ft (4.2 – 6.8 m) | ~300–1200 Hz | 1–2 beeps / sec | Noticeably quickening |
| Near | 5 – 14 ft (1.6 – 4.2 m) | ~1200–1750 Hz | 2–5 beeps / sec | Fast, higher pitch; **amber LED starts pulsing** |
| Very near | 1 – 5 ft (0.3 – 1.6 m) | ~1750–2000 Hz | 5–10 beeps / sec | Rapid, high-pitched; **amber LED stays on** |
| Win | < 1 ft (0.3 m) | — | — | Victory jingle plays |

Beyond 30 ft (9 m) the beep pitch stays at its lowest (300 Hz) and cadence stays
at its slowest (one beep every 800 ms). Moving farther away does not change the
sound — you are already at maximum "cold."

---

## The amber LED

The amber LED gives a visual hint that mirrors the audio:

- **Off** — you are in the Far or Mid zone
- **Pulses** — you are in the Near zone (within ~14 ft)
- **Solid on** — you are in the Very Near zone (within ~5 ft)

---

## Winning

When you get within about 1 foot (30 cm) of the target:

1. The servo centres and holds (pointing straight ahead).
2. The beeping stops.
3. The board plays a **4-note ascending jingle**: C → E → G → high C (like a
   triumph fanfare). The last note is held for an extra beat.
4. All three LEDs (green, amber, red) flash together 6 times.
5. The green LED stays on.
6. The serial terminal prints: `>>> TARGET REACHED - YOU WIN! <<<`

The board then waits. Nothing will happen until you press the button.

---

## Playing again

Press the **B1 button** (the blue button on the Nucleo board, labelled B1 or USER).

- A brand-new target is placed at a random location.
- Your position and heading estimate resets to zero from wherever you are standing.
- The servo moves to the new direction.
- Beeping resumes immediately.

**Note:** Your starting position for the new round is wherever you are standing
when you press B1, not necessarily where you started the first round.

---

## LED summary

| LED | Colour | Meaning |
|---|---|---|
| Green (PB13) | Solid on | Actively seeking a target |
| Amber (PB14) | Pulses / solid | Getting close (Near / Very Near zone) |
| Amber (PB14) | 4 slow blinks once at startup | Hardware random number generator unavailable — game continues normally with a fixed random seed |
| Red (PB15) | 2 blinks, then halts | MPU-6050 sensor not found — check wiring |
| Red (PB15) | 3 blinks, then halts | I2C communication error — check wiring |
| Red (PB15) | Fast strobe, halts | Fatal hardware init failure |
| All three | 6 rapid flashes | Win celebration |

---

## Tips

- **Stand still during calibration.** Even a small wobble during those first
  2 seconds adds a permanent offset to your heading. If the servo seems to
  point in the wrong direction from the start, power-cycle and hold still.

- **Move at a natural walking pace.** The position estimate is based on
  integrating acceleration. Very fast movements and abrupt stops accumulate
  more error. Slow, smooth movement works best.

- **Trust the servo direction more than the distance.** The heading/bearing
  estimate is much more reliable than the absolute position. If the beep says
  "close" but the servo is pointing sideways, trust the servo — you may be
  near the target in terms of raw noise but the bearing is geometrically more
  accurate.

- **The grid is virtual.** There are no physical walls. A target could be
  placed up to ~20 ft (6.1 m) from your starting point in any horizontal
  direction.

- **The position drifts.** This is inherent to dead-reckoning with a phone-grade
  accelerometer. After 30–60 seconds of movement expect several feet of
  accumulated error. The beeping and servo will still guide you, but the
  serial terminal's position numbers will diverge from reality over time.

---

## Custom victory sound

### What is currently possible

The board generates audio as a **square-wave tone** — the same way an 8-bit
game console makes beeps. It cannot play back MP3s or WAV files directly.

The current win jingle is a hardcoded 4-note melody:

```
C5 (523 Hz) → E5 (659 Hz) → G5 (784 Hz) → C6 (1047 Hz) → hold C6
```

### Option 1 — Change the melody (works today, easy)

Edit the `notes[]` array in `Core/Src/audio.c` inside `Audio_WinJingle()`.
Each entry is a frequency in Hz. You can add as many notes as you want and
change the `HAL_Delay` durations to control how long each note lasts.

```c
// Example: "Super Mario" style fanfare
static const uint32_t notes[] = {
    523U,  /* C5 */
    523U,  /* C5 */
    0U,    /* rest */
    523U,  /* C5 */
    0U,    /* rest */
    415U,  /* Ab4 */
    523U,  /* C5 */
    659U,  /* E5 */
    784U,  /* G5 */
};
```

Add a `HAL_Delay` (in milliseconds) after each note to set its length. A rest
is a note with frequency 0 (calls `Audio_SetTone(htim, 0)` which silences output).
The current code plays each note for 150 ms and holds the last one for 300 ms.

Common musical note frequencies for reference:

| Note | Freq (Hz) | Note | Freq (Hz) |
|---|---|---|---|
| C4 (middle C) | 262 | C5 | 523 |
| D4 | 294 | D5 | 587 |
| E4 | 330 | E5 | 659 |
| F4 | 349 | F5 | 698 |
| G4 | 392 | G5 | 784 |
| A4 | 440 | A5 | 880 |
| B4 | 494 | B5 | 988 |
| — | — | C6 | 1047 |

### Option 2 — Multi-note melody with custom timing (works today, moderate effort)

Instead of a fixed delay per note, use a note-duration table:

```c
typedef struct { uint32_t freq_hz; uint32_t dur_ms; } Note;

static const Note melody[] = {
    { 784U, 100U },   /* G5 short */
    { 784U, 100U },   /* G5 short */
    { 784U, 100U },   /* G5 short */
    { 622U, 700U },   /* Eb5 long */
    { 0U,   50U  },   /* rest     */
    { 698U, 100U },   /* F5 short */
    /* ... */
};
for (uint32_t i = 0; i < sizeof melody / sizeof melody[0]; i++) {
    Audio_SetTone(htim, melody[i].freq_hz);
    HAL_Delay(melody[i].dur_ms);
}
Audio_Off(htim);
```

This gives you full control over rhythm and timing without any extra hardware.

### Option 3 — Sampled audio (requires hardware change, significant effort)

The STM32G474RE chip has a built-in 12-bit DAC on pins PA4 and PA5. By wiring
one of those pins to the STEMMA amp's signal input (instead of the current PA0
PWM pin), you could play back a short recorded sound stored as raw audio data
in the chip's flash memory.

**Constraints:**
- The chip has 512 KB of flash total, shared with the firmware code (~50 KB used).
  That leaves roughly 460 KB for audio data.
- At 8 kHz mono 8-bit (telephone quality): ~57 seconds of audio.
- At 16 kHz mono 16-bit (clearer): ~14 seconds of audio.
- Requires rewiring PA4 → amp signal input and removing the PA0 connection.
- Requires new firmware: a DMA-driven DAC playback routine to output the sample
  array at the correct sample rate without blocking the super-loop.

This is the only way to play a truly custom recorded sound (a voice clip, a
sound effect, a short music loop). It is a hardware change so it cannot be done
by editing code alone.
