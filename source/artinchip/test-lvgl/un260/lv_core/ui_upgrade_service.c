#include "un260/lv_system/ui_i18n.h"
#include "ui_upgrade_service.h"
#include "un260/lv_system/ui_update_message.h"
#include "un260/lv_system/ui_lang.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#include "un260/storage/usb_storage.h"
#include "un260/app_service/upgrade_session.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_system/app_clock.h"

#define UI_UPGRADE_BUNDLE_FILE_PATH    USB_STORAGE_MOUNT_POINT "/update/UN260_UPDATE.upk"
#define UI_UPGRADE_SCRIPT_PATH         "/usr/local/bin/un260_update_dispatch"
#define UI_UPGRADE_STATUS_FILE_PATH    "/tmp/ui_update.status"
#define UI_UPGRADE_INSTALLED_HASH_PATH "/var/lib/un260-updater/installed.fnv64"
#define UI_UPGRADE_STATUS_MAX_SIZE     512U
#define UI_UPGRADE_DATA_WAIT_TIMEOUT_MS 30000U

typedef struct {
    bool valid;
    dev_t device;
    ino_t inode;
    off_t size;
    time_t mtime;
    time_t ctime;
    long mtime_nsec;
    long ctime_nsec;
    uint64_t hash;
} ui_upgrade_hash_cache_t;

typedef struct {
    bool running;
    bool finished;
    bool success;
    bool preparing_data;
    uint32_t data_prepare_started_ms;
    pid_t child_pid;
    uint64_t selected_hash;
    char child_log_path[96];
    char status_content[UI_UPGRADE_STATUS_MAX_SIZE];
    size_t status_content_len;
    ui_upgrade_service_status_t status;
} ui_upgrade_service_ctx_t;

static ui_upgrade_service_ctx_t g_ui_upgrade_service;
static ui_upgrade_hash_cache_t g_ui_upgrade_pkg_hash_cache;
static bool g_ui_upgrade_full_bundle_selected = false;

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
        cache->ctime == before.st_ctime &&
        cache->mtime_nsec == before.st_mtim.tv_nsec &&
        cache->ctime_nsec == before.st_ctim.tv_nsec) {
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
        before.st_ctime != after.st_ctime ||
        before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
        ui_upgrade_service_hash_cache_clear(cache);
        return false;
    }

    cache->valid = true;
    cache->device = after.st_dev;
    cache->inode = after.st_ino;
    cache->size = after.st_size;
    cache->mtime = after.st_mtime;
    cache->ctime = after.st_ctime;
    cache->mtime_nsec = after.st_mtim.tv_nsec;
    cache->ctime_nsec = after.st_ctim.tv_nsec;
    cache->hash = hash;
    *hash_out = hash;
    return true;
}

static bool ui_upgrade_service_parse_hash(const char *text, uint64_t *hash_out)
{
    uint64_t value = 0;
    if (!text || !hash_out || strlen(text) != 16U) return false;
    for (unsigned i = 0; i < 16U; ++i) {
        unsigned digit;
        if (text[i] >= '0' && text[i] <= '9') digit = (unsigned)(text[i] - '0');
        else if (text[i] >= 'a' && text[i] <= 'f') digit = (unsigned)(text[i] - 'a') + 10U;
        else return false;
        value = (value << 4) | digit;
    }
    *hash_out = value;
    return true;
}

static bool ui_upgrade_service_read_installed_bundle_hash(uint64_t* hash_out)
{
    FILE* fp;
    char text[19];
    size_t size;

    if (hash_out == NULL) return false;

    fp = fopen(UI_UPGRADE_INSTALLED_HASH_PATH, "r");
    if (fp == NULL) return false;
    size = fread(text, 1, sizeof(text) - 1U, fp);
    bool valid = !ferror(fp) && feof(fp) && size == 17U && text[16] == '\n';
    fclose(fp);
    if (!valid) return false;
    text[16] = '\0';
    return ui_upgrade_service_parse_hash(text, hash_out);
}

