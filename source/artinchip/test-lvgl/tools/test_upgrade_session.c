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
#include "un260/lv_core/ui_upgrade_service.h"

/* Exact production start/reap/reset/set-status functions are injected by the
 * runner. Only filesystem, media and process boundaries are simulated. */
#define UI_UPGRADE_SCRIPT_PATH "/test-only/ui_update.sh"
#define UI_UPGRADE_STATUS_FILE_PATH "/test-only/ui_update.status"
typedef struct { bool valid; uint64_t hash; } hash_cache_t;
static struct {
    bool running, finished, success;
    pid_t child_pid;
    char status_content[512];
    size_t status_content_len;
    ui_upgrade_service_status_t status;
} g_ui_upgrade_service;
static hash_cache_t g_ui_upgrade_pkg_hash_cache;
static bool g_ui_upgrade_bundle_selected;
static bool script_exists, media_ready, cleanup_ok;
static pid_t fork_result, wait_result;
static int wait_status;
static unsigned forks, waits;
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
    ui_upgrade_service_update_child_state();assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    wait_result=123;ui_upgrade_service_update_child_state();
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE&&g_ui_upgrade_service.child_pid<0);
    ui_upgrade_service_reset();assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    wait_result=123;ui_upgrade_service_update_child_state();
    assert(g_ui_upgrade_service.status.finished&&g_ui_upgrade_service.status.success);
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
    puts("PASS shared upgrade owner, start/preflight/fork failures, terminal-file/exit-code/signal matrix, real reap, retry and unknown-monitor hold");
    return 0;
}
