#ifndef UN260_WORKSPACE_STORE_H
#define UN260_WORKSPACE_STORE_H
#include "un260/workspace/workspace_model.h"
/* One independent bounded worker: image decoding cannot block history FIFO.
 * All API calls except the worker itself run on the application/UI thread. */
void workspace_store_init(void);
bool workspace_store_poll(void);
bool workspace_store_ready(void);
bool workspace_store_busy(void);
const char *workspace_store_message(void);
const workspace_model_t *workspace_store_get(void);
bool workspace_store_save(const workspace_model_t *model);
bool workspace_store_scan_usb(void);
uint32_t workspace_store_usb_images(void);
bool workspace_store_import_avatar(unsigned image);
const workspace_avatar_t *workspace_store_avatar(void);
unsigned workspace_store_revision(void);
bool workspace_store_last_success(void);
/* Snapshot is copied before the worker starts. Report producer owns the whitelist. */
bool workspace_store_export_support(const char *report);
#endif
