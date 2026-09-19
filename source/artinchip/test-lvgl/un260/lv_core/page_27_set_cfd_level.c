#include "page_27_set_cfd_level.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/currency/currency_state.h"
#include "un260/cfd/cfd.h"
#include "un260/gesture/gesture_service.h"
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *profiles[CFD_SCENE_COUNT], *cells[CFD_SCENE_COUNT][CFD_ITEM_COUNT];
static lv_obj_t *values[CFD_SCENE_COUNT][CFD_ITEM_COUNT], *currency_label, *save_button, *retry_button;
static cfd_state_value_t original, draft;
static uint8_t selected_scene, original_scene;
static bool ready, saving, leave_home;
static const ui_text_id_t profile_names[] = {
    UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM1, UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM2,
    UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM3
};
static const char *channel_names[] = { "UV", "MG", "MT", "IR" };

static bool dirty(void)
{
    return ready && (selected_scene != original_scene ||
        memcmp(draft.levels, original.levels, sizeof(draft.levels)) != 0);
}

static void refresh(void)
{
    if (!frame.root) return;
    bool busy = cfd_service_busy();
    for (unsigned scene = 0; scene < CFD_SCENE_COUNT; ++scene) {
        if (scene == selected_scene) lv_obj_add_state(profiles[scene], LV_STATE_CHECKED);
        else lv_obj_clear_state(profiles[scene], LV_STATE_CHECKED);
        if (!ready || busy) lv_obj_add_state(profiles[scene], LV_STATE_DISABLED);
        else lv_obj_clear_state(profiles[scene], LV_STATE_DISABLED);
        for (unsigned item = 0; item < CFD_ITEM_COUNT; ++item) {
            if (ready) lv_label_set_text_fmt(values[scene][item], "%u", draft.levels[scene][item]);
            else lv_label_set_text(values[scene][item], "--");
            if (scene == selected_scene) lv_obj_add_state(cells[scene][item], LV_STATE_CHECKED);
            else lv_obj_clear_state(cells[scene][item], LV_STATE_CHECKED);
            if (!ready || busy) lv_obj_add_state(cells[scene][item], LV_STATE_DISABLED);
            else lv_obj_clear_state(cells[scene][item], LV_STATE_DISABLED);
        }
    }
    if (!dirty() || busy) lv_obj_add_state(save_button, LV_STATE_DISABLED);
    else lv_obj_clear_state(save_button, LV_STATE_DISABLED);
    if (saving) lv_obj_add_state(frame.back, LV_STATE_DISABLED);
    else lv_obj_clear_state(frame.back, LV_STATE_DISABLED);
    if (ready || busy) lv_obj_add_flag(retry_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(retry_button, LV_OBJ_FLAG_HIDDEN);
}

static void query(void)
{
    char code[4];
    currency_state_get_active_code(code);
    ready = false;
    lv_label_set_text_fmt(currency_label, "%s / Profiles", code);
    bool sent = cfd_service_request_query(code);
    lv_label_set_text(frame.message, sent ? "Reading levels from controller..." :
        "Could not read levels. Retry to enable editing.");
    refresh();
}

static void retry(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && !cfd_service_busy()) query();
}

static void leave(void *user_data)
{
    (void)user_data;
    if (leave_home) { ui_manager_clear_stack(); ui_manager_switch(UI_PAGE_MAIN); }
    else ui_manager_pop_page();
}

static void ask_leave(bool home)
{
    if (saving) return;
    leave_home = home;
    if (dirty()) settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,
        "Discard changes?", "Your levels have not been applied.",
        "Discard", "Keep editing", leave, NULL, NULL);
    else leave(NULL);
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) ask_leave(false);
}

static bool gesture(gesture_action_t action)
{
    if (!frame.root || !lv_obj_is_visible(frame.root)) return false;
    if (saving || settings_detail_overlay_is_open()) return true;
    if (action == GESTURE_ACTION_HOME && dirty()) { ask_leave(true); return true; }
    return false;
}

static void profile(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !ready || cfd_service_busy()) return;
    selected_scene = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    refresh();
    lv_label_set_text(frame.message, dirty() ? "Unsaved changes." : "Select a profile; tap its channel level to cycle 1-5.");
}

static void cell(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !ready || cfd_service_busy()) return;
    unsigned key = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    unsigned scene = key / CFD_ITEM_COUNT, item = key % CFD_ITEM_COUNT;
    if (scene >= CFD_SCENE_COUNT) return;
    if (selected_scene != scene) selected_scene = scene;
    else {
        uint8_t level = draft.levels[scene][item];
        draft.levels[scene][item] = level >= CFD_LEVEL_MAX ? CFD_LEVEL_MIN : level + 1;
    }
    refresh();
    lv_label_set_text(frame.message, dirty() ? "Unsaved changes." : "Select a profile; tap its channel level to cycle 1-5.");
}

