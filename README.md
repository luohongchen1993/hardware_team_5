# Hot/Cold Navigation Game · STM32G474RE

![Language](https://img.shields.io/badge/language-C11-blue.svg)
![Platform](https://img.shields.io/badge/platform-STM32G474RE-red.svg)
![Architecture](https://img.shields.io/badge/core-Cortex--M4F%20%40%20170%20MHz-orange.svg)
![Loop Rate](https://img.shields.io/badge/loop-100%20Hz%20bare--metal-brightgreen.svg)
![Build](https://img.shields.io/badge/build-37%20KB%20flash%20%7C%203%20KB%20RAM-lightgrey.svg)

> A physical treasure-hunt game running entirely on a microcontroller. Walk around holding the board — a **servo physically points toward a hidden virtual target** and a **speaker beeps faster and higher as you close in**. Reach it and a victory jingle plays. Press a button to go again.

---

## How it feels to play

You power on the board, hold it still for two seconds while it calibrates, and the green LED lights up. A random target has just been placed somewhere in a 40 × 40 ft virtual grid around you.

The servo arm is your compass — when it points straight ahead you are headed directly at the target; when it swings sideways, you need to turn. The speaker is your radar — slow low thumps when you are cold, building to a rapid high-pitched chatter when you are burning hot. Get within one foot and the board erupts in a four-note fanfare and a full LED celebration.

Press B1, and the game resets with a fresh target.

---

## Hardware

| # | Component | Notes |
|---|---|---|
| 1 | **STM32 Nucleo-64 (G474RE)** | The main board — provides MCU, ST-Link debugger, USB-serial |
| 2 | **MPU-6050 / GY-521** | 6-axis IMU (accelerometer + gyroscope) over I2C |
| 3 | **SG90-class servo** | Physically points toward the target |
| 4 | **Adafruit STEMMA Speaker P3885** | Self-contained Class-D amp + 1 W speaker; accepts a direct PWM signal |
| 5 | **3× LEDs** (green, amber, red) | Status and proximity feedback (optional but recommended) |
| 6 | **3× 330 Ω resistors** | Current limiting for LEDs |
| — | USB cable (micro-B) | Power + programming + serial telemetry — one cable does everything |

> The HC-SR04 ultrasonic sensor referenced in older docs is **not used**. This is the navigation-game firmware only.

---

## Wiring

All grounds must share a common GND rail. Servo and speaker power on 5 V; the MPU-6050 on 3.3 V.

| Signal | Chip pin | Arduino / Morpho label | Connects to |
|---|---|---|---|
| I2C clock | PB8 | **D15** (CN5) | MPU-6050 **SCL** |
| I2C data | PB9 | **D14** (CN5) | MPU-6050 **SDA** |
| Servo PWM | PA6 | **D12** (CN5) | Servo **signal** wire |
| Audio PWM | PA0 | **A0** (CN8) | STEMMA Speaker **SIG** |
| Green LED | PB13 | Morpho CN10-30 | LED anode → 330 Ω → pin |
| Amber LED | PB14 | Morpho CN10-28 | LED anode → 330 Ω → pin |
| Red LED | PB15 | Morpho CN10-26 | LED anode → 330 Ω → pin |
| Replay button | PC13 | **B1** (on-board) | Already wired |
| Debug UART | PA2 / PA3 | **D1 / D0** (CN9) | ST-Link USB (auto-connected) |
| 3.3 V | 3V3 | CN6 power header | MPU-6050 VCC |
| 5 V | 5V | CN6 power header | Servo + Speaker power |
| Ground | GND | CN6 + many others | Every device GND |

**MPU-6050 detail**

| Sensor pin | Connects to | Why |
|---|---|---|
| VCC | 3V3 | Sensor is 3.3 V only — never 5 V |
| GND | GND | Common ground |
| SCL | PB8 | I2C clock |
| SDA | PB9 | I2C data |
| AD0 | GND | Sets I2C address to 0x68 |
| XDA, XCL, INT | — | Not used — leave unconnected |

**STEMMA Speaker detail** — use the 3-pin JST PH cable or the sew pads:

| JST pin | Colour | Connects to |
|---|---|---|
| 1 (SIG) | White | PA0 / A0 |
| 2 (+) | Red | 5 V (louder) or 3.3 V |
| 3 (−) | Black | GND |

> **Wiring must-dos:**
> - **All grounds together** — board, sensor, servo, and speaker share one GND rail.
> - **Servo power:** if the board resets when the servo moves, give the servo its own 5 V supply (keep GND common). The Nucleo's 5 V rail is marginal for servo stall current.
> - **MPU-6050 strictly on 3.3 V** — 5 V will damage it.

---

## Build & Flash

Verified: builds warning-free (`-Wall -Wextra`) with the Arm GNU Toolchain 15.2 against STM32CubeG4, producing **≈ 37 KB flash / 3 KB RAM**.

### One-time setup (Windows — no admin required)

```sh
# Install tools via scoop (https://scoop.sh)
scoop bucket add extras
scoop install gcc-arm-none-eabi make openocd

# Download the STM32CubeG4 HAL/CMSIS package
git clone --recurse-submodules https://github.com/STMicroelectronics/STM32CubeG4 \
  ~/STM32Cube/Repository/STM32CubeG4
```

### Build

Run from this folder in **Git Bash**. Use a relative path for `CUBE_HAL_DIR` — an absolute Windows path with a drive-letter colon (`C:\...`) confuses `make` and `gcc`.

```sh
make CUBE_HAL_DIR=../../STM32Cube/Repository/STM32CubeG4
```

Output: `build/navgame.elf`, `build/navgame.bin`, `build/navgame.hex`

### Flash

```sh
# Via OpenOCD + ST-Link (board plugged in via USB):
make flash CUBE_HAL_DIR=../../STM32Cube/Repository/STM32CubeG4

# Or via STM32CubeProgrammer:
STM32_Programmer_CLI -c port=SWD -w build/navgame.elf -rst
```

### Serial telemetry

Open the ST-Link virtual COM port at **115200 8N1** in any terminal (PuTTY, Tera Term, minicom). You will see a live navigation feed at 10 Hz:

```
=== Navigation Game (STM32G474RE) ===
RNG: hardware seed acquired.
Calibrating IMU - hold the board still...
Target placed at (3.24, -1.87) m. Go find it!
[1234] SEEKING    | pos(  0.12, -0.03) | hdg   17.4 | dist   3.82 m | brg  330.2 rel  312.8 | MID
```

---

## Architecture

The firmware is a single 100 Hz bare-metal super-loop — no RTOS, no heap, no dynamic allocation. Every iteration is O(1) time and O(1) space. The 170 MHz Cortex-M4F with hardware FPU completes the full navigation math in under 0.5 ms, leaving ~95% idle headroom per cycle.

```
main()
└── Game_Init()   — peripherals, IMU check, PRNG seed, calibration, first target
└── for(;;) Game_Tick()
        ├── STATE_CALIBRATE  — blocking ~2 s, gyro bias average, Nav_Init
        ├── STATE_SEEKING    — 100 Hz:
        │       MPU6050_ReadScaled()
        │       Nav_Update()          — gyro heading + step-detection dead-reckoning
        │       Servo_PointBearing()  — relative bearing → TIM3 CCR
        │       Audio_Update()        — proximity → pitch + cadence → TIM2 ARR/CCR
        │       LED_Set()
        │       SerialLog_PrintNav()  — every 10th tick
        │       win check → STATE_WIN
        ├── STATE_WIN        — jingle, LED flash, wait for B1 → STATE_SEEKING
        └── STATE_ERROR      — actuators safe, red LED blink code, halt
```

### Source modules

| File | Responsibility |
|---|---|
| `main.c` | Clock tree (170 MHz PLL), peripheral init, super-loop |
| `config.h` | Every tunable constant — no magic numbers anywhere else |
| `mpu6050.c/.h` | I2C driver: init, burst read, scaling, calibration, bus recovery |
| `navigation.c/.h` | Gyro heading + step-detection dead-reckoning (pedometer), distance/bearing (pure math) |
| `servo.c/.h` | PWM pulse → angle → relative bearing on TIM3 CH1 |
| `audio.c/.h` | Proximity → pitch + beep cadence on TIM2 CH1; win jingle |
| `game.c/.h` | State machine, xorshift32 PRNG, target placement, win detection |
| `led.c/.h` | Bitmask GPIO API for three status LEDs |
| `serial_log.c/.h` | Formatted UART telemetry over USART2 → ST-Link VCP |

### Peripheral map

| Peripheral | Pin(s) | Config |
|---|---|---|
| I2C1 (MPU-6050) | PB8=SCL, PB9=SDA | 400 kHz, addr 0x68, AF4, open-drain + pull-up |
| TIM3 CH1 (servo) | PA6 | PSC=169 → 1 MHz tick; ARR=19999 → 50 Hz; CCR = pulse µs |
| TIM2 CH1 (audio) | PA0 | PSC=169 → 1 MHz tick; ARR = (1 MHz / freq) − 1; CCR = 50% duty |
| USART2 (debug) | PA2=TX, PA3=RX | 115200 8N1 → ST-Link VCP |
| RNG | HSI48 | Seeds xorshift32 PRNG; fixed-seed fallback if unavailable |
| GPIO (LEDs) | PB13/14/15 | Push-pull output |
| GPIO (button) | PC13 | Input, pressed = LOW (on-board B1) |

---

## Proximity Beep Reference

Both pitch and beep speed increase continuously as you close in. The table below shows approximate thresholds.

| Zone | Distance | Pitch | Speed | Amber LED |
|---|---|---|---|---|
| Cold | > 22 ft (6.8 m) | 300 Hz | 1 beep / 800 ms | Off |
| Warm | 14 – 22 ft (4–7 m) | 300–1200 Hz | 1–2 / sec | Off |
| Hot | 5 – 14 ft (1.5–4 m) | 1200–1750 Hz | 2–5 / sec | Pulsing |
| Burning | 1 – 5 ft (0.3–1.5 m) | 1750–2000 Hz | 5–10 / sec | Solid |
| **Win** | **< 1 ft (0.3 m)** | **Victory jingle** | — | **Solid** |

**Win jingle:** C5 (523 Hz) → E5 (659 Hz) → G5 (784 Hz) → C6 (1047 Hz, held). All notes 150 ms; final note 300 ms.

---

## LED Reference

| LED | State | Meaning |
|---|---|---|
| Green | Solid on | Seeking — game is active |
| Amber | Pulsing | Getting close (Hot zone) |
| Amber | Solid on | Very close (Burning zone) |
| Amber | 4 slow blinks at startup | Hardware RNG unavailable; fixed seed used (non-fatal) |
| All three | 6 rapid flashes | Win! |
| Red | 2 blinks then halt | MPU-6050 not found — check wiring / I2C address |
| Red | 3 blinks then halt | I2C runtime fault after retries |
| Red | Fast strobe | Fatal HAL init error |

---

## Sensor Fusion Overview

**Heading (yaw)** is integrated from the gyroscope's Z axis, with the zero-rate bias measured and removed during the 2-second calibration. It is gyro-only (no magnetometer), so it drifts slowly over minutes.

**Position** uses **step dead-reckoning (a pedometer)**, not acceleration double-integration. A slow low-pass of the accelerometer magnitude tracks gravity; each footfall makes the dynamic part spike, which is counted as a step (with hysteresis and a refractory window so one footfall counts once). Every step advances the position by one stride (`STRIDE_M`, default 0.65 m) along the current heading. Because each step is a bounded, discrete update, error does not snowball, and tilting the board does not inject phantom motion (magnitude is orientation-independent).

> **Notes & tuning:** `STRIDE_M` and `STEP_ACCEL_THRESH_MS2` (in `config.h`) may need a small tweak per user/board — raise the threshold if it counts phantom steps, lower it if it misses real ones. Heading still drifts slowly (gyro-only); for a few-minute game this is fine. Survey-grade absolute position would need a magnetometer (heading) plus a UWB/optical reference (position).

---

## Customising the Win Sound

The board generates audio as a square-wave tone (like an 8-bit game console). To change the victory melody, edit the `notes[]` array in `Audio_WinJingle()` in `Core/Src/audio.c`. Each entry is a frequency in Hz; `0` is a rest.

```c
static const uint32_t notes[] = { 523U, 659U, 784U, 1047U };
//                                  C5    E5    G5    C6
```

Common note frequencies: C4=262, D4=294, E4=330, F4=349, G4=392, A4=440, B4=494, C5=523, D5=587, E5=659, G5=784, A5=880, C6=1047.

For full rhythm control (different durations per note), see the **Custom Victory Sound** section in [`USER_GUIDE.md`](USER_GUIDE.md).

---

## Documentation

| File | Contents |
|---|---|
| [`USER_GUIDE.md`](USER_GUIDE.md) | Player guide: gameplay, beep zones, LED meanings, win sequence, replay, custom sound options |
| [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) | Technical deep-dive: every module explained in plain English, known issues and bugs |
| [`CLAUDE.md`](CLAUDE.md) | Engineering specification: pinout, build system, coding conventions, timing budget, state machine |

---

## Project History

This repo briefly hosted a health-monitor scaffold (vibration / DS18B20 temperature / HC-SR04 ultrasonic). The team chose the navigation game. The health-monitor code is preserved in git history at commit `4837b87` and is not part of this firmware. The MPU-6050 driver, servo PWM mapping, LED abstraction, and HAL init were reused from that earlier work.
