#ifndef COUNTING_REPORT_SYNC_H
#define COUNTING_REPORT_SYNC_H

#include "counting_data_types.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_REPORT_IGNORED,
    COUNTING_REPORT_READY,
    COUNTING_REPORT_REUSED,
    COUNTING_REPORT_FAILED
} counting_report_result_t;

typedef enum {
    COUNTING_REPORT_FAILURE_NONE,
    COUNTING_REPORT_FAILURE_REJECT,
    COUNTING_REPORT_FAILURE_SERIAL,
    COUNTING_REPORT_FAILURE_CAPACITY
} counting_report_failure_t;

/* One controller, one untagged detail transaction. All calls belong to the
 * protocol consumer; neither timers nor views write these snapshots. */
void counting_report_begin(bool add, counting_sim_t *data);
void counting_report_reset(void);
void counting_report_shutdown(void);
void counting_report_schedule(const counting_session_state_t *session,
                              uint32_t now);
void counting_report_poll(const counting_session_state_t *session,
                          uint32_t now);
bool counting_report_discard_stale(uint8_t cmd, const uint8_t *buf, uint8_t len);
bool counting_report_accept_reject(const counting_session_state_t *session);
bool counting_report_reject_start(void);
bool counting_report_reject_started(void);
void counting_report_touch(uint32_t now);
bool counting_report_accept_serial(const counting_session_state_t *session);
counting_report_result_t counting_report_reject_end(
    const counting_session_state_t *session, const counting_sim_t *data, uint32_t now);
counting_report_result_t counting_report_serial(
    const counting_session_state_t *session, counting_sim_t *data,
    const uint8_t *buf, uint8_t len, uint32_t now);
int counting_report_live_slot(counting_sim_t *data, bool *cleared);
counting_report_failure_t counting_report_take_failure(void);

#endif