static ui_upgrade_package_hash_status_t
ui_upgrade_service_get_package_hash_status(const char* package_path)
{
    uint64_t package_hash = 0;
    uint64_t installed_hash = 0;

    if (!ui_upgrade_service_hash_file_cached(package_path,
                                             &g_ui_upgrade_pkg_hash_cache,
                                             &package_hash)) {
        return UI_UPGRADE_PACKAGE_HASH_ERROR;
    }

    if (!ui_upgrade_service_read_installed_bundle_hash(&installed_hash)) {
        return UI_UPGRADE_PACKAGE_HASH_DIFFERENT;
    }
    return package_hash == installed_hash ?
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
    if (strcmp(stage_name, "prepare") == 0) return UI_UPGRADE_STAGE_PREPARE;
    if (strcmp(stage_name, "verify") == 0) return UI_UPGRADE_STAGE_VERIFY;
    if (strcmp(stage_name, "extract") == 0) return UI_UPGRADE_STAGE_EXTRACT;
    if (strcmp(stage_name, "preflight") == 0) return UI_UPGRADE_STAGE_PREFLIGHT;
    if (strcmp(stage_name, "install") == 0 ||
        strcmp(stage_name, "write") == 0) return UI_UPGRADE_STAGE_INSTALL;
    if (strcmp(stage_name, "sync") == 0) return UI_UPGRADE_STAGE_SYNC;
    if (strcmp(stage_name, "finish") == 0) return UI_UPGRADE_STAGE_FINISH;
    if (strcmp(stage_name, "success") == 0) return UI_UPGRADE_STAGE_SUCCESS;
    if (strcmp(stage_name, "fail") == 0) return UI_UPGRADE_STAGE_FAIL;
    if (strcmp(stage_name, "restart") == 0) return UI_UPGRADE_STAGE_RESTART_REQUIRED;
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

static void ui_upgrade_service_load_status_file(void)
{
    FILE* fp;
    char content[UI_UPGRADE_STATUS_MAX_SIZE];
    char parse_content[UI_UPGRADE_STATUS_MAX_SIZE];
    char* line;
    char* save_ptr = NULL;
    char step_text[64] = {0};
    char result_text[UI_UPGRADE_RESULT_TEXT_CAPACITY] = {0};
    char stage_name[32] = {0};
    ui_upgrade_stage_t stage;
    size_t content_len;
    bool has_progress = false;
    bool has_stage = false;
    bool has_step = false;
    bool has_writer = false;
    bool has_hash = false;
    int writer = -1;
    uint64_t selected_hash = 0;
    int progress = -1;
    int success = -1;

    if (g_ui_upgrade_service.child_pid <= 0) return;

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
            if (has_progress || !ui_upgrade_service_parse_int(line + 9, 0, 100, &progress)) {
                return;
            }
            has_progress = true;
        } else if (strncmp(line, "stage=", 6) == 0) {
            if (has_stage || strlen(line + 6) >= sizeof(stage_name)) {
                return;
            }
            snprintf(stage_name, sizeof(stage_name), "%s", line + 6);
            has_stage = true;
        } else if (strncmp(line, "step=", 5) == 0) {
            if (has_step) return;
            snprintf(step_text, sizeof(step_text), "%s", line + 5);
            has_step = true;
        } else if (strncmp(line, "message=", 8) == 0) {
            snprintf(result_text, sizeof(result_text), "%s", line + 8);
        } else if (strncmp(line, "success=", 8) == 0) {
            /* The updater emits an empty result until it reaches a terminal
             * stage. Accept that pending state without discarding progress. */
            if (line[8] != '\0' &&
                !ui_upgrade_service_parse_int(line + 8, -1, 1, &success)) {
                return;
            }
        } else if (strncmp(line, "writer_pid=", 11) == 0) {
            if (has_writer || !ui_upgrade_service_parse_int(line + 11, 1, INT_MAX, &writer)) return;
            has_writer = true;
        } else if (strncmp(line, "bundle_fnv64=", 13) == 0) {
            if (has_hash || !ui_upgrade_service_parse_hash(line + 13, &selected_hash)) return;
            has_hash = true;
        }
        line = strtok_r(NULL, "\n", &save_ptr);
    }

    if (!has_progress || !has_stage || !has_step || !has_writer ||
        writer != g_ui_upgrade_service.child_pid) {
        return;
    }
    stage = ui_upgrade_service_stage_from_name(stage_name);
    /* Before the dispatcher fingerprints USB it may report only its own
     * failure. Progress/completion always belongs to the selected package. */
    if ((has_hash && selected_hash != g_ui_upgrade_service.selected_hash) ||
        (!has_hash && !(stage == UI_UPGRADE_STAGE_FAIL && success == 0))) return;
    if (stage == UI_UPGRADE_STAGE_NONE ||
        (success == 1 && stage != UI_UPGRADE_STAGE_SUCCESS) ||
        (success == 0 && stage != UI_UPGRADE_STAGE_FAIL) ||
        (success < 0 && (stage == UI_UPGRADE_STAGE_SUCCESS ||
                         stage == UI_UPGRADE_STAGE_FAIL))) {
        return;
    }

    memcpy(g_ui_upgrade_service.status_content, content, content_len + 1U);
    g_ui_upgrade_service.status_content_len = content_len;

    if (stage == UI_UPGRADE_STAGE_FAIL ||
        progress >= g_ui_upgrade_service.status.progress) {
        g_ui_upgrade_service.status.progress = progress;
    }
    g_ui_upgrade_service.status.stage = stage;
    snprintf(g_ui_upgrade_service.status.step_text,
             sizeof(g_ui_upgrade_service.status.step_text), "%s", step_text);
    if (result_text[0] != '\0') {
        snprintf(g_ui_upgrade_service.status.result_text,
                 sizeof(g_ui_upgrade_service.status.result_text), "%s", result_text);
    }

    if (stage == UI_UPGRADE_STAGE_RESTART_REQUIRED) {
        ui_upgrade_service_set_status(false, true, false, progress < 100 ? progress : 99,
                                      UI_UPGRADE_STAGE_RESTART_REQUIRED,
                                      UI_N_("Restart to continue"),
                                      UI_N_("Keep the USB drive connected and restart to finish the update."));
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = false;
    } else if (success == 1) {
        ui_upgrade_service_set_status(false, true, true, 100,
                                      UI_UPGRADE_STAGE_SUCCESS,
                                      UI_N_("Upgrade complete"),
                                      result_text[0] ? result_text : UI_N_("System upgrade completed successfully"));
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = true;
    } else if (success == 0) {
        ui_upgrade_service_set_status(false, true, false, progress,
                                      UI_UPGRADE_STAGE_FAIL,
                                      step_text,
                                      result_text[0] ? result_text : UI_N_("The upgrade package is invalid. Please check the file and try again."));
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = false;
    }
}

