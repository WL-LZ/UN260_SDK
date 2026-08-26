#include "machine_state.h"
#include "un260/lv_system/user_cfg.h"

#include <stddef.h>

static machine_state_snapshot_t g_machine_state = {
    .mode = MODE_MDC,
    .speed = 0,
    .add_enabled = false,
    .fo_mode = 0,
    .work_mode = 0,
    .cfd_mode = 0,
    .buzzer_enabled = true,
    .batch_enabled = true,
    .batch_num = 0,
    .batch_mode = PCS_BATCH_MODE,
    .batch_amount = 0,
    .aging_running = false,
    .double_note_level = DOUBLE_NOTE_LEVEL_MIN,
    .flap_position = FLAP_POSITION_UP,
    .reject_pocket_max = REJECT_POCKET_MIN_CAPACITY,
};

static const char *const g_machine_start_error_desc[] = {
    [0x00] = "No Error",
    [0x01] = "Upper Channel Error",
    [0x02] = "Lower Channel Error",
    [0x03] = "Reject Exit Error",
    [0x04] = "Reject Pocket Error",
    [0x05] = "Reject Pocket Full Only",
    [0x06] = "Stacker Pocket Error",
    [0x07] = "Stacker Pocket Full Only",
    [0x08] = "Stacker and Reject Pockets Full",
    [0x09] = "Upper and Lower Channels Not Closed",
    [0x0A] = "Genuine Note Exit Error",
    [0x0B] = "Dust Cover / Baffle Closure Error",
    [0x0C] = "Flap Error",
    [0x0D] = "Encoder Disk Error",
};

void machine_state_get_snapshot(machine_state_snapshot_t *snapshot)
{
    if (snapshot != NULL) *snapshot = g_machine_state;
}

void machine_state_confirm_mode(uint8_t mode)
{
    g_machine_state.mode = mode;
}

uint8_t machine_state_mode(void)
{
    return g_machine_state.mode;
}

void machine_state_confirm_buzzer(bool enabled)
{
    g_machine_state.buzzer_enabled = enabled;
}

bool machine_state_buzzer_enabled(void)
{
    return g_machine_state.buzzer_enabled;
}

void machine_state_confirm_add(bool enabled)
{
    g_machine_state.add_enabled = enabled;
}

bool machine_state_add_enabled(void)
{
    return g_machine_state.add_enabled;
}

void machine_state_confirm_fo_mode(uint8_t mode)
{
    g_machine_state.fo_mode = mode;
}

uint8_t machine_state_fo_mode(void)
{
    return g_machine_state.fo_mode;
}

void machine_state_confirm_speed(uint8_t speed)
{
    g_machine_state.speed = speed;
}

uint8_t machine_state_speed(void)
{
    return g_machine_state.speed;
}

void machine_state_confirm_work_mode(uint8_t mode)
{
    g_machine_state.work_mode = mode;
}

uint8_t machine_state_work_mode(void)
{
    return g_machine_state.work_mode;
}

void machine_state_confirm_cfd_mode(uint8_t mode)
{
    g_machine_state.cfd_mode = mode;
}

uint8_t machine_state_cfd_mode(void)
{
    return g_machine_state.cfd_mode;
}

void machine_state_confirm_batch(bool enabled, uint8_t num)
{
    g_machine_state.batch_enabled = enabled;
    g_machine_state.batch_num = num;
}

void machine_state_sync_batch_num(uint8_t num)
{
    g_machine_state.batch_num = num;
}

void machine_state_confirm_batch_enable(bool enabled)
{
    g_machine_state.batch_enabled = enabled;
}

bool machine_state_batch_enabled(void)
{
    return g_machine_state.batch_enabled;
}

uint8_t machine_state_batch_num(void)
{
    return g_machine_state.batch_num;
}

void machine_state_confirm_batch_amount(uint32_t amount)
{
    g_machine_state.batch_amount = amount;
}

uint32_t machine_state_batch_amount(void)
{
    return g_machine_state.batch_amount;
}

void machine_state_confirm_batch_mode(uint8_t mode)
{
    if (mode == PCS_BATCH_MODE || mode == AMOUNT_BATCH_MODE) {
        g_machine_state.batch_mode = mode;
    }
}

uint8_t machine_state_batch_mode(void)
{
    return g_machine_state.batch_mode;
}

void machine_state_confirm_aging_running(bool running)
{
    g_machine_state.aging_running = running;
}

bool machine_state_aging_running(void)
{
    return g_machine_state.aging_running;
}

void machine_state_confirm_double_note_level(uint8_t level)
{
    g_machine_state.double_note_level = level;
}

uint8_t machine_state_double_note_level(void)
{
    return g_machine_state.double_note_level;
}

void machine_state_confirm_flap_position(uint8_t position)
{
    g_machine_state.flap_position = position;
}

uint8_t machine_state_flap_position(void)
{
    return g_machine_state.flap_position;
}

void machine_state_confirm_reject_pocket_max(uint8_t capacity)
{
    if (capacity < REJECT_POCKET_MIN_CAPACITY) {
        capacity = REJECT_POCKET_MIN_CAPACITY;
    } else if (capacity > REJECT_POCKET_MAX_CAPACITY) {
        capacity = REJECT_POCKET_MAX_CAPACITY;
    }
    g_machine_state.reject_pocket_max = capacity;
}

uint8_t machine_state_reject_pocket_max(void)
{
    return g_machine_state.reject_pocket_max;
}

const char *machine_start_error_desc(uint8_t code)
{
    if (code >= sizeof(g_machine_start_error_desc) /
                sizeof(g_machine_start_error_desc[0])) {
        return NULL;
    }
    return g_machine_start_error_desc[code];
}
