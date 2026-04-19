# Hardware PWM on Jetson Orin Nano Super: The Real Fix

## TL;DR

Hardware PWM on the Orin Nano Super's 40-pin header requires two things that neither the JetPack docs nor `jetson-gpio` do for you:

1. Enable the PWM controller **clocks** via `bpmp` debug.
2. Write the **pinmux** registers so the pads route to PWM instead of GPIO.

Do both and sysfs PWM just works. The bit-bang workaround originally in this repo is no longer necessary.

Pin / controller / register map:

| Header pin | SoC GPIO | Controller     | pwmchip  | Pinmux reg   | Pinmux value |
|------------|----------|----------------|----------|--------------|--------------|
| 15         | GPIO12   | PWM1 @ 3280000 | pwmchip0 | `0x02440020` | `0x00000404` |
| 32         | GPIO07   | PWM7 @ 32e0000 | pwmchip3 | `0x02434080` | `0x00000404` |
| 33         | GPIO13   | PWM5 @ 32c0000 | pwmchip2 | `0x02434040` | `0x00000405` |

## Credits

* **Rubberazer** (author of [JETGPIO](https://github.com/Rubberazer/JETGPIO)) identified that PWM clocks are disabled by default on Orin and must be enabled through `/sys/kernel/debug/bpmp/debug/clk/pwmN/state`. JETGPIO v1.2 ships the original `pwm_enabler.sh`.
* **lhoang** (NVIDIA) provided the pinmux register addresses and values on the NVIDIA DevTalk thread linked below.
* Discussion thread: [Hardware PWM not routed to 40-pin header on Orin Nano Super (JetPack 6 / L4T R36)](https://forums.developer.nvidia.com/t/hardware-pwm-not-routed-to-40-pin-header-on-orin-nano-super-jetpack-6-l4t-r36-sysfs-writes-succeed-but-no-signal-output/366289).

## Quick install (persistent across reboots)

```bash
sudo install -m 0755 jetgpio-fix/jetson-pwm-enable.sh /usr/local/sbin/jetson-pwm-enable.sh
sudo install -m 0644 jetgpio-fix/jetson-pwm-enable.service /etc/systemd/system/jetson-pwm-enable.service
sudo systemctl daemon-reload
sudo systemctl enable --now jetson-pwm-enable.service
```

The service runs once at boot, enables the PWM clocks, and writes the pinmux registers for pins 15, 32, and 33.

## Driving a servo (sysfs, hardware PWM)

After the service is active:

```bash
# Pin 33 (PWM5)
echo 0 | sudo tee /sys/class/pwm/pwmchip2/export
echo 20000000 | sudo tee /sys/class/pwm/pwmchip2/pwm0/period       # 50 Hz
echo 1500000  | sudo tee /sys/class/pwm/pwmchip2/pwm0/duty_cycle   # 1.5 ms neutral
echo 1        | sudo tee /sys/class/pwm/pwmchip2/pwm0/enable
```

A complete sweep example is in `jetgpio-fix/servo_test.sh`.

Wiring (standard hobby servo):

```
Pin 2 or 4  (5V)  ──── red    ─── VCC
Pin 6       (GND) ──── brown  ─── GND
Pin 33      (PWM5)──── orange ─── Signal
```

Pin 33 is a 3.3 V signal. Micro servos (SG90/MG90S class) typically accept it directly. Anything larger should use an external 5 V supply with common ground, not the header's 5 V rail.

## Tested on

* Board: NVIDIA Jetson Orin Nano Super (8 GB)
* JetPack 6, L4T R36, kernel 5.15.148-tegra
* Power mode: MAXN Super

## Files

| Path | Purpose |
|------|---------|
| `jetgpio-fix/jetson-pwm-enable.sh` | Clock + pinmux enable script (installs to `/usr/local/sbin`) |
| `jetgpio-fix/jetson-pwm-enable.service` | Systemd unit that runs the script at boot |
| `jetgpio-fix/servo_test.sh` | Sysfs servo sweep on pin 33 |
| `jetgpio-fix/pwm_enabler.sh` | Original clock-only enabler from JETGPIO v1.2 |
| `jetgpio-fix/pwm_enable.service` | Original JETGPIO systemd unit |
| `servo_sweep.py`, `servo_pwm.c` | Legacy bit-bang workaround from before the real fix was known. Kept for reference. |
| `findings.md`, `wiring.txt` | Original investigation notes and wiring diagram. |

## Historical note

The first version of this repo documented a Python/C bit-bang workaround after hardware PWM appeared to be unroutable to the header. That conclusion was wrong: the pinmux writes the original investigation tried were the right approach, but the PWM controller clocks had to be enabled first. Without the clock, the pinmux change has no visible effect, which is what made the original debugging so confusing.

## License

MIT. See [LICENSE](LICENSE).
