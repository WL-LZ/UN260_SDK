#define _POSIX_C_SOURCE 200809L
#include "startup_devices.h"
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
#ifndef STARTUP_DEVICE_SCRIPT
#define STARTUP_DEVICE_SCRIPT "/etc/init.d/S00lvgl"
#endif
#ifndef STARTUP_DEVICE_TIMEOUT_MS
#define STARTUP_DEVICE_TIMEOUT_MS 12000U
#endif

static unsigned long long monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned long long)now.tv_sec * 1000U + now.tv_nsec / 1000000U;
}

bool startup_devices_prepare(void)
{
    const char *enabled = getenv("UN260_STARTUP_PREPARE");
    if (!enabled || strcmp(enabled, "1") != 0) return true;
    posix_spawnattr_t attr;
    if (posix_spawnattr_init(&attr) != 0) return false;
    int error = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    if (!error) error = posix_spawnattr_setpgroup(&attr, 0);
    char *argv[] = { STARTUP_DEVICE_SCRIPT, "prepare-devices", NULL };
    pid_t child = -1;
    if (!error) error = posix_spawn(&child, argv[0], NULL, &attr, argv, environ);
    posix_spawnattr_destroy(&attr);
    if (error) {
        fprintf(stderr, "startup devices: launch failed (%d)\n", error);
        return false;
    }
    unsigned long long start = monotonic_ms();
    bool terminated = false, killed = false;
    for (;;) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child)
            return !terminated && WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if (result < 0 && errno != EINTR) return false;
        unsigned long long elapsed = monotonic_ms() - start;
        if (!terminated && elapsed >= STARTUP_DEVICE_TIMEOUT_MS) {
            /* Only our unreaped child's group, never other system services. */
            kill(-child, SIGTERM);
            terminated = true;
            fprintf(stderr, "startup devices: preparation timeout\n");
        }
        if (terminated && !killed && elapsed >= STARTUP_DEVICE_TIMEOUT_MS + 250U) {
            kill(-child, SIGKILL);
            killed = true;
        }
        const struct timespec pause = {0, 10000000};
        nanosleep(&pause, NULL);
    }
}
