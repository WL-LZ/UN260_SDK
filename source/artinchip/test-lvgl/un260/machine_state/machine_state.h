#ifndef __MACHINE_STATE_H__
#define __MACHINE_STATE_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t mode;
    uint8_t speed;
    bool add_enabled;
    uint8_t fo_mode;
    uint8_t work_mode;
    uint8_t cfd_mode;
    bool buzzer_enabled;
    bool batch_enabled;
    uint8_t batch_num;
    uint8_t batch_mode;
    uint32_t batch_amount;
    bool aging_running;
    uint8_t double_note_level;
    uint8_t flap_position;
    uint8_t reject_pocket_max;
} machine_state_snapshot_t;

void machine_state_get_snapshot(machine_state_snapshot_t *snapshot);

void machine_state_confirm_mode(uint8_t mode);
uint8_t machine_state_mode(void);

void machine_state_confirm_buzzer(bool enabled);
bool machine_state_buzzer_enabled(void);

void machine_state_confirm_add(bool enabled);
bool machine_state_add_enabled(void);

void machine_state_confirm_fo_mode(uint8_t mode);
uint8_t machine_state_fo_mode(void);

void machine_state_confirm_speed(uint8_t speed);
uint8_t machine_state_speed(void);

void machine_state_confirm_work_mode(uint8_t mode);
uint8_t machine_state_work_mode(void);

void machine_state_confirm_cfd_mode(uint8_t mode);
uint8_t machine_state_cfd_mode(void);

void machine_state_confirm_batch(bool enabled, uint8_t num);
void machine_state_sync_batch_num(uint8_t num);
void machine_state_confirm_batch_enable(bool enabled);
bool machine_state_batch_enabled(void);
uint8_t machine_state_batch_num(void);
void machine_state_confirm_batch_amount(uint32_t amount);
uint32_t machine_state_batch_amount(void);
void machine_state_confirm_batch_mode(uint8_t mode);
uint8_t machine_state_batch_mode(void);

void machine_state_confirm_aging_running(bool running);
bool machine_state_aging_running(void);

void machine_state_confirm_double_note_level(uint8_t level);
uint8_t machine_state_double_note_level(void);

void machine_state_confirm_flap_position(uint8_t position);
uint8_t machine_state_flap_position(void);

void machine_state_confirm_reject_pocket_max(uint8_t capacity);
uint8_t machine_state_reject_pocket_max(void);

/* Returns NULL when the controller start-error code is unknown. */
const char *machine_start_error_desc(uint8_t code);

#endif
