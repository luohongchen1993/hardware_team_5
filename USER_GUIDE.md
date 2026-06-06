# Navigation Game — Complete Player Guide

---

## What is this game?

You hold a circuit board and walk around the room. Somewhere in the space around you, an invisible target has been placed. Your job is to find it.

The board gives you two clues:

- **A servo arm** that physically rotates to point toward the target — like a compass needle. When it points straight ahead, walk forward. When it swings left, turn left.
- **A speaker** that beeps faster and higher as you get closer — like a Geiger counter near radiation. Slow low thumps = cold. Rapid high-pitched chirps = burning hot.

Get within one foot of the target and you win. A victory jingle plays, the board celebrates, you press a button, and a new target appears.

That's it. Walk toward the beeping.

---

## Before You Start — Check These First

Go through this list before powering on. Skipping steps here causes confusing behaviour later.

- [ ] All three ground wires are connected to the same GND rail (board, sensor, servo, speaker)
- [ ] The MPU-6050 sensor is connected to the **3.3 V** pin, NOT the 5 V pin
- [ ] The servo signal wire goes to **PA6 (Arduino D12)**
- [ ] The speaker signal wire goes to **PA0 (Arduino A0)**
- [ ] The USB cable is plugged in (this is the power source)
- [ ] You have a clear open space of at least 10 × 10 ft to walk in
- [ ] You know where the **B1 button** is on the board (the small blue button labelled USER or B1)

---

## Startup: What Happens When You Power On

The moment you plug in USB (or press reset), the board runs a **hardware self-test** to prove every part is working before the game starts. This lasts about 2 seconds.

### What you will see and hear:

```
1. Green LED turns on   (150 ms)
2. Amber LED turns on   (150 ms)
3. Red LED turns on     (150 ms)
4. All LEDs off briefly
5. All LEDs flash once together
6. Servo sweeps: CENTRE → hard LEFT → hard RIGHT → CENTRE
7. Three short ascending beeps: bloop... bloop... bloop
```

**If all of that happens:** every piece of hardware is working. 

**If the servo does not move:** check wiring to PA6 and the 5 V power.

**If you hear no beeps:** check wiring to PA0 and the speaker power wire.

