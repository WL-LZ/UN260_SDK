#include "backlight_service.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifndef UN260_BACKLIGHT_ROOT
#define UN260_BACKLIGHT_ROOT "/sys/class/backlight"
#endif
#ifndef UN260_BACKLIGHT_STATE
#define UN260_BACKLIGHT_STATE "/etc/ui_state"
#endif
static char g_device[256];
static int g_max, g_level;
static int read_number(const char *path)
{
    FILE *fp = fopen(path, "r");
    int n = -1; char tail;
    if(!fp) return -1;
    if(fscanf(fp, "%d %c", &n, &tail) != 1) n = -1;
    fclose(fp);
    return n;
}
bool backlight_service_probe(void)
{
    glob_t paths = {0};
    char path[320];
    g_device[0] = 0; g_max = g_level = 0;
    if(glob(UN260_BACKLIGHT_ROOT "/*", 0, NULL, &paths) != 0) { globfree(&paths); return false; }
    for(size_t i = 0; i < paths.gl_pathc; ++i) {
        if(strlen(paths.gl_pathv[i]) >= sizeof(g_device)) continue;
        snprintf(path, sizeof(path), "%s/max_brightness", paths.gl_pathv[i]);
        int maximum = read_number(path);
        if(maximum <= 1 || maximum > 4095) continue;
        snprintf(path, sizeof(path), "%s/brightness", paths.gl_pathv[i]);
        int level = read_number(path);
        if(level < 0 || level > maximum || access(path, W_OK)) continue;
        snprintf(g_device, sizeof(g_device), "%s", paths.gl_pathv[i]);
        g_max = maximum; g_level = level;
        break;
    }
    globfree(&paths);
    return g_device[0] != 0;
}
int backlight_service_max(void) { return g_max; }
int backlight_service_level(void) { return g_level; }
const char *backlight_service_device(void) { return g_device; }
bool backlight_service_set(int level)
{
    char path[320], value[24];
    if(!g_device[0] || level < 1 || level > g_max) return false;
    snprintf(path, sizeof(path), "%s/brightness", g_device);
    /* Read the live node: panel power management and external tools can
       change brightness independently of this process's cached value. */
    int live = read_number(path);
    if(live == level) { g_level = live; return true; }
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if(fd < 0) return false;
    int len = snprintf(value, sizeof(value), "%d\n", level);
    bool ok = write(fd, value, len) == len;
    if(close(fd)) ok = false;
    int actual = read_number(path);
    if(actual >= 0 && actual <= g_max) g_level = actual;
    snprintf(path, sizeof(path), "%s/actual_brightness", g_device);
    int reported = read_number(path);
    snprintf(path, sizeof(path), "%s/bl_power", g_device);
    int power = read_number(path);
    /* Driver readback is NOT proof of physical brightness. Keep diagnostics
       limited to user adjustments for checking PWM/wiring on real hardware. */
    fprintf(stderr, "BACKLIGHT_SET requested=%d readback=%d reported=%d max=%d power=%d write_ok=%d device=%s\n",
            level, actual, reported, g_max, power, ok, g_device);
    return ok && actual == level;
}
bool backlight_service_save(void)
{
    if(!g_device[0] || g_level < 1 || g_max < 2) return false;
    if(mkdir(UN260_BACKLIGHT_STATE, 0755) != 0 && errno != EEXIST) return false;
    const char *tmp = UN260_BACKLIGHT_STATE "/brightness.cfg.tmp";
    const char *final = UN260_BACKLIGHT_STATE "/brightness.cfg";
    int percent = (g_level * 100 + g_max / 2) / g_max;
    if(percent < 1) percent = 1;
    if(read_number(final) == percent) return true;
    FILE *fp = fopen(tmp, "w");
    if(!fp) return false;
    bool ok = fprintf(fp, "%d\n", percent) > 0;
    if(fflush(fp) || fsync(fileno(fp))) ok = false;
    if(fclose(fp)) ok = false;
    if(!ok || rename(tmp, final)) { unlink(tmp); return false; }
    int fd = open(UN260_BACKLIGHT_STATE, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if(fd < 0) return false;
    ok = fsync(fd) == 0;
    close(fd);
    return ok;
}
void backlight_service_init(void)
{
    if(!backlight_service_probe()) {
        fprintf(stderr, "BACKLIGHT unavailable: no writable multi-level backlight\n");
        return;
    }
    int percent = read_number(UN260_BACKLIGHT_STATE "/brightness.cfg");
    if(percent >= 1 && percent <= 100) {
        int level = (g_max * percent + 50) / 100;
        if(level < 1) level = 1;
        if(!backlight_service_set(level)) fprintf(stderr, "BACKLIGHT restore failed; retaining driver value\n");
    }
    fprintf(stderr, "BACKLIGHT device=%s level=%d max=%d\n", g_device, g_level, g_max);
}
