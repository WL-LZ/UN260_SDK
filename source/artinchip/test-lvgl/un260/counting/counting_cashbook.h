#ifndef UN260_COUNTING_CASHBOOK_H
#define UN260_COUNTING_CASHBOOK_H
#include "un260/lv_system/ui_history_data.h"
#include "un260/workspace/cashbook.h"
void counting_cashbook_capture(cashbook_result_t *, const ui_history_record_t *, const counting_sim_t *);
/* Called after the corresponding History commit succeeds. True only when durable. */
bool counting_cashbook_commit(const cashbook_result_t *);
bool counting_cashbook_arm_verify(uint32_t group);
uint32_t counting_cashbook_verify_group(void);
void counting_cashbook_cancel_verify(void);
void counting_cashbook_on_start(void);
#endif
