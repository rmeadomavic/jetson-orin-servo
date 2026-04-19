#!/bin/bash
# Enable hardware PWM on Jetson Orin Nano Super 40-pin header.
# Combines PWM clock enables (Rubberazer, JETGPIO v1.2) with pinmux register
# writes (lhoang, NVIDIA DevTalk #366289) so pins 15, 32, and 33 route to the
# PWM controllers instead of GPIO.

set -e

# Enable PWM controller clocks via bpmp debug.
for c in pwm1 pwm5 pwm7 pwm8; do
    echo 1 > "/sys/kernel/debug/bpmp/debug/clk/$c/state"
done

# Pinmux: route header pins to PWM function.
#   Pin 15 (GPIO12) -> PWM1  (pwmchip0, 3280000.pwm)
#   Pin 32 (GPIO07) -> PWM7  (pwmchip3, 32e0000.pwm)
#   Pin 33 (GPIO13) -> PWM5  (pwmchip2, 32c0000.pwm)
busybox devmem 0x02440020 32 0x00000404
busybox devmem 0x02434080 32 0x00000404
busybox devmem 0x02434040 32 0x00000405
