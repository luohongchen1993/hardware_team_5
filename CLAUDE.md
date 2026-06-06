# CLAUDE.md — hardware_team_5

Embedded C firmware for a **"hot/cold" navigation game**: the user walks around
holding a Nucleo-G474RE board; an IMU estimates motion; a servo physically points
toward a hidden virtual target; a speaker beeps faster/higher as the user nears it.
Reach the target → win jingle.

This file is the source of truth for engineering decisions, pinout, build/flash, and
conventions. Read it before writing or changing code; keep it updated when decisions change.

---

## Project history / repo notes

- **Remote:** https://github.com/luohongchen1993/hardware_team_5 (default branch `main`).
  Standing rule: **push all work here.** Pushing account `PatniNayansh` (write access).
- This repo briefly hosted an unrelated **health-monitor** scaffold (vibration / DS18B20 temp /
  HC-SR04 ultrasonic). The team chose the navigation game; the health-monitor code is **preserved
  in history at commit `4837b87`** (and parallel WIP at `7a297b9`). Recover with
  `git checkout 4837b87 -- <path>` if ever needed. Reused from it: the MPU-6050 I2C driver, servo
  PWM mapping, LED abstraction, and the CubeMX-style HAL init.
- `.claude/` and build artifacts are git-ignored.

---

## Assumptions worth confirming on real hardware

1. **Audio = Adafruit STEMMA Speaker (P3885)** on PA0 — a self-contained Class-D amp + 1 W 8 Ohm
   speaker that accepts a plain analog signal (up to supply Vpp, no AC-coupling needed). The PWM
   square-wave tone drives it directly: signal->PA0, Vin->3-5 V, GND->GND. CONFIRMED hardware,
   no code change. (This is the larger STEMMA connector, NOT the I2C "STEMMA QT" variant.)
2. **MPU-6050 I2C address 0x68** (AD0 low). Change `MPU6050_I2C_ADDR` in `config.h` for 0x69.
3. **2D navigation** in the horizontal X-Y plane; target at z=0; servo points a horizontal bearing.
4. **Dead-reckoning position drifts** (consumer MEMS double-integration). Bounded by ZUPT but
   approximate. Heading/bearing is reliable. An external position reference (UWB/optical/encoder)
   would be needed for real accuracy — out of scope for v1.
5. **I2C `Timing = 0x00702991`** is a CubeMX-derived constant (PCLK1 source). If the IMU fails to
   init (red LED, 2 blinks), regenerate `TIMINGR` in CubeMX for the actual I2C kernel clock.
6. **Servo / heading sign**: if the servo points away from the target, flip `SERVO_BEARING_SIGN`
   in `config.h` (−1.0f). Yaw integrates gyro Z; mounting orientation may invert it.

---

## Tech stack — resolved decisions

| Decision | Choice | Rationale |
|---|---|---|
| MCU / board | STM32G474RE / Nucleo-G474RE | Cortex-M4F @ **170 MHz**, FPU, hardware RNG |
| Runtime | **Bare-metal 100 Hz super-loop** (no RTOS) | One periodic task; deterministic, easy to debug |
| Drivers | **STM32 HAL** (CubeMX-style init) | Fastest path; no hot path needs LL (see budget) |
| Language | **C11** (`-std=gnu11`) | designated inits, `stdint`/`stdbool` |
| FP | hardware FPU, `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` | all math single-precision `float` |
| Memory | **no heap / no dynamic allocation** | determinism on a 128 KB-SRAM MCU |
| Loop rate | **100 Hz → 10 ms deadline** | matches IMU ODR; huge compute headroom |
| Debug I/O | `snprintf` + UART over **USART2** (ST-Link VCP), 115200 8N1 | zero extra wiring |

---

## Pin & peripheral map (Nucleo-G474RE)

> Final authority is the CubeMX `.ioc`. APB1 timer kernel clock = 170 MHz (APB1 prescaler /1).

