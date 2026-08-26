#include "un260/app_service/setting_service.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/mode_codec.h"
#include "un260/protocol/protocol_request.h"
#include <stddef.h>
#include <stdio.h>

#ifndef SETTING_BATCH_TRACE_ENABLE
#define SETTING_BATCH_TRACE_ENABLE 0
#endif

#define SETTING_REQUEST_TIMEOUT_MS 800U

typedef struct {
    protocol_request_t request;
    uint8_t target;
} setting_basic_request_slot_t;

#define SETTING_BASIC_REQUEST_SLOT_INITIALIZER \
    { PROTOCOL_REQUEST_INITIALIZER(SETTING_REQUEST_TIMEOUT_MS), 0 }

static setting_basic_request_slot_t g_mode_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;
static setting_basic_request_slot_t g_add_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;
static setting_basic_request_slot_t g_fo_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;
static setting_basic_request_slot_t g_speed_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;
static setting_basic_request_slot_t g_work_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;
static setting_basic_request_slot_t g_beep_request =
    SETTING_BASIC_REQUEST_SLOT_INITIALIZER;

static bool setting_basic_request_begin(setting_basic_request_slot_t *slot,
                                        uint8_t cmd_g,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        uint8_t target)
{
    if (slot == NULL || !protocol_request_begin(&slot->request)) return false;

    slot->target = target;
    if (protocol_send(cmd_g, payload, payload_len) < 0) {
        protocol_request_finish(&slot->request);
        return false;
    }

    return true;
}

static bool setting_basic_request_is_pending(const setting_basic_request_slot_t *slot)
{
    return slot != NULL && protocol_request_is_pending(&slot->request);
}

static bool setting_basic_request_take_result(setting_basic_request_slot_t *slot,
                                              uint8_t *target)
{
    if (slot == NULL || !protocol_request_take_result(&slot->request)) {
        return false;
    }

    if (target != NULL) *target = slot->target;
    return true;
}

static void setting_basic_request_cancel(setting_basic_request_slot_t *slot)
{
    if (slot != NULL) protocol_request_finish(&slot->request);
}

static bool setting_basic_request_take_timeout(setting_basic_request_slot_t *slot)
{
    return slot != NULL && protocol_request_take_timeout(&slot->request);
}

typedef struct {
    protocol_request_t request;
    setting_batch_request_type_t type;
    setting_batch_snapshot_t target;
    setting_batch_snapshot_t previous;
#if SETTING_BATCH_TRACE_ENABLE
    uint32_t trace_next_seq;
    uint32_t trace_active_seq;
    uint64_t trace_tx_ms;
#endif
} setting_batch_request_slot_t;

static setting_batch_request_slot_t g_batch_request = {
    .request = PROTOCOL_REQUEST_INITIALIZER(SETTING_REQUEST_TIMEOUT_MS),
    .type = SETTING_BATCH_REQUEST_NONE,
    .target = { false, 0 },
    .previous = { false, 0 },
};

typedef struct {
    protocol_request_t request;
    uint8_t target;
    uint8_t previous;
} setting_value_request_slot_t;

#define SETTING_VALUE_REQUEST_SLOT_INITIALIZER \
    { PROTOCOL_REQUEST_INITIALIZER(SETTING_REQUEST_TIMEOUT_MS), 0, 0 }

static setting_value_request_slot_t g_double_note_request =
    SETTING_VALUE_REQUEST_SLOT_INITIALIZER;
static setting_value_request_slot_t g_flap_request =
    SETTING_VALUE_REQUEST_SLOT_INITIALIZER;
static setting_value_request_slot_t g_reject_pocket_request =
    SETTING_VALUE_REQUEST_SLOT_INITIALIZER;

typedef struct {
    protocol_request_t request;
} setting_action_request_slot_t;

#define SETTING_ACTION_REQUEST_SLOT_INITIALIZER \
    { PROTOCOL_REQUEST_INITIALIZER(SETTING_REQUEST_TIMEOUT_MS) }

static setting_action_request_slot_t g_aging_request =
    SETTING_ACTION_REQUEST_SLOT_INITIALIZER;
static setting_action_request_slot_t g_factory_request =
    SETTING_ACTION_REQUEST_SLOT_INITIALIZER;

