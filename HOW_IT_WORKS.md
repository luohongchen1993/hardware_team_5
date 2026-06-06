# How the Navigation Game Firmware Works

Target hardware: **STM32G474RE** on a Nucleo-G474RE development board.
The user walks around holding the board. A servo physically points toward a
hidden virtual target and a speaker beeps faster/higher as they get closer.
Reach the target → win jingle → press the button to play again.

---

## Big picture

The firmware is written in plain C with no operating system. A single loop
runs 100 times per second. Each iteration reads the IMU sensor, estimates
where the user is in the room, updates the servo and speaker, and checks if
they've found the target.

```
Power on
  → Clock/peripheral setup
  → IMU identity check (halt with 2 red blinks on fail)
  → 2-second calibration (hold still) → green LED on
  → Place a random target somewhere in a ~40 ft × 40 ft virtual grid
  → 100 Hz game loop
       Read IMU → update position estimate
       → point servo toward target
       → beep faster/louder as distance shrinks
       → win when within ~1 ft
  → Win jingle, servo centres, press B1 to play again
```

---

## File-by-file breakdown

### `main.c` — Boot and super-loop

The entry point. It does four things and nothing else:

1. **Turn on the clocks.** The internal 16 MHz oscillator feeds a PLL that
   multiplies it up to 170 MHz. A separate 48 MHz oscillator feeds the
   random-number generator.
2. **Configure every peripheral** (I2C, two timer PWM outputs, UART, RNG, GPIO).
3. **Hand control to the game** via `Game_Init()`.
4. **Spin forever** calling `Game_Tick()` as fast as possible. The game module
   itself paces the loop to exactly 100 Hz.

The `Error_Handler` at the bottom is called by any peripheral that fails to
configure at boot — it disables interrupts and strobes the red LED forever,
signalling a wiring or hardware fault.

---

### `config.h` — All magic numbers live here

Nothing in the logic files is allowed to contain a hardcoded number. Every
threshold, pin assignment, frequency, and timing constant is defined here with
a name. Changing the game's behaviour (win radius, beep speed, servo travel,
etc.) means editing this one file.

Key constants:

| Name | Value | What it controls |
|---|---|---|
| `LOOP_HZ` | 100 | How many times per second the game updates |
| `GRID_HALF_EXTENT_M` | 6.1 m (~20 ft) | How far away targets can be placed |
| `WIN_RADIUS_M` | 0.3 m (~1 ft) | How close to win |
| `COMP_FILTER_ALPHA` | 0.98 | How much the gyro dominates orientation (vs accel) |
| `CALIB_SAMPLES` | 400 | How many samples to average at startup for bias |
| `AUDIO_FREQ_FAR/NEAR_HZ` | 300 / 2000 Hz | Pitch range for the proximity beep |
| `SERVO_PULSE_MID_US` | 1500 µs | Servo centred = pointing straight ahead |

---

### `mpu6050.c` — IMU driver (hardware access)

The MPU-6050 is a 6-axis inertial measurement unit: three axes of accelerometer
(measures force, including gravity) and three axes of gyroscope (measures rotation rate).
It communicates over I2C at 400 kHz.

**Init sequence:** Read the WHO_AM_I register (should be 0x68) to confirm the chip
is alive, then write five configuration registers to set ranges (±2 g accel,
±500 deg/s gyro), apply a 44 Hz digital low-pass filter, and set 125 Hz output rate.

**Reading data:** Every call bursts 14 bytes starting at register 0x3B: six bytes
of accelerometer, two bytes of temperature (read but discarded), six bytes of gyroscope.
The driver retries up to 3 times on I2C errors, attempting a bus DeInit/Init recovery
between attempts.

**Calibration:** At startup the device averages 400 samples taken over ~2 seconds
while the board is held still. This measures the gyro's zero-rate bias (the output
it produces even when stationary, typically a few tenths of a degree/second). That
bias is subtracted from every subsequent gyro reading.

---

### `navigation.c` — Sensor fusion and position estimate (pure math, no hardware)

