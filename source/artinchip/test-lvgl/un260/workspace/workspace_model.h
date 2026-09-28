#ifndef UN260_WORKSPACE_MODEL_H
#define UN260_WORKSPACE_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#define WORKSPACE_USERS 8
#define WORKSPACE_PROFILES 8
#define WORKSPACE_BATCHES 10
#define WORKSPACE_NAME 24
#define WORKSPACE_EMPLOYEE_ID 16
#define WORKSPACE_TEAM 32
#define WORKSPACE_AVATAR_SIDE 64
#define WORKSPACE_AVATAR_BYTES (64U * 64U * 4U)
typedef struct {
    uint32_t present;
    uint8_t pixels[WORKSPACE_AVATAR_BYTES];
} workspace_avatar_t;
typedef struct {
    uint32_t id;
    char name[WORKSPACE_NAME + 1];
    uint8_t speed, work, add, sort, beep, batch_enabled, batch;
    uint8_t mode; /* 0 keep current (migrated profiles), 1 mixed, 2 single, 3 count. */
} workspace_profile_t;
typedef struct {
    uint32_t id;
    char name[WORKSPACE_NAME + 1];
    uint8_t quick_enabled, profile_count, batch_count;
    uint8_t batches[WORKSPACE_BATCHES];
    uint8_t qr_after_count; /* Occupies zeroed v1 padding; existing offsets unchanged. */
    workspace_profile_t profiles[WORKSPACE_PROFILES];
    workspace_avatar_t avatar;
    char employee_id[WORKSPACE_EMPLOYEE_ID+1];
    char team[WORKSPACE_TEAM+1];
} workspace_user_t;
typedef struct {
    uint32_t version, next_id, active_id, user_count;
    workspace_user_t users[WORKSPACE_USERS];
} workspace_model_t;
void workspace_defaults(workspace_model_t *model);
bool workspace_name_valid(const char *name);
bool workspace_model_valid(const workspace_model_t *model);
const workspace_user_t *workspace_active(const workspace_model_t *model);
workspace_user_t *workspace_find(workspace_model_t *model, uint32_t id);
bool workspace_add_user(workspace_model_t *model, const char *name, uint32_t *id);
bool workspace_add_profile(workspace_model_t *model, uint32_t user, const workspace_profile_t *profile);
bool workspace_delete_profile(workspace_model_t *model, uint32_t user, uint32_t profile);
/* Next saved slot; never mutate the actual machine state. */
uint8_t workspace_next_batch(const workspace_user_t *user, uint8_t current);
#endif