static bool setting_action_request_begin(setting_action_request_slot_t *slot,
                                         uint8_t cmd_g)
{
    const uint8_t payload = 0x01;

    if (slot == NULL || !protocol_request_begin(&slot->request)) return false;
    if (protocol_send(cmd_g, &payload, 1) < 0) {
        protocol_request_finish(&slot->request);
        return false;
    }
    return true;
}

static bool setting_action_request_take_result(setting_action_request_slot_t *slot,
                                               uint8_t status,
                                               uint8_t success_status,
                                               uint8_t failure_status,
                                               setting_action_result_t *result)
{
    if (slot == NULL || result == NULL ||
        (status != success_status && status != failure_status) ||
        !protocol_request_take_result(&slot->request)) {
        return false;
    }

    result->success = status == success_status;
    result->timeout = false;
    return true;
}

static bool setting_action_request_take_timeout(setting_action_request_slot_t *slot,
                                                setting_action_result_t *result)
{
    if (slot == NULL || result == NULL ||
        !protocol_request_take_timeout(&slot->request)) {
        return false;
    }

    result->success = false;
    result->timeout = true;
    return true;
}

static bool setting_value_request_begin(setting_value_request_slot_t *slot,
                                        uint8_t cmd_g,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        uint8_t target,
                                        uint8_t previous)
{
    if (slot == NULL || !protocol_request_begin(&slot->request)) return false;

    slot->target = target;
    slot->previous = previous;
    if (protocol_send(cmd_g, payload, payload_len) < 0) {
        protocol_request_finish(&slot->request);
        return false;
    }

    return true;
}

static bool setting_value_request_take_result(setting_value_request_slot_t *slot,
                                              uint8_t status,
                                              uint8_t success_status,
                                              uint8_t failure_status,
                                              setting_value_result_t *result)
{
    if (slot == NULL || result == NULL) return false;
    if (status != success_status && status != failure_status) return false;
    if (!protocol_request_take_result(&slot->request)) return false;

    result->target = slot->target;
    result->previous = slot->previous;
    result->success = (status == success_status);
    result->timeout = false;
    return true;
}

static bool setting_value_request_take_timeout(setting_value_request_slot_t *slot,
                                               setting_value_result_t *result)
{
    if (slot == NULL || result == NULL ||
        !protocol_request_take_timeout(&slot->request)) {
        return false;
    }

    result->target = slot->target;
    result->previous = slot->previous;
    result->success = false;
    result->timeout = true;
    return true;
}

static void setting_value_request_clear(setting_value_request_slot_t *slot)
{
    if (slot != NULL) protocol_request_finish(&slot->request);
}

#if SETTING_BATCH_TRACE_ENABLE
static const char *setting_batch_trace_source(setting_batch_request_type_t type)
{
    if (type == SETTING_BATCH_REQUEST_NUMBER) return "NUMBER";
    if (type == SETTING_BATCH_REQUEST_SWITCH) return "SWITCH";
    return "NONE";
}

static const char *setting_batch_trace_status(uint8_t status)
{
    return status == 0x01 ? "SUCCESS" : "FAIL";
}

static void setting_batch_trace_tx(void)
{
    g_batch_request.trace_active_seq = ++g_batch_request.trace_next_seq;
    g_batch_request.trace_tx_ms = protocol_request_now_ms();
    printf("[BATCH_TRACE] TX seq=%u time_ms=%llu source=%s payload=%u target_enable=%u target_num=%u previous_enable=%u previous_num=%u\n",
           g_batch_request.trace_active_seq,
           (unsigned long long)g_batch_request.trace_tx_ms,
           setting_batch_trace_source(g_batch_request.type),
           (unsigned)g_batch_request.target.num,
           (unsigned)g_batch_request.target.enable,
           (unsigned)g_batch_request.target.num,
           (unsigned)g_batch_request.previous.enable,
           (unsigned)g_batch_request.previous.num);
}

static void setting_batch_trace_reject(setting_batch_request_type_t source, uint8_t payload)
{
    uint64_t now_ms = protocol_request_now_ms();

    printf("[BATCH_TRACE] REJECT time_ms=%llu source=%s payload=%u active_seq=%u active_source=%s active_target_enable=%u active_target_num=%u\n",
           (unsigned long long)now_ms, setting_batch_trace_source(source), (unsigned)payload,
           g_batch_request.trace_active_seq,
           setting_batch_trace_source(g_batch_request.type),
           (unsigned)g_batch_request.target.enable,
           (unsigned)g_batch_request.target.num);
}