| Function | Peripheral | Pin(s) | Config |
|---|---|---|---|
| IMU (MPU-6050) | **I2C1** | PB8=SCL, PB9=SDA (AF4, OD, pull-up) | 400 kHz, addr **0x68** |
| Servo PWM | **TIM3_CH1** | PA6 (AF2) | PSC=169→1 MHz; ARR=19999→**50 Hz**; CCR = pulse µs (1000–2000) |
| Audio PWM | **TIM2_CH1** | PA0 (AF1) | PSC=169→1 MHz; ARR=(1e6/freq)−1; CCR=(ARR+1)/2 (50%) |
| Status LEDs | GPIO | PB13 green, PB14 amber, PB15 red | push-pull; fault blink codes on red |
| Replay button | GPIO | PC13 (B1) | input; pressed = LOW |
| RNG seed | **RNG** (HSI48) | — | seeds xorshift32; fixed-seed fallback |
| Debug UART | **USART2** | PA2=TX, PA3=RX (AF7) | 115200 8N1 → ST-Link VCP |

Clock: HSI16 → PLL (M/4, N=85, R/2) → **170 MHz**, Range 1 boost, Flash 4 WS; HSI48 → RNG.

---

## Build & flash

✅ **Verified build** — compiles warning-free (`-Wall -Wextra`) with Arm GNU Toolchain 15.2
against STM32CubeG4; ≈ 37 KB flash / 3 KB RAM. Produces `build/navgame.{elf,bin,hex}`.

