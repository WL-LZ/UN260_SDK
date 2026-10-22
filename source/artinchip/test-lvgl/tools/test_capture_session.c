#include "un260/data_collection/data_collection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Message formatting is checked separately by test_i18n_messages.py. */
void ui_message_key(ui_message_t *m, const char *key)
{
    memset(m, 0, sizeof(*m));
    snprintf(m->key, sizeof(m->key), "%s", key);
}
void ui_message_uint(ui_message_t *m, const char *key, uint32_t value)
{
    ui_message_key(m, key);
    m->format = UI_MESSAGE_UINT;
    m->numbers[0] = value;
}
bool ui_message_render(const ui_message_t *m, char *out, size_t size)
{
    if (m->format == UI_MESSAGE_UINT) snprintf(out, size, m->key, m->numbers[0]);
    else snprintf(out, size, "%s", m->key);
    return true;
}
static data_collection_reply_result_t rx(uint8_t code, uint32_t now)
{
    const uint8_t frame[] = {0xFD, 0xDF, 6, 0xC0, code, 0x0A};
    return data_collection_reply_handle(frame, sizeof(frame), now);
}
static void mode(data_collect_mode_t expected)
{
    assert(data_collection_state_mode() == expected);
}
static void status(const char *expected)
{
    assert(!strcmp(data_collection_state_status(), expected));
}
static void confirm(data_collect_mode_t target, uint32_t now)
{
    assert(data_collection_request_begin(target, "Waiting", now));
    assert(rx(target == DATA_COLLECT_MODE_ALL ? 1 : target == DATA_COLLECT_MODE_FALSE ? 3 : 0xFF,
              now + 1) == (target == DATA_COLLECT_MODE_NONE ? DATA_COLLECTION_REPLY_EXITED :
                                                              DATA_COLLECTION_REPLY_REQUEST_CONFIRMED));
    assert(!data_collection_request_pending());
    mode(target);
}
int main(void)
{
    assert(DATA_COLLECTION_REQUEST_TIMEOUT_MS == 1500U);
    assert(data_collection_reply_handle(NULL, 6, 1) == DATA_COLLECTION_REPLY_INVALID);
    const uint8_t short_frame[] = {0xFD, 0xDF, 6, 0xC0, 1};
    assert(data_collection_reply_handle(short_frame, sizeof(short_frame), 2) == DATA_COLLECTION_REPLY_INVALID);
    assert(!data_collection_request_begin((data_collect_mode_t)99, "invalid", 3));
    assert(rx(3, 4) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    mode(DATA_COLLECT_MODE_NONE); /* Unsolicited startup ACK only reports status. */
    status("FALSE REPORT collection mode ready.");

    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL, "Waiting", 10));
    assert(!data_collection_request_begin(DATA_COLLECT_MODE_FALSE, "Waiting", 11));
    assert(rx(3, 12) == DATA_COLLECTION_REPLY_IGNORED);
    assert(rx(0xFF, 13) == DATA_COLLECTION_REPLY_IGNORED);
    assert(data_collection_request_pending());
    assert(rx(1, 14) == DATA_COLLECTION_REPLY_REQUEST_CONFIRMED);
    mode(DATA_COLLECT_MODE_ALL);
    assert(!data_collection_request_pending());
    assert(rx(2, 15) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    status("Collection completed. Data can be copied from USB.");
    mode(DATA_COLLECT_MODE_ALL);
    assert(rx(5, 16) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    status("USB ready. You can start counting.");
    assert(rx(4, 17) == DATA_COLLECTION_REPLY_UNKNOWN);
    status("Collection reply: 0x04");
    assert(rx(7, 18) == DATA_COLLECTION_REPLY_UNKNOWN);
    status("Collection reply: 0x07");

    confirm(DATA_COLLECT_MODE_FALSE, 30); /* Legacy mode selection permits switching directly. */
    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL, "Waiting", 40));
    data_collection_request_cancel(); /* Failed send restores the preceding status. */
    assert(!data_collection_request_pending());
    mode(DATA_COLLECT_MODE_FALSE);
    status("FALSE REPORT collection mode ready.");
    assert(rx(1, 41) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    mode(DATA_COLLECT_MODE_FALSE);

    assert(data_collection_request_begin(DATA_COLLECT_MODE_NONE, "Closing", 50));
    assert(rx(5, 51) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    assert(rx(2, 52) == DATA_COLLECTION_REPLY_STATUS_UPDATED);
    assert(data_collection_request_pending());
    mode(DATA_COLLECT_MODE_FALSE);
    assert(rx(0xFF, 53) == DATA_COLLECTION_REPLY_EXITED);
    assert(!data_collection_request_pending());
    mode(DATA_COLLECT_MODE_NONE);
    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL, "Waiting", 60));
    assert(rx(6, 61) == DATA_COLLECTION_REPLY_REQUEST_FAILED);
    assert(!data_collection_request_pending());
    mode(DATA_COLLECT_MODE_NONE);
    status("USB not ready. Collection mode error.");

    confirm(DATA_COLLECT_MODE_FALSE, 100);
    assert(data_collection_request_begin(DATA_COLLECT_MODE_NONE, "Closing", 200));
    assert(!data_collection_request_take_timeout(1699));
    assert(data_collection_request_take_timeout(1700));
    assert(!data_collection_request_pending());
    mode(DATA_COLLECT_MODE_FALSE);
    status("Collection mode request timed out.");
    assert(rx(0xFF, 1701) == DATA_COLLECTION_REPLY_IGNORED);
    mode(DATA_COLLECT_MODE_FALSE);
    confirm(DATA_COLLECT_MODE_NONE, 1710);

    confirm(DATA_COLLECT_MODE_FALSE, 2000);
    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL, "Waiting", 2100));
    assert(data_collection_request_take_timeout(3600));
    assert(rx(1, 6600) == DATA_COLLECTION_REPLY_STATUS_UPDATED); /* Expired late-ACK guard. */
    mode(DATA_COLLECT_MODE_FALSE);
    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL, "Waiting", UINT32_MAX - 500U));
    assert(!data_collection_request_take_timeout(998));
    assert(data_collection_request_take_timeout(999)); /* Millisecond counter wrap. */
    mode(DATA_COLLECT_MODE_FALSE);
    confirm(DATA_COLLECT_MODE_NONE, 7000);
    data_collection_request_cancel();
    assert(!data_collection_request_pending() && data_collection_state_pcs() == 0);
    puts("PASS legacy capture: ACK matching, C0/02 completion, no C0/07 dependency, mode switch, cancellation, USB error, timeout, late ACK and clock wrap");
    return 0;
}
