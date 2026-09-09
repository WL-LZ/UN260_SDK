#ifndef UI_HISTORY_EXPORT_DATA_H
#define UI_HISTORY_EXPORT_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* UI-thread request. IDs are validated and copied before USB access. An empty,
 * stale or duplicate selection fails; it never means all stored records. */
bool ui_history_export_data_request_records(const uint32_t *record_nos,
                                            size_t record_count);
bool ui_history_export_data_request(void);

#endif