Takes scaled IMU data as input, produces position and heading as output.
No hardware access — all floating-point math. Called once per 100 Hz tick.

**Heading (compass direction):**

Heading (yaw) is integrated from the gyroscope's Z axis each tick (the zero-rate
bias is measured and removed during calibration). There is no magnetometer, so
heading has no absolute reference and drifts slowly over minutes — acceptable for
a few-minute game.

**Position — step dead-reckoning (a pedometer):**

Rather than double-integrating acceleration (which diverges in seconds on a
consumer MEMS sensor), the position is built from counted footsteps:

- A slow low-pass of the accelerometer *magnitude* tracks gravity (~9.81 m/s²).
- The dynamic part (magnitude − baseline) spikes on every footfall.
- A step is counted when that spike crosses a threshold, with hysteresis (it must
  fall back down before the next step counts) and a refractory window (~250 ms
  minimum between steps) so a single footfall is never double-counted.
- Each counted step advances the position by one stride (`STRIDE_M`, default
  0.65 m) along the current heading.

Because every step is a bounded, discrete update, position error does **not**
snowball, and holding the board tilted does not inject phantom motion (magnitude
is orientation-independent). `STRIDE_M` and `STEP_ACCEL_THRESH_MS2` in `config.h`
tune stride length and step sensitivity per user/board.

**Distance and bearing:**

```
dx = target.x - pos.x
dy = target.y - pos.y
distance = sqrt(dx² + dy²)
bearing  = atan2(dy, dx)          → absolute world-frame angle to target
relative = bearing − heading      → angle from where I'm facing to the target
```

The relative bearing is what the servo uses: 0° means the target is straight ahead,
+90° means hard right, −90° means hard left.

---

### `servo.c` — Servo pointer

The servo is an SG90-class hobby servo driven by a 50 Hz PWM signal on TIM3.
A 1 ms pulse = full left (0°), 1.5 ms = centre (90°), 2 ms = full right (180°).
The timer prescaler turns the 170 MHz clock into a 1 MHz tick so that the CCR
register value equals the pulse width in microseconds directly — simple arithmetic.

`Servo_PointBearing` maps the relative bearing angle (clamped to ±90°) onto the
1000–2000 µs pulse range. Centre (1500 µs) means the target is straight ahead.
The `SERVO_BEARING_SIGN` config flag inverts the direction if the servo is
mounted backwards.

---

### `audio.c` — Proximity speaker

The speaker (Adafruit STEMMA Class-D amp) is driven by a PWM square wave on TIM2.
Changing the timer's auto-reload register changes the period → changes the frequency.
The compare register is always set to half the period (50% duty cycle), producing
a clean square wave.

A **proximity value p** is computed each tick: 0.0 when at the edge of the 9m
sensing range, 1.0 when at the 0.3 m win threshold.

- **Pitch** scales linearly from 300 Hz (far) to 2000 Hz (close).
- **Beep cadence** scales from one beep every 800 ms (far) to one beep every
  90 ms (close), Geiger-counter style.

The on/off toggling is non-blocking — it checks elapsed time with `HAL_GetTick()`
and flips the tone on/off without ever sleeping or blocking the main loop.

The **win jingle** is a blocking ascending arpeggio (C5-E5-G5-C6) played once
when the target is reached. Blocking is acceptable there because gameplay has
already stopped.

The function returns an `AudioCue` enum (FAR / MID / NEAR / VERY_NEAR) that the
game uses to decide whether to also pulse the amber LED.

---

### `game.c` — State machine and orchestration

The central coordinator. Owns all module-level state: peripheral handles, the
nav estimator, the current target position, the PRNG.

**States:**

```
INIT       → transient; peripherals initialising in Game_Init()
CALIBRATE  → blocking ~2 s, collects at-rest bias, places first target, → SEEKING
SEEKING    → 100 Hz gameplay: read → fuse → servo → audio → win check
WIN        → play jingle, wait for B1 press, then reset and → SEEKING
ERROR      → actuators safed, red LED blinks forever (unrecoverable I2C fault)
```

