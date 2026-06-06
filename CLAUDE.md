# CLAUDE.md — hardware_team_5

Embedded C firmware for a **"hot/cold" navigation game**: the user walks around holding a
Nucleo-G474RE board; an IMU estimates their motion; a servo physically points toward a hidden
virtual target; a speaker beeps faster/higher as they get closer. Reach the target → win jingle.

This file is the single source of truth for engineering decisions, pinout, build/flash, and
conventions. Read it before writing or changing code. Keep it updated when decisions change.

---

## ⚠️ Assumptions to confirm before hardware bring-up

I made concrete calls on every ambiguous point so we can move fast. Skim these — correct me if
your physical hardware differs, then I'll adjust the relevant module (all are isolated behind a
driver layer, so changes are local).

1. **Audio = PWM square-wave tone to a small speaker/piezo** (not an I2C audio chip). "STEMMA QT"
   is electrically just I2C, and there is no common drop-in I2C "speaker." Variable-pitch
   reinforcement beeps are trivial and rock-solid over PWM. If your module is actually an I2C DAC
   (e.g. MCP4725) or I2S amp, say so — `audio.c` is abstracted behind `audio_set_tone()` so only
   that file changes.
2. **MPU-6050 I2C address = 0x68** (AD0 tied low — the default). Use 0x69 if AD0 is high.
3. **2D navigation** in the horizontal X-Y plane (top-down). `z` exists in structs but the target
   sits at z=0 and the servo points a horizontal bearing. 3D is a later stretch.
