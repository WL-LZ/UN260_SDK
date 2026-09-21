#include "page_27_set_cfd_level.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_loading_orbit.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/currency/currency_state.h"
#include "un260/cfd/cfd.h"
#include "un260/gesture/gesture_service.h"
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *profiles[CFD_SCENE_COUNT], *cells[CFD_ITEM_COUNT][CFD_LEVEL_MAX];
static lv_obj_t *currency_label, *save_button, *retry_button;
static cfd_state_value_t original, draft;
static uint8_t selected_scene, original_scene;
static bool ready, saving, leave_home;
static lv_obj_t *loading, *loading_text, *loading_orbit;
static lv_timer_t *loading_timer;
static bool loading_cycle_done;
static void refresh(void);
static void loading_done(lv_timer_t *timer)
{
    lv_timer_del(timer);loading_timer=NULL;loading_cycle_done=true;refresh();
}
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
    if(loading && ready && loading_cycle_done){
        lv_obj_del(loading);loading=loading_text=loading_orbit=NULL;
    }else if(loading && !busy && !ready){
        if(loading_orbit){lv_obj_del(loading_orbit);loading_orbit=NULL;}
        lv_label_set_text(loading_text,"Levels unavailable. Tap Retry below.");
    }
    for (unsigned scene = 0; scene < CFD_SCENE_COUNT; ++scene) {
        if (scene == selected_scene) lv_obj_add_state(profiles[scene], LV_STATE_CHECKED);
        else lv_obj_clear_state(profiles[scene], LV_STATE_CHECKED);
        if (!ready || busy) lv_obj_add_state(profiles[scene], LV_STATE_DISABLED);
        else lv_obj_clear_state(profiles[scene], LV_STATE_DISABLED);
    }
    for(unsigned item=0;item<CFD_ITEM_COUNT;item++)for(unsigned level=0;level<CFD_LEVEL_MAX;level++){
        lv_obj_t *o=cells[item][level];
        if(ready&&draft.levels[selected_scene][item]==level+1)lv_obj_add_state(o,LV_STATE_CHECKED);else lv_obj_clear_state(o,LV_STATE_CHECKED);
        if(!ready||busy)lv_obj_add_state(o,LV_STATE_DISABLED);else lv_obj_clear_state(o,LV_STATE_DISABLED);
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
    if(loading_timer){lv_timer_del(loading_timer);loading_timer=NULL;}
    if(loading)lv_obj_del(loading);
    loading=lv_settings_panel(frame.body,0,0,1232,242);
    lv_obj_add_flag(loading,LV_OBJ_FLAG_CLICKABLE);
    loading_orbit=lv_loading_orbit_create_sized(loading,48);
    lv_obj_set_pos(loading_orbit,592,65);
    loading_text=lv_settings_label(loading,"Reading detection levels",0,137,&lv_font_instrument_sans_medium_18,0x536B79);
    lv_obj_set_width(loading_text,1232);lv_obj_set_style_text_align(loading_text,LV_TEXT_ALIGN_CENTER,0);
    loading_cycle_done=false;loading_timer=lv_timer_create(loading_done,900,NULL);
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
    lv_label_set_text(frame.message, dirty() ? "Unsaved changes." : "Choose a profile, then select each channel level.");
}

static void cell(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !ready || cfd_service_busy()) return;
    unsigned key = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    unsigned item=key/CFD_LEVEL_MAX,level=key%CFD_LEVEL_MAX+1;
    if(item>=CFD_ITEM_COUNT)return;
    draft.levels[selected_scene][item]=level;
    refresh();
    lv_label_set_text(frame.message, dirty() ? "Unsaved changes." : "Choose a profile, then select each channel level.");
}

static void save(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !dirty() || cfd_service_busy()) return;
    if (!cfd_service_request_update(&draft, selected_scene)) {
        lv_label_set_text(frame.message, "Could not send levels. Your changes are kept.");
        return;
    }
    saving = true;
    /* Preserve the footer through short ACK round trips; controls stay locked. */
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
    lv_obj_t *rows=lv_settings_list(frame.body,0,0,1230,298);
    lv_obj_set_style_pad_all(rows,0,0);
    lv_obj_set_style_pad_right(rows,12,0);
    lv_obj_set_style_pad_row(rows,0,0);
    lv_obj_t *row=lv_settings_control_row(rows,0,0,1218,44,false);
    lv_obj_set_height(row,52);
    currency_label=lv_settings_label(row,"",24,20,&lv_font_instrument_sans_medium_18,0x1D2B34);
    lv_obj_t *base=lv_settings_segment_base(row,530,4,678,44);
    for(unsigned scene=0;scene<CFD_SCENE_COUNT;scene++)
        profiles[scene]=lv_settings_segment(base,scene,CFD_SCENE_COUNT,ui_text_get(profile_names[scene]),profile,(void*)(uintptr_t)scene);
    const char *hints[]={"Ultraviolet detection","Magnetic detection","Magnetic thread detection","Infrared detection"};
    for(unsigned item=0;item<CFD_ITEM_COUNT;item++){
        row=lv_settings_control_row(rows,0,0,1218,44,true);
        lv_settings_label(row,channel_names[item],24,20,&lv_font_instrument_sans_medium_18,0x1D2B34);
        lv_settings_label(row,hints[item],100,23,&lv_font_instrument_sans_medium_14,0x586B78);
        base=lv_settings_segment_base(row,712,8,496,44);
        for(unsigned level=0;level<CFD_LEVEL_MAX;level++){
            char text[2]={(char)('1'+level),0};
            cells[item][level]=lv_settings_segment(base,level,CFD_LEVEL_MAX,text,cell,(void*)(uintptr_t)(item*CFD_LEVEL_MAX+level));
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
    if(loading_timer){lv_timer_del(loading_timer);loading_timer=NULL;}
    gesture_service_clear_page_policy(UI_PAGE_CFD_LEVEL_SETTING);
    settings_detail_dialog_hide();
    cfd_service_cancel_query();
    /* A transmitted update remains owned by the service until reply/timeout. */
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(profiles, 0, sizeof(profiles));
    memset(cells, 0, sizeof(cells));
    currency_label = save_button = retry_button = NULL;
    ready = false;
    loading=loading_text=loading_orbit=NULL;
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
    lv_label_set_text(frame.message, "Levels confirmed. Select a channel level to edit.");
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
