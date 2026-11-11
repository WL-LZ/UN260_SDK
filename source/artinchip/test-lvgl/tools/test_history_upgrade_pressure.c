#define main history_fixture_main
#include "test_history_storage.c"
#undef main
#include <dirent.h>
#include <sys/file.h>
#ifndef HISTORY_INDEX_BACKUP
#define HISTORY_INDEX_BACKUP ".committed-index-v1.cfg"
#endif
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_core/ui_upgrade_service.h"

static counting_session_state_t g_counting_session;
static counting_sim_t final_batch;
static bool g_deferred_frame_valid;
static bool ui_factory_reset_blocks_navigation(void){return false;}
static bool touch_calibration_busy(void){return false;}
bool app_command_runtime_count_start_busy(void){return false;}
static bool counting_action_clear_pending(void){return false;}
static bool app_add_runtime_busy(void){return false;}
static bool protocol_frame_queue_has_pending(void){return false;}
bool counting_multi_query_busy(void){return false;}
static bool counting_multi_extra_busy(void){return false;}
bool workspace_store_busy(void){return false;}
static bool standby_store_busy(void){return false;}
static bool standby_store_storage_ready(void){return true;}
static counting_sim_t *counting_data_mutable(void){return &final_batch;}
extern uint32_t app_clock_uptime_ms(void);
#include "un260/app_service/upgrade_session.h"
#ifndef HISTORY_INDEX_APP_RESERVE_KB
/* Previous deployed history code has no temporary borrowing operation. */
bool ui_history_data_return_space(uint32_t now,bool requested){(void)now;(void)requested;return true;}
#endif
#include "actual_data_gate.h"
#define ui_history_export_data_poll() ((void)0)
#define ui_manager_get_current_page() 0
#define UI_PAGE_HISTORY 1
#define ui_page_19_history_refresh() ((void)0)
#define app_counting_runtime_report_history_commit(s,r,p) ((void)(s),(void)(r),(void)(p))
#include "actual_history_poll.h"
void pressure_ui_reset(void);
void pressure_ui_tick(void);
unsigned pressure_ui_launches(void);

static void test_one_click_while_returning(bool click_first)
{
    const char *start=getenv("UN260_RETURN_START"), *ready=getenv("UN260_RETURN_READY");
    const char *release=getenv("UN260_RETURN_RELEASE");
    puts("HISTORY AVAILABLE WHILE BUSY");fflush(stdout);
    for(unsigned i=0;access(start,F_OK) && i<10000;i++) usleep(1000);
    assert(access(start,F_OK)==0);
    pressure_ui_reset();
    if (click_first) {
        assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    } else {
        app_counting_runtime_poll_history(NULL,NULL,3000);
    }
    for(unsigned i=0;access(ready,F_OK) && i<10000;i++) usleep(1000);
    assert(access(ready,F_OK)==0);
    int contender=open(HISTORY_UPDATER_LOCK_DIR "/un260-updater.flock",O_RDWR);
    assert(contender>=0 && flock(contender,LOCK_EX|LOCK_NB)<0 && errno==EWOULDBLOCK);
    if (!click_first) assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
    assert(ui_upgrade_service_is_preparing_data() && !pressure_ui_launches());
    int marker=open(release,O_CREAT|O_EXCL|O_WRONLY,0600);
    assert(marker>=0 && !close(marker));
    ui_upgrade_service_status_t status={0};
    for(unsigned i=0;!pressure_ui_launches() && i<10000;i++) {
        pressure_ui_tick();app_counting_runtime_poll_history(NULL,NULL,3001+i);
        ui_upgrade_service_poll(&status);usleep(1000);
    }
    assert(pressure_ui_launches()==1 && upgrade_session_owner()==UPGRADE_SESSION_UI);
    assert(!flock(contender,LOCK_EX|LOCK_NB));close(contender);
    struct stat st;assert(!lstat(UI_HISTORY_INDEX_PATH,&st) && S_ISREG(st.st_mode));
    assert(storage_worker_pending_status()==STORAGE_JOB_SUCCEEDED);
    assert(storage_worker_shutdown());
}