4. **Dead-reckoning position is demo-grade and WILL drift.** Double-integrating consumer-MEMS
   accelerometer noise diverges in seconds. We bound it with zero-velocity updates (ZUPT) but do
   not expect survey-grade accuracy. Heading/bearing (gyro+accel) is solid; absolute displacement
   is the weak link. See [Navigation](#navigation--sensor-fusion). If this matters, we should add
   an external position reference (UWB/optical/encoder) — out of scope for v1.

---

## Tech stack — resolved decisions

| Decision | Choice | Rationale |
|---|---|---|
| MCU / board | STM32G474RE / Nucleo-G474RE | Cortex-M4F @ **170 MHz**, single-precision FPU, has hardware RNG |
| Runtime model | **Bare-metal fixed-rate super-loop** (no RTOS) | One periodic task; deterministic 100 Hz tick is simpler to reason about and debug under hackathon time pressure than FreeRTOS scheduling |
| Driver layer | **STM32 HAL** (CubeMX-generated base) | Fastest path to working peripherals; LL only if a hot path needs it (none expected — see timing budget) |
| Language | **C11**, `-std=gnu11` | Designated initializers, `stdint`, `stdbool`, `static_assert` |
| FP | Hardware FPU, `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` | All math is single-precision `float` |
| Memory | **No heap / no dynamic allocation.** All state static or stack. | Determinism; avoids fragmentation on a 128 KB-SRAM MCU |
| Main loop rate | **100 Hz → 10 ms deadline** per cycle | Matches IMU sample rate; ample headroom (see budget) |
| Debug I/O | `printf` over **USART2** (PA2/PA3 = ST-Link VCP), 115200 8N1 | Zero extra wiring; telemetry of state/distance/bearing |

---

## Pin & peripheral map (Nucleo-G474RE)

> Defaults below. Final authority is the CubeMX `.ioc` clock tree + pinout — verify there.
> APB1 timer kernel clock is assumed **170 MHz** (APB1 prescaler /1, or /2 with timer-clock doubling).

| Function | Peripheral | Pin(s) | Config |
|---|---|---|---|
| IMU (MPU-6050) | **I2C1** | PB8=SCL, PB9=SDA | Fast mode 400 kHz, 7-bit addr **0x68**, internal pull-ups + 4.7k externals if needed |
| Servo PWM | **TIM2_CH1** | PA0 (CN8/A0) | PSC=169 → 1 MHz tick (1 µs). ARR=19999 → 20 ms = **50 Hz**. CCR1 = pulse in µs (1000–2000) |
| Audio PWM | **TIM3_CH1** | PA6 (CN10/D12) | PSC=169 → 1 MHz tick. ARR=(1e6/freq)−1, CCR1=(ARR+1)/2 (50% duty). Freq set per proximity |
| Status LED | GPIO (LD2) | **PA5** | On-board green LED; used for blink fault codes (see below). NOTE: shared with TIM2_CH1 alt — that's why servo is on TIM2_CH1 **PA0**, not PA5 |
| RNG seed | **RNG** peripheral | — | Seeds the deterministic PRNG at init; fallback fixed seed if unavailable |
| Debug UART | **USART2** | PA2=TX, PA3=RX | 115200 8N1, routed to ST-Link Virtual COM Port |

CubeMX peripherals to enable: I2C1, TIM2 (PWM Gen CH1), TIM3 (PWM Gen CH1), RNG, USART2 (Async),
GPIO PA5 output. System clock: HSI → PLL → **170 MHz** SYSCLK, Flash 4 WS, boost mode on.

---

## Repository & Git workflow

- **Remote:** `origin` → https://github.com/luohongchen1993/hardware_team_5 (default branch `main`).
- **Standing rule from the team: push everything we make to this repo.** Commit working increments
  and push to `origin/main` as features land. (Pushing is pre-authorized for this project.)
- Pushing account: `PatniNayansh`. If commit authorship should be a different teammate, set a
  repo-local `user.name`/`user.email`.
- `.claude/` and build artifacts are git-ignored — never commit them.
- Co-author trailer on commits: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## Build & flash

CubeMX project type: **Makefile** (Project Manager → Toolchain/IDE → "Makefile"), so we can build
from the CLI. Toolchain: `arm-none-eabi-gcc` (Arm GNU Toolchain). Flash via STM32CubeProgrammer CLI.

```sh
make                # build → build/hardware_team_5.elf (+ .bin/.hex)
make clean
# Flash (Windows: ensure STM32_Programmer_CLI is on PATH, ST-Link plugged in):
STM32_Programmer_CLI -c port=SWD -w build/hardware_team_5.elf -rst
# Serial telemetry (115200): use any terminal on the ST-Link VCP (e.g. `pyserial`/PuTTY).
```

Alternative: import into **STM32CubeIDE** for one-click build/flash/debug. Either works; the
generated HAL/init code is the same. CubeMX regenerates `Core/` init code — keep application logic
in our own files (see structure) so regeneration never clobbers it (or guard with USER CODE blocks).

---

## Project structure

```
CLAUDE.md                 ← this file
README.md
Makefile                  ← CubeMX-generated
hardware_team_5.ioc       ← CubeMX project (pinout/clocks) — edit peripherals here
Core/
  Inc/
    main.h
    config.h              ← all tunables: rates, thresholds, grid size, pin macros
    mpu6050.h
    servo.h
    audio.h
    navigation.h
    game.h
  Src/
    main.c                ← HAL init + 100 Hz super-loop calling game_tick()
    mpu6050.c             ← I2C read/write helpers, init, calibrate, read-scaled
    servo.c               ← bearing(°) → pulse(µs) → TIM2 CCR
    audio.c               ← proximity → tone freq/cadence → TIM3; win jingle
    navigation.c          ← complementary filter, dead-reckoning + ZUPT, distance/bearing
    game.c                ← state machine, grid/target gen, PRNG, win detection
Drivers/                  ← STM32 HAL + CMSIS (CubeMX-generated; do not hand-edit)
build/                    ← artifacts (git-ignored)
```

---

## Module responsibilities

- **mpu6050** — All I2C. Every helper returns `HAL_StatusTypeDef`. Init verifies `WHO_AM_I==0x68`,
  wakes the device, sets ranges/DLPF. Calibration averages N at-rest samples for gyro bias (and
  the gravity vector). `mpu6050_read_scaled()` fills SI-unit accel (m/s²) and gyro (°/s).
- **navigation** — Pure math, no HW. Complementary filter for orientation; rotate body accel to
  world frame; remove gravity; integrate to velocity/position with ZUPT; compute Euclidean distance
  and bearing to target. Holds the estimated `Coordinate`.
- **servo** — Maps **relative bearing error** (target bearing − current heading, wrapped to
  (−180,180], clamped to ±90°) to a 1000–2000 µs pulse. Center 1500 µs = "straight ahead /
  on target"; 1000 µs = hard left; 2000 µs = hard right. Writes `TIM2->CCR1`.
- **audio** — Maps proximity to (a) **pitch** (e.g. 300 Hz far → 2 kHz near) and (b) **beep
  cadence** (slow → Geiger-counter-fast). `audio_win_jingle()` plays the victory sequence.
  Single entry point `audio_update(distance_m)`; silence via `audio_off()`.
- **game** — Owns `GameState`, the grid, the target coordinate, and the deterministic PRNG
  (xorshift32 seeded from RNG peripheral). `game_init()` calibrates + places target;
  `game_tick(dt)` runs sensor→nav→servo→audio→win-check each cycle.

---

## Core data structures

```c
typedef enum {
    STATE_INIT = 0,   // power-up, peripherals coming online
    STATE_CALIBRATE,  // averaging at-rest IMU samples; establishing origin/reference
    STATE_SEEKING,    // active gameplay: navigate toward target
    STATE_WIN,        // target reached; play jingle, servo centered & held
    STATE_HALT,       // user-requested stop / awaiting reset
    STATE_ERROR       // unrecoverable fault; LED blink code active
} GameState;

typedef struct { float x, y, z; } Coordinate;             // meters, world frame; origin = power-up pose

typedef struct {                                          // raw two's-complement from MPU-6050
    int16_t ax, ay, az;   // accelerometer
    int16_t temp;         // on-die temp (read but unused)
    int16_t gx, gy, gz;   // gyroscope
} MPU6050_Raw;

typedef struct {                                          // SI units after scaling + bias removal
    float ax, ay, az;     // m/s^2
    float gx, gy, gz;     // deg/s
} MPU6050_Scaled;

typedef struct {                                          // outputs of one nav update
    Coordinate pos;       // estimated position (m)
    float heading_deg;    // yaw vs origin (deg)
    float distance_m;     // Euclidean distance to target (m)
    float bearing_deg;    // absolute bearing to target (deg, world frame)
} NavState;
```

Output types (per spec): servo pulse = `uint16_t` µs; audio cue level = `enum`/`uint8_t`;
distance = `float` meters.

---

## Sensor scaling (MPU-6050, ±2g / ±250°/s)

```c
#define ACCEL_LSB_PER_G     16384.0f          // ±2g range
#define GRAVITY_MS2         9.80665f
#define GYRO_LSB_PER_DPS    131.0f            // ±250°/s range
// accel_mps2 = (raw / ACCEL_LSB_PER_G) * GRAVITY_MS2 - bias
// gyro_dps   =  raw / GYRO_LSB_PER_DPS - bias
```

### MPU-6050 register map (the ones we touch)

| Reg | Addr | Use |
|---|---|---|
| WHO_AM_I | 0x75 | must read 0x68 (identity check) |
| PWR_MGMT_1 | 0x6B | 0x00 wake; or 0x01 = PLL w/ X-gyro clock |
| SMPLRT_DIV | 0x19 | sample-rate divider |
| CONFIG | 0x1A | DLPF (digital low-pass) setting |
| GYRO_CONFIG | 0x1B | 0x00 = ±250°/s |
| ACCEL_CONFIG | 0x1C | 0x00 = ±2g |
| ACCEL_XOUT_H | 0x3B | burst-read 14 bytes → accel(6)+temp(2)+gyro(6) |

---

## Navigation / sensor fusion

- **Orientation:** complementary filter. Roll/pitch from the accelerometer gravity vector; yaw
  (heading) from gyro Z integration. `angle = α·(angle + gyro·dt) + (1−α)·accel_angle`, α≈0.98.
- **Displacement (dead-reckoning, demo-grade):** rotate body-frame accel into world frame using the
  orientation estimate, subtract gravity, integrate twice (a→v→p) with `dt`. **ZUPT:** when accel
  magnitude ≈ 1 g and gyro ≈ 0 for a short window, force velocity to 0 to bound drift. Honest
  expectation: meters of drift over tens of seconds. (See assumption #4.)
- **Distance:** `sqrtf((tx−x)² + (ty−y)²)` (2D). **Bearing:** `atan2f(ty−y, tx−x)` → degrees,
  normalized to [0,360). Servo shows `wrap(bearing − heading)` clamped to ±90°.

---

## Error handling & LED (LD2 / PA5) blink codes

Every I2C helper returns `HAL_StatusTypeDef`. On `HAL_ERROR`/`HAL_TIMEOUT`: retry up to
`I2C_RETRY_COUNT` (default 3); if still failing, transition to `STATE_ERROR` and run the blink code.
NACK and timeout are distinguished via `HAL_I2C_GetError()`.

| Code | Meaning | Pattern on LD2 |
|---|---|---|
| 2 blinks | MPU-6050 not found / WHO_AM_I mismatch | 2 short, pause, repeat |
| 3 blinks | I2C runtime timeout/NACK after retries | 3 short, pause, repeat |
| 4 blinks | RNG unavailable (fell back to fixed seed) — non-fatal, logs & continues | 4 short, once |
| solid ON | WIN reached | steady |
| fast strobe | Fatal `Error_Handler()` / unrecoverable HALT | ~10 Hz continuous |

Fallback policy: sensor init failure ⇒ halt in `STATE_ERROR` (no servo/audio actuation). RNG
failure ⇒ warn (code 4), seed PRNG with a fixed constant, continue.

---

## Timing & complexity (per 100 Hz cycle, budget = 10 ms @ 170 MHz)

Every per-cycle operation is **O(1) time and O(1) space** — no loops over data, no allocation.

| Step | Est. cost | Notes |
|---|---|---|
| I2C burst read 14 B @ 400 kHz | ~0.4 ms | dominant I/O; could go FM+ 1 MHz if needed |
| Scale + bias | < 5 µs | a few mults |
| Complementary filter | < 10 µs | FPU; a few trig calls |
| Dead-reckon + ZUPT | < 10 µs | integrators |
| Distance/bearing | < 15 µs | one `sqrtf`, one `atan2f` on FPU |
| Servo CCR write | < 1 µs | register store |
| Audio update | < 5 µs | ARR/CCR store |
| **Total compute** | **< 0.5 ms** | ~95% of the 10 ms cycle is idle/wait — huge headroom |

Loop cadence is enforced by a SysTick/timer time-base (busy-wait or WFI until next 10 ms tick), not
by accumulated work, so jitter stays low. No path is data-dependent in length ⇒ worst case = typical.

---

## Coding conventions

- C11, no heap, no recursion, no VLAs. Fixed-size buffers only; `static_assert` sizes where useful.
- I2C/HW functions return `HAL_StatusTypeDef`; pure logic returns by value or via out-params.
- `snake_case` functions/vars, `PascalCase` typedefs, `UPPER_SNAKE` macros/consts.
- All tunables live in `config.h` (rates, thresholds, grid extent, filter α, tone range, pins).
- Each functional block carries a comment explaining *what* and *why* (per spec).
- `float` everywhere for math; the FPU makes it free vs fixed-point, and it's clearer. (Q15 noted
  in spec as an option — not used unless we hit a measured perf/precision issue.)
- No magic numbers in logic — promote to named `config.h` macros.

### Key constants (config.h)
```c
#define LOOP_HZ            100
#define LOOP_DT_S          (1.0f / LOOP_HZ)
#define GRID_HALF_EXTENT_M 6.096f     // ~20 ft each cardinal direction from origin
#define WIN_RADIUS_M       0.3048f    // ~1 ft win threshold (configurable)
#define COMP_FILTER_ALPHA  0.98f
#define SERVO_PULSE_MIN_US 1000       // 0°  (hard left)
#define SERVO_PULSE_MID_US 1500       // 90° (straight ahead / on target)
#define SERVO_PULSE_MAX_US 2000       // 180°(hard right)
#define AUDIO_FREQ_FAR_HZ  300
#define AUDIO_FREQ_NEAR_HZ 2000
#define I2C_RETRY_COUNT    3
#define MPU6050_ADDR_7B    0x68
```

---

## Behavioral spec — state machine

```
INIT ─ peripherals up ─▶ CALIBRATE ─ origin set + target placed ─▶ SEEKING
SEEKING (every 10 ms): read IMU → fuse → nav → drive servo → audio_update(distance)
SEEKING ─ distance < WIN_RADIUS_M ─▶ WIN (jingle, servo centered & held, LED solid)
WIN ─ user input (button/UART) ─▶ HALT or back to CALIBRATE (re-roll target)
any state ─ unrecoverable fault ─▶ ERROR (blink code, actuators safe/off)
```

- **Init:** sample accel+gyro to set reference orientation and origin (0,0,0). Build the grid
  (±`GRID_HALF_EXTENT_M`). Place target at a deterministic-random coord (RNG-seeded xorshift32).
- **Runtime:** as above at 100 Hz.
- **Win:** distance < threshold → win jingle, stop servo motion, optionally await reset.

---

## Open TODO / not-yet-decided

- [ ] Confirm the 4 assumptions at top (esp. audio hardware, IMU address).
- [ ] Reset trigger in WIN state: on-board B1 button (PC13) vs a UART keypress? (default: B1)
- [ ] Whether to expose live telemetry framing over UART for a PC-side visualizer.
- [ ] Generate the CubeMX base project (`.ioc`, HAL init, Makefile) — needed before app code builds.
```
