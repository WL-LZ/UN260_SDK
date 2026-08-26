#include "currency_state.h"

#include <string.h>

static const char g_currency_default_codes[][4] = {
    CURRENCY_AUTO_CODE,
    "USD", "CNY", "EUR", "AED", "SAR", "OMR", "QAR", "MAD",
    "EGP", "DZD", "INR", "PKR", "GBP", "IQD"
};

static currency_state_snapshot_t g_currency_state = {
    .count = sizeof(g_currency_default_codes) / sizeof(g_currency_default_codes[0]),
    .codes = {
        CURRENCY_AUTO_CODE,
        "USD", "CNY", "EUR", "AED", "SAR", "OMR", "QAR", "MAD",
        "EGP", "DZD", "INR", "PKR", "GBP", "IQD"
    },
    .active_code = "CNY",
    .active_currency = CURR_CNY_ITEM,
    .active_index = 2,
};

/* The protocol list contains real currencies only. AUTO/MIX are UI features. */
static char g_currency_sync_codes[CONTROLLER_MAX_CURRENCIES][4];
static bool g_currency_sync_seen[CONTROLLER_MAX_CURRENCIES];
static uint8_t g_currency_sync_count;
static bool g_currency_sync_active;
static char g_detected_code[4];
static bool g_auto_selected;

static bool currency_state_code_is_valid(const char *code)
{
    return code != NULL &&
           code[0] >= 'A' && code[0] <= 'Z' &&
           code[1] >= 'A' && code[1] <= 'Z' &&
           code[2] >= 'A' && code[2] <= 'Z' &&
           code[3] == '\0';
}

