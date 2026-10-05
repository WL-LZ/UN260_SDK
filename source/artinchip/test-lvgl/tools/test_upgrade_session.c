#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "un260/app_service/upgrade_session.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_update_message.h"

/* Exact production start/reap/reset/set-status functions are injected by the
 * runner. Only filesystem, media and process boundaries are simulated. */
#define UI_UPGRADE_SCRIPT_PATH "/test-only/ui_update.sh"
#define UI_UPGRADE_STATUS_FILE_PATH "/test-only/ui_update.status"
#define UI_UPGRADE_DATA_WAIT_TIMEOUT_MS 30000U
typedef struct { bool valid; uint64_t hash; } hash_cache_t;
static struct {
    bool running, finished, success, preparing_data;
    uint32_t data_prepare_started_ms;
    pid_t child_pid;
    char status_content[512];
    size_t status_content_len;
    ui_upgrade_service_status_t status;
} g_ui_upgrade_service;
static hash_cache_t g_ui_upgrade_pkg_hash_cache;
static bool g_ui_upgrade_bundle_selected;
static bool g_ui_upgrade_full_bundle_selected;
static bool script_exists, media_ready, cleanup_ok;
static pid_t fork_result, wait_result;
static int wait_status;
static unsigned forks, waits;
static app_ui_upgrade_readiness_t readiness;
static uint32_t now_ms;
uint32_t app_clock_uptime_ms(void) { return now_ms; }
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void) { return readiness; }
static bool ui_upgrade_service_file_exists(const char *p) { (void)p; return script_exists; }
void ui_upgrade_service_detect(ui_upgrade_detect_info_t *info) {
    *info=(ui_upgrade_detect_info_t){media_ready,media_ready,media_ready,
        media_ready?UI_UPGRADE_PACKAGE_HASH_DIFFERENT:UI_UPGRADE_PACKAGE_HASH_NOT_CHECKED};
}
static void ui_upgrade_service_load_status_file(void) {}
static void ui_upgrade_service_hash_cache_clear(hash_cache_t *cache) { memset(cache,0,sizeof(*cache)); }
static int mock_unlink(const char *p) { (void)p; errno=cleanup_ok?ENOENT:EACCES; return -1; }
static pid_t mock_fork(void) { forks++; errno=EAGAIN; return fork_result; }
static pid_t mock_waitpid(pid_t p,int *status,int flags) { (void)p;(void)flags;waits++;*status=wait_status;errno=ECHILD;return wait_result; }
#define unlink mock_unlink
#define fork mock_fork
#define waitpid mock_waitpid
#include "actual_upgrade_session_functions.h"
#undef waitpid
#undef fork
#undef unlink

