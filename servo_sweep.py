#!/usr/bin/env python3
"""
servo_sweep.py — Bit-bang servo sweep on Jetson Orin Nano Super

Sweeps a hobby servo from 1010us to 1990us on Pin 33 (BOARD numbering).
Hardware PWM is NOT routed to the 40-pin header on this board, so we
bit-bang the GPIO directly using time.sleep() for timing.

Wiring:
  Pin 33 -> Servo signal (orange)
  Pin 4  -> Servo VCC (red, 5V)
  Pin 6  -> Servo GND (brown)

Usage: sudo python3 servo_sweep.py
"""

import Jetson.GPIO as GPIO
import time
import signal
import sys

# --- Configuration ---
SERVO_PIN = 33          # BOARD pin numbering
PERIOD = 0.020          # 20ms = 50Hz
MIN_PULSE = 0.001010    # 1010us — one end of travel
MAX_PULSE = 0.001990    # 1990us — other end of travel
STEP = 0.000010         # 10us per step
PULSES_PER_STEP = 5     # pulses to hold at each position

running = True


def signal_handler(sig, frame):
    global running
    running = False


def main():
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(SERVO_PIN, GPIO.OUT, initial=GPIO.LOW)

    print(f"Sweeping servo on Pin {SERVO_PIN}: "
          f"{MIN_PULSE*1e6:.0f}us <-> {MAX_PULSE*1e6:.0f}us")
    print("Ctrl+C to stop\n")

    try:
        while running:
            # Sweep up
            pulse = MIN_PULSE
            while pulse <= MAX_PULSE and running:
                for _ in range(PULSES_PER_STEP):
                    if not running:
                        break
                    GPIO.output(SERVO_PIN, GPIO.HIGH)
                    time.sleep(pulse)
                    GPIO.output(SERVO_PIN, GPIO.LOW)
                    time.sleep(PERIOD - pulse)
                pulse += STEP

            # Sweep down
            pulse = MAX_PULSE
            while pulse >= MIN_PULSE and running:
                for _ in range(PULSES_PER_STEP):
                    if not running:
                        break
                    GPIO.output(SERVO_PIN, GPIO.HIGH)
                    time.sleep(pulse)
                    GPIO.output(SERVO_PIN, GPIO.LOW)
                    time.sleep(PERIOD - pulse)
                pulse -= STEP

    finally:
        GPIO.output(SERVO_PIN, GPIO.LOW)
        GPIO.cleanup()
        print("\nStopped.")


if __name__ == "__main__":
    main()
