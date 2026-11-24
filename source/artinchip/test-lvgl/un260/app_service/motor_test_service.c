#include "motor_test_service.h"
#include "work_mode_service.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_request.h"
#include "un260/lv_system/app_clock.h"

/* Start and Stop replies share a command; keep their owner across page
 * destruction and treat overlapping replies for the same motor as ambiguous. */
static struct { motor_test_phase_t phase; bool running, queued; } motors[3];
/* Stop replies identify the motor by command. Track stops for other motors
 * independently so an unconfirmed operation cannot delay their stop. */
static struct {
    bool queued, pending, timed_out;
    uint32_t started;
    uint64_t reply_since;
} stops[3];
static int active = -1;
static bool active_run, timed_out, stale_start_reply, ambiguous_start, first_reply_failed;
static uint32_t started, next_send;
static uint64_t active_reply_since, active_stop_since;
static bool spacing;

motor_test_snapshot_t motor_test_service_get(unsigned i)
{
    if(i>=3)return (motor_test_snapshot_t){0};
    return (motor_test_snapshot_t){motors[i].phase,motors[i].running,active==(int)i||stops[i].pending,motors[i].queued||stops[i].queued};
}
bool motor_test_service_busy(void)
{
    if(active>=0)return true;
    for(unsigned i=0;i<3;i++)if(motors[i].running||motors[i].queued||stops[i].queued||stops[i].pending)return true;
    return false;
}
/* Stop has its own wire priority. An unsent stop has no ACK of its own.
 * In particular, Start may settle while the stop waits for the buzzer. */
static bool send_stop(unsigned i)
{
    const uint8_t payload[2] = { i == 2 ? 1 : 0, i == 2 ? 2 : 0 };
    const uint64_t attempt = protocol_request_now_ms();
    int sent = protocol_send_stop_when_feedback_clear(0x52 + i, payload, 2);
    stops[i].queued = sent == 0;
    if (sent < 0) {
        motors[i].phase = MOTOR_SEND_FAILED;
        return false;
    }
    if (sent == 0) {
        motors[i].phase = MOTOR_QUEUED;
        return true;
    }

    motors[i].phase = MOTOR_STOPPING;
    if (active == (int)i) {
        if (active_run) {
            active_run = false;
            active_stop_since = attempt;
            stale_start_reply = ambiguous_start = true;
            first_reply_failed = false;
            motors[i].running = true;
        }
        timed_out = false;
        started = app_clock_uptime_ms();
    } else {
        /* A late ACK for an earlier idempotent Stop can still settle a retry. */
        if (!stops[i].pending) stops[i].reply_since = attempt;
        stops[i].pending = true;
        stops[i].timed_out = false;
        stops[i].started = app_clock_uptime_ms();
    }
    return true;
}

bool motor_test_service_request(unsigned i, bool run)
{
    if (i >= 3) return false;
    if (run) {
        if (!work_mode_service_diagnostic_ready() || active == (int)i ||
            motors[i].running || motors[i].queued || stops[i].queued || stops[i].pending) return false;
        motors[i].queued = true;
        motors[i].phase = MOTOR_QUEUED;
        return true;
    }
    if (motors[i].queued) {
        motors[i].queued = false;
        motors[i].phase = MOTOR_IDLE;
        return true;
    }
    if (stops[i].queued || (stops[i].pending && !stops[i].timed_out) ||
        (active == (int)i && !active_run && !timed_out)) return true;
    if (active != (int)i && !motors[i].running && !stops[i].pending) return true;
    return send_stop(i);
}

void motor_test_service_stop_all(void)
{
    for (unsigned i = 0; i < 3; i++) (void)motor_test_service_request(i, false);
}

void motor_test_service_poll(uint32_t now)
{
    bool other_stop_pending = false;
    for (unsigned i = 0; i < 3; i++) {
        if (stops[i].queued) {
            (void)send_stop(i);
            if (stops[i].queued) return;
        }
        if (stops[i].pending) {
            other_stop_pending = true;
            if ((int32_t)(now - stops[i].started) >= 5000) {
                stops[i].timed_out = true;
                motors[i].phase = MOTOR_UNCONFIRMED;
            }
        }
    }
    if (active >= 0) {
        if ((int32_t)(now - started) >= 5000) {
            timed_out = true;
            motors[active].phase = MOTOR_UNCONFIRMED;
        }
        return;
    }
    if (other_stop_pending || (spacing && (int32_t)(now - next_send) < 0)) return;
    spacing = false;
    for (unsigned i = 0; i < 3; i++) if (motors[i].queued) {
        if (!work_mode_service_diagnostic_ready()) {
            motors[i].queued = false;
            motors[i].phase = MOTOR_IDLE;
            continue;
        }
        const uint8_t payload[2] = {1, 1};
        const uint64_t attempt = protocol_request_now_ms();
        int sent = protocol_send_when_feedback_clear(0x52 + i, payload, 2);
        if (sent == 0) return;
        motors[i].queued = false;
        if (sent < 0) {
            motors[i].phase = MOTOR_SEND_FAILED;
            return;
        }
        work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN << i, true);
        active = (int)i;
        active_run = true;
        active_reply_since = attempt;
        started = app_clock_uptime_ms();
        timed_out = stale_start_reply = ambiguous_start = false;
        motors[i].phase = MOTOR_STARTING;
        return;
    }
}
void motor_test_service_on_reply(uint8_t command,uint8_t result)
{
    if(command>=0x52&&command<=0x54&&(result==1||result==2)){
        unsigned motor=command-0x52;
        if(active!=(int)motor&&stops[motor].pending){
            if(protocol_request_result_time_ms()<stops[motor].reply_since)return;
            stops[motor].queued=false;
            stops[motor].pending=false;stops[motor].timed_out=false;
            if(result==1){
                motors[motor].running=false;motors[motor].phase=MOTOR_IDLE;
                work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN<<motor,false);
            }else motors[motor].phase=MOTOR_REJECTED;
            return;
        }
    }
    if(active<0||command!=0x52+active||(result!=1&&result!=2))return;
    unsigned i=(unsigned)active;
    const uint64_t received = protocol_request_result_time_ms();
    if (received < active_reply_since) return;
    if (!active_run && received < active_stop_since) {
        /* RX may be queued before Stop while UI delivery follows the write.
         * That result belongs to Start; no pre-Stop duplicate can settle Stop. */
        stale_start_reply = ambiguous_start = false;
        return;
    }
    if(stale_start_reply){stale_start_reply=false;first_reply_failed=result!=1;return;}
    if(ambiguous_start){
        ambiguous_start=false;
        /* Mixed results do not identify which command failed. Preserve the
         * uncertain motor state and allow a new Stop request. */
        if(first_reply_failed||result!=1){
            timed_out=true;motors[i].phase=MOTOR_UNCONFIRMED;return;
        }
    }
    if(result==1)motors[i].running=active_run;
    motors[i].phase=result==2?MOTOR_REJECTED:motors[i].running?MOTOR_RUNNING:MOTOR_IDLE;
    if(active_run && stops[i].queued) {
        if (motors[i].running) motors[i].phase = MOTOR_QUEUED;
        else stops[i].queued = false; /* Start was explicitly rejected. */
    } else if (!active_run) {
        stops[i].queued = false; /* An earlier Stop already settled this retry. */
    }
    if(!motors[i].running)work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN<<i,false);
    active=-1;timed_out=false;spacing=true;next_send=app_clock_uptime_ms()+80;
}
