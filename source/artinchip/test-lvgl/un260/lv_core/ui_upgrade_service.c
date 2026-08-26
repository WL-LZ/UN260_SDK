#include "ui_upgrade_service.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#include "un260/lv_system/app_clock.h"
#include "un260/storage/usb_storage.h"

#define UI_UPGRADE_FILE_PATH           USB_STORAGE_MOUNT_POINT "/update/test_lvgl"
#define UI_UPGRADE_RUNNING_FILE_PATH   "/proc/self/exe"
#define UI_UPGRADE_SCRIPT_PATH         "/usr/bin/ui_update.sh"  
#define UI_UPGRADE_STATUS_FILE_PATH    "/tmp/ui_update.status"
#define UI_UPGRADE_STATUS_MAX_SIZE     512U

typedef struct {
    bool valid;
    dev_t device;
    ino_t inode;
    off_t size;
    time_t mtime;
    time_t ctime;
    uint64_t hash;
} ui_upgrade_hash_cache_t;

typedef struct {
    bool running;
    bool finished;
    bool success;
    pid_t child_pid;
    unsigned long start_ms;
    char status_content[UI_UPGRADE_STATUS_MAX_SIZE];
    size_t status_content_len;
    ui_upgrade_service_status_t status;
} ui_upgrade_service_ctx_t;

static ui_upgrade_service_ctx_t g_ui_upgrade_service;
static bool g_ui_upgrade_running_hash_ready = false;
static uint64_t g_ui_upgrade_running_hash = 0;
static ui_upgrade_hash_cache_t g_ui_upgrade_pkg_hash_cache;

static unsigned long ui_upgrade_service_now_ms(void)
{
    return (unsigned long)app_clock_monotonic_ms();
}

