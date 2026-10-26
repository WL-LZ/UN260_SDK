#ifndef COUNTING_REJECT_SN_REPLY_H
#define COUNTING_REJECT_SN_REPLY_H

#include <stdbool.h>
#include <stdint.h>

#include "counting_detail_state.h"
#include "counting_data_types.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_DETAIL_REPLY_INVALID = 0,
    COUNTING_DETAIL_REPLY_IGNORED,
    COUNTING_DETAIL_REPLY_START,
    COUNTING_DETAIL_REPLY_DATA,
    COUNTING_DETAIL_REPLY_END,
    COUNTING_DETAIL_REPLY_MEMORY_ERROR,
} counting_detail_reply_result_t;

/* Callbacks run synchronously during dispatch; context is borrowed, not retained. */
typedef struct {
    void *context;
    void (*on_history_frame)(void *context,
                             const char *tag,
                             const uint8_t *buf,
                             uint8_t len);
    void (*on_reject_analysis_ready)(void *context);
    void (*on_reject_report_changed)(void *context);
    void (*on_summary_changed)(void *context, bool refresh_main);
    void (*on_serial_data_started)(void *context);
    void (*on_serial_report_ready)(void *context);
    void (*on_serial_ui_complete)(void *context, bool begin_end_anim);
    void (*on_serial_item_changed)(void *context);
    void (*on_live_serial_received)(void *context,
                                    int denomination,
                                    const char *serial_number);
    void (*on_history_record_ready)(void *context);
    void (*on_detail_complete)(void *context);
} counting_reject_sn_reply_hooks_t;

counting_detail_reply_result_t counting_reject_sn_reply_dispatch(
    uint8_t cmd,
    counting_detail_state_t *detail,
    counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks);

#endif