int main(int argc,char **argv)
{
    assert(argc==2);
    ui_thread=pthread_self();
    ui_history_data_init();
    const ui_history_store_t *state=ui_history_data_get();
    if (!strcmp(argv[1],"unreadable")) {
        assert(!ui_history_data_is_available());
        puts("Unreadable foreign index protected");return 0;
    }
    assert(ui_history_data_is_available() && state->record_count==1000);
    unsigned last_id=state->next_record_no,prior_total=state->total_notes_counted;
    if (!strcmp(argv[1],"max-text")) {
        ui_history_store_t *wide=malloc(sizeof(*wide));assert(wide);
        memcpy(wide,state,sizeof(*wide));
        for(unsigned i=0;i<state->record_count;i++) {
            ui_history_record_t *rec=&wide->records[i];
            *rec=state->records[state->record_count-1-i];
            fill_escaped_text(rec->denom_text,sizeof(rec->denom_text));
            fill_escaped_text(rec->sn_text,sizeof(rec->sn_text));
            fill_escaped_text(rec->sn_detail_text,sizeof(rec->sn_detail_text));
            fill_escaped_text(rec->error_frame_text,sizeof(rec->error_frame_text));
            fill_escaped_text(rec->start_frame_text,sizeof(rec->start_frame_text));
            fill_escaped_text(rec->end_frame_text,sizeof(rec->end_frame_text));
            fill_escaped_text(rec->session_log,sizeof(rec->session_log));
        }
        storage_job_id_t seed;
        assert(storage_worker_submit(history_save_snapshot,wide,history_snapshot_size(wide),&seed));
        free(wide);
        assert(storage_worker_wait(seed)==STORAGE_JOB_SUCCEEDED);
        assert(storage_worker_release(seed) && storage_worker_shutdown());
        return 0;
    }
    if (!strcmp(argv[1],"click-before-return") || !strcmp(argv[1],"click-during-return")) {
        test_one_click_while_returning(!strcmp(argv[1],"click-before-return"));
        return 0;
    }
    if (!strcmp(argv[1],"read-wait")) {
        struct stat st;assert(!lstat(UI_HISTORY_INDEX_PATH,&st) && S_ISLNK(st.st_mode));
        puts("HISTORY AVAILABLE WHILE BUSY");fflush(stdout);
        for(unsigned i=0;i<10000;i++) {
            app_counting_runtime_poll_history(NULL,NULL,i*20);
            if (!lstat(UI_HISTORY_INDEX_PATH,&st) && S_ISREG(st.st_mode) &&
                storage_worker_pending_status()==STORAGE_JOB_SUCCEEDED) break;
            usleep(1000);
        }
        assert(S_ISREG(st.st_mode) && ui_history_data_is_available());
        assert(storage_worker_shutdown());return 0;
    }
    if (!strcmp(argv[1],"read")) {
        printf("DURABLE next_id=%u total=%u newest=%u\n",last_id,prior_total,state->records[0].record_no);
        return 0;
    }
    counting_history_session_start(NULL,0);
    g_counting_session.history_record.valid=true;
    g_counting_session.history_record.end_seen=true;
    g_counting_session.history_record.pcs=1;
    g_counting_session.history_record.amount=5;
    final_batch.denom[0].value=5; final_batch.denom[0].pcs=1; final_batch.denom_number=1;
    assert(counting_data_ensure_serial_capacity(&final_batch,1));
    final_batch.sn_str[0]=strdup("FINAL0000001"); final_batch.denom_mix[0]=5;
    if (!strcmp(argv[1],"eio-retry")) atomic_store(&fault,FAIL_FILE_SYNC);
    int held=-1;
    bool lock_retry=!strcmp(argv[1],"lock-retry");
    if (lock_retry) {
        held=open(HISTORY_UPDATER_LOCK_DIR "/un260-updater.flock",O_RDWR|O_CREAT,0600);
        assert(held>=0 && flock(held,LOCK_EX|LOCK_NB)==0);
    }
    hold_worker(true);
    pressure_ui_reset();
    ui_upgrade_start_result_t started=ui_upgrade_service_start();
    if (!strcmp(argv[1],"blocked") && started==UI_UPGRADE_START_DATA_SAVE_FAILED) {
        /* Invalid ABC can fail before the worker reaches the index-write
         * barrier. Immediate rejection and deferred failure both preserve
         * the old durable index and must launch no installer. */
        hold_worker(false);
        assert(!pressure_ui_launches() && ui_history_data_status()==STORAGE_JOB_FAILED);
        puts("REPRODUCED immediate save rejection, launches=0");return 0;
    }
    assert(started==UI_UPGRADE_START_OK);
    assert(ui_upgrade_service_is_preparing_data() && pressure_ui_launches()==0);
    hold_worker(false);
    ui_upgrade_service_status_t status={0};
    for(unsigned i=0;i<10000;i++) {
        pressure_ui_tick();
        ui_history_data_poll(app_clock_uptime_ms());
        ui_upgrade_service_poll(&status);
        if (pressure_ui_launches() || status.finished) break;
        usleep(1000);
    }
    if (lock_retry || !strcmp(argv[1],"eio-retry")) {
        if (lock_retry) { assert(held>=0 && !close(held)); }
        assert(!pressure_ui_launches() && status.finished && !status.success);
        char attempted[512]={0},name[512];
        char prefix[32];
        snprintf(prefix,sizeof(prefix),"detail-%u-",last_id);
        DIR *entries=opendir(UI_HISTORY_STORE_DIR);assert(entries);
        struct dirent *entry;name[0]=0;
        while ((entry=readdir(entries))) {
            if (!strncmp(entry->d_name,prefix,strlen(prefix))) {
                snprintf(name,sizeof(name),"%s/%s",UI_HISTORY_STORE_DIR,entry->d_name);break;
            }
        }
        closedir(entries);assert(name[0]);
        struct stat unchanged,current;
        assert(readlink(name,attempted,sizeof(attempted))>0 && !stat(name,&unchanged));
        char backup[512];snprintf(backup,sizeof(backup),"%s/%s/%s",UI_HISTORY_APP_VOLUME,HISTORY_OBJECTS,HISTORY_INDEX_BACKUP);
        assert(access(backup,F_OK)!=0);
        atomic_store(&fault,FAIL_NONE);
        for(unsigned i=0;i<9000;i++){
            pressure_ui_tick();ui_history_data_poll(app_clock_uptime_ms());
            (void)counting_history_poll_commit(&g_counting_session,&final_batch,app_clock_uptime_ms());
            if(counting_history_is_idle() && ui_history_data_status()!=STORAGE_JOB_FAILED) break;
            usleep(1000);
        }
        assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_READY);
        assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
        assert(!stat(name,&current) && current.st_dev==unchanged.st_dev && current.st_ino==unchanged.st_ino);
    }
    if (!strcmp(argv[1],"blocked")) {
        printf("REPRODUCED launches=%u failed=%u history_status=%u next_id=%u\n",pressure_ui_launches(),status.finished&&!status.success,ui_history_data_status(),ui_history_data_get()->next_record_no);
        assert(!pressure_ui_launches() && status.finished && !status.success);
        return 0;
    }
    assert(pressure_ui_launches()==1 && counting_history_is_idle());
    assert(ui_history_data_status()!=STORAGE_JOB_FAILED && ui_history_data_status()!=STORAGE_JOB_PENDING);
    state=ui_history_data_get();
    assert(state->total_notes_counted==prior_total+1 && state->next_record_no==last_id+1 &&
           state->record_count==1000 && state->records[0].record_no==last_id);
    history_record_detail_t final={0};
    assert(ui_history_record_read_detail(&state->records[0],&final));
    assert(final.serial_count==1 && !strcmp(final.serials[0].sn,"FINAL0000001"));
    history_record_detail_release(&final);
    printf("COMMITTED launches=%u newest=%u count=%u total=%u\n",pressure_ui_launches(),last_id,state->record_count,state->total_notes_counted);
    assert(storage_worker_shutdown());
    return 0;
}
