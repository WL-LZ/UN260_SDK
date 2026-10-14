#ifndef UN260_BOOT_TRIAL_H
#define UN260_BOOT_TRIAL_H
#include <stdbool.h>
#include "un260/boot/boot_bundle.h"

typedef struct {
    bool pending;
    char bundle_id[65];
    char manifest_sha256[65];
} boot_trial_visual_t;

/* Read-only display permission, never application/recovery authorization.
 * 0: no transaction; 1: committed cold-boot trial; -1: defer to recovery.
 * The caller serializes root-service use with the updater's native lock;
 * PID 1 runs before there can be an updater. No persistent file is changed. */
int boot_trial_visual_read(const char *journal, const char *boot_id,
                          boot_trial_visual_t *visual);
bool boot_trial_visual_matches(const boot_trial_visual_t *visual,
                               const boot_bundle_t *bundle);
/* Nonblocking root-service lock; caller closes after first-frame handoff.
 * Never needed by PID 1 or the check-only bundle validator. */
int boot_trial_visual_lock(void);
bool boot_trial_visual_recovery_ready(const char *script, const char *checksum);
#endif
