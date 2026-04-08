# Technical Findings: Servo PWM on Jetson Orin Nano Super

## Summary

Hardware PWM chips on the Jetson Orin Nano Super (`pwmchip0` through `pwmchip3`) exist in sysfs and accept configuration writes, but **no electrical PWM signal is routed to any pin on the 40-pin GPIO header**. The Jetson.GPIO software PWM has too much jitter for servo control. The working solution is GPIO bit-bang toggling on Pin 33.

## Test Environment

- **Board:** NVIDIA Jetson Orin Nano Super (8GB)
- **JetPack:** 6
- **L4T:** R36 (Linux 5.15.148-tegra)
- **Jetson.GPIO:** 2.1.12
- **Power Mode:** MAXN Super
- **Servo:** Standard hobby micro servo (SG90 type)

## Results

| # | Method | Pin(s) | Signal Output? | Servo Moves? | Notes |
|---|--------|--------|----------------|-------------|-------|
| 1 | sysfs `pwmchip0` | 15, 32, 33 | No | No | Writes succeed, `enable` reads back 1, but no output |
| 2 | sysfs `pwmchip2` | 15, 32, 33 | No | No | Same behavior as pwmchip0 |
| 3 | sysfs `pwmchip3` | 15, 32, 33 | No | No | Same behavior as pwmchip0 |
| 4 | Pinmux devmem fix + sysfs PWM | 15 | No | No | Wrote pinmux register to PWM mode, still no signal |
| 5 | Pinmux devmem fix + sysfs PWM | 32 | No | No | Same as Pin 15 |
| 6 | Pinmux devmem fix + sysfs PWM | 33 | No | No | Same as Pin 15 |
| 7 | Jetson.GPIO software PWM | 33 | Yes (noisy) | Erratic | Pulse width jitter ~200-500us, servo buzzes/vibrates |
| 8 | **Python bit-bang (time.sleep)** | **33** | **Yes** | **Yes** | **Consistent 50Hz, some jitter but servo tracks** |
| 9 | **C bit-bang (clock_nanosleep)** | **33** | **Yes** | **Yes** | **Tighter timing but comparable servo behavior** |

## Key Insight

The pwmchip sysfs interface on the Orin Nano Super is deceptive. All operations succeed without error:

```bash
# These all succeed — but produce NO electrical output
echo 0 > /sys/class/pwm/pwmchip0/export
echo 20000000 > /sys/class/pwm/pwmchip0/pwm0/period
echo 1500000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable
cat /sys/class/pwm/pwmchip0/pwm0/enable  # Returns "1"
```

The registers accept writes and report the expected state, but the PWM signals are not physically routed to header pins. This is a hardware/pinmux routing issue specific to the Orin Nano Super carrier board, not a software configuration problem.

## Comparison to Older Jetson Nano B01

On the original Jetson Nano B01 (and Jetson Nano 2GB), **Pins 32 and 33 are genuine hardware PWM outputs**:

- Pin 32 maps to `pwmchip0/pwm0`
- Pin 33 maps to `pwmchip2/pwm0`

The sysfs PWM interface works correctly on these older boards. Standard tutorials and libraries that reference hardware PWM on pins 32/33 **do not apply** to the Orin Nano Super.

## JETGPIO Library Note

The [JETGPIO](https://github.com/Rubberazer/JETGPIO) library documents **Pin 15** as the only hardware PWM output on the Orin Nano. Our testing showed no signal output on Pin 15 even after applying the pinmux fix via `busybox devmem 0x2440020 w 0x5`.

## Why Bit-Bang Works

Direct GPIO toggling via `Jetson.GPIO` (which uses the kernel GPIO character device interface) reliably controls pin state. The timing loop:

1. Set pin HIGH
2. Sleep for pulse width (1000-2000us)
3. Set pin LOW
4. Sleep for remainder of 20ms period

Python's `time.sleep()` on the Orin Nano Super has sufficient precision (~50us jitter) for hobby servo control.

## Recommendations

1. **Use Python bit-bang** (`servo_sweep.py`) for simplicity
2. **Use C bit-bang** (`servo_pwm.c`) only if you need deterministic sub-microsecond timing
3. **Do not waste time on sysfs PWM** — it will appear to work but produces no output
4. **Pin 33** is confirmed working; other GPIO pins likely work too
5. For multi-servo or precision applications, use an external PCA9685 I2C servo driver
