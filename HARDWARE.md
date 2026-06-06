# Hardware Reference — Health Monitor

STM32G474RE Nucleo-64 + all peripheral components.

---

## 1. STM32G474RE Nucleo-64

| Property | Value |
|----------|-------|
| CPU | ARM Cortex-M4 @ 170 MHz |
| Flash | 512 KB |
| SRAM | 128 KB |
| FPU | Yes (fpv4-sp-d16, hard-float ABI) |
| Debug/Flash | ST-Link v2 (onboard) |
| Supply | 3.3 V (on-board regulator from USB or VIN) |

**Pin header used:**

| STM32 Pin | Arduino Label | Assigned To |
|-----------|--------------|-------------|
| PA2 | D1 (TX) | USART2 TX → ST-Link USB-CDC |
| PA3 | D0 (RX) | USART2 RX |
| PA4 | — | free (DAC1_OUT1 reserved) |
| PA5 | D13 / LD2 | DS18B20 1-Wire (open-drain) |
| PA6 | D12 | TIM3_CH1 → SG90 servo PWM |
| PA8 | D7 | HC-SR04 ECHO (input) |
| PA9 | D8 | HC-SR04 TRIG (output) |
| PB6 | D10 | TIM4_CH1 → STEMMA Speaker audio |
| PB8 | D15 (SCL) | I2C1_SCL → MPU-6050 |
| PB9 | D14 (SDA) | I2C1_SDA → MPU-6050 |
| PB13 | — | LED Green (output PP) |
| PB14 | — | LED Amber (output PP) |
| PB15 | — | LED Red (output PP) |

---

## 2. MPU-6050 — 6-Axis IMU

| Property | Value |
|----------|-------|
| Interface | I2C, 400 kHz fast mode |
| I2C Address | 0x68 (AD0 → GND) |
| Accel range | ±2 g (configured), 16384 LSB/g |
| Gyro range | ±500 °/s (configured), 65.5 LSB/(°/s) |
| Sample rate | 125 Hz (SMPLRT_DIV = 7) |
| DLPF | 44 Hz bandwidth (CONFIG = 0x03) |
| Supply | 3.3 V |

**Wiring:**

| MPU-6050 Pin | Connect to |
|-------------|-----------|
| VCC | 3.3 V |
| GND | GND |
| SDA | PB9 + 4.7 kΩ pull-up to 3.3 V |
| SCL | PB8 + 4.7 kΩ pull-up to 3.3 V |
| AD0 | GND (sets address to 0x68) |
| INT | PC1 (optional, not used in polling mode) |

**Used for:** vibration RMS (32-sample windowed magnitude deviation from 1 g).

---

## 3. HC-SR04 — Ultrasonic Distance Sensor

| Property | Value |
|----------|-------|
| Interface | GPIO trigger + echo pulse measurement |
| Supply | 5 V (VCC), signal lines 3.3 V compatible |
| Range | 2 cm – 400 cm |
| Accuracy | ~3 mm |
| Trigger | 10 µs HIGH pulse on TRIG pin |
| Echo | HIGH pulse width proportional to distance |
| Distance formula | `distance_mm = (echo_us × 10) / 58` |
| Timeout | 30 000 µs (~5 m) |

**Wiring:**

| HC-SR04 Pin | Connect to |
|------------|-----------|
| VCC | 5 V |
| GND | GND |
| Trig | PA9 (output push-pull) |
| Echo | PA8 (input, 3.3 V — add 1 kΩ + 2 kΩ divider if sensor outputs 5 V logic) |

**Timing resource:** TIM2 free-running at 1 MHz (PSC=169, ARR=0xFFFFFFFF).

**Used for:** guard/cover removal detection. Breach if distance < 50 mm or > 200 mm.

---

## 4. DS18B20 — 1-Wire Temperature Sensor

| Property | Value |
|----------|-------|
| Interface | 1-Wire (single GPIO, open-drain) |
| Supply | 3.3 V or 5 V (parasitic or external) |
| Resolution | 12-bit (0.0625 °C) |
| Conversion time | up to 750 ms (12-bit) |
| Range | −55 °C to +125 °C |

