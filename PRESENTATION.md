# Hot/Cold Navigation Game — Presentation Guide

*A simple script + cheat-sheet for presenting to judges. You don't need to read any code —
everything you need to say is here, in plain English, with the impressive technical bits explained.*

---

## 1. The 30-second pitch (say this first)

> "We built a **physical treasure-hunt game that runs entirely on a single microcontroller** —
> no phone, no GPS, no Wi-Fi, no computer. You hold the board and walk around a room. A **motor
> arm physically points toward a hidden target** like a compass needle, and a **speaker beeps
> faster and higher the closer you get**, like a Geiger counter. Get within a few feet and it
> plays a victory fanfare. Everything — sensing your motion, the math, the sound, the pointer —
> happens on the chip in real time, a hundred times a second."

That one paragraph is your whole project. Everything below is backup.

---

## 2. Why it's cool (the hook for judges)

- **It's physical and interactive.** Judges can *hold it and play it*, not just look at a screen.
- **It's fully self-contained.** A $15 microcontroller does all the work — no cloud, no phone app.
- **It's real-time and reliable.** The board reacts to your movement instantly (100 times/second).
- **It shows real engineering judgment** — we hit a hard problem (tracking position from cheap
  sensors), the textbook method failed, and we engineered a smarter solution. (See section 6.)

---

## 3. Live demo script (≈ 2 minutes)

**Before judges arrive:** plug in the USB cable, set the board flat, let it finish starting up.

1. **Power on / point at the startup show.**
   *"When it powers on it runs a self-test — watch: the lights chase, the arm sweeps left-to-right,
   and it plays three beeps. That tells us every part is alive before we even start."*

2. **Calibration.**
   *"It holds still for two seconds to learn what 'not moving' looks like — that's how it stays
   accurate."* (Keep it still.)

3. **Game starts (green light on).**
   *"A hidden target was just placed at a random spot within about 20 feet — even I don't know
   where it is. The chip's hardware random-number generator picked it."*

4. **Show the compass.** Slowly turn in place.
   *"The arm always points toward the target relative to the way I'm facing. Watch it swing as I
   turn — when it points straight ahead, that's the way to walk."*

5. **Walk toward it.**
   *"Listen — the beeps speed up and rise in pitch as I get closer. Slow and low means cold; fast
   and high means burning hot. The amber light pulses when I'm hot and goes solid when I'm burning."*

6. **Win.**
   *"There it is — victory fanfare, the arm does a little celebration dance, the lights flash."*

7. **Press B1.**
   *"One button drops a brand-new random target, and we go again."*

**If something acts up:** you can always say *"this is live hardware on a breadboard"* and press
reset (or unplug/replug USB) to restart cleanly. The red light blink-codes tell us exactly what's
wrong if a wire is loose (2 blinks = sensor, 3 blinks = connection).

---

## 4. How it works — explained in three simple layers

Every 1/100th of a second, the board does the same three things: **SENSE → THINK → ACT.**

```
        SENSE                     THINK                       ACT
  ┌──────────────────┐    ┌────────────────────┐    ┌──────────────────────┐
  │ Motion sensor    │    │ Where am I?        │    │ Turn the servo arm   │
  │ (accelerometer + │ →  │ Which way am I     │ →  │ to point at target   │
  │  gyroscope)      │    │ facing?            │    │                      │
  │ over I2C         │    │ How far + which    │    │ Beep faster/higher   │
  │                  │    │ way is the target? │    │ as you get closer    │
  └──────────────────┘    └────────────────────┘    └──────────────────────┘
```

**SENSE** — A tiny 6-axis motion sensor (the MPU-6050) reports how the board is accelerating and
turning, ~125 times a second over a 2-wire connection (I2C).

**THINK** — The chip turns those raw numbers into useful answers:
- **Which way am I facing?** It adds up the turn-rate from the gyroscope → a compass heading.
- **Where am I?** It works like a **pedometer**: it detects each footstep and moves you one stride
  in the direction you're facing.
- **Where's the target?** Simple geometry (distance and angle) from your position to the hidden spot.

**ACT** — It converts those answers into the real world:
- **The servo** is told an exact angle so it physically points the right way.
- **The speaker** is given a tone whose pitch and beep-speed depend on how close you are.

---

## 5. The clever engineering bits (talking points that impress)

Pick two or three of these depending on the judge's interest.

- **"Everything runs bare-metal on the chip."** No operating system, no app, no internet. One
  170 MHz processor does sensing, math, sound, and motor control in a tight loop — and it finishes
  all the work for each cycle in **under half a millisecond**, so it's idle 95% of the time. Rock
  solid and instant.

- **"It's hard real-time."** The main loop runs at a fixed **100 times per second**, every time,
  with no dynamic memory and no surprises — the same discipline used in cars and medical devices.

