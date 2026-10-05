#define _GNU_SOURCE
#include "un260/storage/ui_factory_reset_store.h"
#include "un260/storage/storage_worker.h"
#include "un260/storage/workspace_store.h"
#include "un260/workspace/workspace_model.h"
#include "un260/lv_system/backlight_service.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool brightness_saved;
static bool fail_directory_sync, fail_journal_write, fail_journal_file_sync, fail_journal_rename;
static bool journal_temporary_fd(int fd)
{
    char link[80], path[512]; snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t count = readlink(link, path, sizeof(path) - 1);
    if (count < 0) return false;
    path[count] = 0; return strstr(path, "/factory_reset.pending.tmp") != NULL;
}
ssize_t __real_write(int fd, const void *data, size_t size);
ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fail_journal_write && journal_temporary_fd(fd)) { errno = ENOSPC; return -1; }
    return __real_write(fd, data, size);
}
int __real_renameat(int old_dir, const char *old_path, int new_dir, const char *new_path);
int __wrap_renameat(int old_dir, const char *old_path, int new_dir, const char *new_path)
{
    if (fail_journal_rename && !strcmp(old_path, "factory_reset.pending.tmp")) { errno = EIO; return -1; }
    return __real_renameat(old_dir, old_path, new_dir, new_path);
}
int __real_fsync(int fd);
int __wrap_fsync(int fd)
{
    if (fail_journal_file_sync && journal_temporary_fd(fd)) { errno = EIO; return -1; }
    struct stat info;
    if (fail_directory_sync && !fstat(fd, &info) && S_ISDIR(info.st_mode)) {
        errno = EIO; return -1;
    }
    return __real_fsync(fd);
}
bool usb_storage_prepare(void) { return false; }
static void file_write(const char *name, const char *text)
{
    char path[512]; snprintf(path, sizeof(path), "%s/%s", UI_FACTORY_RESET_DIRECTORY, name);
    FILE *file = fopen(path, "w"); assert(file); assert(fputs(text, file) >= 0); assert(!fclose(file));
}
static bool exists(const char *name)
{
    char path[512]; snprintf(path, sizeof(path), "%s/%s", UI_FACTORY_RESET_DIRECTORY, name);
    return !access(path, F_OK);
}
static void unlink_file(const char *name)
{
    char path[512]; snprintf(path, sizeof(path), "%s/%s", UI_FACTORY_RESET_DIRECTORY, name);
    assert(!unlink(path));
}
static void wait_workspace(void)
{
    for (unsigned i = 0; workspace_store_busy() && i < 10000; ++i) {
        usleep(1000); (void)workspace_store_poll();
    }
    assert(!workspace_store_busy()); assert(workspace_store_last_success());
}
static bool wait_reset(void)
{
    bool success;
    for (unsigned i = 0; i < 10000; ++i) {
        if (ui_factory_reset_store_poll(&success)) return success;
        usleep(1000);
    }
    assert(!"Reset worker did not finish"); return false;
}
static workspace_model_t *read_model(void)
{
    char path[512]; snprintf(path, sizeof(path), "%s/workspace.v1", UI_FACTORY_RESET_DIRECTORY);
    FILE *file = fopen(path, "rb"); assert(file);
    unsigned header[4]; assert(fread(header, sizeof(header), 1, file) == 1);
    workspace_model_t *model = malloc(sizeof(*model)); assert(model);
    assert(header[0] == 0x31535755U && header[1] == 3 && header[2] == sizeof(*model));
    assert(fread(model, sizeof(*model), 1, file) == 1 && fgetc(file) == EOF);
    assert(!fclose(file)); assert(workspace_model_valid(model)); return model;
}
static void preferences_seed(void)
{
    brightness_saved = false;
    const char *files[] = {"ui_state.cfg", "password.cfg", "password_visibility.cfg", "screenshot.cfg",
        "screen_recording.cfg", "performance_monitor.cfg", "performance_profile.cfg", "gestures.cfg",
        "touch_feedback.cfg", "language.cfg", "brightness.cfg", "standby.cfg", "work_mode.cfg", "touch_calibration.cfg"};
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        file_write(files[i], "old preference\n");
        char temporary[80]; snprintf(temporary, sizeof(temporary), "%s.tmp", files[i]);
        file_write(temporary, "interrupted preference\n");
    }
}
static void preferences_absent(void)
{
    const char *files[] = {"ui_state.cfg", "password.cfg", "password_visibility.cfg", "screenshot.cfg",
        "screen_recording.cfg", "performance_monitor.cfg", "performance_profile.cfg", "gestures.cfg",
        "touch_feedback.cfg", "language.cfg", "brightness.cfg", "standby.cfg", "work_mode.cfg", "touch_calibration.cfg"};
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (brightness_saved && !strcmp(files[i], "brightness.cfg")) { assert(exists(files[i])); continue; }
        assert(!exists(files[i]));
        char temporary[80]; snprintf(temporary, sizeof(temporary), "%s.tmp", files[i]);
        assert(!exists(temporary));
    }
}
static bool unrelated_job(const void *snapshot, size_t size)
{
    assert(size == sizeof(unsigned) && *(const unsigned *)snapshot == 7); return true;
}
int main(void)
{
    assert(ui_factory_reset_store_resume());
    assert(!mkdir(UI_FACTORY_RESET_DIRECTORY, 0700));
    workspace_store_init(); wait_workspace();
    workspace_model_t *model = malloc(sizeof(*model)); assert(model); workspace_defaults(model);
    unsigned id; assert(workspace_add_user(model, "Alice", &id)); model->active_id = id;
    assert(workspace_delete_user(model, 1)); assert(model->users[0].id == 2 && model->next_id == 3);
    strcpy(model->users[0].employee_id, "A-002"); strcpy(model->users[0].team, "Branch One");
    model->users[0].avatar.present = 1; model->users[0].avatar.pixels[100] = 42;
    model->users[0].quick_enabled = 0; model->users[0].qr_after_count = 1;
    model->users[0].batches[1] = 13; strcpy(model->users[0].profiles[0].name, "Special count");
    model->users[0].profiles[0].mode = 2;
    assert(workspace_store_save(model)); wait_workspace();
    file_write("cashbook.v1", "business ledger\n"); file_write("standby_1.bin", "personal photograph\n");
    file_write("standby_1.bin.source", "photo source\n"); file_write("unrelated.cfg", "unowned file\n");
    char history[512]; snprintf(history, sizeof(history), "%s/count_history", UI_FACTORY_RESET_DIRECTORY);
    assert(!mkdir(history, 0700)); file_write("count_history/index.cfg", "business history\n");
    preferences_seed();

    /* Journal write/file flush/rename failures cannot authorize mutation. */
    bool *journal_failures[] = {&fail_journal_write, &fail_journal_file_sync, &fail_journal_rename};
    for (unsigned i = 0; i < sizeof(journal_failures) / sizeof(journal_failures[0]); ++i) {
        *journal_failures[i] = true;
        assert(ui_factory_reset_store_begin()); assert(!wait_reset());
        assert(!exists("factory_reset.pending") && exists("ui_state.cfg") && exists("touch_calibration.cfg"));
        *journal_failures[i] = false;
    }
    /* Simulated power loss mid-temp write: boot never treats it as committed. */
    file_write("factory_reset.pending.tmp", "UN260_UI_");
    assert(ui_factory_reset_store_resume()); assert(exists("ui_state.cfg"));
    /* Before durable acknowledgement journal: failure cannot clear preferences. */
    fail_directory_sync = true;
    assert(ui_factory_reset_store_begin()); assert(!ui_factory_reset_store_begin());
    assert(!wait_reset()); assert(exists("ui_state.cfg") && exists("touch_calibration.cfg"));
    assert(storage_worker_pending_status() == STORAGE_JOB_SUCCEEDED);
    assert(ui_factory_reset_store_begin()); assert(!wait_reset());
    assert(exists("ui_state.cfg")); /* Existing journal still needs a durable directory checkpoint. */
    fail_directory_sync = false;
    assert(ui_factory_reset_store_begin()); assert(wait_reset()); preferences_absent();
    assert(exists("factory_reset.pending"));
    assert(exists("cashbook.v1") && exists("count_history/index.cfg") && exists("standby_1.bin") &&
           exists("standby_1.bin.source") && exists("unrelated.cfg"));
    workspace_model_t *reset = read_model();
    workspace_model_t *defaults = malloc(sizeof(*defaults)); assert(defaults); workspace_defaults(defaults);
    assert(reset->next_id == 3 && reset->active_id == 2 && reset->users[0].id == 2);
    assert(!strcmp(reset->users[0].name, "Alice") && !strcmp(reset->users[0].employee_id, "A-002") &&
           !strcmp(reset->users[0].team, "Branch One") && reset->users[0].avatar.pixels[100] == 42);
    assert(reset->users[0].quick_enabled == defaults->users[0].quick_enabled &&
           reset->users[0].qr_after_count == defaults->users[0].qr_after_count);
    assert(!memcmp(reset->users[0].profiles, defaults->users[0].profiles, sizeof(reset->users[0].profiles)));
    assert(!memcmp(reset->users[0].batches, defaults->users[0].batches, sizeof(reset->users[0].batches)));
    /* Runtime caches stay usable for Restart; startup journal defeats an old writer. */
    assert(!workspace_store_get()->users[0].quick_enabled);
    file_write("touch_calibration.cfg", "old cached calibration\n");
    /* Early boot-light runs before main journal replay and restores old PWM. */
    file_write("brightness.cfg", "80\n"); backlight_service_init();
    assert(backlight_service_level() == 8);
    assert(ui_factory_reset_store_resume()); preferences_absent();
    backlight_service_init(); assert(backlight_service_level() == 8);
    char property[512]; snprintf(property, sizeof(property), "%s/test-bl/device/of_node/default-brightness-level", UN260_BACKLIGHT_ROOT);
    assert(!unlink(property)); assert(!backlight_service_restore_factory_default());
    assert(ui_factory_reset_store_startup_pending() && exists("factory_reset.pending"));
    FILE *default_file = fopen(property, "wb"); assert(default_file);
    const unsigned char default_index[] = {0, 0, 0, 1};
    assert(fwrite(default_index, 1, 4, default_file) == 4 && !fclose(default_file));
    assert(backlight_service_restore_factory_default() && backlight_service_level() == 1);
    brightness_saved = true;
    assert(ui_factory_reset_store_startup_pending() && exists("factory_reset.pending"));
    assert(ui_factory_reset_store_finish_startup()); assert(!exists("factory_reset.pending"));
    file_write("touch_calibration.cfg", "new calibration after reboot\n");
    assert(ui_factory_reset_store_resume()); assert(exists("touch_calibration.cfg"));

    /* A partial deletion retains the journal; the next startup completes it. */
    preferences_seed(); unlink_file("screenshot.cfg");
    char blocker[512]; snprintf(blocker, sizeof(blocker), "%s/screenshot.cfg", UI_FACTORY_RESET_DIRECTORY);
    assert(!mkdir(blocker, 0700));
    assert(ui_factory_reset_store_begin()); assert(!wait_reset()); assert(exists("factory_reset.pending"));
    unsigned payload = 7; storage_job_id_t later;
    assert(storage_worker_submit(unrelated_job, &payload, sizeof(payload), &later));
    assert(storage_worker_wait(later) == STORAGE_JOB_SUCCEEDED && storage_worker_release(later));
    assert(!rmdir(blocker)); assert(ui_factory_reset_store_resume()); preferences_absent();
    assert(backlight_service_restore_factory_default()); brightness_saved = true;
    assert(ui_factory_reset_store_finish_startup());
    assert(!exists("factory_reset.pending") && exists("count_history/index.cfg"));

    /* Invalid/malicious journals never authorize deletion or follow symlinks. */
    file_write("factory_reset.pending", "invalid journal\n"); file_write("ui_state.cfg", "keep\n");
    assert(!ui_factory_reset_store_resume()); assert(exists("ui_state.cfg"));
    unlink_file("factory_reset.pending");
    char journal[512]; snprintf(journal, sizeof(journal), "%s/factory_reset.pending", UI_FACTORY_RESET_DIRECTORY);
    assert(!symlink("ui_state.cfg", journal)); assert(!ui_factory_reset_store_resume());
    assert(exists("ui_state.cfg")); unlink_file("factory_reset.pending");
    assert(storage_worker_shutdown()); free(model); free(reset); free(defaults);
    puts("PASS UI reset journal durability/failure/retry/startup replay, calibration defaults, operator ID preservation, shared FIFO, early boot-light PWM/default durability and business data preservation");
}
