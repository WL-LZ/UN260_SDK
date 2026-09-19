#ifndef WORK_MODE_SERVICE_H
#define WORK_MODE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

enum { WORK_MODE_AUTO = 0, WORK_MODE_MANUAL = 1 };
typedef enum {
    WORK_MODE_WAITING_SYNC,
    WORK_MODE_SAVING,
    WORK_MODE_WAITING_IDLE,
    WORK_MODE_SWITCHING,
    WORK_MODE_READY,
    WORK_MODE_RESTORING,
    WORK_MODE_FAILED
} work_mode_phase_t;
typedef struct {
    work_mode_phase_t phase;
    bool actual_valid, preferred_valid, diagnostic, pending, restore_pending;
    uint8_t actual, preferred;
} work_mode_snapshot_t;

void work_mode_service_init(void);
void work_mode_service_poll(uint32_t now_ms, bool machine_busy);
void work_mode_service_stop(void);
/* Called on real navigation commits, never off-screen page prewarming.
 * Changing active does not transmit synchronously, so multiple owner changes
 * within one page transition coalesce before the next UI poll. */
void work_mode_service_set_diagnostic(bool active);
bool work_mode_service_diagnostic_active(void);
bool work_mode_service_diagnostic_ready(void);
const char *work_mode_service_status_text(void);
void work_mode_service_get_snapshot(work_mode_snapshot_t *snapshot);
void work_mode_service_retry(void);
enum {
    WORK_MODE_OPERATION_CALIBRATION = 1U << 0,
    WORK_MODE_OPERATION_MOTOR_MAIN = 1U << 1,
    WORK_MODE_OPERATION_MOTOR_STACKER = 1U << 2,
    WORK_MODE_OPERATION_MOTOR_REJECT = 1U << 3,
    WORK_MODE_OPERATION_AGING = 1U << 4
};
/* Hardware action owners must release on confirmed stop/completion/failure,
 * not when the page disappears or a receive timeout expires. */
void work_mode_service_hold_operation(uint32_t owner, bool active);

/* Sole 0x38 command owner. Normal UI requests change preference only on a
 * matching controller reply. Diagnostic mode rejects ordinary AUTO changes. */
bool work_mode_service_request(uint8_t mode);
/* Frame is already CRC checked by the protocol parser. Accepts only 6-byte
 * mode echoes or 7-byte 02+mode controller synchronization frames. */
bool work_mode_service_handle_reply(const uint8_t *frame, uint8_t length);
bool work_mode_service_take_failure(void);

#endif