- **"We turned a square wave into music."** The speaker is driven by switching a pin on and off at
  the exact frequency of the note — that's how old-school game consoles made sound. We use it for
  the proximity beeps *and* a 4-note victory fanfare, with zero extra audio hardware.

- **"The servo is a physical display."** We map a compass angle to a precise electrical pulse
  (1–2 thousandths of a second wide) that commands the servo to an exact position — so direction
  becomes something you can *see and feel*, not just read.

- **"Truly random targets."** The chip has a dedicated hardware random-number generator (it uses
  electrical noise) — we use it to seed the target so every game is genuinely different.

- **"It's robust."** If the sensor hiccups, the code retries and recovers automatically. If a wire
  is loose, it doesn't crash — it blinks a diagnostic code on the red LED so you know exactly what
  to fix. And it self-tests every part at power-on.

---

## 6. The standout story: how we made navigation actually work

*This is your best "we're real engineers" moment — judges love a problem-and-solution story.*

> "The classic textbook way to track position from this kind of sensor is to measure acceleration
> and add it up twice to get distance. **We tried that — and it failed.** Cheap sensors are noisy,
> and those errors snowball into garbage within a few seconds. Worse, just *tilting* the board
> made it think you'd moved.
>
> So we switched approaches. Instead of measuring distance directly, we treat it like a
> **pedometer**: we detect each footstep and step the position forward by one stride in the
> direction you're facing. Each step is a small, bounded update, so errors don't pile up — and
> because it keys off the *strength* of the motion, tilting the board no longer fools it. That one
> design decision is the difference between a demo that flails and one that actually guides you."

It also shows you understand the trade-off: *heading still slowly drifts over several minutes
because there's no compass chip — a known, honest limitation, and an easy future upgrade.*

---

## 7. The impressive numbers (drop these in)

| Spec | Value |
|---|---|
| Processor | STM32G474 — ARM Cortex-M4F @ **170 MHz** with hardware floating-point |
| Loop rate | **100 Hz** (updates every 10 milliseconds), hard real-time |
| Compute used per cycle | **< 0.5 ms** → ~95% idle headroom |
| Program size | **~37 KB** of flash (out of 512 KB), ~3 KB of RAM (out of 128 KB) |
| Memory model | **Zero dynamic allocation** — fully deterministic |
| Sensor rate | ~125 readings/second over I2C |
| Win distance | within **~3 feet (0.9 m)** of the hidden target |
| Play area | up to **~20 feet** in any direction |

---

## 8. What each part of the code does (one line each)

| Module | In plain English |
|---|---|
| `main` | Sets up the chip's clock and all the pins, then runs the 100 Hz loop |
| `mpu6050` | Talks to the motion sensor; reads it, scales the numbers, recovers from glitches |
| `navigation` | The "brain": heading from the gyro + step-counting pedometer + distance/angle to target |
| `servo` | Turns a compass angle into the exact pulse that aims the motor arm |
| `audio` | Turns "how close are you" into pitch + beep-speed; plays the victory fanfare |
| `game` | The rules: calibrate → seek → win → replay, plus the random target and self-test |
| `led` / `serial_log` | Status lights, and a live text feed over USB for debugging |
| `config.h` | Every adjustable number in one place (stride length, win distance, tones…) |

---

## 9. Likely judge questions — and simple answers

**"Does it need a phone or internet?"**
No. Everything runs on the one chip. The USB cable is only for power (and optional debug text).

**"How does it know where the target is?"**
It doesn't sense the target — the target is virtual. The chip places it at a random spot at the
start, then tracks *your* movement and does the geometry to point you there.

**"How accurate is it?"**
The direction (compass) is reliable. The distance is good enough to find the target by walking,
because we use step-counting instead of error-prone acceleration math. Over many minutes the
heading drifts slightly — adding a compass chip would fix that, and is on our list.

**"Why a microcontroller and not a Raspberry Pi / phone?"**
It's cheaper, lower-power, instant-on, and hard real-time — the right tool for a responsive
physical device. No OS to boot, no app to install.

**"What was the hardest part?"**
Position tracking (see section 6) — and getting reliable real-time sensor communication on a
breadboard, which is why we built in automatic retry/recovery and diagnostic blink codes.

**"How long did the loop take to compute?"**
Under half a millisecond out of the ten we have — so there's huge headroom to add features.

---

## 10. If we had more time (shows vision)

- Add a **magnetometer (compass chip)** so heading never drifts.
- A **PC visualizer** — the board already streams live position/heading/distance over USB.
- **Multiplayer / difficulty levels** (smaller win radius, moving targets, a countdown timer).
- **Recorded sound effects** using the chip's built-in audio output for richer feedback.

---

*Hardware: STM32 Nucleo-G474RE board · MPU-6050 motion sensor · SG90 servo · Adafruit STEMMA
speaker · 3 status LEDs. All code is C, runs bare-metal, and is on GitHub.*
