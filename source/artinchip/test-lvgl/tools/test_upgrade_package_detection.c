#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/storage/usb_storage.h"
#include "un260/app_service/upgrade_session.h"
#include "un260/app_service/app_command_runtime.h"

static char full_path[512], delta_path[512], legacy_path[512], running_path[512];
static char script_path[512], status_path[512], installed_path[512];
#define UI_UPGRADE_BUNDLE_FILE_PATH full_path
#define UI_UPGRADE_DELTA_FILE_PATH delta_path
#define UI_UPGRADE_LEGACY_FILE_PATH legacy_path
#define UI_UPGRADE_RUNNING_FILE_PATH running_path
#define UI_UPGRADE_SCRIPT_PATH script_path
#define UI_UPGRADE_STATUS_FILE_PATH status_path
#define UI_UPGRADE_INSTALLED_HASH_PATH installed_path
#define UI_UPGRADE_STATUS_MAX_SIZE 512U
#define UI_UPGRADE_DATA_WAIT_TIMEOUT_MS 30000U
#define UI_N_(message) (message)

typedef void lv_event_t;
enum { UPGRADE_POPUP_STATE_IDLE, UPGRADE_POPUP_STATE_PROMPT, UPGRADE_POPUP_STATE_FAIL };
enum { UI_NOTICE_INFO };
static struct { int state; void *root; bool hide_to_prompt; } g_upgrade_popup;
static bool g_upgrade_popup_detect_latched;
static usb_storage_status_t media = { true, true };
static bool fail_full_read;
static unsigned prompts, notices, forks;
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void) { return APP_UI_UPGRADE_READY; }
uint32_t app_clock_uptime_ms(void) { return 0; }

void usb_storage_refresh(usb_storage_status_t *status) { *status = media; }
static FILE *mock_fopen(const char *path, const char *mode)
{
    if (fail_full_read && !strcmp(path, full_path) && !strcmp(mode, "rb")) {
        errno = EIO;
        return NULL;
    }
    return fopen(path, mode);
}
static pid_t mock_fork(void) { ++forks; return 123; }
static void ui_notice_post_text(int kind, const char *scope, const char *title,
                                const char *description)
{
    (void)kind; (void)scope; (void)title; (void)description;
    ++notices;
}
static void upgrade_popup_show_prompt(void)
{
    ++prompts;
    g_upgrade_popup.state = UPGRADE_POPUP_STATE_PROMPT;
    g_upgrade_popup.root = &g_upgrade_popup;
}
static void upgrade_popup_hide(void)
{
    bool prompt = g_upgrade_popup.hide_to_prompt;
    memset(&g_upgrade_popup, 0, sizeof(g_upgrade_popup));
    if (prompt) upgrade_popup_show_prompt();
}
#define fopen mock_fopen
#define fork mock_fork
#include "actual_upgrade_package_functions.h"
#undef fork
#undef fopen

