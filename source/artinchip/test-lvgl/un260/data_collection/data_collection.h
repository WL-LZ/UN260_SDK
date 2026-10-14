#include "un260/lv_system/ui_message.h"
#ifndef DATA_COLLECTION_H
#define DATA_COLLECTION_H

#include <stdbool.h>
#include <stdint.h>

#define DATA_COLLECTION_REQUEST_TIMEOUT_MS 3000U

typedef enum {
    DATA_COLLECT_MODE_NONE = 0,
    DATA_COLLECT_MODE_ALL = 0x01,
    DATA_COLLECT_MODE_FALSE = 0x02,
} data_collect_mode_t;

typedef enum {
    DATA_COLLECTION_REPLY_INVALID = 0,
    DATA_COLLECTION_REPLY_STATUS_UPDATED,
    DATA_COLLECTION_REPLY_REQUEST_CONFIRMED,
    DATA_COLLECTION_REPLY_REQUEST_FAILED,
    DATA_COLLECTION_REPLY_EXITED,
    DATA_COLLECTION_REPLY_IGNORED,
    DATA_COLLECTION_REPLY_UNKNOWN,
} data_collection_reply_result_t;

typedef enum {
 DATA_COLLECTION_OFF,DATA_COLLECTION_READY,DATA_COLLECTION_OPENING,DATA_COLLECTION_OPEN_UNKNOWN,
 DATA_COLLECTION_COUNTING,DATA_COLLECTION_COLLECTING,DATA_COLLECTION_WAITING,DATA_COLLECTION_SUCCESS,
 DATA_COLLECTION_CLOSING,DATA_COLLECTION_CLOSE_UNKNOWN,DATA_COLLECTION_ERROR
} data_collection_phase_t;
typedef struct {
 data_collect_mode_t mode,target;
 data_collection_phase_t phase;
 bool pending,uncertain,counting;
 uint16_t pcs;
 uint32_t completed;
} data_collection_snapshot_t;
void data_collection_snapshot(data_collection_snapshot_t *out);
void data_collection_count_start(void);
void data_collection_count_end(uint16_t pcs);
data_collect_mode_t data_collection_state_mode(void);
uint16_t data_collection_state_pcs(void);
const char *data_collection_state_status(void);
void data_collection_state_select_mode(data_collect_mode_t mode, const char *status);
void data_collection_state_reset_pcs(void);
void data_collection_state_set_status(const char *status);
void data_collection_state_set_message(const ui_message_t *message);
void data_collection_state_exit(const char *status);

bool data_collection_request_begin(data_collect_mode_t target_mode,
                                   const char *status,
                                   uint32_t now_ms);
void data_collection_request_cancel(void);
bool data_collection_request_pending(void);
bool data_collection_request_take_timeout(uint32_t now_ms);

data_collection_reply_result_t data_collection_reply_handle(
    const uint8_t *buf, uint8_t len, uint32_t now_ms);

#endif