**Target placement:** The PRNG (xorshift32) generates a random X,Y coordinate
within the ±6.1 m grid. It is seeded from the hardware RNG at startup; if the
hardware RNG is unavailable, a fixed seed (0x00C0FFEE) is used and the amber LED
blinks 4 times as a non-fatal warning.

**Win replay:** Pressing B1 calls `Nav_Init` to reset the position/heading estimate
to zero, reads the current accel to re-seed the gravity baseline, and places a new
random target. The game immediately resumes SEEKING.

**Loop pacing:** `Game_Tick()` is called from `main()`'s infinite loop as fast as
possible. The SEEKING state checks `HAL_GetTick()` and returns early if fewer than
10 ms have elapsed since the last cycle, enforcing exactly 100 Hz.

---

### `led.c` — LED bitmask API

Thin wrapper over `HAL_GPIO_WritePin`. Three LEDs on GPIOB (PB13 green, PB14 amber,
PB15 red) are controlled by a 3-bit mask (`LED_GREEN | LED_AMBER | LED_RED`).
The GPIO pins are configured in `main.c`; this file only sets/clears/toggles them.

**LED meanings during gameplay:**
- Green solid: seeking
- Amber pulse: target is getting close (within ~45% of audio range)
- Red blink 2×: MPU-6050 not found / init failed
- Red blink 3×: I2C runtime fault after all retries
- Amber blink 4× (once): using fixed PRNG seed, hardware RNG unavailable

---

### `serial_log.c` — UART telemetry

Formats a ~160-character line every 10 game ticks (10 Hz) and transmits it
over USART2 at 115200 baud to the ST-Link USB virtual COM port. Contains:
timestamp, state, XY position, heading, distance to target, bearing, and
audio cue level. Useful for debugging navigation behaviour from a PC terminal.

---

## Data flow per 100 Hz tick (SEEKING state)

```
MPU6050_ReadScaled()
    ↓ MPU6050_Scaled (ax,ay,az m/s², gx,gy,gz deg/s)
Nav_Update()
    ↓ NavState (pos.x, pos.y, heading_deg, distance_m, bearing_deg)
Nav_RelativeBearing()
    ↓ float degrees (-90 to +90)
Servo_PointBearing()        Audio_Update()
    ↓ TIM3 CCR updated          ↓ TIM2 ARR/CCR updated, returns AudioCue
LED_Set() ← cue level ──────────┘
SerialLog_PrintNav() ← every 10th tick
win check: distance_m < WIN_RADIUS_M → STATE_WIN
```

---

## Bugs and issues found

### 1. Relative bearing in telemetry is not angle-wrapped (minor)

**File:** `serial_log.c`, line 62

**Problem:** The logged relative bearing is computed as
`n->bearing_deg - n->heading_deg` without wrapping to the (−180, 180] range.
If the heading is 350° and the bearing is 10°, the log prints −340° instead of
the correct 20°. The servo uses `Nav_RelativeBearing()` which does wrap correctly,
so this is a display-only bug — game behaviour is unaffected.

**Fix:**
```c
// Before (wrong):
(double)(n->bearing_deg - n->heading_deg),

// After (correct):
(double)Nav_RelativeBearing(n),
```

---

### 2. `AUDIO_CUE_WIN` is a dead enum value

**File:** `audio.h` (declaration), `serial_log.c` (handled but never reached)

**Problem:** `AudioCue` defines five values: FAR, MID, NEAR, VERY_NEAR, WIN.
`Audio_Update()` only ever returns the first four. Once the game reaches the WIN
state, `Audio_Update` is never called again (the win jingle is a separate blocking
function). `AUDIO_CUE_WIN` can never be returned. The `cue_name()` helper in
`serial_log.c` has a branch for it that can never execute.

**Impact:** None functional. Minor dead code.

