#include "un260/protocol/protocol_tx_gate.h"
#include "un260/protocol/protocol_send.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static uint64_t now = 1000;
uint64_t app_clock_monotonic_ms(void) { return now; }
static unsigned writes;
static bool fail;
static atomic_bool contended;

static const uint8_t on[] = {0xFD,0xDF,6,0x4A,1,0x0A};
static const uint8_t off[] = {0xFD,0xDF,6,0x4A,2,0x0A};
static const uint8_t mode[] = {0xFD,0xDF,6,0x04,1,0x0A};
static const uint8_t speed[] = {0xFD,0xDF,6,0x16,1,0x0A};
static const uint8_t start[] = {0xFD,0xDF,6,0x67,1,0x0A};

static int write_frame(const uint8_t *frame,uint16_t len)
{
    assert(frame && len == 6);
    ++writes;
    return fail ? -1 : len;
}
static int send(const uint8_t *frame, protocol_tx_policy_t policy)
{
    return protocol_tx_gate_send(frame, 6, policy, write_frame);
}
static void *try_optional(void *unused)
{
    (void)unused;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    assert(send(start, PROTOCOL_TX_AFTER_FEEDBACK) == 0);
    atomic_store(&contended, true);
    return NULL;
}
static int write_contended(const uint8_t *frame, uint16_t len)
{
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, try_optional, NULL));
    pthread_join(thread, NULL);
    assert(atomic_load(&contended));
    return write_frame(frame, len);
}
int main(void)
{
    protocol_tx_gate_reset();
    assert(protocol_tx_gate_send(mode, 6, PROTOCOL_TX_IMMEDIATE, write_contended) == 6);
    assert(send(speed, PROTOCOL_TX_IMMEDIATE) == 6);
    assert(writes == 2);
    now += 50;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    protocol_tx_gate_on_reply(start, 6);
    now += 10;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    protocol_tx_gate_on_reply(mode, 6);
    now += 10;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    protocol_tx_gate_on_reply(speed, 6);
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    now += 10;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 6);
    assert(writes == 3);

    /* ON is on the wire. An unrelated or wrong-stage reply cannot release it.
     * The owner gets an actual write only after the matching reply. */
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 0);
    protocol_tx_gate_on_reply(speed, 6);
    protocol_tx_gate_on_reply(off, 6);
    protocol_tx_gate_on_reply(on, 5);
    assert(send(start, PROTOCOL_TX_AFTER_FEEDBACK) == 0 && writes == 3);
    protocol_tx_gate_on_reply(on, 6);
    now += 10;
    assert(send(off, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    assert(send(start, PROTOCOL_TX_AFTER_FEEDBACK) == 6);
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 6);
    protocol_tx_gate_on_reply(start, 6);
    protocol_tx_gate_on_reply(mode, 6);
    now += 50;
    assert(send(off, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 6);
    protocol_tx_gate_on_reply(on, 6);
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 0);
    assert(send(speed, PROTOCOL_TX_STOP_AFTER_FEEDBACK) == 0);
    /* A real immediate stop must not wait for a sound reply. */
    assert(send(speed, PROTOCOL_TX_IMMEDIATE) == 6);
    now += PROTOCOL_FEEDBACK_REPLY_MS - 1;
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 0);
    ++now;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 0);
    assert(send(speed, PROTOCOL_TX_STOP_AFTER_FEEDBACK) == 6);
    now += 40;
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 6);
    assert(send(start, PROTOCOL_TX_AFTER_FEEDBACK) == 6);

    protocol_tx_gate_reset();
    fail = true;
    assert(send(mode, PROTOCOL_TX_IMMEDIATE) == -1);
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    now += 10;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == -1);
    fail = false;
    assert(send(mode, PROTOCOL_TX_AFTER_FEEDBACK) == 6);
    now += 299;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 0);
    ++now;
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 6);
    protocol_tx_gate_reset();
    assert(send(on, PROTOCOL_TX_FEEDBACK_IF_IDLE) == 6);
    assert(protocol_tx_gate_send(NULL, 6, PROTOCOL_TX_FEEDBACK_IF_IDLE, write_frame) == -1);
    puts("PASS TX gate: matching business and sound replies, atomic admission, bounded release, immediate stop, reset");
}