**If you see red LEDs blinking and everything stops:** see the [Error Codes](#error-codes) section below.

---

## Step 1 — Calibration (Hold Still!)

Immediately after the self-test, the **amber LED turns on** and stays on. This is the calibration phase.

### What to do:

**Set the board flat on a table or hold it perfectly still for 2 full seconds.**

The board is measuring what "not moving" looks like so it can subtract that noise from your movements during the game. If you move it now, your heading will be wrong for the whole round.

### What you should see:
- Amber LED: solid on
- Speaker: silent
- Servo: staying at centre

### What you should NOT do during calibration:
- ❌ Do not walk around
- ❌ Do not tilt the board
- ❌ Do not bump the table

> **If the game always seems to point you in the wrong direction**, this is almost always caused by moving during calibration. Unplug USB, plug it back in, and hold still.

---

## Step 2 — Game Starts

After calibration (about 2 seconds), you will see:

- Amber LED turns **off**
- Green LED turns **on** (stays on the whole round)
- Servo moves to its starting position
- Beeping starts immediately

A hidden target has just been placed somewhere within roughly **20 feet** of where you are standing. It could be in any direction — behind you, to the side, ahead.

**Pick up the board (if you put it down), hold it at about waist height, roughly flat (face-up), and start walking.**

---

## Step 3 — Finding the Target

### How to use the servo

The servo arm points toward the target **relative to the direction you are facing right now**.

Think of it like a compass that always points at the target, but it reads relative to you:

| Servo position | What it means | What to do |
|---|---|---|
| Pointing straight ahead | Target is directly in front of you | Walk forward |
| Swung to the LEFT | Target is to your left | Turn LEFT until it centres |
| Swung to the RIGHT | Target is to your right | Turn RIGHT until it centres |
| Pointing hard left (maximum) | Target is behind you to the left | Turn around |
| Pointing hard right (maximum) | Target is behind you to the right | Turn around |

**Important:** The servo updates 100 times per second, so it responds instantly as you turn. Turn slowly and watch it swing until it centres — then walk in that direction.

### How to use the beeps

Every beep is a single short tone. The pitch and the speed both increase as you close in.

| What you hear | Zone | Distance | Amber LED |
|---|---|---|---|
| Slow, low thumps — one every second | Cold | > 22 ft away | Off |
| Faster, slightly higher tones | Warm | 14–22 ft away | Off |
| Noticeably quick, clearly higher pitch | Hot | 5–14 ft away | **Pulsing** |
| Rapid high-pitched chirps | Burning | 1–5 ft away | **Solid on** |
| You stop and the jingle plays | **WIN** | < 1 ft away | Solid |

**The pitch and speed change continuously** — there are no sudden jumps. As you step toward the target you should hear the tone rising and the cadence quickening at the same time.

### The golden rule: trust the servo, use the beeps for distance

The servo direction is more reliable than the absolute position estimate. If the beep says you are close but the servo is pointing sideways, trust the servo and turn — you are probably at the right distance but facing the wrong way.

---

## Step 4 — Winning

When you get within about **one foot (30 cm)** of the target location:

1. Servo centres and holds still
2. Beeping stops
3. The board plays a **four-note ascending fanfare** (C → E → G → HIGH C)
4. The servo does a **three-swing celebration waggle** left-right-left
5. All three LEDs flash together six times
6. Green LED stays on
7. The board waits for you to press B1

Serial terminal (if connected) prints:
```
>>> TARGET REACHED - YOU WIN! <<<
Press B1 to play again.
```

Nothing will happen until you press the button — take your time celebrating.

---

## Step 5 — Playing Again

Press the **B1 button** — the small blue button on the Nucleo board labelled **B1** or **USER**.

What happens when you press it:
- A brand-new target is placed at a random location
- The position estimate resets — **wherever you are standing right now becomes the new origin**
- The servo immediately moves toward the new target
- Beeping starts again

> **Tip:** Stand still for a moment before pressing B1 so the new round starts cleanly. The position estimate is most accurate right after a reset.

---

## Error Codes

If the board stops and the red LED starts blinking in a repeating pattern, count the blinks:

| Blink pattern | Cause | How to fix |
|---|---|---|
| **2 blinks** then 1 second pause, repeating | MPU-6050 sensor not found on I2C | Check that SDA→PB9, SCL→PB8, VCC→3.3V, GND→GND, AD0→GND. Confirm the sensor is not damaged. |
| **3 blinks** then 1 second pause, repeating | I2C communication failed after 3 retries | Same wiring check as above. Try a shorter/better wire on SDA/SCL. Make sure all grounds are connected. |
| Fast strobe (continuous rapid flashing) | Fatal hardware init failure | Power-cycle the board. If it persists, check that the board itself is not damaged. |

**Non-fatal warning — 4 amber blinks at startup (once only):**
The hardware random number generator was unavailable. The game continues normally using a fixed random seed — targets are still random-feeling, just from a predetermined sequence. This is harmless.

---

## Troubleshooting

**The servo points the wrong direction from the very start.**
Caused by moving during calibration. Power-cycle (unplug/replug USB) and hold the board completely still for the 2-second amber-LED phase.

**The beeping says I'm close but I can't find anything.**
This is expected behaviour. The position estimate drifts over time (this is a known hardware limitation of dead-reckoning — see note below). Follow the servo direction rather than distance alone. The bearing is reliable; the absolute position is approximate.

**The servo doesn't move at all.**
Check the wiring on PA6 (Arduino D12). Make sure the servo power (red wire) has 5 V and that the servo's ground wire is connected to the board's GND. If the startup self-test did not show the servo sweeping, the servo was already not working at boot.

**The beeping is very quiet or sounds distorted.**
Adjust the small trim potentiometer on the STEMMA Speaker board with a small screwdriver. Turn it clockwise to increase volume. Also confirm the speaker is powered from 5 V for full volume.

**The board resets itself when the servo moves.**
The servo is drawing too much current from the board's 5 V rail. Power the servo from a separate 5 V supply (USB power bank, separate regulator, etc.) while keeping the ground wire connected to the board GND.

**The game starts before I finish holding still.**
The calibration phase is exactly 2 seconds long. Put the board down on a flat surface before power-on and leave it there until the green LED turns on.

**The target seems to always be in the same general area game after game.**
This happens when the hardware RNG is unavailable (4 amber blinks at startup). The game is using a fixed seed and will produce the same sequence of targets every time. This is harmless but predictable.

> **On position drift:** The board estimates its position by measuring acceleration and integrating it twice to get distance. This technique (dead-reckoning) accumulates errors quickly — expect a few feet of error after 30–60 seconds of walking. The servo direction (bearing) does not have this problem because it uses the gyroscope, which is much more accurate. After several minutes of play the position estimate may be significantly off, but the servo will still point you roughly in the right direction. Starting a new round with B1 resets the estimate.

---

## Demo Script — Showing It To Someone

Here is a script for demonstrating the game to someone who has never seen it. The whole demo takes about 3 minutes.

**Setup (before they arrive):**
1. Power on the board and hold it still through calibration
2. Let the game start — wait until the green LED is on and beeping begins
3. Stand at the starting point

**Script:**

*"This is a navigation game running on a bare-metal microcontroller — no phone, no GPS, no Wi-Fi. Somewhere in this room, an invisible target has been placed at a random location. The board doesn't know where it is — it figures out where I'VE been by measuring the acceleration of my every step."*

[Hold up the board]

*"The servo arm is my compass — it points toward the target relative to whichever way I'm facing. Watch what happens when I turn."*

[Slowly rotate in place — the servo visibly tracks the direction of the target]

*"The speaker is my Geiger counter. Right now it's beeping slowly — I'm far away."*

[Start walking toward the target — the beeping speeds up and rises in pitch]

*"Hear that? Getting faster, getting higher. I'm getting close."*

[Get within a few feet — rapid beeping, amber LED solid]

*"The amber light means burning hot."*

[Find the target — win jingle, servo waggle, LED flash]

*"There it is. The servo just celebrated."*

[Press B1]

*"And just like that, a new target appears somewhere else. Every game is different because the target is placed using the chip's hardware random number generator."*

---

## Tips for Your Best Score

- **Slow down near the target.** When the beeps are rapid, take small careful steps. The win radius is only one foot — you can walk right through it at full pace.

- **Do a slow spin when you feel lost.** Stand still and turn in a full circle, watching the servo. It will sweep from one side to the other as you rotate through the target direction — the centre point of that sweep is where the target is.

- **Keep the board flat.** The sensor assumes gravity is pointing straight down. Tilting the board more than about 30° confuses the tilt estimate and can skew the bearing calculation.

- **Short games are more accurate than long ones.** The position estimate drifts with time and distance walked. If you cannot find the target after a few minutes, press B1 to reset with a fresh start.

- **If the servo is pegged hard left or right:** the target is somewhere behind you. Do a 180° turn.

---

## Custom Victory Sound

The board generates audio as a square-wave tone — the same way 1980s video game consoles made sound. It cannot play MP3s or WAV files, but you can change the melody.

**To change the win jingle**, edit the `notes[]` array in `Core/Src/audio.c` inside `Audio_WinJingle()`. Each number is a frequency in Hz.

```c
static const uint32_t notes[] = { 523U, 659U, 784U, 1047U };
//                                  C5    E5    G5    C6
```

Note frequency reference:

| | C | D | E | F | G | A | B |
|---|---|---|---|---|---|---|---|
| **Octave 4** | 262 | 294 | 330 | 349 | 392 | 440 | 494 |
| **Octave 5** | 523 | 587 | 659 | 698 | 784 | 880 | 988 |
| **Octave 6** | 1047 | 1175 | 1319 | 1397 | 1568 | 1760 | 1976 |

`0` = silence/rest. Each note plays for 150 ms; the last one is held for 300 ms.

For a note-by-note rhythm table with custom durations, see [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) — Custom Victory Sound → Option 2.

For true sampled audio (voice clips, recorded sounds), the chip has a built-in 12-bit DAC that could play PCM audio from flash memory, but this requires rewiring and significant new firmware. See Option 3 in [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md).
