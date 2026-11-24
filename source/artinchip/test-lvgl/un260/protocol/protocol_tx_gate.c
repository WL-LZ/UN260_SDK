#include "protocol_tx_gate.h"
#include "protocol_send.h"
#include "un260/lv_system/app_clock.h"
#include <pthread.h>
#include <string.h>

/* Bound sound deferral even for normal commands with no reply. */
#define REPLY_WINDOW_MS 300U
#define SETTLE_MS 8U
/* Leave one normal UI poll opportunity to an owner which had to defer.
 * If that owner cancels, no new feedback frame is held indefinitely. */
#define BUSINESS_TURN_MS 40U

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t pending_until[256], quiet_until, business_turn_until, stop_turn_until;
static uint8_t feedback_step;
static uint64_t feedback_until;

static bool feedback_lane_busy(uint64_t now)
{
    if (now < quiet_until || now < feedback_until || now < business_turn_until) return true;
    for (unsigned i = 0; i < 256; ++i)
        if (now < pending_until[i]) return true;
    return false;
}

int protocol_tx_gate_send(const uint8_t *frame, uint16_t len, protocol_tx_policy_t policy,
                          protocol_tx_write_t write_frame)
{
    if (!frame || len < 5 || !write_frame) return -1;
    if (policy == PROTOCOL_TX_IMMEDIATE) pthread_mutex_lock(&mutex);
    else if (pthread_mutex_trylock(&mutex)) return 0;
    uint64_t now = app_clock_monotonic_ms();
    if ((policy == PROTOCOL_TX_AFTER_FEEDBACK || policy == PROTOCOL_TX_STOP_AFTER_FEEDBACK) &&
        now < feedback_until) {
        business_turn_until = feedback_until + BUSINESS_TURN_MS;
        if (policy == PROTOCOL_TX_STOP_AFTER_FEEDBACK) stop_turn_until = business_turn_until;
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    if ((policy == PROTOCOL_TX_AFTER_FEEDBACK && now < stop_turn_until) ||
        (policy == PROTOCOL_TX_FEEDBACK_IF_IDLE && feedback_lane_busy(now))) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    /* The reply observer uses the same lock, including when the peer replies
     * before write_frame returns. Only an actual write starts the window. */
    int result = write_frame(frame, len);
    now = app_clock_monotonic_ms();
    quiet_until = now + SETTLE_MS;
    if (result == len) {
        if (len == 6 && frame[3] == 0x4A && (frame[4] == 1 || frame[4] == 2)) {
            feedback_step = frame[4];
            feedback_until = now + PROTOCOL_FEEDBACK_REPLY_MS;
        } else {
            pending_until[frame[3]] = now + REPLY_WINDOW_MS;
        }
    }
    pthread_mutex_unlock(&mutex);
    return result;
}

void protocol_tx_gate_on_reply(const uint8_t *frame, uint16_t len)
{
    if (!frame || len < 5) return;
    pthread_mutex_lock(&mutex);
    if (frame[3] == 0x4A) {
        /* A late ON echo must not release a subsequent OFF window or vice versa.
         * 0 is the controller's explicit buzzer failure response. */
        if (feedback_until && len == 6 && frame[2] == len &&
            (frame[4] == feedback_step || frame[4] == 0)) {
            uint64_t now = app_clock_monotonic_ms();
            if (business_turn_until > now + BUSINESS_TURN_MS) business_turn_until = now + BUSINESS_TURN_MS;
            if (stop_turn_until > now + BUSINESS_TURN_MS) stop_turn_until = now + BUSINESS_TURN_MS;
            feedback_step = 0;
            feedback_until = 0;
        }
    } else {
        pending_until[frame[3]] = 0;
    }
    quiet_until = app_clock_monotonic_ms() + SETTLE_MS;
    pthread_mutex_unlock(&mutex);
}

void protocol_tx_gate_reset(void)
{
    pthread_mutex_lock(&mutex);
    memset(pending_until, 0, sizeof(pending_until));
    quiet_until = business_turn_until = stop_turn_until = feedback_until = 0;
    feedback_step = 0;
    pthread_mutex_unlock(&mutex);
}