**One-time tool setup** (Windows, no admin, via [scoop](https://scoop.sh)):

```sh
scoop bucket add extras
scoop install gcc-arm-none-eabi make openocd
git clone --recurse-submodules https://github.com/STMicroelectronics/STM32CubeG4 \
  ~/STM32Cube/Repository/STM32CubeG4     # HAL/CMSIS + startup (uses git submodules)
```

**Build/flash** from this folder in **Git Bash**. Use a *relative* `CUBE_HAL_DIR` — an absolute
Windows path's drive-letter `:` trips up `make`/`gcc`, and MSYS `/c/...` paths aren't understood
by the native gcc:

```sh
make CUBE_HAL_DIR=../../STM32Cube/Repository/STM32CubeG4
make flash CUBE_HAL_DIR=../../STM32Cube/Repository/STM32CubeG4   # OpenOCD + ST-Link
# or: STM32_Programmer_CLI -c port=SWD -w build/navgame.elf -rst
```

We commit the linker script, Makefile, and `Core/Inc/stm32g4xx_hal_conf.h` (HAL module/clock
config — required, copied from the Cube template). HAL driver sources, `system_stm32g4xx.c`, and
`startup_stm32g474xx.s` come from the Cube package via the Makefile. Or import into
**STM32CubeIDE** for one-click build/flash/debug. The newlib `_read/_write/...` "not implemented"
linker warnings are harmless (no runtime file I/O).

---

## Source layout & module responsibilities

```
Core/Inc, Core/Src:
  main.c        HAL init (clock/I2C1/TIM2/TIM3/USART2/RNG/GPIO) + 100 Hz super-loop
  config.h      ALL tunables: rates, thresholds, grid, filter, audio, pins
  mpu6050.*     I2C driver: WHO_AM_I, init, raw int16 read, scaled SI read, gyro calibrate
  navigation.*  complementary filter + dead-reckon + ZUPT + distance/bearing (pure math)
  servo.*       pulse-µs / angle / point-relative-bearing on TIM3_CH1
  audio.*       proximity → pitch + beep cadence on TIM2_CH1; win jingle
  game.*        state machine, PRNG (xorshift32), target placement, win detection
  led.*         bitmask GPIO LED API
  serial_log.*  UART telemetry (state/pos/heading/distance/bearing)
```

---

## Core data structures

```c
typedef enum { STATE_INIT, STATE_CALIBRATE, STATE_SEEKING,
               STATE_WIN, STATE_HALT, STATE_ERROR } GameState;

typedef struct { float x, y, z; } Coordinate;          // metres, world frame; origin=power-up

typedef struct { int16_t ax,ay,az, temp, gx,gy,gz; } MPU6050_Raw;   // raw two's-complement
typedef struct { float ax,ay,az;  float gx,gy,gz; } MPU6050_Scaled; // m/s^2, deg/s
typedef struct { float gx,gy,gz; } MPU6050_Bias;                    // gyro zero-rate (deg/s)

typedef struct {                 // navigation outputs (+ integrator internals)
    Coordinate pos; float vel_x,vel_y; float heading_deg, roll_deg, pitch_deg;
    float distance_m, bearing_deg; uint32_t still_count;
} NavState;
```

Output types (per spec): servo pulse `uint16_t` µs; audio cue `enum AudioCue`/uint8_t;
distance `float` metres.

---

## Sensor scaling (MPU-6050)

Accel **±2 g** (16384 LSB/g → m/s²); Gyro **±500 °/s** (65.5 LSB/°/s). (±500 chosen over the
spec's ±250 so brisk handheld turns don't saturate; resolution still ~0.015 °/s.)

| Reg | Addr | Use |
|---|---|---|
| WHO_AM_I | 0x75 | must read 0x68 |
| PWR_MGMT_1 | 0x6B | 0x00 wake |
| CONFIG | 0x1A | 0x03 DLPF ~44 Hz |
| GYRO_CONFIG | 0x1B | 0x08 = ±500 °/s |
| ACCEL_CONFIG | 0x1C | 0x00 = ±2 g |
| SMPLRT_DIV | 0x19 | 0x07 → 125 Hz ODR |
| ACCEL_XOUT_H | 0x3B | burst 14 B → accel(6)+temp(2)+gyro(6) |

---

## Navigation / sensor fusion

- **Orientation:** complementary filter — roll/pitch from accel gravity vector, yaw from gyro Z
  integration. `angle = α(angle + ω·dt) + (1−α)·accel_angle`, α=0.98.
- **Displacement (demo-grade):** rotate body horizontal accel to world frame by heading, integrate
  twice; **ZUPT** zeroes velocity when |a|≈g and |ω|≈0. Flat-board assumption; metres of drift
  expected — see assumption #4.
- **Distance:** `sqrtf(dx²+dy²)`. **Bearing:** `atan2f(dy,dx)`→[0,360). Servo shows
  `wrap(bearing−heading)` clamped to ±90° (centre = on target).

---

## Error handling & LED blink codes (red = PB15)

Every I2C helper returns `HAL_StatusTypeDef`; reads retry `I2C_RETRY_COUNT` times with bus
DeInit/Init recovery before declaring a fault.

| Code | Meaning | Behaviour |
|---|---|---|
| Red ×2 | MPU-6050 not found / init fail | halt in STATE_ERROR (actuators safe) |
| Red ×3 | I2C runtime/calibration fault after retries | halt in STATE_ERROR |
| Amber ×4 (once) | RNG unavailable → fixed seed | **non-fatal**, continue |
| Red fast strobe | HAL `Error_Handler()` (init failure) | halt |

---

## Timing & complexity (per 100 Hz cycle, budget = 10 ms @ 170 MHz)

Every per-cycle op is **O(1) time, O(1) space** — no loops over data, no allocation.

| Step | Est. cost |
|---|---|
| I2C burst read 14 B | ~0.4 ms (dominant) |
| scale + complementary filter | < 20 µs |
| dead-reckon + ZUPT | < 10 µs |
| distance/bearing (`sqrtf`,`atan2f` on FPU) | < 15 µs |
| servo + audio register writes | < 10 µs |
| **total compute** | **< 0.5 ms** → ~95% idle headroom |

Loop is paced by `HAL_GetTick()` (SysTick) to LOOP_HZ; no path is data-dependent in length, so
worst case = typical. (CALIBRATE and the WIN jingle are intentionally blocking, one-shot, outside
the seeking loop.)

---

## Coding conventions

- C11, no heap, no recursion, no VLAs. Fixed-size buffers only.
- HW functions return `HAL_StatusTypeDef`; pure logic returns by value / out-params.
- `snake_case` funcs/vars, `PascalCase` typedefs, `UPPER_SNAKE` macros.
- All tunables in `config.h`; no magic numbers in logic. Comment each functional block.
- `float` for math (FPU makes it free vs fixed-point Q15, and it's clearer).

---

## State machine

```
INIT → CALIBRATE (gyro bias, origin, place target) → SEEKING
SEEKING @100Hz: read IMU → fuse → servo(rel bearing) → audio(distance) → win check
SEEKING → WIN (distance < WIN_RADIUS_M): jingle, servo centred, LEDs celebrate
WIN → SEEKING on B1 press (new target, reset estimate)
any → ERROR (blink code, actuators safe) on unrecoverable I2C fault
```

## Open TODO

- [x] Toolchain + HAL set up; firmware builds clean (Arm GNU 15.2 + STM32CubeG4).
- [ ] Flash to the board and verify on hardware: servo/heading sign, I2C `TIMINGR`.
- [ ] Optional: commit a CubeMX `.ioc`; PC-side visualiser fed by the UART telemetry.
```
