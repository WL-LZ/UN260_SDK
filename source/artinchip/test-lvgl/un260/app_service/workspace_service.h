#ifndef UN260_WORKSPACE_SERVICE_H
#define UN260_WORKSPACE_SERVICE_H
#include "un260/workspace/workspace_model.h"
void workspace_service_init(void);
bool workspace_service_poll(uint32_t now_ms);
const char *workspace_service_switch_blocker(void);
bool workspace_service_switch(uint32_t id);
bool workspace_service_apply(const workspace_profile_t *profile, uint32_t now_ms);
bool workspace_service_applying(void);
void workspace_service_cancel_apply(void);
const char *workspace_service_apply_message(void);
bool workspace_service_quick_enabled(void);
bool workspace_service_set_quick_enabled(bool enabled);
bool workspace_service_batch_next(void);
#endif
