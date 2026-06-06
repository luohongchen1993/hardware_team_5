# hardware_team_5 — Navigation Game (STM32G474RE)

A handheld "hot/cold" navigation game. Hold the Nucleo-G474RE board, and an
IMU estimates your motion while a **servo physically points toward a hidden
virtual target** and a **speaker beeps faster/higher as you get closer**.
Reach the target and a victory jingle plays.

## How it works

1. **Power-up / calibrate** — hold still ~2 s. The MPU-6050 gyro bias is
   measured, the origin is set to (0,0,0), and a target is placed at a
   random spot in a ~20 ft (6.1 m) grid (seeded from the hardware RNG).
2. **Seek** (100 Hz loop) — read the IMU, run a complementary filter for
   heading + dead-reckoning (with zero-velocity updates) for position,
   compute distance & bearing to the target, point the servo, and play the
   proximity tone.
3. **Win** — within ~1 ft (0.3 m): victory jingle, servo centres & holds,
   LEDs celebrate. Press **B1** to drop a new target and play again.

> ⚠️ Honest limitation: position from double-integrated MEMS acceleration
> drifts (metres over tens of seconds). Heading/bearing is reliable; absolute
> displacement is approximate. See `CLAUDE.md` for the full rationale.

## Hardware / wiring

| Peripheral | MCU | Pins |
|---|---|---|
| MPU-6050 IMU (I2C @ 0x68) | I2C1 | PB8=SCL, PB9=SDA, 3V3, GND |
| Servo (SG90-class) | TIM3_CH1 | **PA6** = signal, 5V, GND |
| Speaker / piezo (PWM tone) | TIM2_CH1 | **PA0** = signal, GND |
| Status LEDs (green/amber/red) | GPIO | PB13 / PB14 / PB15 (+ resistors to GND) |
| Replay button | GPIO | B1 (PC13, on-board) |
| Telemetry | USART2 | ST-Link VCP, 115200 8N1 |

## Build & flash

Needs the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`) and the **STM32CubeG4** firmware package (for HAL/CMSIS
+ startup). One-time setup and details are in [`CLAUDE.md`](CLAUDE.md).

```sh
make CUBE_HAL_DIR=/path/to/STM32Cube_FW_G4_V1.5.0      # -> build/navgame.elf
make flash                                             # OpenOCD + ST-Link
# or: STM32_Programmer_CLI -c port=SWD -w build/navgame.elf -rst
```

Watch telemetry on the ST-Link virtual COM port at 115200 baud.

## Source layout

```
Core/Inc, Core/Src   application firmware (see module list in CLAUDE.md)
Makefile             CLI build (arm-none-eabi-gcc)
STM32G474RETx_FLASH.ld   linker script
CLAUDE.md            full engineering spec, pinout, conventions, timing
```

> The earlier health-monitor scaffold (vibration/temp/ultrasonic) is preserved
> in git history at commit `4837b87` and is not part of this firmware.