**Wiring:**

| DS18B20 Pin | Connect to |
|------------|-----------|
| VDD | 3.3 V |
| GND | GND |
| DQ | PA5 (open-drain) + 4.7 kΩ pull-up to 3.3 V |

**Timing resource:** TIM2 shared with HC-SR04 (not used simultaneously).

**Protocol:** SKIP_ROM → CONVERT_T → poll busy bit → SKIP_ROM → READ_SCRATCHPAD.
Raw 16-bit value divided by 16 = °C.

**Used for:** overheating detection. THERMAL_WARN ≥ 45 °C, CRITICAL ≥ 65 °C.

---

## 5. SG90 — Micro Servo

| Property | Value |
|----------|-------|
| Interface | PWM 50 Hz |
| Supply | 5 V (separate from MCU 3.3 V logic) |
| Signal | 3.3 V logic level works |
| 0° | 1 ms pulse (CCR = 1000 at 1 MHz timer) |
| 90° | 1.5 ms pulse (CCR = 1500) |
| 180° | 2 ms pulse (CCR = 2000) |

**Wiring:**

| SG90 Wire | Connect to |
|-----------|-----------|
| Red | 5 V |
| Brown | GND |
| Orange (signal) | PA6 (TIM3_CH1, AF2) |

**Timer:** TIM3, CH1, PSC=169, ARR=19999 → 50 Hz.

**Used for:** physical severity indicator. 0° = NOMINAL, 45° = VIB_WARN, 90° = THERMAL_WARN, 180° = BREACH. Sweeps in CRITICAL.

---

## 6. Status LEDs (×3)

| Color | Pin | State |
|-------|-----|-------|
| Green | PB13 | NOMINAL |
| Amber | PB14 | VIBRATION_WARN or THERMAL_WARN |
| Red | PB15 | SAFETY_BREACH |
| All flash | — | CRITICAL |

330 Ω series resistor on each LED recommended (GPIO sourcing ~8 mA max at 3.3 V).

---

## 7. Adafruit STEMMA Speaker — P3885

| Property | Value |
|----------|-------|
| Amplifier | Class D, 1 W |
| Speaker | 8 Ω, 1 W |
| Supply | 3–5 V |
| Audio input | Any signal up to supply voltage p-p; internally AC-coupled |
| Connector | 3-pin JST PH (or sew pads) |
| Volume | Adjustable via trimmer pot |

**Wiring:**

| Speaker Pin | Connect to |
|------------|-----------|
| GND | GND |
| VCC | 3.3 V or 5 V |
| Audio In | PB6 (TIM4_CH1 PWM, AF2) |

**Signal generation:** TIM4 CH1 outputs a 50% duty-cycle square wave at the desired audio
frequency. The Class D amp is AC-coupled internally so the PWM DC offset is rejected.

**Alarm tones per health state:**

| State | Pattern |
|-------|---------|
| NOMINAL | Silence |
| VIBRATION_WARN | 880 Hz — 200 ms on / 800 ms off, repeat |
| THERMAL_WARN | 660 Hz — 150 ms on / 100 ms off / 150 ms on / 700 ms off, repeat |
| SAFETY_BREACH | 1400 Hz — 100 ms on / 100 ms off, continuous fast beep |
| CRITICAL | Siren: ramp 800 → 1600 Hz over 500 ms, repeat |

---

## Peripheral Resource Summary

| Resource | Use |
|----------|-----|
| I2C1 | MPU-6050 |
| TIM2 (free-running, 1 MHz) | HC-SR04 echo timing + DS18B20 1-Wire bit timing |
| TIM3 CH1 (PWM, 50 Hz) | SG90 servo |
| TIM4 CH1 (PWM, audio) | STEMMA Speaker tones |
| USART2 | Serial debug log → ST-Link USB-CDC |
| GPIOA PA5 (open-drain) | DS18B20 1-Wire |
| GPIOA PA8/PA9 | HC-SR04 ECHO/TRIG |
| GPIOB PB13/14/15 | LEDs |
| SysTick | HAL 1 kHz timebase |