static void setting_batch_trace_rx(uint8_t status)
{
    uint64_t now_ms = protocol_request_now_ms();
    uint64_t latency_ms = now_ms - g_batch_request.trace_tx_ms;

    printf("[BATCH_TRACE] RX seq=%u time_ms=%llu latency_ms=%llu status=%s source=%s payload=%u target_enable=%u target_num=%u previous_enable=%u previous_num=%u\n",
           g_batch_request.trace_active_seq, (unsigned long long)now_ms,
           (unsigned long long)latency_ms,
           setting_batch_trace_status(status),
           setting_batch_trace_source(g_batch_request.type),
           (unsigned)g_batch_request.target.num,
           (unsigned)g_batch_request.target.enable,
           (unsigned)g_batch_request.target.num,
           (unsigned)g_batch_request.previous.enable,
           (unsigned)g_batch_request.previous.num);
}
#endif

static bool setting_batch_request_begin(setting_batch_request_type_t type,
                                        bool target_enable,
                                        uint8_t sent_num,
                                        bool previous_enable,
                                        uint8_t previous_num)
{
    if (protocol_request_is_pending(&g_batch_request.request)) {
#if SETTING_BATCH_TRACE_ENABLE
        setting_batch_trace_reject(type, sent_num);
#endif
        return false;
    }

    g_batch_request.type = type;
    g_batch_request.target.enable = target_enable;
    g_batch_request.target.num = sent_num;
    g_batch_request.previous.enable = previous_enable;
    g_batch_request.previous.num = previous_num;
    if (!protocol_request_begin(&g_batch_request.request)) return false;

#if SETTING_BATCH_TRACE_ENABLE
    setting_batch_trace_tx();
#endif
    if (protocol_send(0x06, &sent_num, 1) < 0) {
        protocol_request_finish(&g_batch_request.request);
        g_batch_request.type = SETTING_BATCH_REQUEST_NONE;
        return false;
    }

    return true;
}

bool setting_service_request_mode(uint8_t target)
{
    uint8_t protocol_mode;

    if (!mode_codec_encode(target, &protocol_mode)) return false;
    return setting_basic_request_begin(&g_mode_request, 0x04,
                                       &protocol_mode, 1, target);
}

bool setting_service_request_auto_currency(void)
{
    const uint8_t protocol_mode = 0x01;

    return setting_basic_request_begin(&g_mode_request, 0x04,
                                       &protocol_mode, 1,
                                       SETTING_MODE_TARGET_AUTO_CURRENCY);
}

bool setting_service_mode_is_pending(void)
{
    return setting_basic_request_is_pending(&g_mode_request);
}

bool setting_service_take_mode_result(uint8_t *target)
{
    return setting_basic_request_take_result(&g_mode_request, target);
}

void setting_service_cancel_mode_request(void)
{
    setting_basic_request_cancel(&g_mode_request);
}

bool setting_service_request_add(bool target)
{
    uint8_t add_cmd;

    add_cmd = target ? 0x01 : 0x00;
    return setting_basic_request_begin(&g_add_request, 0x39,
                                       &add_cmd, 1, target ? 1 : 0);
}

bool setting_service_take_add_result(bool *target)
{
    uint8_t raw_target;

    if (!setting_basic_request_take_result(&g_add_request,
                                           target != NULL ? &raw_target : NULL)) {
        return false;
    }

    if (target != NULL) *target = raw_target != 0;
    return true;
}

bool setting_service_request_fo_mode(uint8_t target)
{
    uint8_t fo_cmd;

    fo_cmd = target;
    return setting_basic_request_begin(&g_fo_request, 0x3a,
                                       &fo_cmd, 1, target);
}

bool setting_service_take_fo_mode_result(uint8_t *target)
{
    return setting_basic_request_take_result(&g_fo_request, target);
}

bool setting_service_request_speed(uint8_t target)
{
    uint8_t speed_cmd = 0x01;

    if (target == 0) {
        speed_cmd = 0x03;
    } else if (target == 1) {
        speed_cmd = 0x02;
    } else {
        speed_cmd = 0x01;
    }
    return setting_basic_request_begin(&g_speed_request, 0x16,
                                       &speed_cmd, 1, target);
}