static bool ui_upgrade_service_child_busy(void)
{
    FILE *fp = fopen(g_ui_upgrade_service.child_log_path, "r");
    char line[192];
    if (!fp) return false;
    bool busy = false;
    size_t total = 0;
    while (total < 4096U && fgets(line, sizeof(line), fp)) {
        total += strlen(line);
        if (!strcmp(line, "Updater already running or stale lock\n") ||
            !strcmp(line, "Updater already running or unsafe lock record\n")) { busy = true; break; }
    }
    fclose(fp);
    return busy;
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
        /* Even a success status file cannot prove the writer has exited.
         * Replace it with the unknown observation; retain the shared lease. */
        g_ui_upgrade_service.running = false;
        g_ui_upgrade_service.finished = true;
        g_ui_upgrade_service.success = false;
        ui_upgrade_service_set_status(false, true, false,
                                      g_ui_upgrade_service.status.progress,
                                      UI_UPGRADE_STAGE_FAIL,
                                      UI_N_("Update result unknown"),
                                      UI_N_("The updater process result could not be confirmed. Keep power connected."));
        return;
    }

    /* A status file alone can precede the updater process exit. Keep the
     * shared lease until waitpid proves that the writer has stopped. */
    upgrade_session_end(UPGRADE_SESSION_UI);
    ui_upgrade_service_load_status_file();
    g_ui_upgrade_service.child_pid = -1;
    g_ui_upgrade_service.running = false;
    g_ui_upgrade_service.finished = true;

    bool exited_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (g_ui_upgrade_service.status.finished) {
        /* Never let an early success file override a failed/signalled writer.
         * An explicit failure file remains authoritative even on exit code 0. */
        if ((g_ui_upgrade_service.status.success ||
             g_ui_upgrade_service.status.stage == UI_UPGRADE_STAGE_RESTART_REQUIRED) &&
            !exited_ok) {
            g_ui_upgrade_service.success = false;
            ui_upgrade_service_set_status(false, true, false,
                                          g_ui_upgrade_service.status.progress,
                                          UI_UPGRADE_STAGE_FAIL,
                                          UI_N_("Updater exited with an error"),
                                          UI_N_("The updater did not finish successfully. Check the update result before retrying."));
        }
        return;
    }

    if (exited_ok) {
        g_ui_upgrade_service.success = false;
        ui_upgrade_service_set_status(false, true, false,
                                      g_ui_upgrade_service.status.progress,
                                      UI_UPGRADE_STAGE_FAIL,
                                      UI_N_("Update result unknown"),
                                      UI_N_("The updater did not report a completed result. Check the update result before retrying."));
    } else {
        g_ui_upgrade_service.success = false;
        bool busy = (WIFEXITED(status) && WEXITSTATUS(status) == 75) || ui_upgrade_service_child_busy();
        ui_upgrade_service_set_status(false, true, false, 12,
                                      UI_UPGRADE_STAGE_FAIL,
                                      UI_N_("Updater exited with an error"),
                                      busy ? UI_N_("Wait for the update to finish.") :
                                      UI_N_("The updater did not finish successfully. Check the update result before retrying."));
    }
}

