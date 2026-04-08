/*
 * servo_pwm.c — Precise bit-bang servo PWM for Jetson Orin Nano Super
 * Hardware PWM chips aren't routed to the 40-pin header on this board,
 * so we bit-bang GPIO with nanosecond-precision timing.
 *
 * NOTE: The Python version (servo_sweep.py) is simpler and in practice
 * performs comparably for standard hobby servos. Use this C version if
 * you need tighter timing guarantees or lower-level control.
 *
 * Usage: sudo ./servo_pwm <gpio_chip> <gpio_line> <pulse_us>
 *   gpio_chip: /dev/gpiochipN
 *   gpio_line: line number within the chip
 *   pulse_us:  pulse width in microseconds (1010-1990)
 *
 * Or pipe commands via stdin for sweep control.
 *
 * Build: gcc -O2 -o servo_pwm servo_pwm.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/gpio.h>

static volatile int running = 1;
static int fd = -1;
static int line_fd = -1;

static void cleanup(int sig) {
    running = 0;
}

static void precise_sleep_ns(long ns) {
    /* Busy-wait spin for short durations (pulse high) — no context switch */
    if (ns < 3000000L) { /* < 3ms: spin */
        struct timespec now, target;
        clock_gettime(CLOCK_MONOTONIC, &now);
        target.tv_sec = now.tv_sec + (now.tv_nsec + ns) / 1000000000L;
        target.tv_nsec = (now.tv_nsec + ns) % 1000000000L;
        while (1) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec > target.tv_sec ||
                (now.tv_sec == target.tv_sec && now.tv_nsec >= target.tv_nsec))
                break;
        }
    } else { /* > 3ms: use kernel sleep (low period) */
        struct timespec ts = { .tv_sec = ns / 1000000000L, .tv_nsec = ns % 1000000000L };
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
}

static void set_line(int val) {
    struct gpiohandle_data data = { .values = { val } };
    ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <gpio_chip> <line> <pulse_us|sweep>\n", argv[0]);
        fprintf(stderr, "  Static:  sudo %s /dev/gpiochip0 43 1500\n", argv[0]);
        fprintf(stderr, "  Sweep:   sudo %s /dev/gpiochip0 43 sweep\n", argv[0]);
        return 1;
    }

    const char *chip = argv[1];
    int line = atoi(argv[2]);
    int sweep_mode = (strcmp(argv[3], "sweep") == 0);
    int pulse_us = sweep_mode ? 1500 : atoi(argv[3]);

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    fd = open(chip, O_RDONLY);
    if (fd < 0) { perror("open gpiochip"); return 1; }

    struct gpiohandle_request req = {
        .lineoffsets = { line },
        .flags = GPIOHANDLE_REQUEST_OUTPUT,
        .default_values = { 0 },
        .lines = 1,
    };
    snprintf(req.consumer_label, sizeof(req.consumer_label), "servo");

    if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("GPIO_GET_LINEHANDLE_IOCTL");
        close(fd);
        return 1;
    }
    line_fd = req.fd;

    /* Lock memory and set real-time scheduling for minimal jitter */
    mlockall(MCL_CURRENT | MCL_FUTURE);
    struct sched_param sp = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0) {
        fprintf(stderr, "Warning: could not set RT priority (run as root)\n");
    }

    long period_ns = 20000000L; /* 50Hz = 20ms */

    if (sweep_mode) {
        int min_us = 1010, max_us = 1990, step = 10;
        printf("Sweeping %d-%d us on %s line %d\n", min_us, max_us, chip, line);
        printf("Ctrl+C to stop\n\n");

        while (running) {
            /* Sweep up */
            for (int us = min_us; us <= max_us && running; us += step) {
                long pulse_ns = (long)us * 1000L;
                /* Send 5 pulses per position */
                for (int i = 0; i < 5 && running; i++) {
                    set_line(1);
                    precise_sleep_ns(pulse_ns);
                    set_line(0);
                    precise_sleep_ns(period_ns - pulse_ns);
                }
            }
            /* Sweep down */
            for (int us = max_us; us >= min_us && running; us -= step) {
                long pulse_ns = (long)us * 1000L;
                for (int i = 0; i < 5 && running; i++) {
                    set_line(1);
                    precise_sleep_ns(pulse_ns);
                    set_line(0);
                    precise_sleep_ns(period_ns - pulse_ns);
                }
            }
        }
    } else {
        long pulse_ns = (long)pulse_us * 1000L;
        printf("Holding %d us on %s line %d (Ctrl+C to stop)\n", pulse_us, chip, line);
        while (running) {
            set_line(1);
            precise_sleep_ns(pulse_ns);
            set_line(0);
            precise_sleep_ns(period_ns - pulse_ns);
        }
    }

    set_line(0);
    close(line_fd);
    close(fd);
    printf("\nStopped.\n");
    return 0;
}