static void save(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !dirty() || cfd_service_busy()) return;
    if (!cfd_service_request_update(&draft, selected_scene)) {
        lv_label_set_text(frame.message, "Could not send levels. Your changes are kept.");
        return;
    }
    saving = true;
    lv_label_set_text(frame.message, "Applying levels - waiting for controller.");
    refresh();
}

void ui_page_27_set_cfd_level_create(lv_obj_t *parent)
{
    if (frame.root) return;
    ready = false;
    leave_home = false;
    selected_scene = original_scene = 0;
    cfd_state_get(&original);
    draft = original;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_TITLE), .icon = "ShieldCheck", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    currency_label = lv_settings_label(frame.body, "", 0, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    for (unsigned item = 0; item < CFD_ITEM_COUNT; ++item) {
        lv_obj_t *label = lv_settings_label(frame.body, channel_names[item], 248 + item * 244, 16,
            &lv_font_instrument_sans_medium_16, 0x586B78);
        lv_obj_set_width(label, 224);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
    for (unsigned scene = 0; scene < CFD_SCENE_COUNT; ++scene) {
        int y = 54 + scene * 62;
        profiles[scene] = lv_settings_button(frame.body, 0, y, 220, 52,
            ui_text_get(profile_names[scene]), false, profile, (void *)(uintptr_t)scene);
        for (unsigned item = 0; item < CFD_ITEM_COUNT; ++item) {
            cells[scene][item] = lv_settings_button(frame.body, 248 + item * 244, y, 224, 52,
                "", false, cell, (void *)(uintptr_t)(scene * CFD_ITEM_COUNT + item));
            values[scene][item] = lv_settings_label(cells[scene][item], "--", 0, 0,
                &lv_font_instrument_sans_medium_18, 0x1D2B34);
            lv_obj_center(values[scene][item]);
            lv_obj_set_style_bg_color(cells[scene][item], lv_color_hex(0xFFFFFF), 0);
        }
    }
    retry_button = lv_settings_button(frame.footer, 964, 0, 124, 46, "Retry", false, retry, NULL);
    save_button = lv_settings_button(frame.footer, 1100, 0, 132, 46,
        ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UPDATE), true, save, NULL);
    gesture_service_set_page_policy(UI_PAGE_CFD_LEVEL_SETTING, NULL, gesture);
    if (saving) {
        lv_label_set_text(frame.message, "Waiting for controller.");
        refresh();
    } else query();
}

void ui_page_27_set_cfd_level_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_CFD_LEVEL_SETTING);
    settings_detail_dialog_hide();
    cfd_service_cancel_query();
    /* A transmitted update remains owned by the service until reply/timeout. */
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(profiles, 0, sizeof(profiles));
    memset(cells, 0, sizeof(cells));
    memset(values, 0, sizeof(values));
    currency_label = save_button = retry_button = NULL;
    ready = false;
}

void ui_page_27_set_cfd_level_on_info(const uint8_t *data, uint16_t len)
{
    if (!data || len < 16 || data[3] < 1 || data[3] > CFD_SCENE_COUNT) return;
    cfd_state_value_t config = {0};
    memcpy(config.currency, data, 3);
    unsigned pos = 4;
    for (unsigned scene = 0; scene < CFD_SCENE_COUNT; ++scene)
        for (unsigned item = 0; item < CFD_ITEM_COUNT; ++item) {
            uint8_t level = data[pos++];
            if (level < CFD_LEVEL_MIN || level > CFD_LEVEL_MAX) return;
            config.levels[scene][item] = level;
        }
    if (!cfd_service_take_query_result(config.currency) &&
        !cfd_service_take_update_result(&config, (uint8_t)(data[3] - 1))) return;
    cfd_state_confirm(&config);
    saving = false;
    if (!frame.root) return;
    original = draft = config;
    selected_scene = original_scene = data[3] - 1;
    ready = true;
    lv_label_set_text_fmt(currency_label, "%s / Profiles", config.currency);
    refresh();
    lv_label_set_text(frame.message, "Levels confirmed. Tap the selected profile's channel to cycle 1-5.");
}

void ui_page_27_set_cfd_level_on_request_failed(void)
{
    bool was_saving = saving;
    saving = false;
    if (!frame.root) return;
    refresh();
    lv_label_set_text(frame.message, was_saving ?
        "No confirmation received. Your changes are kept; retry Update." :
        "Could not read levels. Retry to enable editing.");
}
