#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "un260/app_service/workspace_service.c"
static workspace_model_t model;
static counting_sim_t count;
static counting_multi_t multi;
const counting_multi_t *counting_multi_current(void){return &multi;}
static bool ready=true,busy,running,diagnostic,pending,capacity=true,connected=true,fail_send;
static unsigned commands,last_step,last_value;
static unsigned actuals[7]={1,0,1,0,0,0,1};
static bool async_save,save_done,save_success=true;
static workspace_model_t save_draft;
uint32_t counting_cashbook_verify_group(void){return 0;}
uint8_t machine_state_mode(void){return actuals[6]==2?MODE_SDC:actuals[6]==3?MODE_CNT:MODE_MDC;}
void workspace_store_init(void){}
bool workspace_store_poll(void){if(!busy||!save_done)return false;busy=false;save_done=false;if(save_success)model=save_draft;return true;}
bool workspace_store_last_success(void){return !busy&&save_success;}
bool workspace_store_ready(void){return ready;}
bool workspace_store_busy(void){return busy;}
const workspace_model_t *workspace_store_get(void){return &model;}
bool workspace_store_save(const workspace_model_t *m){if(busy||!workspace_model_valid(m))return false;if(async_save){save_draft=*m;busy=true;}else model=*m;return true;}
bool app_command_runtime_count_start_busy(void){return running;}
bool app_command_runtime_result_pending(void){return pending;}
bool machine_state_aging_running(void){return false;}
bool work_mode_service_diagnostic_active(void){return diagnostic;}
bool counting_history_can_start(void){return capacity;}
const counting_sim_t *counting_data_current(void){return &count;}
bool protocol_send_is_ready(void){return connected;}
boot_stage_t boot_service_get_stage(void){return BOOT_STAGE_DONE;}
uint8_t machine_state_speed(void){return actuals[0];}
uint8_t machine_state_fo_mode(void){return actuals[1];}
bool machine_state_buzzer_enabled(void){return actuals[2];}
uint8_t machine_state_batch_num(void){return actuals[3];}
bool machine_state_batch_enabled(void){return actuals[3]!=0;}
bool machine_state_add_enabled(void){return actuals[4];}
uint8_t machine_state_work_mode(void){return actuals[5];}
static bool send(unsigned step,unsigned target){commands++;last_step=step;last_value=target;return !fail_send;}
bool setting_service_request_mode(uint8_t n){return send(6,n==MODE_SDC?2:n==MODE_CNT?3:1);}
bool setting_service_request_speed(uint8_t n){return send(0,n);}
bool setting_service_request_fo_mode(uint8_t n){return send(1,n);}
bool setting_service_request_beep(bool n){return send(2,n);}
bool setting_service_request_batch_switch(bool e,uint8_t n,bool old,uint8_t previous){(void)old;(void)previous;return send(3,e?n:0);}
bool setting_service_request_add(bool n){return send(4,n);}
bool setting_service_request_work_mode(uint8_t n){return send(5,n);}
int main(void)
{
    workspace_defaults(&model);uint32_t id;assert(workspace_add_user(&model,"Taylor",&id));
    assert(!workspace_service_switch_blocker());
    running=true;assert(!workspace_service_switch(id));running=false;
    pending=true;assert(!workspace_service_switch(id));pending=false;
    capacity=false;assert(!workspace_service_switch(id));capacity=true;
    diagnostic=true;assert(!workspace_service_switch(id));diagnostic=false;
    actuals[4]=1;count.total_pcs=50;
    assert(!workspace_service_switch(id));assert(workspace_service_batch_next());assert(last_step==3&&last_value==10);
    actuals[4]=0;assert(workspace_service_switch(id));assert(model.active_id==id);assert(commands==1);
    count.multi_currency_result=true;multi.total_pcs=50;assert(!workspace_service_switch(1));
    multi.total_pcs=0;count.multi_currency_result=false;
    workspace_profile_t p={.speed=2,.sort=1,.beep=0,.batch_enabled=1,.batch=100,.add=1,.work=1};
    assert(workspace_service_apply(&p,10));assert(!workspace_service_switch(1));
    unsigned before=commands;workspace_service_poll(20);assert(commands==before+1&&last_step==0);
    workspace_service_poll(200);assert(commands==before+1); /* No send is not an ACK. */
    actuals[last_step]=last_value;workspace_service_poll(300);assert(last_step==1);
    workspace_service_poll(2201);assert(!workspace_service_applying());assert(strstr(workspace_service_apply_message(),"Stopped at Sorting"));
    before=commands;workspace_service_poll(3000);assert(commands==before);
    assert(workspace_service_apply(&p,4000));workspace_service_poll(4001);workspace_service_cancel_apply();
    before=commands;actuals[last_step]=last_value;workspace_service_poll(4010);assert(commands==before&&!workspace_service_applying());
    fail_send=true;assert(workspace_service_apply(&p,5000));workspace_service_poll(5001);assert(!workspace_service_applying());fail_send=false;
    assert(workspace_service_apply(&p,6000));
    for(unsigned i=0;i<12&&workspace_service_applying();i++){workspace_service_poll(6100+i*20);actuals[last_step]=last_value;}
    assert(!workspace_service_applying());assert(strstr(workspace_service_apply_message(),"Profile applied"));
    assert(!memcmp(actuals,(unsigned[]){2,1,0,100,1,1,1},sizeof(actuals)));
    /* Explicit profile mode is sent last and becomes applied only after ACK. */
    count.total_pcs=0;actuals[4]=0;p.add=0;p.mode=2;
    assert(workspace_service_apply(&p,8000));workspace_service_poll(8001);
    assert(last_step==6&&last_value==2&&workspace_service_applying());
    workspace_service_poll(8100);assert(workspace_service_applying());
    actuals[6]=2;workspace_service_poll(8200);assert(!workspace_service_applying());
    /* Editing an active preset must save first, then send once, never fake ACK. */
    workspace_defaults(&model);actuals[3]=10;actuals[4]=1;count.total_pcs=50;
    uint8_t slots[]={0,13,50,100,150};async_save=true;before=commands;
    assert(workspace_service_save_batches(1,slots,5,10,13));
    assert(busy&&commands==before&&actuals[3]==10);
    workspace_service_poll(9000);assert(commands==before);
    /* No Menu callback needed: save completion belongs to the service. */
    save_done=true;workspace_service_poll(9010);
    assert(commands==before+1&&last_step==3&&last_value==13&&actuals[3]==10);
    assert(count.total_pcs==50&&actuals[4]==1&&model.users[0].batches[1]==13);
    workspace_service_poll(9020);assert(commands==before+1);
    actuals[3]=13; /* ACK integration is covered by test_batch_reply.py. */

    /* Non-active edits, OFF and removed active slots must not apply another slot. */
    slots[2]=60;before=commands;
    assert(workspace_service_save_batches(1,slots,5,13,13));save_done=true;workspace_service_poll(9030);assert(commands==before);
    actuals[3]=0;slots[1]=14;
    assert(workspace_service_save_batches(1,slots,5,0,0));save_done=true;workspace_service_poll(9040);assert(commands==before);
    actuals[3]=14;uint8_t removed[]={0,60,100,150};
    assert(workspace_service_save_batches(1,removed,4,0,0));save_done=true;workspace_service_poll(9050);assert(commands==before&&actuals[3]==14);

    /* Failed storage, changed controller state, running and failed send stay honest. */
    workspace_defaults(&model);actuals[3]=10;slots[1]=13;slots[2]=50;
    assert(workspace_service_save_batches(1,slots,5,10,13));save_success=false;save_done=true;
    workspace_service_poll(9060);assert(commands==before&&actuals[3]==10&&model.users[0].batches[1]==10);save_success=true;
    assert(workspace_service_save_batches(1,slots,5,10,13));actuals[3]=50;save_done=true;
    workspace_service_poll(9070);assert(commands==before&&actuals[3]==50);
    workspace_defaults(&model);actuals[3]=10;
    running=true;assert(!workspace_service_save_batches(1,slots,5,10,13));running=false;
    assert(workspace_service_save_batches(1,slots,5,10,13));running=true;save_done=true;
    workspace_service_poll(9080);assert(commands==before);running=false;
    workspace_service_poll(9090);assert(commands==before); /* No surprise delayed apply. */
    workspace_defaults(&model);assert(workspace_service_save_batches(1,slots,5,10,13));
    fail_send=true;save_done=true;workspace_service_poll(9100);fail_send=false;
    assert(commands==before+1&&actuals[3]==10&&strstr(workspace_service_batch_save_message(),"not changed"));
    assert(!workspace_service_save_batches(999,slots,5,10,13));
    assert(!workspace_service_save_batches(1,slots,1,10,13));
    slots[1]=200;assert(!workspace_service_save_batches(1,slots,5,10,200));
    puts("PASS workspace service: profile apply + active Batch edit save/send/ACK boundaries, non-active/OFF/delete, ADD, storage/send failure, changed state, lifecycle and no delayed retry");
}
