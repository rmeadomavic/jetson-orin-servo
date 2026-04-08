# Servo Control on Jetson Orin Nano Super — GPIO Bit-Bang Workaround

## Problem

The NVIDIA Jetson Orin Nano Super has hardware PWM chips (`pwmchip0` through `pwmchip3`) visible via sysfs, but **none of them are routed to the 40-pin GPIO header**. The standard `Jetson.GPIO` library's software PWM is too imprecise for hobby servo control (pulse widths drift by hundreds of microseconds, causing jitter and failed positioning).

This means the conventional approaches to servo control on Jetson boards do not work on the Orin Nano Super:

- **sysfs hardware PWM** — chips accept writes, report as enabled, but produce no electrical signal on header pins
- **Pinmux fixes via devmem** — register writes succeed but do not route PWM to pins
- **Jetson.GPIO software PWM** — too much timing jitter for servo signal requirements

## What Was Tested and Failed

| Method | Pins Tested | Result |
|--------|-------------|--------|
| sysfs `pwmchip0` | 15, 32, 33 | No signal output |
| sysfs `pwmchip2` | 15, 32, 33 | No signal output |
| sysfs `pwmchip3` | 15, 32, 33 | No signal output |
| Pinmux devmem fixes | 15, 32, 33 | Register writes succeed, still no signal |
| Jetson.GPIO software PWM | 33 | Signal present but too imprecise for servos |

**Note:** Pin 15 is documented by the [JETGPIO](https://github.com/Rubberazer/JETGPIO) library as the only hardware PWM output on the Orin Nano. Our testing showed no signal output on Pin 15 even after applying the pinmux fix.

## What Works: Python Bit-Bang on Pin 33

Direct GPIO bit-bang toggling using `Jetson.GPIO` on **Pin 33** (BOARD numbering) produces clean, consistent servo signals. The Python `time.sleep()` timing is sufficient for the 1000-2000 us pulse widths that hobby servos require.

## Wiring

```
Jetson Orin Nano Super              Servo
40-Pin Header                       Connector
┌──────────┐                       ┌─────────┐
│ Pin 4  (5V)  ├───── Red ────────┤ VCC     │
│ Pin 6  (GND) ├───── Brown ──────┤ GND     │
│ Pin 33 (GPIO)├───── Orange ─────┤ Signal  │
└──────────┘                       └─────────┘
```

See `wiring.txt` for the full ASCII diagram.

## Quick Start

### Python (recommended)

```bash
sudo python3 servo_sweep.py
```

This sweeps the servo back and forth between 1010 us and 1990 us on Pin 33. Press Ctrl+C to stop cleanly.

### Minimal Example

```python
import Jetson.GPIO as GPIO
import time

GPIO.setmode(GPIO.BOARD)
GPIO.setup(33, GPIO.OUT, initial=GPIO.LOW)

try:
    while True:
        # 1500us pulse = center position, 20ms period (50Hz)
        GPIO.output(33, GPIO.HIGH)
        time.sleep(0.0015)
        GPIO.output(33, GPIO.LOW)
        time.sleep(0.0185)
except KeyboardInterrupt:
    pass
finally:
    GPIO.output(33, GPIO.LOW)
    GPIO.cleanup()
```

### C Version

A C implementation (`servo_pwm.c`) is also provided for users who want tighter timing via `clock_nanosleep` and real-time scheduling. In practice, the Python version performed comparably for standard hobby servos.

```bash
gcc -O2 -o servo_pwm servo_pwm.c
sudo ./servo_pwm /dev/gpiochip0 43 sweep
```

## Tested On

- **Board:** NVIDIA Jetson Orin Nano Super (8GB)
- **JetPack:** 6
- **L4T:** R36
- **Jetson.GPIO:** 2.1.12
- **Power Mode:** MAXN Super

## Files

| File | Description |
|------|-------------|
| `servo_sweep.py` | Python bit-bang servo sweep script |
| `servo_pwm.c` | C bit-bang servo control (nanosecond timing) |
| `wiring.txt` | ASCII wiring diagram |
| `findings.md` | Detailed technical investigation notes |

## License

MIT License. See [LICENSE](LICENSE).
