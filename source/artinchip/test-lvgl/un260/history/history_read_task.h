#ifndef HISTORY_READ_TASK_H
#define HISTORY_READ_TASK_H
#include "un260/lv_system/ui_history_data.h"
#include "history_query.h"

typedef struct {
    bool ok;
    history_record_detail_t *detail;
    size_t count;
    uint32_t ids[UI_HISTORY_MAX_RECORDS];
    uint8_t matches[UI_HISTORY_MAX_RECORDS];
} history_read_result_t;
storage_job_id_t history_read_detail_request(const ui_history_record_t *record);
storage_job_id_t history_read_query_request(const ui_history_record_t *records,size_t count,const history_query_t *query);
/* Returns false while pending. Completed errors have result.ok=false.
 * Caller owns detail on success; even cancellation never borrows page memory. */
bool history_read_take(storage_job_id_t id,history_read_result_t *out);
void history_read_cancel(storage_job_id_t id);
void history_read_poll_discarded(void);
#endif