static void write_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(content, 1, strlen(content), file) == strlen(content));
    assert(!fclose(file));
}
static void record_installed(const char *package)
{
    uint64_t hash;
    assert(ui_upgrade_service_hash_file_fnv1a64(package, &hash));
    char content[32];
    snprintf(content, sizeof(content), "%016" PRIx64 "\n", hash);
    write_file(installed_path, content);
}
static void reset_idle(void)
{
    upgrade_session_end(upgrade_session_owner());
    g_ui_upgrade_service.running = false;
    g_ui_upgrade_service.child_pid = -1;
    ui_upgrade_service_reset();
    memset(&g_upgrade_popup, 0, sizeof(g_upgrade_popup));
    g_upgrade_popup_detect_latched = false;
    fail_full_read = false;
    media = (usb_storage_status_t){ true, true };
}
static ui_upgrade_detect_info_t detect_and_prompt(void)
{
    ui_upgrade_detect_info_t info;
    ui_upgrade_service_detect(&info);
    lv_upgrade_popup_process_detect(&info);
    return info;
}
static void fixture_path(char *path, size_t size, const char *directory, const char *name)
{
    int written = snprintf(path, size, "%s/%s", directory, name);
    assert(written > 0 && (size_t)written < size);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    fixture_path(full_path, sizeof(full_path), argv[1], "UN260_UPDATE.upk");
    fixture_path(delta_path, sizeof(delta_path), argv[1], "UN260_UPDATE_DELTA.upk");
    fixture_path(legacy_path, sizeof(legacy_path), argv[1], "test_lvgl");
    fixture_path(running_path, sizeof(running_path), argv[1], "running");
    fixture_path(script_path, sizeof(script_path), argv[1], "ui_update.sh");
    fixture_path(status_path, sizeof(status_path), argv[1], "status");
    fixture_path(installed_path, sizeof(installed_path), argv[1], "installed.fnv64");
    write_file(script_path, "fixture updater; never executed");
    write_file(running_path, "legacy ELF fixture");
    write_file(full_path, "full UPK fixture");
    write_file(delta_path, "delta UPK fixture");
    write_file(legacy_path, "legacy ELF fixture");
    record_installed(full_path);

    reset_idle();
    ui_upgrade_detect_info_t info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH);
    assert(ui_upgrade_service_can_reinstall());
    assert(!prompts && notices == 1 && !forks);
    detect_and_prompt();
    assert(!prompts && notices == 1 && !forks); /* Detection never starts an install. */
    g_upgrade_popup.root = NULL;g_upgrade_popup.state = UPGRADE_POPUP_STATE_IDLE;
    detect_and_prompt();assert(!prompts && notices == 1); /* UI rebuild retains the detection latch. */
    reset_idle();detect_and_prompt();assert(!prompts && notices == 2); /* Cold boot still does not prompt MATCH. */
    media.device_present = false;detect_and_prompt();
    media.device_present = true;detect_and_prompt();assert(!prompts && notices == 3);

    reset_idle();write_file(full_path, "changed full UPK fixture");
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT && prompts == 1);
    detect_and_prompt();assert(prompts == 1);
    g_upgrade_popup.root = NULL;g_upgrade_popup.state = UPGRADE_POPUP_STATE_IDLE;
    detect_and_prompt();assert(prompts == 1);
    media.device_present = false;detect_and_prompt();
    media.device_present = true;detect_and_prompt();assert(prompts == 2);
    reset_idle();detect_and_prompt();assert(prompts == 3); /* Uninstalled content still prompts on a new boot. */
    reset_idle();write_file(full_path, "full UPK fixture");assert(!unlink(installed_path));
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT && prompts == 4); /* Missing completion hash stays actionable. */
    reset_idle();write_file(full_path, "full UPK fixture");record_installed(full_path);
    info = detect_and_prompt();assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH && prompts == 4);
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_OK);
    assert(forks == 1 && upgrade_session_owner() == UPGRADE_SESSION_UI);
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_BUSY && forks == 1);
    reset_idle();
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH);
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_OK && forks == 2);

    reset_idle();
    g_upgrade_popup.state = UPGRADE_POPUP_STATE_FAIL;
    g_upgrade_popup.root = &g_upgrade_popup;
    unsigned before = prompts;
    upgrade_popup_on_retry_click(NULL);
    assert(prompts == before + 1 && g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT);
    detect_and_prompt();
    assert(prompts == before + 1 && g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT && g_upgrade_popup.root);
    assert(forks == 2); /* Retry returns to a clickable prompt. */
    media.device_present = false;
    detect_and_prompt();
    assert(!ui_upgrade_service_can_reinstall() && !g_upgrade_popup_detect_latched);
    assert(!g_upgrade_popup.root);
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_PACKAGE_NOT_READY && forks == 2);

    reset_idle();
    media.mounted = false;
    info = detect_and_prompt();
    assert(!info.package_found && !ui_upgrade_service_can_reinstall());
    reset_idle();
    fail_full_read = true;
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_ERROR);
    assert(!ui_upgrade_service_can_reinstall());
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_PACKAGE_NOT_READY && forks == 2);

    reset_idle();
    assert(!unlink(full_path));
    record_installed(delta_path);
    before = prompts;
    unsigned before_notices = notices;
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH);
    assert(!ui_upgrade_service_can_reinstall());
    assert(prompts == before && notices == before_notices + 1);
    upgrade_popup_on_retry_click(NULL);
    assert(prompts == before && !g_upgrade_popup.root);

    reset_idle();
    write_file(delta_path, "new delta fixture");
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT);
    assert(!ui_upgrade_service_can_reinstall() && prompts == before + 1);
    reset_idle();
    assert(!unlink(delta_path));
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH);
    assert(!g_ui_upgrade_bundle_selected && !ui_upgrade_service_can_reinstall());
    write_file(legacy_path, "new legacy ELF fixture");
    reset_idle();
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT);
    assert(!ui_upgrade_service_can_reinstall());

    reset_idle();
    assert(!unlink(legacy_path));
    info = detect_and_prompt();
    assert(!info.package_found && !ui_upgrade_service_can_reinstall());
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_PACKAGE_NOT_READY && forks == 2);
    write_file(full_path, "full UPK fixture");
    record_installed(full_path);
    info = detect_and_prompt();
    assert(info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH);
    assert(upgrade_session_begin(UPGRADE_SESSION_CONTROLLER));
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_BUSY && forks == 2);
    upgrade_session_end(UPGRADE_SESSION_CONTROLLER);
    assert(upgrade_session_begin(UPGRADE_SESSION_IMAGE));
    assert(ui_upgrade_service_start() == UI_UPGRADE_START_BUSY && forks == 2);
    upgrade_session_end(UPGRADE_SESSION_IMAGE);
    ui_upgrade_service_reset();
    assert(!ui_upgrade_service_can_reinstall());
    puts("PASS MATCH never auto-prompts on boot/rebuild/reinsert; DIFFERENT prompts once per connection/boot; explicit MATCH repeat/retry remains available; legacy/delta/media/owner guards unchanged");
    return 0;
}
