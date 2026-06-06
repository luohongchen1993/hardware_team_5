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

## Pinout — what connects where

Pin names like `PB8` are the chip's labels; on the Nucleo-64 the same pins also have
**Arduino header** labels (`D15`, `A0`, …) printed beside the outer sockets — use
whichever is easier to find. The board has two header styles:

- **Arduino headers** (outer sockets): `CN5` top-right, `CN6` top-left (power), `CN8`
  bottom-left, `CN9` bottom-right.
- **ST Morpho headers** (the two long inner rows): `CN7` (left), `CN10` (right).

**Confirmed parts:** STM32 Nucleo-64 **STM32G474RE** · Adafruit STEMMA Speaker **P3885**
(analog Class-D amp + 1 W 8 Ω speaker) · **MPU-6050 / GY-521** accel + gyro. *(The HC-SR04
in some docs is NOT used — it was the earlier health-monitor idea.)*

### Master reference (every pin the program uses)

| Function | Chip pin | Board location (label) | Connects to |
|---|---|---|---|
| I2C clock (SCL) | PB8 | Arduino **D15** (CN5) | MPU-6050 **SCL** |
| I2C data (SDA) | PB9 | Arduino **D14** (CN5) | MPU-6050 **SDA** |
| Servo signal | PA6 | Arduino **D12** (CN5) | servo **signal** (orange/yellow) |
| Audio signal | PA0 | Arduino **A0** (CN8) | speaker **Signal** (white) |
| Green LED | PB13 | Morpho **CN10 pin 30** | LED + (via ~330 Ω) |
| Amber LED | PB14 | Morpho **CN10 pin 28** | LED + (via ~330 Ω) |
| Red LED | PB15 | Morpho **CN10 pin 26** | LED + (via ~330 Ω) |
| Replay button | PC13 | on-board **B1** | already wired |
| Debug TX | PA2 | Arduino **D1** (CN9) | ST-Link USB (auto) |
| Debug RX | PA3 | Arduino **D0** (CN9) | ST-Link USB (auto) |
| 3.3 V power | 3V3 | **CN6** power header | MPU-6050 VCC |
| 5 V power | 5V | **CN6** power header | servo + speaker power |
| Ground | GND | **CN6** (and many others) | every device GND |

> Morpho pin numbers follow the standard Nucleo-64 layout — double-check against the
> labels on your board / ST manual **UM2505** before soldering. The 3 LEDs are optional;
> if the inner Morpho row is awkward, ask and I'll move them to Arduino pins.

### 1) MPU-6050 / GY-521 motion sensor (I2C — uses 5 of 8 pins)

| Sensor pin | Connect to | Notes |
|---|---|---|
| VCC | **3V3** | sensor runs on 3.3 V |
| GND | **GND** | ground |
| SCL | **PB8 / D15** | I2C clock |
| SDA | **PB9 / D14** | I2C data |
| AD0 | **GND** | address = 0x68 (default). Tie to 3V3 for 0x69. |
| XDA, XCL | — | auxiliary I2C (external magnetometer) — leave unconnected |
| INT | — | data-ready interrupt — not used (we poll) — leave unconnected |

### 2) Servo motor (3 wires)

| Servo wire (typical colour) | Connect to | Notes |
|---|---|---|
| Signal (orange / yellow) | **PA6 / D12** | 3.3 V control pulse (servos accept this) |
| Power (red) | **5V** | see power note below |
| Ground (brown / black) | **GND** | ground |

### 3) Adafruit STEMMA Speaker P3885 (3 wires)

Self-contained amp + speaker that takes a plain **0–3 V audio signal** (internally
decoupled — no AC-coupling needed), so the PWM tone from PA0 drives it directly. Use
**either** the 3-pin JST PH cable **or** the alligator/sew pads:

| JST PH pin | Pad / colour | Connect to |
|---|---|---|
| 1 | Signal (SIG) — white | **PA0 / A0** |
| 2 | Power (+) — red | **3V3 or 5V** (3–5 V; 5 V = louder) |
| 3 | Ground (−) — black | **GND** |

Set volume with the on-board trim pot (small screwdriver). *(This is the larger **STEMMA**
connector — not the smaller STEMMA QT.)*

### 4) Status LEDs — optional (3 LEDs)

Long leg (+) → the pin **through a ~330 Ω resistor**; short leg (−) → GND.
Green → **PB13 (CN10-30)**, Amber → **PB14 (CN10-28)**, Red → **PB15 (CN10-26)**.

### 5) Replay button & 6) Debug — nothing to wire

The blue **B1** button (PC13) and the USB debug log (PA2/PA3 → ST-Link) are already wired
on the board; the log reaches your PC over the same USB cable.

> ⚠️ **Wiring must-dos**
> - **Tie all grounds together** — board, sensor, servo, and speaker share one common GND, or readings/sound fail.
> - **Servo power:** if it jitters or the board resets, give the servo its own **5 V** supply (keep its GND common). The board's 5V can be weak for servos.
> - **Sensor power:** MPU-6050 on **3V3**, never 5 V.

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
