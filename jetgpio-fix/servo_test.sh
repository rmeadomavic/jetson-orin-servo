#!/bin/bash
# Servo test on pin 33 (pwmchip2, PWM5).
# Assumes jetson-pwm-enable.service has run (clocks + pinmux).
set -e

CHIP=/sys/class/pwm/pwmchip2
PWM=$CHIP/pwm0

[ -d $PWM ] || echo 0 > $CHIP/export
sleep 0.1

# guard against re-run (duty must be <= period; zero duty first)
echo 0 > $PWM/duty_cycle 2>/dev/null || true
echo 20000000 > $PWM/period          # 50 Hz
echo 1500000  > $PWM/duty_cycle      # 1.5 ms -> neutral
echo 1 > $PWM/enable

echo "neutral (1.5ms), 2s"
sleep 2

for us in 1000000 2000000 1500000; do
  echo $us > $PWM/duty_cycle
  echo "duty $us ns"
  sleep 1.5
done

echo 0 > $PWM/enable