void ui_upgrade_service_reset(void)
{
    if (g_ui_upgrade_service.running ||
        g_ui_upgrade_service.child_pid > 0) {
        return;
    }

    if (g_ui_upgrade_service.child_log_path[0]) unlink(g_ui_upgrade_service.child_log_path);
    memset(&g_ui_upgrade_service, 0, sizeof(g_ui_upgrade_service));
    ui_upgrade_service_hash_cache_clear(&g_ui_upgrade_pkg_hash_cache);
    g_ui_upgrade_full_bundle_selected = false;
    g_ui_upgrade_service.child_pid = -1;
    ui_upgrade_service_set_status(false, false, false, 0,
                                  UI_UPGRADE_STAGE_NONE, "", "");
}

void ui_upgrade_service_detect(ui_upgrade_detect_info_t* info)
{
    usb_storage_status_t storage_status;
    bool bundle_found;

    if (info == NULL) return;

    usb_storage_refresh(&storage_status);
    info->usb_present = storage_status.device_present;
    info->usb_mounted = storage_status.mounted;
    bundle_found = info->usb_present && info->usb_mounted &&
                   ui_upgrade_service_file_exists(UI_UPGRADE_BUNDLE_FILE_PATH);
    info->package_found = bundle_found;
    info->package_hash_status = UI_UPGRADE_PACKAGE_HASH_NOT_CHECKED;
    g_ui_upgrade_full_bundle_selected = bundle_found;

    if (info->package_found) {
        info->package_hash_status =
            ui_upgrade_service_get_package_hash_status(UI_UPGRADE_BUNDLE_FILE_PATH);
    } else {
        ui_upgrade_service_hash_cache_clear(&g_ui_upgrade_pkg_hash_cache);
    }
}

bool ui_upgrade_service_can_reinstall(void)
{
    return g_ui_upgrade_full_bundle_selected && g_ui_upgrade_pkg_hash_cache.valid;
}

