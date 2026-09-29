#ifndef FAULT_GUIDE_CATALOG_H
#define FAULT_GUIDE_CATALOG_H
#include "un260/machine_state/machine_fault.h"

typedef enum { MF_FRONT, MF_TOP, MF_REAR, MF_SIDE } mf_view_t;
typedef enum { MF_MACHINE, MF_HOPPER, MF_REJECT, MF_STACKER, MF_PATH,
               MF_ENCODERS, MF_IMAGEBOARD } mf_zone_t;
typedef enum { MF_FOCUS, MF_OPEN, MF_REMOVE, MF_CLEAN, MF_CLOSE } mf_action_t;
typedef struct { const char *key; } mf_text_t;
typedef struct {
    mf_text_t short_title, title, body;
    mf_view_t view;
    mf_zone_t zone;
    mf_action_t action;
} mf_step_t;
typedef struct {
    mf_text_t title, location;
    uint8_t step_count;
    mf_step_t steps[3];
} mf_guide_t;

/* Presentation catalog; it accepts decoded identities and owns no protocol
 * transmission, retry policy, or device state. Unknown identities are retained. */
void fault_guide_lookup(machine_fault_key_t key, mf_guide_t *guide);
void fault_guide_format_code(machine_fault_key_t key, char *buffer, size_t size);
const char *fault_guide_text(mf_text_t text);
#endif
