#ifndef WORK_MODE_STORE_H
#define WORK_MODE_STORE_H

#include <stdbool.h>
#include <stdint.h>

/* Local values: 0 = automatic, 1 = manual. Temporary manual is never a
 * preference. restore_pending survives an interrupted diagnostic session. */
typedef struct {
    bool preferred_valid;
    uint8_t preferred;
    bool restore_pending;
} work_mode_record_t;

bool work_mode_store_begin_load(void);
bool work_mode_store_begin_save(const work_mode_record_t *record);
bool work_mode_store_busy(void);
/* Returns one completed I/O result without blocking the UI thread. A missing
 * file is a successful load with preferred_valid=false; corruption is failure. */
bool work_mode_store_poll(work_mode_record_t *record, bool *was_load, bool *ok);

#endif