static ui_upgrade_start_result_t ui_upgrade_service_launch(void)
{
    ui_upgrade_detect_info_t detect_info;
    pid_t pid;

    if (!ui_upgrade_service_file_exists(UI_UPGRADE_SCRIPT_PATH)) {
        return UI_UPGRADE_START_SCRIPT_NOT_FOUND;
    }

    /* USB timestamps may be coarse. The start boundary always fingerprints
     * the current bytes; the dispatcher rechecks them before extraction. */
    ui_upgrade_service_hash_cache_clear(&g_ui_upgrade_pkg_hash_cache);
    ui_upgrade_service_detect(&detect_info);
    if (!detect_info.usb_present ||
        !detect_info.usb_mounted ||
        !detect_info.package_found ||
        (detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_MATCH &&
         detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_DIFFERENT)) {
        return UI_UPGRADE_START_PACKAGE_NOT_READY;
    }

    pid = fork();
    if (pid < 0) {
        return UI_UPGRADE_START_FORK_FAILED;
    }

    if (pid == 0) {
        char bundle_hash[17], child_log[96];
        snprintf(child_log, sizeof(child_log), "/tmp/un260-update-ui.%ld.log", (long)getpid());
        int log_fd = open(child_log, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (log_fd < 0) _exit(126);
        if (dup2(log_fd, STDOUT_FILENO) < 0 || dup2(log_fd, STDERR_FILENO) < 0) _exit(126);
        close(log_fd);
        snprintf(bundle_hash, sizeof(bundle_hash), "%016" PRIx64,
                 g_ui_upgrade_pkg_hash_cache.hash);
        execl("/bin/sh", "sh", UI_UPGRADE_SCRIPT_PATH,
              "--bundle-fnv", bundle_hash, (char*)NULL);
        _exit(127);
    }

    g_ui_upgrade_service.running = true;
    g_ui_upgrade_service.finished = false;
    g_ui_upgrade_service.success = false;
    g_ui_upgrade_service.child_pid = pid;
    g_ui_upgrade_service.selected_hash = g_ui_upgrade_pkg_hash_cache.hash;
    if (g_ui_upgrade_service.child_log_path[0]) unlink(g_ui_upgrade_service.child_log_path);
    snprintf(g_ui_upgrade_service.child_log_path, sizeof(g_ui_upgrade_service.child_log_path),
             "/tmp/un260-update-ui.%ld.log", (long)pid);
    g_ui_upgrade_service.status_content[0] = '\0';
    g_ui_upgrade_service.status_content_len = 0;

    ui_upgrade_service_set_status(true, false, false, 0,
                                  UI_UPGRADE_STAGE_PREPARE,
                                  "prepare", "");
    return UI_UPGRADE_START_OK;
}

ui_upgrade_start_result_t ui_upgrade_service_start(void)
{
    ui_upgrade_detect_info_t detect_info;
    app_ui_upgrade_readiness_t readiness;
    ui_upgrade_start_result_t result;

    if (g_ui_upgrade_service.running ||
        g_ui_upgrade_service.child_pid > 0 ||
        upgrade_session_owner() != UPGRADE_SESSION_NONE) {
        return UI_UPGRADE_START_BUSY;
    }
    if (!ui_upgrade_service_file_exists(UI_UPGRADE_SCRIPT_PATH)) {
        return UI_UPGRADE_START_SCRIPT_NOT_FOUND;
    }
    ui_upgrade_service_detect(&detect_info);
    if (!detect_info.usb_present || !detect_info.usb_mounted ||
        !detect_info.package_found ||
        (detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_MATCH &&
         detect_info.package_hash_status != UI_UPGRADE_PACKAGE_HASH_DIFFERENT)) {
        return UI_UPGRADE_START_PACKAGE_NOT_READY;
    }
    if (!upgrade_session_begin(UPGRADE_SESSION_UI)) return UI_UPGRADE_START_BUSY;

    readiness = app_command_runtime_prepare_ui_upgrade();
    if (readiness == APP_UI_UPGRADE_WAIT) {
        g_ui_upgrade_service.running = true;
        g_ui_upgrade_service.finished = false;
        g_ui_upgrade_service.success = false;
        g_ui_upgrade_service.preparing_data = true;
        g_ui_upgrade_service.data_prepare_started_ms = app_clock_uptime_ms();
        g_ui_upgrade_service.child_pid = -1;
        ui_upgrade_service_set_status(true, false, false, 0,
                                      UI_UPGRADE_STAGE_PREPARE,
                                      UI_N_("Saving records"), "");
        return UI_UPGRADE_START_OK;
    }
    if (readiness != APP_UI_UPGRADE_READY) {
        upgrade_session_end(UPGRADE_SESSION_UI);
        return readiness == APP_UI_UPGRADE_BUSY ? UI_UPGRADE_START_DATA_BUSY :
                                                 UI_UPGRADE_START_DATA_SAVE_FAILED;
    }
    result = ui_upgrade_service_launch();
    if (result != UI_UPGRADE_START_OK) upgrade_session_end(UPGRADE_SESSION_UI);
    return result;
}

bool ui_upgrade_service_is_preparing_data(void)
{
    return g_ui_upgrade_service.preparing_data;
}

bool ui_upgrade_service_cancel_prepare(void)
{
    if (!g_ui_upgrade_service.preparing_data) return false;
    g_ui_upgrade_service.preparing_data = false;
    g_ui_upgrade_service.running = false;
    g_ui_upgrade_service.finished = false;
    g_ui_upgrade_service.success = false;
    upgrade_session_end(UPGRADE_SESSION_UI);
    ui_upgrade_service_set_status(false, false, false, 0, UI_UPGRADE_STAGE_NONE, "", "");
    return true;
}

static void ui_upgrade_service_prepare_data_poll(void)
{
    app_ui_upgrade_readiness_t readiness;
    ui_upgrade_start_result_t result;
    const char *message;

    if (!g_ui_upgrade_service.preparing_data) return;
    readiness = app_command_runtime_prepare_ui_upgrade();
    if (readiness == APP_UI_UPGRADE_WAIT) {
        if ((uint32_t)(app_clock_uptime_ms() - g_ui_upgrade_service.data_prepare_started_ms) <
            UI_UPGRADE_DATA_WAIT_TIMEOUT_MS) return;
        message = UI_N_("Saving records timed out. Please try again.");
    } else message = NULL;
    g_ui_upgrade_service.preparing_data = false;
    if (readiness == APP_UI_UPGRADE_READY) {
        result = ui_upgrade_service_launch();
        if (result == UI_UPGRADE_START_OK) return;
        message = UI_N_("The upgrade could not be started. Please check the USB drive and try again.");
    } else if (message == NULL) {
        message = readiness == APP_UI_UPGRADE_BUSY ?
                  UI_N_("Counting is in progress. Stop counting before updating.") :
                  UI_N_("Records could not be saved. Please try again.");
    }
    upgrade_session_end(UPGRADE_SESSION_UI);
    g_ui_upgrade_service.running = false;
    g_ui_upgrade_service.finished = true;
    g_ui_upgrade_service.success = false;
    ui_upgrade_service_set_status(false, true, false, 0,
                                  UI_UPGRADE_STAGE_FAIL, "", message);
}

void ui_upgrade_service_poll(ui_upgrade_service_status_t* status)
{
    ui_upgrade_service_prepare_data_poll();
    if (g_ui_upgrade_service.child_pid > 0) {
        ui_upgrade_service_load_status_file();
    }
    ui_upgrade_service_update_child_state();

    if (status != NULL) {
        *status = g_ui_upgrade_service.status;
        language_t language=ui_lang_get();
        ui_update_message_render(language,g_ui_upgrade_service.status.step_text,status->step_text,sizeof(status->step_text));
        ui_update_message_render(language,g_ui_upgrade_service.status.result_text,status->result_text,sizeof(status->result_text));
        if (g_ui_upgrade_service.child_pid > 0 && status->finished) {
            status->running = true;
            status->finished = false;
            status->success = false;
            if (status->stage == UI_UPGRADE_STAGE_SUCCESS ||
                status->stage == UI_UPGRADE_STAGE_RESTART_REQUIRED) {
                status->stage = UI_UPGRADE_STAGE_FINISH;
                if (status->progress == 100) status->progress = 99;
                snprintf(status->step_text, sizeof(status->step_text), "%s",ui_tr("Finalizing update"));
                snprintf(status->result_text, sizeof(status->result_text),
                         "%s",ui_tr("Keep the USB drive and power connected until the updater exits."));
            }
        }
    }
}

void ui_upgrade_service_reboot(void)
{
    sync();
    system("reboot");
}
