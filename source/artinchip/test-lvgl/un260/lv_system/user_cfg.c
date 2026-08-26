#include "lvgl/lvgl.h"
#include"user_cfg.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_CURRENCY_COUNT 14
#define UI_STATE_DIR "/etc/ui_state"
#define USER_PASSWORD_PATH UI_STATE_DIR "/password.cfg"
#define SCREENSHOT_CFG_PATH UI_STATE_DIR "/screenshot.cfg"
#define SCREENSHOT_CFG_TMP_PATH UI_STATE_DIR "/screenshot.cfg.tmp"
#define SCREEN_RECORDING_CFG_PATH UI_STATE_DIR "/screen_recording.cfg"
#define SCREEN_RECORDING_CFG_TMP_PATH UI_STATE_DIR "/screen_recording.cfg.tmp"
#define PERFORMANCE_MONITOR_CFG_PATH UI_STATE_DIR "/performance_monitor.cfg"
#define PERFORMANCE_MONITOR_CFG_TMP_PATH UI_STATE_DIR "/performance_monitor.cfg.tmp"
#define PERFORMANCE_PROFILE_CFG_PATH UI_STATE_DIR "/performance_profile.cfg"
#define PERFORMANCE_PROFILE_CFG_TMP_PATH UI_STATE_DIR "/performance_profile.cfg.tmp"
#define GESTURE_CFG_PATH UI_STATE_DIR "/gestures.cfg"
#define GESTURE_CFG_TMP_PATH UI_STATE_DIR "/gestures.cfg.tmp"

static bool g_screenshot_enabled = true;
static bool g_screen_recording_enabled = false;
static bool g_performance_monitor_enabled = false;
static bool g_performance_profile_enabled = false;
static bool g_gesture_enabled = false;

static char g_user_password[USER_PASSWORD_MAX_LEN + 1] = "1111";

static bool user_cfg_password_is_valid(const char* password)
{
    size_t len;

    if (!password) return false;

    len = strlen(password);
    if (len == 0 || len > USER_PASSWORD_MAX_LEN) return false;

    for (size_t i = 0; i < len; i++) {
        if (password[i] < '0' || password[i] > '9') {
            return false;
        }
    }

    return true;
}

static void user_cfg_password_store(const char *password)
{
    size_t len = strlen(password);

    if (len > USER_PASSWORD_MAX_LEN) {
        len = USER_PASSWORD_MAX_LEN;
    }
    memcpy(g_user_password, password, len);
    g_user_password[len] = '\0';
}

bool user_cfg_password_load(void)
{
    FILE* fp;
    char buf[USER_PASSWORD_MAX_LEN + 4];
    size_t len;

    fp = fopen(USER_PASSWORD_PATH, "r");
    if (!fp) return false;

    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }

    if (!user_cfg_password_is_valid(buf)) {
        return false;
    }

    user_cfg_password_store(buf);
    return true;
}

bool user_cfg_password_save(const char* password)
{
    FILE* fp;

    if (!user_cfg_password_is_valid(password)) {
        return false;
    }

    mkdir(UI_STATE_DIR, 0755);
    fp = fopen(USER_PASSWORD_PATH, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "%s\n", password);
    fclose(fp);
    user_cfg_password_store(password);
    return true;
}

const char *user_cfg_password_get(void)
{
    return g_user_password;
}

static bool user_cfg_bool_load(const char *path, bool default_value,
                               bool *value_out)
{
    FILE* fp;
    int value;

    if (path == NULL || value_out == NULL) {
        return false;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        *value_out = default_value;
        return false;
    }

    if (fscanf(fp, "%d", &value) != 1 || (value != 0 && value != 1)) {
        fclose(fp);
        *value_out = default_value;
        return false;
    }

    fclose(fp);
    *value_out = value != 0;
    return true;
}

static bool user_cfg_bool_save(const char *path, const char *temp_path,
                               bool enabled, bool *value_out)
{
    FILE* fp;
    int fd;
    bool write_ok = true;

    if (path == NULL || temp_path == NULL || value_out == NULL) {
        return false;
    }
    if (mkdir(UI_STATE_DIR, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    fp = fopen(temp_path, "w");
    if (fp == NULL) {
        return false;
    }

    if (fprintf(fp, "%d\n", enabled ? 1 : 0) < 0 || fflush(fp) != 0) {
        fclose(fp);
        unlink(temp_path);
        return false;
    }

    fd = fileno(fp);
    if (fd < 0 || fsync(fd) != 0) {
        write_ok = false;
    }
    if (fclose(fp) != 0) {
        write_ok = false;
    }
    if (!write_ok) {
        unlink(temp_path);
        return false;
    }

    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        return false;
    }

    *value_out = enabled;
    return true;
}

bool user_cfg_screenshot_load(void)
{
    return user_cfg_bool_load(SCREENSHOT_CFG_PATH, true,
                              &g_screenshot_enabled);
}

bool user_cfg_screenshot_save(bool enabled)
{
    return user_cfg_bool_save(SCREENSHOT_CFG_PATH, SCREENSHOT_CFG_TMP_PATH,
                              enabled, &g_screenshot_enabled);
}

bool user_cfg_screenshot_enabled(void)
{
    return g_screenshot_enabled;
}

bool user_cfg_screen_recording_load(void)
{
    return user_cfg_bool_load(SCREEN_RECORDING_CFG_PATH, false,
                              &g_screen_recording_enabled);
}

bool user_cfg_screen_recording_save(bool enabled)
{
    return user_cfg_bool_save(SCREEN_RECORDING_CFG_PATH,
                              SCREEN_RECORDING_CFG_TMP_PATH,
                              enabled, &g_screen_recording_enabled);
}

bool user_cfg_screen_recording_enabled(void)
{
    return g_screen_recording_enabled;
}

bool user_cfg_performance_monitor_load(void)
{
    return user_cfg_bool_load(PERFORMANCE_MONITOR_CFG_PATH, false,
                              &g_performance_monitor_enabled);
}

bool user_cfg_performance_monitor_save(bool enabled)
{
    return user_cfg_bool_save(PERFORMANCE_MONITOR_CFG_PATH,
                              PERFORMANCE_MONITOR_CFG_TMP_PATH,
                              enabled, &g_performance_monitor_enabled);
}

bool user_cfg_performance_monitor_enabled(void)
{
    return g_performance_monitor_enabled;
}

bool user_cfg_performance_profile_load(void)
{
    return user_cfg_bool_load(PERFORMANCE_PROFILE_CFG_PATH, false,
                              &g_performance_profile_enabled);
}

bool user_cfg_performance_profile_save(bool enabled)
{
    return user_cfg_bool_save(PERFORMANCE_PROFILE_CFG_PATH,
                              PERFORMANCE_PROFILE_CFG_TMP_PATH,
                              enabled, &g_performance_profile_enabled);
}

bool user_cfg_performance_profile_enabled(void)
{
    return g_performance_profile_enabled;
}

bool user_cfg_gesture_load(void)
{
    return user_cfg_bool_load(GESTURE_CFG_PATH, false, &g_gesture_enabled);
}

bool user_cfg_gesture_save(bool enabled)
{
    return user_cfg_bool_save(GESTURE_CFG_PATH, GESTURE_CFG_TMP_PATH,
                              enabled, &g_gesture_enabled);
}

bool user_cfg_gesture_enabled(void)
{
    return g_gesture_enabled;
}
