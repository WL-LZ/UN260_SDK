#ifndef UI_HISTORY_EXPORT_DATA_H
#define UI_HISTORY_EXPORT_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* UI-thread request. IDs are validated and copied before USB access. An empty,
 * stale or duplicate selection fails; it never means all stored records.
 * True means an immutable export job was accepted. Poll publishes its final
 * result through the existing notice system; it never means synchronous success. */
bool ui_history_export_data_request_records(const uint32_t *record_nos,
                                            size_t record_count);
bool ui_history_export_data_request(void);
/* UI-thread completion handoff for the accepted asynchronous export. */
void ui_history_export_data_poll(void);

#endif