bool setting_service_take_speed_result(uint8_t *target)
{
    return setting_basic_request_take_result(&g_speed_request, target);
}

bool setting_service_request_work_mode(uint8_t target)
{
    uint8_t work_cmd;

    work_cmd = (target == 1) ? 0x00 : 0x01;
    return setting_basic_request_begin(&g_work_request, 0x38,
                                       &work_cmd, 1, target);
}

bool setting_service_take_work_mode_result(uint8_t *target)
{
    return setting_basic_request_take_result(&g_work_request, target);
}

bool setting_service_request_beep(bool target)
{
    uint8_t beep_cmd;

    beep_cmd = target ? 0x01 : 0x02;
    return setting_basic_request_begin(&g_beep_request, 0x15,
                                       &beep_cmd, 1, target ? 1 : 0);
}

bool setting_service_take_beep_result(bool *target)
{
    uint8_t raw_target;

    if (!setting_basic_request_take_result(&g_beep_request,
                                           target != NULL ? &raw_target : NULL)) {
        return false;
    }

    if (target != NULL) *target = raw_target != 0;
    return true;
}

uint32_t setting_service_take_basic_timeouts(void)
{
    uint32_t timeouts = SETTING_REQUEST_TIMEOUT_NONE;

    if (setting_basic_request_take_timeout(&g_mode_request)) timeouts |= SETTING_REQUEST_TIMEOUT_MODE;
    if (setting_basic_request_take_timeout(&g_add_request)) timeouts |= SETTING_REQUEST_TIMEOUT_ADD;
    if (setting_basic_request_take_timeout(&g_fo_request)) timeouts |= SETTING_REQUEST_TIMEOUT_FO_MODE;
    if (setting_basic_request_take_timeout(&g_speed_request)) timeouts |= SETTING_REQUEST_TIMEOUT_SPEED;
    if (setting_basic_request_take_timeout(&g_work_request)) timeouts |= SETTING_REQUEST_TIMEOUT_WORK_MODE;
    if (setting_basic_request_take_timeout(&g_beep_request)) timeouts |= SETTING_REQUEST_TIMEOUT_BEEP;
    return timeouts;
}

bool setting_service_request_batch_number(uint8_t num, bool previous_enable, uint8_t previous_num)
{
    return setting_batch_request_begin(SETTING_BATCH_REQUEST_NUMBER,
                                       true, num,
                                       previous_enable, previous_num);
}

bool setting_service_request_batch_switch(bool target_enable, uint8_t sent_num, bool previous_enable, uint8_t previous_num)
{
    return setting_batch_request_begin(SETTING_BATCH_REQUEST_SWITCH,
                                       target_enable, sent_num,
                                       previous_enable, previous_num);
}

bool setting_service_batch_take_result(uint8_t status, setting_batch_result_t *result)
{
    if (status != 0x01 && status != 0x02) {
#if SETTING_BATCH_TRACE_ENABLE
        if (protocol_request_can_take_result(&g_batch_request.request)) {
            printf("[BATCH_TRACE] RX_INVALID time_ms=%llu status=%u pending=1\n",
                   (unsigned long long)protocol_request_now_ms(), (unsigned)status);
        }
#endif
        return false;
    }
    if (result == NULL) {
#if SETTING_BATCH_TRACE_ENABLE
        if (protocol_request_can_take_result(&g_batch_request.request)) {
            printf("[BATCH_TRACE] RX_RESULT_NULL time_ms=%llu status=%s pending=1 active_seq=%u\n",
                   (unsigned long long)protocol_request_now_ms(), setting_batch_trace_status(status),
                   g_batch_request.trace_active_seq);
        }
#endif
        return false;
    }
    if (!protocol_request_take_result(&g_batch_request.request)) {
#if SETTING_BATCH_TRACE_ENABLE
        printf("[BATCH_TRACE] RX_UNMATCHED time_ms=%llu status=%s\n",
               (unsigned long long)protocol_request_now_ms(), setting_batch_trace_status(status));
#endif
        return false;
    }

#if SETTING_BATCH_TRACE_ENABLE
    setting_batch_trace_rx(status);
#endif
    result->type = g_batch_request.type;
    result->target = g_batch_request.target;
    result->previous = g_batch_request.previous;
    g_batch_request.type = SETTING_BATCH_REQUEST_NONE;
    return true;
}