static void reset_fixture(void) {
    upgrade_session_end(upgrade_session_owner());
    memset(&g_ui_upgrade_service,0,sizeof(g_ui_upgrade_service));
    g_ui_upgrade_service.child_pid=-1;
    script_exists=media_ready=cleanup_ok=true;fork_result=123;wait_result=0;wait_status=0;
    readiness=APP_UI_UPGRADE_READY;
    now_ms=0;
}
static void test_restart_exit_matrix(void) {
    const int statuses[]={0,1<<8,SIGTERM};
    for(unsigned i=0;i<sizeof(statuses)/sizeof(statuses[0]);++i) {
        reset_fixture();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
        g_ui_upgrade_service.status.running=false;
        g_ui_upgrade_service.status.finished=true;
        g_ui_upgrade_service.status.stage=UI_UPGRADE_STAGE_RESTART_REQUIRED;
        g_ui_upgrade_service.status.progress=96;
        strcpy(g_ui_upgrade_service.status.result_text,"Restart to continue");
        ui_upgrade_service_status_t polled;
        ui_upgrade_service_poll(&polled);
        assert(polled.running&&!polled.finished&&!polled.success);
        assert(polled.stage==UI_UPGRADE_STAGE_FINISH&&polled.progress==96);
        assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
        wait_result=123;wait_status=statuses[i];ui_upgrade_service_poll(&polled);
        assert(polled.finished&&!polled.running&&!polled.success);
        assert(polled.stage==(i==0?UI_UPGRADE_STAGE_RESTART_REQUIRED:UI_UPGRADE_STAGE_FAIL));
        assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    }
    reset_fixture();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    g_ui_upgrade_service.status.finished=true;
    g_ui_upgrade_service.status.stage=UI_UPGRADE_STAGE_RESTART_REQUIRED;
    wait_result=-1;ui_upgrade_service_update_child_state();
    assert(g_ui_upgrade_service.status.stage==UI_UPGRADE_STAGE_FAIL);
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
}
static void test_data_gate(void) {
    ui_upgrade_service_status_t polled;
    const app_ui_upgrade_readiness_t failures[]={APP_UI_UPGRADE_BUSY,APP_UI_UPGRADE_FAILED};
    for(unsigned i=0;i<2;++i) {
        reset_fixture();readiness=failures[i];unsigned before=forks;
        assert(ui_upgrade_service_start()==(i?UI_UPGRADE_START_DATA_SAVE_FAILED:UI_UPGRADE_START_DATA_BUSY));
        assert(forks==before&&upgrade_session_owner()==UPGRADE_SESSION_NONE);
        readiness=APP_UI_UPGRADE_WAIT;
        assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
        assert(forks==before&&upgrade_session_owner()==UPGRADE_SESSION_UI);
        ui_upgrade_service_poll(&polled);assert(polled.running&&!polled.finished&&forks==before);
        ui_upgrade_service_reset();assert(g_ui_upgrade_service.preparing_data);
        assert(ui_upgrade_service_start()==UI_UPGRADE_START_BUSY);
        readiness=failures[i];ui_upgrade_service_poll(&polled);
        assert(polled.finished&&!polled.success&&forks==before);
        assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    }
    reset_fixture();readiness=APP_UI_UPGRADE_WAIT;unsigned before=forks;
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    readiness=APP_UI_UPGRADE_READY;ui_upgrade_service_poll(&polled);
    assert(forks==before+1&&g_ui_upgrade_service.child_pid==123&&polled.running);
    reset_fixture();readiness=APP_UI_UPGRADE_WAIT;
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);before=forks;
    media_ready=false;readiness=APP_UI_UPGRADE_READY;ui_upgrade_service_poll(&polled);
    assert(forks==before&&polled.finished&&!polled.success);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    reset_fixture();readiness=APP_UI_UPGRADE_WAIT;now_ms=UINT32_MAX-1000U;
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);before=forks;
    now_ms+=29999U;ui_upgrade_service_poll(&polled);
    assert(polled.running&&forks==before);
    now_ms+=1U;ui_upgrade_service_poll(&polled);
    assert(polled.finished&&!polled.success&&strstr(polled.result_text,"timed out"));
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE&&forks==before);
    readiness=APP_UI_UPGRADE_READY;assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    assert(forks==before+1);
    reset_fixture();readiness=APP_UI_UPGRADE_WAIT;
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);before=forks;
    assert(ui_upgrade_service_is_preparing_data()&&ui_upgrade_service_cancel_prepare());
    assert(!ui_upgrade_service_is_preparing_data()&&!ui_upgrade_service_cancel_prepare());
    ui_upgrade_service_poll(&polled);
    assert(!polled.running&&!polled.finished&&forks==before&&upgrade_session_owner()==UPGRADE_SESSION_NONE);
    readiness=APP_UI_UPGRADE_READY;assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    assert(!ui_upgrade_service_cancel_prepare()&&g_ui_upgrade_service.child_pid==123);
}
static void test_terminal_file_exit_matrix(void) {
    const int statuses[]={0,1<<8,SIGTERM};
    for(unsigned i=0;i<sizeof(statuses)/sizeof(statuses[0]);++i) {
        for(unsigned success=0;success<2;++success) {
            reset_fixture();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
            g_ui_upgrade_service.status.finished=true;
            g_ui_upgrade_service.status.success=success;
            g_ui_upgrade_service.status.stage=success?UI_UPGRADE_STAGE_SUCCESS:UI_UPGRADE_STAGE_FAIL;
            strcpy(g_ui_upgrade_service.status.result_text,success?"File success":"Specific package failure");
            wait_result=123;wait_status=statuses[i];
            ui_upgrade_service_update_child_state();
            assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
            assert(g_ui_upgrade_service.status.finished);
            assert(g_ui_upgrade_service.status.success==(success&&i==0));
            if(!success) assert(!strcmp(g_ui_upgrade_service.status.result_text,"Specific package failure"));
            else if(i>0) assert(strstr(g_ui_upgrade_service.status.result_text,"did not finish successfully"));
        }
    }
}
int main(void) {
    reset_fixture();
    assert(!upgrade_session_begin(UPGRADE_SESSION_NONE));
    assert(upgrade_session_begin(UPGRADE_SESSION_CONTROLLER));
    assert(!upgrade_session_begin(UPGRADE_SESSION_CONTROLLER));
    assert(!upgrade_session_begin(UPGRADE_SESSION_IMAGE));
    upgrade_session_end(UPGRADE_SESSION_IMAGE);
    assert(upgrade_session_owner()==UPGRADE_SESSION_CONTROLLER);
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_BUSY&&forks==0);
    upgrade_session_end(UPGRADE_SESSION_CONTROLLER);

    script_exists=false;assert(ui_upgrade_service_start()==UI_UPGRADE_START_SCRIPT_NOT_FOUND);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);script_exists=true;
    media_ready=false;assert(ui_upgrade_service_start()==UI_UPGRADE_START_PACKAGE_NOT_READY);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);media_ready=true;
    cleanup_ok=false;assert(ui_upgrade_service_start()==UI_UPGRADE_START_STATUS_CLEANUP_FAILED);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);cleanup_ok=true;
    fork_result=-1;assert(ui_upgrade_service_start()==UI_UPGRADE_START_FORK_FAILED);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);fork_result=123;

    assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    assert(!upgrade_session_begin(UPGRADE_SESSION_CONTROLLER));
    assert(!upgrade_session_begin(UPGRADE_SESSION_IMAGE));
    unsigned before=forks;assert(ui_upgrade_service_start()==UI_UPGRADE_START_BUSY&&forks==before);
    ui_upgrade_service_reset();assert(g_ui_upgrade_service.child_pid==123);
    ui_upgrade_service_update_child_state();assert(waits&&upgrade_session_owner()==UPGRADE_SESSION_UI);
    /* A terminal status file is not proof that the writer process has exited. */
    g_ui_upgrade_service.status.finished=true;g_ui_upgrade_service.status.success=true;
    g_ui_upgrade_service.status.stage=UI_UPGRADE_STAGE_SUCCESS;
    g_ui_upgrade_service.status.progress=100;
    ui_upgrade_service_status_t polled;
    ui_upgrade_service_poll(&polled);
    assert(polled.running&&!polled.finished&&!polled.success);
    assert(polled.progress==99&&polled.stage==UI_UPGRADE_STAGE_FINISH);
    ui_upgrade_service_update_child_state();assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    wait_result=123;ui_upgrade_service_update_child_state();
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE&&g_ui_upgrade_service.child_pid<0);
    ui_upgrade_service_reset();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    wait_result=123;ui_upgrade_service_update_child_state();
    assert(g_ui_upgrade_service.status.finished&&!g_ui_upgrade_service.status.success);
    assert(strstr(g_ui_upgrade_service.status.result_text,"did not report a completed result"));
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);

    reset_fixture();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    /* A file reporting success must not survive an unobservable writer exit. */
    g_ui_upgrade_service.status.finished=true;g_ui_upgrade_service.status.success=true;
    g_ui_upgrade_service.status.progress=100;
    wait_result=-1;ui_upgrade_service_update_child_state();
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    assert(g_ui_upgrade_service.status.finished&&!g_ui_upgrade_service.status.success);
    assert(strstr(g_ui_upgrade_service.status.result_text,"could not be confirmed"));
    ui_upgrade_service_reset();
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    assert(ui_upgrade_service_start()==UI_UPGRADE_START_BUSY);
    test_terminal_file_exit_matrix();
    test_restart_exit_matrix();
    test_data_gate();
    puts("PASS shared upgrade owner, durable-data launch gate, terminal/restart-file exit matrix, no child-exit-only success, real reap, retry and unknown-monitor hold");
    return 0;
}
