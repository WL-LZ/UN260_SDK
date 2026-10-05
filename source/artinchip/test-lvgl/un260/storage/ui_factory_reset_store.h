#ifndef UN260_UI_FACTORY_RESET_STORE_H
#define UN260_UI_FACTORY_RESET_STORE_H
#include <stdbool.h>
/* Startup only, before any preference reader/writer starts. A durable journal
 * from an acknowledged reset is replayed; unrelated business files are kept.
 * A replay retains the journal until finish_startup confirms platform owners. */
bool ui_factory_reset_store_resume(void);
/* Finish only after platform owners (notably backlight) have durably restored
 * defaults. The journal remains replayable if startup fails beforehand. */
bool ui_factory_reset_store_startup_pending(void);
bool ui_factory_reset_store_finish_startup(void);
/* Application service owns exclusivity. Work executes on the shared FIFO;
 * no live UI preference or touch matrix is published before the restart. */
bool ui_factory_reset_store_begin(void);
bool ui_factory_reset_store_poll(bool *success);
#endif
