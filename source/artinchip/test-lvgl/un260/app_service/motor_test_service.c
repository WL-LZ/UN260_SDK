#include "motor_test_service.h"
#include "work_mode_service.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/app_clock.h"

/* The controller acknowledges a command, not an operation ID. Only one motor
 * request may be in flight; keep its owner across page destruction. */
static struct { motor_test_phase_t phase; bool running, queued, desired; } motors[3];
static int active = -1;
static bool active_run, timed_out, stale_start_reply;
static uint32_t started, next_send;
static bool spacing;

motor_test_snapshot_t motor_test_service_get(unsigned i)
{
    if(i>=3)return (motor_test_snapshot_t){0};
    return (motor_test_snapshot_t){motors[i].phase,motors[i].running,active==(int)i,motors[i].queued};
}
bool motor_test_service_busy(void)
{
    if(active>=0)return true;
    for(unsigned i=0;i<3;i++)if(motors[i].running||motors[i].queued)return true;
    return false;
}
bool motor_test_service_request(unsigned i,bool run)
{
    if(i>=3)return false;
    if(run&&(!work_mode_service_diagnostic_ready()||active==(int)i||motors[i].running||motors[i].queued))return false;
    if(!run&&active==(int)i&&active_run&&timed_out){
        /* A lost Start ACK must not prevent physically requesting Stop.
         * Consume one possible late Start reply before accepting a Stop reply.
         * Until then retain the Manual lease and report uncertainty. */
        const uint8_t stop[2]={i==2?1:0,i==2?2:0};
        if(protocol_send(0x52+i,stop,2)<0)return false;
        active_run=false;stale_start_reply=true;timed_out=false;
        motors[i].queued=false;motors[i].running=true;motors[i].phase=MOTOR_STOPPING;
        started=app_clock_uptime_ms();return true;
    }
    if(!run&&active==(int)i&&!active_run){
        /* Repeating a timed-out Stop is idempotent: either matching ACK proves
         * the same desired state. Never supersede an unconfirmed Start. */
        if(!timed_out)return true;
        const uint8_t stop[2]={i==2?1:0,i==2?2:0};
        if(protocol_send(0x52+i,stop,2)<0)return false;
        started=app_clock_uptime_ms();timed_out=false;motors[i].phase=MOTOR_STOPPING;return true;
    }
    if(!run&&!motors[i].running&&active!=(int)i&&!motors[i].queued)return true;
    motors[i].queued=true;motors[i].desired=run;
    if(active!=(int)i)motors[i].phase=MOTOR_QUEUED;
    return true;
}
void motor_test_service_stop_all(void)
{
    for(unsigned i=0;i<3;i++){
        if(active!=(int)i&&!motors[i].running){
            motors[i].queued=false;
            if(motors[i].phase==MOTOR_QUEUED)motors[i].phase=MOTOR_IDLE;
        }else (void)motor_test_service_request(i,false);
    }
}
void motor_test_service_poll(uint32_t now)
{
    if(active>=0){
        if((uint32_t)(now-started)>=5000){
            timed_out=true;motors[active].phase=MOTOR_UNCONFIRMED;
            if(active_run&&motors[active].queued&&!motors[active].desired)
                (void)motor_test_service_request((unsigned)active,false);
        }
        return;
    }
    if(spacing&&(int32_t)(now-next_send)<0)return;
    spacing=false;
    for(unsigned i=0;i<3;i++)if(motors[i].queued){
        bool run=motors[i].desired;
        motors[i].queued=false;
        if(run&&!work_mode_service_diagnostic_ready()){motors[i].phase=MOTOR_IDLE;continue;}
        uint8_t payload[2]={run?1:(i==2?1:0),run?1:(i==2?2:0)};
        if(protocol_send(0x52+i,payload,2)<0){motors[i].phase=MOTOR_SEND_FAILED;return;}
        work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN<<i,true);
        active=(int)i;active_run=run;started=now;timed_out=false;stale_start_reply=false;
        motors[i].phase=run?MOTOR_STARTING:MOTOR_STOPPING;return;
    }
}
void motor_test_service_on_reply(uint8_t command,uint8_t result)
{
    if(active<0||command!=0x52+active||(result!=1&&result!=2))return;
    unsigned i=(unsigned)active;
    if(stale_start_reply){stale_start_reply=false;return;}
    if(result==1)motors[i].running=active_run;
    motors[i].phase=result==2?MOTOR_REJECTED:motors[i].running?MOTOR_RUNNING:MOTOR_IDLE;
    if(!motors[i].running)work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN<<i,false);
    active=-1;timed_out=false;spacing=true;next_send=app_clock_uptime_ms()+80;
}
