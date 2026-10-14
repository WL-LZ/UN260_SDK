#include "buzzer_service.h"
#include "un260/protocol/protocol_send.h"
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define CLICK_MS 35U
#define CLICK_MAX_AGE_MS 250U
#define ON_ACK_MS 120U
#define SLOT_POLL_MS 5U
#define MIN_INTERVAL_MS 120U
#define OFF_ACK_MS 120U
#define OFF_ATTEMPTS 3U
#define RECOVERY_INTERVAL_MS 1000U
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_t worker;
static bool condition_ready, started, running, pending, owned, disabled, off_ack, on_ack, cancelled;
static uint64_t last_click, last_open, on_ack_ms;
static enum { IDLE, WAIT_SLOT, ON, WAIT_OFF } phase;

static uint64_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void init_condition(void)
{
    pthread_condattr_t attr;
    if(pthread_condattr_init(&attr)) return;
    if(!pthread_condattr_setclock(&attr, CLOCK_MONOTONIC))
        condition_ready = pthread_cond_init(&condition, &attr) == 0;
    pthread_condattr_destroy(&attr);
}
/* Called with mutex held. Spurious wakes cannot extend a pulse. */
static void wait_until(uint64_t deadline, bool closing)
{
    struct timespec t = {deadline / 1000, (deadline % 1000) * 1000000};
    while(running && now_ms() < deadline && !(closing ? off_ack : (disabled || cancelled))) {
        int rc = pthread_cond_timedwait(&condition, &mutex, &t);
        if(rc == ETIMEDOUT) break;
        if(rc) { disabled = true; break; }
    }
}
static bool send_state(uint8_t state)
{
    return protocol_send(0x4A, &state, 1) == 6;
}
/* Mutex held on entry/exit. Only the worker waits, and a click has one fixed
 * expiry. Failed sends still need OFF; an unsent expired click does not. */
static bool open_when_idle(void)
{
    uint64_t deadline = last_click + CLICK_MAX_AGE_MS;
    on_ack = false;
    while(running && !disabled && !cancelled && now_ms() < deadline) {
        phase = ON;
        uint8_t state = 1;
        pthread_mutex_unlock(&mutex);
        int result = protocol_send_if_idle(0x4A, &state, 1);
        pthread_mutex_lock(&mutex);
        if(result != 0) { owned = true; if(result == 6) last_open = now_ms(); else disabled = true; return true; }
        phase = WAIT_SLOT;
        uint64_t next = now_ms() + SLOT_POLL_MS;
        wait_until(next < deadline ? next : deadline, false);
    }
    return false;
}
static void wait_open_ack(void)
{
    uint64_t deadline = now_ms() + ON_ACK_MS;
    struct timespec t = {deadline / 1000, (deadline % 1000) * 1000000};
    while(running && !disabled && !cancelled && !on_ack && now_ms() < deadline) {
        int rc = pthread_cond_timedwait(&condition, &mutex, &t);
        if(rc == ETIMEDOUT) break;
        if(rc) { disabled = true; break; }
    }
    if(on_ack && !disabled) wait_until(on_ack_ms + CLICK_MS, false);
}
static void *run(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&mutex);
    while(running) {
        while(running && !pending) pthread_cond_wait(&condition, &mutex);
        if(!running) break;
        pending = false;
        bool recover = disabled;
        phase = WAIT_SLOT;
        if(!recover && !open_when_idle()) { phase = IDLE; continue; }
        if(!recover && !disabled) wait_open_ack();
        owned = true;
        phase = WAIT_OFF;
        off_ack = false;
        for(unsigned attempt = 0; attempt < OFF_ATTEMPTS; ++attempt) {
            pthread_mutex_unlock(&mutex);
            bool closed = send_state(2);
            pthread_mutex_lock(&mutex);
            if(!closed) { disabled = true; break; }
            wait_until(now_ms() + OFF_ACK_MS, true);
            if(off_ack) { owned = disabled = false; break; }
            if(!running) break;
        }
        /* Never replay a stale click. While closure is uncertain, a later
         * contact may only probe OFF; ON resumes after confirmed closure. */
        if(owned) disabled = true;
        phase = IDLE;
    }
    bool close = owned;
    pthread_mutex_unlock(&mutex);
    if(close) (void)send_state(2);
    return NULL;
}
bool buzzer_service_start(void)
{
    pthread_once(&once, init_condition);
    if(!condition_ready) return false;
    pthread_mutex_lock(&mutex);
    if(started) { pthread_mutex_unlock(&mutex); return false; }
    running = true;
    pending = owned = disabled = off_ack = on_ack = cancelled = false;
    phase = IDLE;
    last_click = last_open = 0;
    started = pthread_create(&worker, NULL, run, NULL) == 0;
    if(!started) running = false;
    bool result = started;
    pthread_mutex_unlock(&mutex);
    return result;
}
void buzzer_service_stop(void)
{
    pthread_mutex_lock(&mutex);
    if(!started) { pthread_mutex_unlock(&mutex); return; }
    running = false;
    pending = false;
    pthread_cond_signal(&condition);
    pthread_mutex_unlock(&mutex);
    pthread_join(worker, NULL);
    pthread_mutex_lock(&mutex);
    started = false;
    pthread_mutex_unlock(&mutex);
}
void buzzer_service_click(void)
{
    pthread_mutex_lock(&mutex);
    uint64_t now = now_ms();
    if(running && phase == IDLE && !pending &&
       (!last_open || now - last_open >= MIN_INTERVAL_MS) &&
       (!last_click || now - last_click >= (disabled ? RECOVERY_INTERVAL_MS : MIN_INTERVAL_MS))) {
        last_click = now;
        cancelled = false;
        pending = true;
        pthread_cond_signal(&condition);
    }
    pthread_mutex_unlock(&mutex);
}
void buzzer_service_cancel(void)
{
    pthread_mutex_lock(&mutex);
    pending = false;
    if(!cancelled) { cancelled = true; if(running) pthread_cond_signal(&condition); }
    pthread_mutex_unlock(&mutex);
}
void buzzer_service_on_reply(uint8_t status)
{
    pthread_mutex_lock(&mutex);
    if(running) {
        if(status == 0 && phase != IDLE) disabled = true;
        if(status == 1 && phase == ON && !on_ack) { on_ack = true; on_ack_ms = now_ms(); }
        if(status == 2 && owned && phase != ON) {
            off_ack = true;
            if(phase == IDLE) owned = disabled = false;
        }
        pthread_cond_signal(&condition);
    }
    pthread_mutex_unlock(&mutex);
}