**Fix (optional):** Remove `AUDIO_CUE_WIN` from the enum and the `cue_name` switch,
or have `Audio_WinJingle` return `AUDIO_CUE_WIN` so the enum is used.

---

### 3. WHO_AM_I mask accepts both MPU-6050 address variants

**File:** `mpu6050.c`, line 56

**Problem:**
```c
if (s != HAL_OK || (who & 0x7E) != 0x68) return HAL_ERROR;
```
The mask `0x7E` strips bit 0 and bit 7. This means a chip returning `0x69`
(AD0 pin wired high) also passes the check (`0x69 & 0x7E == 0x68`). If the chip
is physically wired with AD0 high but `MPU6050_I2C_ADDR` in config.h is still
the 0x68 address, init appears to succeed yet all subsequent reads/writes target
the wrong I2C address and will NACK. In practice, the `read_regs` call itself
would likely fail before reaching this comparison, so this is mostly a latent
confusion point rather than a silent failure.

**Fix (optional):** If you only ever use the 0x68 address, tighten the check:
```c
if (s != HAL_OK || who != 0x68) return HAL_ERROR;
```

---

### 4. I2C error handling is inconsistent between SEEKING and WIN replay

**File:** `game.c`, lines 128–130 vs line 174–176

**Problem:** In `do_seek()`, any I2C error calls `error_halt(3)` (red LED blink,
firmware halts). In `do_win()` during replay, an I2C error silently falls through
to `Nav_Init(&s_nav, NULL)` and the game resets without halting. The inconsistency
means a bus fault that would halt the game during play is silently swallowed at
the replay point.

```c
// do_seek — correct, halts on fault:
if (MPU6050_ReadScaled(...) != HAL_OK) error_halt(3);

// do_win — inconsistent, swallows the fault:
if (MPU6050_ReadScaled(...) == HAL_OK) Nav_Init(&s_nav, &s);
else                                   Nav_Init(&s_nav, NULL);  // silent
```

**Fix:** Whether to halt or continue on a bus fault during replay is a design
choice. If you want consistent behaviour, either log a warning and continue, or
call `error_halt(3)` here too. The current silent fallback at least produces a
valid reset (just without the accel seed for tilt), so it is not dangerous.

---

### 5. Nav heading resets to 0° on every replay

**File:** `navigation.c`, line 45

**Problem:** `Nav_Init` sets `n->heading_deg = 0.0f`. This means every time the
user presses B1 to replay, north is redefined as whichever direction they happen
to be facing. The target is then placed relative to that new "north." This is not
a bug per se — the coordinate system intentionally resets — but it means two back-
to-back games are not spatially consistent and the user cannot build intuition
about where targets appear in the physical room.

**Impact:** By design (fresh game each replay), but worth knowing if you later
want persistent coordinate framing.

---

### 6. `Makefile` object file name collision risk (latent)

**File:** `Makefile`, lines 99–103

**Problem:** The Makefile strips directory paths from object file names with
`$(notdir ...)`. If two source files in different directories share the same
filename (e.g., a local `utils.c` and a HAL `utils.c`), their `.o` files would
silently overwrite each other in the `build/` directory, producing a corrupt link
with no error message.

Currently there is no collision (all HAL files have distinct names from app files),
so this is a latent risk, not an active bug. It is the standard STM32 Makefile
pattern and only matters if you add files with clashing names.

---

## Summary table

| # | Severity | File | Issue |
|---|---|---|---|
| 1 | Low (display only) | `serial_log.c:62` | Relative bearing in log not wrapped to (−180, 180] |
| 2 | Cosmetic | `audio.h` | `AUDIO_CUE_WIN` defined but never produced |
| 3 | Low (latent) | `mpu6050.c:56` | WHO_AM_I mask accepts 0x69 chips silently |
| 4 | Low (inconsistency) | `game.c:175` | I2C fault during replay swallowed; seek halts, win does not |
| 5 | Design note | `navigation.c:45` | Heading resets to 0° on every replay |
| 6 | Latent | `Makefile` | Object-name collision if two sources share a filename |
