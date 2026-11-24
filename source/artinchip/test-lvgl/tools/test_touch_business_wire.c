/* Drive production service requests against a byte-stream peer that accepts one
 * command at a time. The busy interval models the failing trace, not MCU code. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>

#include "un260/app_service/app_serial_runtime.c"
#include "un260/app_service/motor_test_service.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/counting/counting_action_service.h"

static int commands[2], responses[2];
static atomic_bool stop_peer, lose_open, lose_close;
static atomic_uint opens, closes, business_frames, business_drops, physical_batch;
static atomic_uint last_motor_payload, last_motor_command;
static bool manual_lease, pause_ui_dispatch;
static unsigned business_timeouts, batch_confirmations, count_confirmations;
static unsigned motor_confirmations;
static unsigned physical_actions[256];

uint64_t app_clock_monotonic_ms(void)
{
    struct timespec now;
    assert(!clock_gettime(CLOCK_MONOTONIC, &now));
    return now.tv_sec * 1000ULL + now.tv_nsec / 1000000;
}
uint32_t app_clock_uptime_ms(void) { return (uint32_t)app_clock_monotonic_ms(); }
bool counting_history_can_start(void) { return true; }
bool work_mode_service_diagnostic_ready(void) { return true; }
void work_mode_service_hold_operation(uint32_t owner, bool active)
{
    (void)owner;
    manual_lease = active;
}
bool uart_tx_is_healthy(int fd) { return fd == commands[1]; }
int uart_send(int fd, const char *data, int length) { return (int)write(fd, data, length); }
void uart_printf(int fd, const char *format, ...) { (void)fd; (void)format; }
void uart_log_hex(int fd, const char *prefix, const uint8_t *p, size_t n, size_t limit)
{ (void)fd; (void)prefix; (void)p; (void)n; (void)limit; }
int uart_recv(int fd, char *p, int n, int timeout)
{
    struct pollfd port = {.fd=fd, .events=POLLIN};
    return poll(&port, 1, timeout)>0 ? (int)read(fd,p,n) : 0;
}

struct received { uint8_t bytes[8]; unsigned len; uint64_t at; };
static struct received received[256];
static unsigned rx_head, rx_tail;
static pthread_mutex_t rx_lock = PTHREAD_MUTEX_INITIALIZER;
bool protocol_frame_queue_push_wait_at(const uint8_t *f, int len, uint32_t wait, uint64_t at)
{
    (void)wait;
    assert(len==6 || len==7);
    if(f[3]==0x4A) return true;
    pthread_mutex_lock(&rx_lock);
    assert(rx_tail-rx_head < 256);
    struct received *message=&received[rx_tail++ % 256];
    memcpy(message->bytes,f,len);
    message->len=len;
    message->at=at;
    pthread_mutex_unlock(&rx_lock);
    return true;
}
static void send_reply(uint8_t cmd, uint8_t result)
{
    uint8_t frame[]={0xFD,0xDF,6,cmd,result,0,0};
    if(cmd==0x0A) { frame[2]=7; frame[5]=1; }
    /* Controller-side trailer as captured on the wire. */
    uint8_t crc=0;
    for(unsigned i=0; i+1u < frame[2]; ++i){
        crc ^= frame[i];
        for(unsigned bit=0; bit<8; ++bit) crc=(uint8_t)((crc<<1) ^ (crc&0x80 ? 0x31 : 0));
    }
    frame[frame[2]-1]=crc;
    assert(write(responses[1],frame,frame[2])==frame[2]);
}
static void *peer(void *unused)
{
    (void)unused;
    uint8_t frame[8], cmd=0, value=0;
    unsigned have=0;
    uint64_t until=0;
    while(!atomic_load(&stop_peer)) {
        uint64_t now=app_clock_monotonic_ms();
        if(until && now>=until) {
            until=0;
            if(cmd!=0x4A || !(value==1 ? atomic_load(&lose_open) : atomic_load(&lose_close)))
                send_reply(cmd,cmd==0x4A ? value : 1);
        }
        struct pollfd port={.fd=commands[0],.events=POLLIN};
        if(poll(&port,1,1)<=0) continue;
        /* Read the three-byte header, then exactly its declared payload. */
        unsigned need = have<3 ? 3 : frame[2];
        assert(need<=sizeof(frame) && need>have);
        int n=(int)read(commands[0],frame+have,need-have);
        assert(n>0); have+=(unsigned)n;
        if(have<3 || have<frame[2]) continue;
        assert(frame[0]==0xFD && frame[1]==0xDF && frame[have-1]==0x0A);
        have=0;
        if(frame[3]!=0x4A) {
            atomic_fetch_add(&business_frames,1);
            if(until) { atomic_fetch_add(&business_drops,1); continue; }
        } else if(until) continue;
        cmd=frame[3]; value=frame[4];
        if(cmd==0x4A) {
            atomic_fetch_add(value==1 ? &opens : &closes,1);
            until=app_clock_monotonic_ms()+ (value==1 ? 34 : 20);
        } else {
            ++physical_actions[cmd];
            if(cmd==0x06) atomic_store(&physical_batch,value);
            if(cmd>=0x52 && cmd<=0x54) {
                assert(frame[2]==7);
                atomic_store(&last_motor_payload,frame[4]*256u+frame[5]);
                atomic_store(&last_motor_command,cmd);
            }
            until=app_clock_monotonic_ms()+16;
        }
    }
    return NULL;
}
static void pump(void)
{
    setting_batch_result_t failed;
    assert(!setting_service_batch_poll_send(&failed,NULL));
    motor_test_service_poll(app_clock_uptime_ms());
    if(pause_ui_dispatch) return;
    pthread_mutex_lock(&rx_lock);
    while(rx_head!=rx_tail) {
        struct received message=received[rx_head++ % 256];
        uint8_t cmd=message.bytes[3];
        protocol_request_result_context_begin(message.at);
        if(cmd==0x06) {
            setting_batch_result_t result;
            assert(setting_service_batch_take_result(cmd,message.bytes[4],&result));
            assert(result.target.num==atomic_load(&physical_batch));
            ++batch_confirmations;
        } else if(cmd==0x0A) {
            assert(counting_action_start_pending());
            counting_action_handle_reply(cmd,message.bytes,message.len);
            assert(!counting_action_start_pending());
            ++count_confirmations;
        } else {
            assert(cmd>=0x52 && cmd<=0x54);
            motor_test_service_on_reply(cmd,message.bytes[4]);
            ++motor_confirmations;
        }
        protocol_request_result_context_end();
    }
    pthread_mutex_unlock(&rx_lock);
    setting_batch_result_t timeout;
    if(setting_service_batch_take_timeout(&timeout)) ++business_timeouts;
    if(counting_action_take_timeouts()) ++business_timeouts;
}
static void settle(unsigned ms)
{
    uint64_t end=app_clock_monotonic_ms()+ms;
    do { pump(); usleep(1000); } while(app_clock_monotonic_ms()<end);
}
static void wait_delta(atomic_uint *value, unsigned before)
{
    for(unsigned i=0; i<500 && atomic_load(value)==before; ++i) settle(1);
    if(atomic_load(value)==before)
        fprintf(stderr,"missing %s after %u; opens=%u closes=%u TX=%u drops=%u batch=%u count=%u motor=%u\n",
                value==&opens ? "ON" : value==&closes ? "OFF" : "business frame", before,
                atomic_load(&opens),atomic_load(&closes),atomic_load(&business_frames),
                atomic_load(&business_drops),batch_confirmations,count_confirmations,motor_confirmations);
    assert(atomic_load(value)>before);
}
static void sound_before_business(bool during_close)
{
    settle(140);
    atomic_uint *event=during_close ? &closes : &opens;
    unsigned before=atomic_load(event);
    buzzer_service_click();
    wait_delta(event,before);
}
static void wait_confirmation(const unsigned *confirmed, unsigned before)
{
    uint64_t deadline=app_clock_monotonic_ms()+1000;
    do { settle(1); } while(*confirmed==before && app_clock_monotonic_ms()<deadline);
    if(*confirmed!=before+1) {
        fprintf(stderr,"business missing: confirmations=%u drops=%u timeouts=%u\n",
                *confirmed,atomic_load(&business_drops),business_timeouts);
        fflush(stderr);
    }
    assert(*confirmed==before+1 && !business_timeouts);
}
static void batch(unsigned target)
{
    unsigned before=batch_confirmations;
    assert(setting_service_request_batch_number(target,true,10));
    wait_confirmation(&batch_confirmations,before);
    assert(atomic_load(&physical_batch)==target);
}
int main(void)
{
    assert(!pipe(commands) && !pipe(responses));
    fd4=commands[1]; fd6=9;
    protocol_rx_service_set_observer(app_serial_runtime_observe_reply);
    assert(protocol_rx_service_start(responses[0],fd6));
    assert(buzzer_service_start());
    pthread_t controller;
    assert(!pthread_create(&controller,NULL,peer,NULL));
    const unsigned values[]={40,90,80,95,20,61};
    for(unsigned round=0; round<3; ++round) {
        for(unsigned i=0; i<sizeof(values)/sizeof(*values); ++i) {
            if(round<2) {
                sound_before_business(round==1);
                batch(values[i]);
            } else {
                settle(140);
                unsigned before=batch_confirmations;
                unsigned sound=atomic_load(&opens);
                assert(setting_service_request_batch_number(values[i],true,10));
                buzzer_service_click();
                wait_confirmation(&batch_confirmations,before);
                wait_delta(&opens,sound);
            }
        }
    }
    for(unsigned round=0; round<2; ++round) {
        sound_before_business(round!=0);
        unsigned before=count_confirmations;
        assert(counting_action_request_start());
        wait_confirmation(&count_confirmations,before);
        for(unsigned motor=0; motor<3; ++motor) {
            for(unsigned start=1; ; --start) {
                sound_before_business(start==0);
                before=motor_confirmations;
                assert(motor_test_service_request(motor,start!=0));
                wait_confirmation(&motor_confirmations,before);
                motor_test_snapshot_t result=motor_test_service_get(motor);
                assert(!result.pending && result.running==(start!=0) && manual_lease==(start!=0));
                assert(atomic_load(&last_motor_command)==0x52+motor);
                assert(atomic_load(&last_motor_payload)==(start ? 257u : motor==2 ? 258u : 0u));
                if(!start) break;
            }
        }
    }
    /* A Start acknowledgement can already be queued on the UI thread while
     * the wire is free for feedback. Request Stop before consuming that ACK. */
    settle(150);
    pause_ui_dispatch=true;
    unsigned confirmed=motor_confirmations;
    unsigned sent=atomic_load(&business_frames);
    assert(motor_test_service_request(0,true));
    wait_delta(&business_frames,sent);
    sound_before_business(false);
    sent=atomic_load(&business_frames);
    assert(motor_test_service_request(0,false));
    assert(motor_test_service_get(0).pending);
    assert(atomic_load(&business_frames)==sent);
    pause_ui_dispatch=false;
    for(unsigned i=0; i<1000 && motor_confirmations<confirmed+2; ++i) settle(1);
    assert(motor_confirmations==confirmed+2 && !motor_test_service_get(0).running);
    assert(!motor_test_service_get(0).pending && !manual_lease);
    assert(atomic_load(&business_frames)==sent+1);

    /* No tone ACK must not strand a running motor. The peer can accept a Stop
     * after its short receive window even when it withholds the tone result. */
    confirmed=motor_confirmations;
    assert(motor_test_service_request(0,true));
    wait_confirmation(&motor_confirmations,confirmed);
    atomic_store(&lose_open,true);
    sound_before_business(false);
    uint64_t stop_requested=app_clock_monotonic_ms();
    confirmed=motor_confirmations;
    assert(motor_test_service_request(0,false));
    wait_confirmation(&motor_confirmations,confirmed);
    assert(app_clock_monotonic_ms()-stop_requested < 500);
    assert(!motor_test_service_get(0).running && !manual_lease);
    settle(300); atomic_store(&lose_open,false);

    atomic_store(&lose_open,true);
    sound_before_business(false); batch(20);
    settle(300); atomic_store(&lose_open,false);
    atomic_store(&lose_close,true);
    sound_before_business(true); batch(61);
    settle(500); atomic_store(&lose_close,false);
    /* With mute, the business route still works. Later accepted input can sound again. */
    buzzer_service_cancel(); unsigned muted=atomic_load(&opens);
    batch(40); settle(180); assert(atomic_load(&opens)==muted);
    /* Once the one-second fault recovery interval has elapsed, the next
     * accepted click probes OFF only. Its ACK permits a subsequent click. */
    settle(1000);
    unsigned closed_before=atomic_load(&closes);
    buzzer_service_click(); wait_delta(&closes,closed_before); settle(40);
    assert(atomic_load(&opens)==muted);
    sound_before_business(false); batch(90);
    buzzer_service_stop();
    atomic_store(&stop_peer,true); pthread_join(controller,NULL); protocol_rx_service_stop();
    assert(atomic_load(&business_drops)==0 && batch_confirmations==22 && count_confirmations==2 && motor_confirmations==16);
    assert(physical_actions[0x06]==22 && physical_actions[0x0A]==2);
    assert(physical_actions[0x52]==8 && physical_actions[0x53]==4 && physical_actions[0x54]==4);
    assert(atomic_load(&business_frames)==40);
    printf("PASS wire: Batch 22 confirmations, count 2, motors 16; 40 TX, 0 business drops, 0 timeouts; sound on/off, missing ACK and recovery\n");
    return 0;
}
