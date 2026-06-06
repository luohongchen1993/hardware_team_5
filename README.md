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

Pin names like `PB8` are the chip's labels. On the Nucleo-64 board many of them
also have an easy **Arduino header** label (e.g. `D15`, `A0`) printed near the
sockets — use whichever is easier to find. "Morpho" means the two long inner
header rows (not the Arduino sockets).

**Confirmed parts in this build:** STM32 Nucleo-64 (STM32G474RE) board · Adafruit
STEMMA Speaker **P3885** (analog amp + speaker) · MPU-6050 accel/gyro. *(The
HC-SR04 distance sensor mentioned in some docs is NOT used by this game — it
belonged to the earlier health-monitor idea.)*

**Master reference (every pin the program uses)**

| Chip pin | Easy label | Used for | Direction | Connect to |
|---|---|---|---|---|
| PB8 | D15 | I2C clock (SCL) → MPU-6050 | out | sensor SCL |
| PB9 | D14 | I2C data (SDA) → MPU-6050 | in/out | sensor SDA |
| PA6 | D12 | Servo control signal | out | servo signal wire |
| PA0 | A0 | Audio signal → STEMMA Speaker | out | speaker signal pad (A+ / "Sig") |
| PB13 | (Morpho) | Green status LED | out | LED + via ~330 Ω → GND |
| PB14 | (Morpho) | Amber status LED | out | LED + via ~330 Ω → GND |
| PB15 | (Morpho) | Red status LED | out | LED + via ~330 Ω → GND |
| PC13 | (on-board B1) | Replay button | in | already wired on board |
| PA2 | D1 | Debug text out (UART TX) | out | auto-routed to USB |
| PA3 | D0 | Debug text in (UART RX) | in | auto-routed to USB |
| 3V3 | — | Power for the sensor | — | sensor VCC |
| 5V | — | Power for the servo | — | servo power wire |
| GND | — | Common ground (shared) | — | every device's GND |

**1) MPU-6050 motion sensor — 4 wires (I2C)**

| Sensor pin | Connect to board | Notes |
|---|---|---|
| VCC | 3V3 | sensor runs on 3.3 V logic |
| GND | GND | ground |
| SCL | PB8 (D15) | clock line |
| SDA | PB9 (D14) | data line |
| AD0 | GND | sets the sensor address to 0x68 (the default) |

**2) Servo motor — 3 wires**

| Servo wire (typical colour) | Connect to board | Notes |
|---|---|---|
| Signal (orange/yellow) | PA6 (D12) | control pulse |
| Power (red) | 5V | see power note below |
| Ground (brown/black) | GND | ground |

**3) Adafruit STEMMA Speaker (P3885) — 3 wires**

A self-contained amplifier + 1 W 8 Ω speaker that accepts a plain audio signal, so the
square-wave tone from PA0 plugs straight in — no extra parts and no code changes.

| Speaker pad | Connect to board | Notes |
|---|---|---|
| Signal (A+ / "Sig") | PA0 (A0) | the tone signal from the chip |
| Vin (power) | 3V3 or 5V | accepts 3–5 V; 5 V is a bit louder |
| GND | GND | ground |

Turn the small screwdriver pot on the board to set the volume. (Uses the larger **STEMMA**
3-pin JST PH connector — *not* the smaller STEMMA QT. You can also use the alligator/sew pads.)

**4) Status LEDs — optional, 3 LEDs**

Each LED's long leg (+) goes to its pin through a ~330 Ω resistor; the short leg (−) goes to GND.
Green → PB13, Amber → PB14, Red → PB15. These three are on the **Morpho** header.

**5) Replay button** — the blue **B1** button already on the board (PC13). Nothing to wire.

**6) Debug messages** — PA2/PA3 are wired by the board to the on-board ST-Link USB chip, so the
text log reaches your computer over the same USB cable. Nothing to wire.

> ⚠️ **Wiring notes**
> - **Tie all grounds (GND) together** — sensor, servo, speaker, and board must share one common ground, or nothing will read correctly.
> - **Servo power:** use a separate **5 V** supply for the servo if it twitches or the board resets — servos can pull more current than the board pin can give. Keep its ground common with the board.
> - **Sensor power:** the MPU-6050 must be on **3V3**, not 5 V.

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