bool setting_service_batch_take_timeout(setting_batch_result_t *result)
{
    if (!result ||
        !protocol_request_take_timeout(&g_batch_request.request)) return false;

    result->type = g_batch_request.type;
    result->target = g_batch_request.target;
    result->previous = g_batch_request.previous;
    g_batch_request.type = SETTING_BATCH_REQUEST_NONE;
    return true;
}

bool setting_service_request_double_note_level(uint8_t target, uint8_t previous)
{
    return setting_value_request_begin(&g_double_note_request, 0x31,
                                       &target, 1, target, previous);
}

bool setting_service_take_double_note_level_result(uint8_t response_level,
                                                   uint8_t status,
                                                   setting_value_result_t *result)
{
    if (response_level != g_double_note_request.target) return false;
    return setting_value_request_take_result(&g_double_note_request, status,
                                             0x01, 0x02, result);
}

bool setting_service_take_double_note_level_timeout(setting_value_result_t *result)
{
    return setting_value_request_take_timeout(&g_double_note_request, result);
}

void setting_service_clear_double_note_level_request(void)
{
    setting_value_request_clear(&g_double_note_request);
}

bool setting_service_request_flap_position(uint8_t target, uint8_t previous)
{
    return setting_value_request_begin(&g_flap_request, 0x42,
                                       &target, 1, target, previous);
}

bool setting_service_take_flap_position_result(uint8_t status, setting_value_result_t *result)
{
    return setting_value_request_take_result(&g_flap_request, status,
                                             0x00, 0x01, result);
}

bool setting_service_take_flap_position_timeout(setting_value_result_t *result)
{
    return setting_value_request_take_timeout(&g_flap_request, result);
}

bool setting_service_request_reject_pocket_max(uint8_t target, uint8_t previous)
{
    uint8_t payload[2] = { 0x01, target };

    return setting_value_request_begin(&g_reject_pocket_request, 0x08,
                                       payload, sizeof(payload),
                                       target, previous);
}

bool setting_service_take_reject_pocket_max_result(uint8_t status, setting_value_result_t *result)
{
    return setting_value_request_take_result(&g_reject_pocket_request, status,
                                             0x01, 0x02, result);
}

bool setting_service_take_reject_pocket_max_timeout(setting_value_result_t *result)
{
    return setting_value_request_take_timeout(&g_reject_pocket_request, result);
}

void setting_service_clear_reject_pocket_max_request(void)
{
    setting_value_request_clear(&g_reject_pocket_request);
}

bool setting_service_request_aging_start(void)
{
    return setting_action_request_begin(&g_aging_request, 0x46);
}

bool setting_service_take_aging_result(uint8_t status,
                                       setting_action_result_t *result)
{
    return setting_action_request_take_result(&g_aging_request, status,
                                              0x00, 0x01, result);
}

bool setting_service_take_aging_timeout(setting_action_result_t *result)
{
    return setting_action_request_take_timeout(&g_aging_request, result);
}

bool setting_service_request_factory_reset(void)
{
    return setting_action_request_begin(&g_factory_request, 0x44);
}

bool setting_service_take_factory_result(uint8_t status,
                                         setting_action_result_t *result)
{
    return setting_action_request_take_result(&g_factory_request, status,
                                              0x01, 0x02, result);
}

bool setting_service_take_factory_timeout(setting_action_result_t *result)
{
    return setting_action_request_take_timeout(&g_factory_request, result);
}

void setting_service_cancel_all(void)
{
    setting_basic_request_cancel(&g_mode_request);
    setting_basic_request_cancel(&g_add_request);
    setting_basic_request_cancel(&g_fo_request);
    setting_basic_request_cancel(&g_speed_request);
    setting_basic_request_cancel(&g_work_request);
    setting_basic_request_cancel(&g_beep_request);

    protocol_request_finish(&g_batch_request.request);
    g_batch_request.type = SETTING_BATCH_REQUEST_NONE;

    setting_value_request_clear(&g_double_note_request);
    setting_value_request_clear(&g_flap_request);
    setting_value_request_clear(&g_reject_pocket_request);
    protocol_request_finish(&g_aging_request.request);
    protocol_request_finish(&g_factory_request.request);
}