static void currency_state_copy_code(char dst[4], const char *src)
{
    if (dst == NULL) return;
    memset(dst, 0, 4);
    if (src == NULL) return;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

bool currency_state_is_auto_code(const char code[4])
{
    return code != NULL && strncmp(code, CURRENCY_AUTO_CODE, 3) == 0;
}

const char *currency_state_display_code(const char code[4])
{
    return currency_state_is_auto_code(code) ? "AUTO" : code;
}

void currency_state_reset(void)
{
    memset(&g_currency_state, 0, sizeof(g_currency_state));
    g_currency_state.count = sizeof(g_currency_default_codes) /
                             sizeof(g_currency_default_codes[0]);
    memcpy(g_currency_state.codes, g_currency_default_codes,
           sizeof(g_currency_default_codes));
    currency_state_copy_code(g_currency_state.active_code, "CNY");
    g_currency_state.active_currency = CURR_CNY_ITEM;
    g_currency_state.active_index = 2;
    g_auto_selected = false;
    memset(g_detected_code, 0, sizeof(g_detected_code));
    memset(g_currency_sync_codes, 0, sizeof(g_currency_sync_codes));
    memset(g_currency_sync_seen, 0, sizeof(g_currency_sync_seen));
    g_currency_sync_count = 0;
    g_currency_sync_active = false;
}

void currency_state_begin_list_sync(void)
{
    memset(g_currency_sync_codes, 0, sizeof(g_currency_sync_codes));
    memset(g_currency_sync_seen, 0, sizeof(g_currency_sync_seen));
    g_currency_sync_count = 0;
    g_currency_sync_active = true;
}

bool currency_state_append_list_code(uint8_t protocol_index, const char code[4])
{
    uint8_t index;

    if (!g_currency_sync_active || protocol_index == 0 ||
        protocol_index > CONTROLLER_MAX_CURRENCIES ||
        !currency_state_code_is_valid(code) ||
        currency_state_is_auto_code(code)) {
        return false;
    }
    index = (uint8_t)(protocol_index - 1);
    for (uint8_t i = 0; i < g_currency_sync_count; i++) {
        if (i != index && g_currency_sync_seen[i] &&
            strncmp(code, g_currency_sync_codes[i], 3) == 0) {
            return false;
        }
    }
    currency_state_copy_code(g_currency_sync_codes[index], code);
    g_currency_sync_seen[index] = true;
    if (g_currency_sync_count < protocol_index) {
        g_currency_sync_count = protocol_index;
    }
    return true;
}

bool currency_state_finish_list_sync(void)
{
    bool valid = g_currency_sync_active && g_currency_sync_count > 0;

    for (uint8_t i = 0; valid && i < g_currency_sync_count; i++) {
        if (!g_currency_sync_seen[i]) valid = false;
    }

    if (valid) {
        memset(g_currency_state.codes, 0, sizeof(g_currency_state.codes));
        currency_state_copy_code(g_currency_state.codes[0], CURRENCY_AUTO_CODE);
        for (uint8_t i = 0; i < g_currency_sync_count; i++) {
            currency_state_copy_code(g_currency_state.codes[i + 1],
                                     g_currency_sync_codes[i]);
        }
        g_currency_state.count = (uint8_t)(g_currency_sync_count + 1);

        if (g_auto_selected) {
            uint8_t real_index;

            if (!currency_state_find_code(g_currency_state.active_code,
                                          &real_index)) {
                currency_state_copy_code(g_currency_state.active_code,
                                         g_currency_state.codes[1]);
                g_currency_state.active_currency =
                    currency_state_code_to_item(g_currency_state.codes[1]);
            }
            g_currency_state.active_index = 0;
        } else if (!currency_state_find_code(g_currency_state.active_code,
                                             &g_currency_state.active_index)) {
            /* Never retain a currency that the controller no longer supports. */
            g_currency_state.active_index = 1;
            currency_state_copy_code(g_currency_state.active_code,
                                     g_currency_state.codes[1]);
            g_currency_state.active_currency =
                currency_state_code_to_item(g_currency_state.codes[1]);
        }
    }

    memset(g_currency_sync_codes, 0, sizeof(g_currency_sync_codes));
    memset(g_currency_sync_seen, 0, sizeof(g_currency_sync_seen));
    g_currency_sync_count = 0;
    g_currency_sync_active = false;
    return valid;
}

curr_item_t currency_state_code_to_item(const char *code)
{
    if (!code || currency_state_is_auto_code(code)) return CURR_COUNT;
    if (strncmp(code, "CNY", 3) == 0) return CURR_CNY_ITEM;
    if (strncmp(code, "USD", 3) == 0) return CURR_USD_ITEM;
    if (strncmp(code, "EUR", 3) == 0) return CURR_EUR_ITEM;
    if (strncmp(code, "GBP", 3) == 0) return CURR_GBP_ITEM;
    if (strncmp(code, "KRW", 3) == 0) return CURR_KRW_ITEM;
    if (strncmp(code, "EGP", 3) == 0) return CURR_EGP_ITEM;
    if (strncmp(code, "ISK", 3) == 0) return CURR_ISK_ITEM;
    if (strncmp(code, "PHP", 3) == 0) return CURR_PHP_ITEM;
    if (strncmp(code, "SOS", 3) == 0) return CURR_SOS_ITEM;
    if (strncmp(code, "TRY", 3) == 0) return CURR_TRY_ITEM;
    if (strncmp(code, "AED", 3) == 0) return CURR_AED_ITEM;
    if (strncmp(code, "SAR", 3) == 0) return CURR_SAR_ITEM;
    if (strncmp(code, "OMR", 3) == 0) return CURR_OMR_ITEM;
    if (strncmp(code, "QAR", 3) == 0) return CURR_QAR_ITEM;
    if (strncmp(code, "MAD", 3) == 0) return CURR_MAD_ITEM;
    if (strncmp(code, "DZD", 3) == 0) return CURR_DZD_ITEM;
    if (strncmp(code, "INR", 3) == 0) return CURR_INR_ITEM;
    if (strncmp(code, "PKR", 3) == 0) return CURR_PKR_ITEM;
    if (strncmp(code, "IQD", 3) == 0) return CURR_IQD_ITEM;
    return CURR_COUNT;
}

bool currency_state_confirm_active_code(const char *code)
{
    curr_item_t currency;
    uint8_t index;
    bool auto_selected = g_auto_selected;

    if (!currency_state_code_is_valid(code) ||
        currency_state_is_auto_code(code)) {
        return false;
    }
    currency_state_copy_code(g_currency_state.active_code, code);
    memset(g_detected_code, 0, sizeof(g_detected_code));
    if (!auto_selected && currency_state_find_code(code, &index)) {
        g_currency_state.active_index = index;
    }
    g_auto_selected = auto_selected;
    currency = currency_state_code_to_item(code);
    if (currency < CURR_COUNT) g_currency_state.active_currency = currency;
    return true;
}

bool currency_state_confirm_active_selection(uint8_t index, const char code[4])
{
    curr_item_t currency;

    if (index >= g_currency_state.count || index >= MAX_CURRENCIES ||
        !currency_state_code_is_valid(code) ||
        currency_state_is_auto_code(code) ||
        strncmp(code, g_currency_state.codes[index], 3) != 0) {
        return false;
    }

    g_currency_state.active_index = index;
    currency_state_copy_code(g_currency_state.active_code, code);
    g_auto_selected = false;
    memset(g_detected_code, 0, sizeof(g_detected_code));
    currency = currency_state_code_to_item(code);
    if (currency < CURR_COUNT) g_currency_state.active_currency = currency;
    return true;
}

bool currency_state_confirm_auto_selection(void)
{
    uint8_t index;

    if (!currency_state_find_code(CURRENCY_AUTO_CODE, &index)) return false;
    g_currency_state.active_index = index;
    g_auto_selected = true;
    memset(g_detected_code, 0, sizeof(g_detected_code));
    return true;
}

bool currency_state_leave_auto_selection(void)
{
    uint8_t index;

    if (!g_auto_selected) return true;
    if (!currency_state_find_code(g_currency_state.active_code, &index)) {
        if (g_currency_state.count <= 1) return false;
        index = 1;
        currency_state_copy_code(g_currency_state.active_code,
                                 g_currency_state.codes[index]);
    }
    g_auto_selected = false;
    g_currency_state.active_index = index;
    memset(g_detected_code, 0, sizeof(g_detected_code));
    return true;
}

void currency_state_begin_count_session(void)
{
    if (g_auto_selected) {
        memset(g_detected_code, 0, sizeof(g_detected_code));
    }
}

bool currency_state_confirm_detected_code(const char code[4])
{
    uint8_t index;

    if (!g_auto_selected ||
        !currency_state_code_is_valid(code) ||
        !currency_state_find_code(code, &index) || index == 0) {
        return false;
    }
    currency_state_copy_code(g_detected_code, code);
    return true;
}

void currency_state_get_effective_code(char code[4])
{
    if (g_auto_selected && g_detected_code[0] != '\0') {
        currency_state_copy_code(code, g_detected_code);
    } else if (g_auto_selected) {
        currency_state_copy_code(code, CURRENCY_AUTO_CODE);
    } else {
        currency_state_copy_code(code, g_currency_state.active_code);
    }
}

void currency_state_confirm_active_currency(curr_item_t currency)
{
    if (currency >= CURR_COUNT) return;
    g_currency_state.active_currency = currency;
}

void currency_state_confirm_active_index(uint8_t index)
{
    /* Protocol indices refer only to the real-currency list. */
    uint8_t ui_index = (uint8_t)(index + 1);

    if (g_auto_selected) return;
    if (ui_index >= g_currency_state.count) return;
    g_currency_state.active_index = ui_index;
}

void currency_state_get_snapshot(currency_state_snapshot_t *snapshot)
{
    if (!snapshot) return;
    *snapshot = g_currency_state;
}

uint8_t currency_state_count(void)
{
    return g_currency_state.count;
}

bool currency_state_get_code(uint8_t index, char code[4])
{
    if (!code || index >= g_currency_state.count || index >= MAX_CURRENCIES) {
        return false;
    }
    currency_state_copy_code(code, g_currency_state.codes[index]);
    return true;
}

bool currency_state_find_code(const char *code, uint8_t *index)
{
    if (!code) return false;
    for (uint8_t i = 0; i < g_currency_state.count; i++) {
        if (strncmp(code, g_currency_state.codes[i], 3) == 0) {
            if (index) *index = i;
            return true;
        }
    }
    return false;
}

void currency_state_get_active_code(char code[4])
{
    currency_state_copy_code(code, g_currency_state.active_code);
}

void currency_state_get_selected_code(char code[4])
{
    currency_state_copy_code(code,
                             g_auto_selected ? CURRENCY_AUTO_CODE
                                             : g_currency_state.active_code);
}

bool currency_state_auto_selected(void)
{
    return g_auto_selected;
}

curr_item_t currency_state_active_currency(void)
{
    return g_currency_state.active_currency;
}

uint8_t currency_state_active_index(void)
{
    return g_currency_state.active_index;
}