static bool ui_upgrade_service_file_exists(const char* path)
{
    struct stat st;
    return (path != NULL) && (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

static bool ui_upgrade_service_hash_file_fnv1a64(const char* path, uint64_t* hash_out)
{
    FILE* fp;
    size_t read_len;
    unsigned char buf[4096];
    uint64_t hash = 1469598103934665603ULL;

    if (path == NULL || hash_out == NULL) {
        return false;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }

    while ((read_len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        size_t i;
        for (i = 0; i < read_len; i++) {
            hash ^= (uint64_t)buf[i];
            hash *= 1099511628211ULL;
        }
    }

    if (ferror(fp) != 0) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    *hash_out = hash;
    return true;
}

static void ui_upgrade_service_hash_cache_clear(ui_upgrade_hash_cache_t* cache)
{
    if (cache != NULL) {
        memset(cache, 0, sizeof(*cache));
    }
}

static bool ui_upgrade_service_hash_file_cached(const char* path,
                                                ui_upgrade_hash_cache_t* cache,
                                                uint64_t* hash_out)
{
    struct stat before;
    struct stat after;
    uint64_t hash = 0;

    if (path == NULL || cache == NULL || hash_out == NULL) {
        return false;
    }

    if (stat(path, &before) != 0 || !S_ISREG(before.st_mode)) {
        ui_upgrade_service_hash_cache_clear(cache);
        return false;
    }

    if (cache->valid &&
        cache->device == before.st_dev &&
        cache->inode == before.st_ino &&
        cache->size == before.st_size &&
        cache->mtime == before.st_mtime &&
        cache->ctime == before.st_ctime) {
        *hash_out = cache->hash;
        return true;
    }

    if (!ui_upgrade_service_hash_file_fnv1a64(path, &hash)) {
        ui_upgrade_service_hash_cache_clear(cache);
        return false;
    }
    if (stat(path, &after) != 0 || !S_ISREG(after.st_mode) ||
        before.st_dev != after.st_dev ||
        before.st_ino != after.st_ino ||
        before.st_size != after.st_size ||
        before.st_mtime != after.st_mtime ||
        before.st_ctime != after.st_ctime) {
        ui_upgrade_service_hash_cache_clear(cache);
        return false;
    }

    cache->valid = true;
    cache->device = after.st_dev;
    cache->inode = after.st_ino;
    cache->size = after.st_size;
    cache->mtime = after.st_mtime;
    cache->ctime = after.st_ctime;
    cache->hash = hash;
    *hash_out = hash;
    return true;
}

static ui_upgrade_package_hash_status_t
ui_upgrade_service_get_package_hash_status(void)
{
    uint64_t package_hash = 0;

    if (!g_ui_upgrade_running_hash_ready) {
        if (!ui_upgrade_service_hash_file_fnv1a64(UI_UPGRADE_RUNNING_FILE_PATH,
                                                  &g_ui_upgrade_running_hash)) {
            return UI_UPGRADE_PACKAGE_HASH_ERROR;
        }
        g_ui_upgrade_running_hash_ready = true;
    }

    if (!ui_upgrade_service_hash_file_cached(UI_UPGRADE_FILE_PATH,
                                             &g_ui_upgrade_pkg_hash_cache,
                                             &package_hash)) {
        return UI_UPGRADE_PACKAGE_HASH_ERROR;
    }

    return package_hash == g_ui_upgrade_running_hash ?
           UI_UPGRADE_PACKAGE_HASH_MATCH :
           UI_UPGRADE_PACKAGE_HASH_DIFFERENT;
}

static void ui_upgrade_service_set_status(bool running,
                                          bool finished,
                                          bool success,
                                          int progress,
                                          ui_upgrade_stage_t stage,
                                          const char* step_text,
                                          const char* result_text)
{
    g_ui_upgrade_service.status.running = running;
    g_ui_upgrade_service.status.finished = finished;
    g_ui_upgrade_service.status.success = success;
    g_ui_upgrade_service.status.progress = progress;
    g_ui_upgrade_service.status.stage = stage;

    snprintf(g_ui_upgrade_service.status.step_text,
             sizeof(g_ui_upgrade_service.status.step_text),
             "%s", step_text ? step_text : "");
    snprintf(g_ui_upgrade_service.status.result_text,
             sizeof(g_ui_upgrade_service.status.result_text),
             "%s", result_text ? result_text : "");
}

static ui_upgrade_stage_t ui_upgrade_service_stage_from_name(const char* stage_name)
{
    if (stage_name == NULL) return UI_UPGRADE_STAGE_NONE;
    if (strcmp(stage_name, "verify") == 0) return UI_UPGRADE_STAGE_VERIFY;
    if (strcmp(stage_name, "write") == 0) return UI_UPGRADE_STAGE_WRITE;
    if (strcmp(stage_name, "finish") == 0) return UI_UPGRADE_STAGE_FINISH;
    if (strcmp(stage_name, "success") == 0) return UI_UPGRADE_STAGE_SUCCESS;
    if (strcmp(stage_name, "fail") == 0) return UI_UPGRADE_STAGE_FAIL;
    return UI_UPGRADE_STAGE_NONE;
}

static bool ui_upgrade_service_parse_int(const char* text,
                                         int min_value,
                                         int max_value,
                                         int* value_out)
{
    char* end;
    long value;

    if (text == NULL || value_out == NULL) {
        return false;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || errno != 0) {
        return false;
    }
    if (*end == '\r') {
        end++;
    }
    if (*end != '\0' || value < min_value || value > max_value) {
        return false;
    }

    *value_out = (int)value;
    return true;
}

static void ui_upgrade_service_set_progress_by_time(void)
{
    unsigned long elapsed_ms = ui_upgrade_service_now_ms() - g_ui_upgrade_service.start_ms;
    int progress = 0;
    ui_upgrade_stage_t stage = UI_UPGRADE_STAGE_VERIFY;
    const char* step_text = "Verifying upgrade package";

    if (elapsed_ms < 1200UL) {
        progress = (int)((elapsed_ms * 16UL) / 1200UL);
        stage = UI_UPGRADE_STAGE_VERIFY;
        step_text = "Verifying upgrade package";
    } else if (elapsed_ms < 5200UL) {
        progress = 24 + (int)(((elapsed_ms - 1200UL) * 50UL) / 4000UL);
        stage = UI_UPGRADE_STAGE_WRITE;
        step_text = "Writing system files";
    } else if (elapsed_ms < 7000UL) {
        progress = 82 + (int)(((elapsed_ms - 5200UL) * 13UL) / 1800UL);
        stage = UI_UPGRADE_STAGE_FINISH;
        step_text = "Finalizing upgrade";
    } else {
        progress = 95;
        stage = UI_UPGRADE_STAGE_FINISH;
        step_text = "Finalizing upgrade";
    }

    if (progress > g_ui_upgrade_service.status.progress) {
        ui_upgrade_service_set_status(true, false, false, progress, stage, step_text, "");
    }
}

static void ui_upgrade_service_load_status_file(void)
{
    FILE* fp;
    char content[UI_UPGRADE_STATUS_MAX_SIZE];
    char parse_content[UI_UPGRADE_STATUS_MAX_SIZE];
    char* line;
    char* save_ptr = NULL;
    char step_text[64] = {0};
    char result_text[128] = {0};
    char stage_name[32] = {0};
    ui_upgrade_stage_t stage;
    size_t content_len;
    bool has_progress = false;
    bool has_stage = false;
    bool has_step = false;
    int progress = -1;
    int success = -1;

    fp = fopen(UI_UPGRADE_STATUS_FILE_PATH, "r");
    if (fp == NULL) return;

    content_len = fread(content, 1, sizeof(content) - 1U, fp);
    if (ferror(fp) != 0 || !feof(fp)) {
        fclose(fp);
        return;
    }
    fclose(fp);

    if (content_len == 0U || content[content_len - 1U] != '\n') {
        return;
    }
    content[content_len] = '\0';
    if (content_len == g_ui_upgrade_service.status_content_len &&
        memcmp(content, g_ui_upgrade_service.status_content, content_len) == 0) {
        return;
    }

    memcpy(parse_content, content, content_len + 1U);
    line = strtok_r(parse_content, "\n", &save_ptr);
    while (line != NULL) {
        if (strncmp(line, "progress=", 9) == 0) {
            if (!ui_upgrade_service_parse_int(line + 9, 0, 100, &progress)) {
                return;
            }
            has_progress = true;
        } else if (strncmp(line, "stage=", 6) == 0) {
            if (sscanf(line + 6, "%31s", stage_name) != 1) {
                return;
            }
            has_stage = true;
        } else if (strncmp(line, "step=", 5) == 0) {
            snprintf(step_text, sizeof(step_text), "%s", line + 5);
            has_step = true;
        } else if (strncmp(line, "message=", 8) == 0) {
            snprintf(result_text, sizeof(result_text), "%s", line + 8);
        } else if (strncmp(line, "success=", 8) == 0) {
            if (!ui_upgrade_service_parse_int(line + 8, 0, 1, &success)) {
                return;
            }
        }
        line = strtok_r(NULL, "\n", &save_ptr);
    }

    if (!has_progress || !has_stage || !has_step) {
        return;
    }
    stage = ui_upgrade_service_stage_from_name(stage_name);
    if (stage == UI_UPGRADE_STAGE_NONE ||
        (success == 1 && stage != UI_UPGRADE_STAGE_SUCCESS) ||
        (success == 0 && stage != UI_UPGRADE_STAGE_FAIL) ||
        (success < 0 && (stage == UI_UPGRADE_STAGE_SUCCESS ||
                         stage == UI_UPGRADE_STAGE_FAIL))) {
        return;
    }

    memcpy(g_ui_upgrade_service.status_content, content, content_len + 1U);
    g_ui_upgrade_service.status_content_len = content_len;

    g_ui_upgrade_service.status.progress = progress;
    g_ui_upgrade_service.status.stage = stage;
    snprintf(g_ui_upgrade_service.status.step_text,
             sizeof(g_ui_upgrade_service.status.step_text), "%s", step_text);
    if (result_text[0] != '\0') {
        snprintf(g_ui_upgrade_service.status.result_text,
                 sizeof(g_ui_upgrade_service.status.result_text), "%s", result_text);
    }

    if (success == 1) {
        ui_upgrade_service_set_status(false, true, true, 100,
                                      UI_UPGRADE_STAGE_SUCCESS,
                                      "Upgrade complete",
                                      result_text[0] ? result_text : "System upgrade completed successfully");
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = true;
    } else if (success == 0) {
        ui_upgrade_service_set_status(false, true, false, 12,
                                      UI_UPGRADE_STAGE_FAIL,
                                      "Upgrade package verification failed",
                                      result_text[0] ? result_text : "The upgrade package is invalid. Please check the file and try again.");
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = false;
    }
}

static void ui_upgrade_service_update_child_state(void)
{
    int status = 0;
    pid_t ret;

    if (g_ui_upgrade_service.child_pid <= 0) return;

    do {
        ret = waitpid(g_ui_upgrade_service.child_pid, &status, WNOHANG);
    } while (ret < 0 && errno == EINTR);

    if (ret == 0) return;
    if (ret < 0) {
        g_ui_upgrade_service.child_pid = -1;
        if (!g_ui_upgrade_service.status.finished) {
            g_ui_upgrade_service.running = false;
            g_ui_upgrade_service.finished = true;
            g_ui_upgrade_service.success = false;
            ui_upgrade_service_set_status(false, true, false, 12,
                                          UI_UPGRADE_STAGE_FAIL,
                                          "Upgrade process monitoring failed",
                                          "Unable to determine the upgrade process result.");
        }
        return;
    }

    g_ui_upgrade_service.child_pid = -1;
    if (!g_ui_upgrade_service.status.finished) {
        ui_upgrade_service_load_status_file();
    }
    g_ui_upgrade_service.running = false;
    g_ui_upgrade_service.finished = true;

    if (g_ui_upgrade_service.status.finished) {
        return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        g_ui_upgrade_service.success = true;
        ui_upgrade_service_set_status(false, true, true, 100,
                                      UI_UPGRADE_STAGE_SUCCESS,
                                      "Upgrade complete",
                                      "The system has been updated successfully. Rebooting the device is recommended.");
    } else {
        g_ui_upgrade_service.success = false;
        ui_upgrade_service_set_status(false, true, false, 12,
                                      UI_UPGRADE_STAGE_FAIL,
                                      "Upgrade package verification failed",
                                      "The upgrade package is invalid. Please check the file and try again.");
    }
}

void ui_upgrade_service_reset(void)
{
    if (g_ui_upgrade_service.running ||
        g_ui_upgrade_service.child_pid > 0) {
        return;
    }

    memset(&g_ui_upgrade_service, 0, sizeof(g_ui_upgrade_service));
    ui_upgrade_service_hash_cache_clear(&g_ui_upgrade_pkg_hash_cache);
    g_ui_upgrade_service.child_pid = -1;
    ui_upgrade_service_set_status(false, false, false, 0,
                                  UI_UPGRADE_STAGE_NONE, "", "");
}

void ui_upgrade_service_detect(ui_upgrade_detect_info_t* info)
{
    usb_storage_status_t storage_status;

    if (info == NULL) return;

    usb_storage_refresh(&storage_status);
    info->usb_present = storage_status.device_present;
    info->usb_mounted = storage_status.mounted;
    info->package_found = info->usb_present && info->usb_mounted &&
                          ui_upgrade_service_file_exists(UI_UPGRADE_FILE_PATH);
    info->package_hash_status = UI_UPGRADE_PACKAGE_HASH_NOT_CHECKED;

    if (info->package_found) {
        info->package_hash_status =
            ui_upgrade_service_get_package_hash_status();
    } else {
        ui_upgrade_service_hash_cache_clear(&g_ui_upgrade_pkg_hash_cache);
    }
}

ui_upgrade_start_result_t ui_upgrade_service_start(void)
{
    ui_upgrade_detect_info_t detect_info;
    pid_t pid;

    if (g_ui_upgrade_service.running ||
        g_ui_upgrade_service.child_pid > 0) {
        return UI_UPGRADE_START_BUSY;
    }
    if (!ui_upgrade_service_file_exists(UI_UPGRADE_SCRIPT_PATH)) {
        return UI_UPGRADE_START_SCRIPT_NOT_FOUND;
    }

    ui_upgrade_service_detect(&detect_info);
    if (!detect_info.usb_present ||
        !detect_info.usb_mounted ||
        !detect_info.package_found ||
        (detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_MATCH &&
         detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_DIFFERENT)) {
        return UI_UPGRADE_START_PACKAGE_NOT_READY;
    }

    if (unlink(UI_UPGRADE_STATUS_FILE_PATH) != 0 && errno != ENOENT) {
        return UI_UPGRADE_START_STATUS_CLEANUP_FAILED;
    }

    pid = fork();
    if (pid < 0) return UI_UPGRADE_START_FORK_FAILED;

    if (pid == 0) {
        FILE* fp = fopen("/tmp/ui_update_child.log", "a");
        if (fp) {
            dup2(fileno(fp), STDOUT_FILENO);
            dup2(fileno(fp), STDERR_FILENO);
            fclose(fp);
        }
        execl("/bin/sh", "sh", UI_UPGRADE_SCRIPT_PATH, (char*)NULL);
        _exit(127);
    }

    g_ui_upgrade_service.running = true;
    g_ui_upgrade_service.finished = false;
    g_ui_upgrade_service.success = false;
    g_ui_upgrade_service.child_pid = pid;
    g_ui_upgrade_service.start_ms = ui_upgrade_service_now_ms();
    g_ui_upgrade_service.status_content[0] = '\0';
    g_ui_upgrade_service.status_content_len = 0;

    ui_upgrade_service_set_status(true, false, false, 0,
                                  UI_UPGRADE_STAGE_VERIFY,
                                  "Verifying upgrade package", "");
    return UI_UPGRADE_START_OK;
}

void ui_upgrade_service_poll(ui_upgrade_service_status_t* status)
{
    if (g_ui_upgrade_service.running) {
        ui_upgrade_service_set_progress_by_time();
        ui_upgrade_service_load_status_file();
    }
    ui_upgrade_service_update_child_state();

    if (status != NULL) {
        *status = g_ui_upgrade_service.status;
        if (g_ui_upgrade_service.child_pid > 0 && status->finished) {
            status->running = true;
            status->finished = false;
        }
    }
}

void ui_upgrade_service_reboot(void)
{
    sync();
    system("reboot");
}
