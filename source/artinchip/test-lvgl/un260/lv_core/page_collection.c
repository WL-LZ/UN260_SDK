#include "un260/font/manrope_fonts.h"
#include "un260/font/ui_message_font.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_collection.h"
#include "page_06_settings.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/app_service/app_collection.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/smart_island.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static lv_obj_t *body, *badge, *badge_all, *badge_reject;
static bool data_collection_dirty=true,data_collection_snapshot_valid;
static data_collect_mode_t data_collection_last_mode;
static int data_collection_last_pcs;
static char data_collection_last_status[128];
static lv_obj_t *dc_btn_all,*dc_btn_false,*dc_btn_start,*dc_btn_disable,*dc_btn_retry;
static lv_obj_t *dc_label_all,*dc_label_false,*dc_check_all,*dc_check_false;
static lv_obj_t *dc_mode_value_label,*dc_pcs_label,*dc_status_label,*dc_gate_label;
static void style_plain(lv_obj_t *o){lv_obj_remove_style_all(o);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);}
static lv_color_t color_primary(void){return lv_color_hex(0x1462CC);}
static lv_color_t color_line(void){return lv_color_hex(0xE3E9ED);}
static lv_color_t color_text(void){return lv_color_hex(0x1D2B34);}
static lv_obj_t *create_label(lv_obj_t *p,const char *t,const lv_font_t *f,lv_color_t c){
 lv_obj_t *l=lv_label_create(p);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,ui_message_font(f),0);lv_obj_set_style_text_color(l,c,0);
 lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return l;
}
static void deleted(lv_event_t *event)
{
    (void)event;
    body=NULL;
    dc_btn_all=dc_btn_false=dc_btn_start=dc_btn_disable=dc_btn_retry=NULL;
    dc_label_all=dc_label_false=dc_check_all=dc_check_false=NULL;
    dc_mode_value_label=dc_pcs_label=dc_status_label=dc_gate_label=NULL;
    data_collection_dirty=true;
    data_collection_snapshot_valid=false;
}

static void badge_click(lv_event_t *event)
{
    (void)event;
    if (app_command_runtime_count_start_busy()) return;
    ui_manager_push_page(UI_PAGE_SETTING);
    page_06_settings_open_collection();
}

/* Small banknote + inspection lens, without abbreviations or warning text. */
static lv_obj_t *capture_icon(lv_obj_t *parent,int x,int y,bool rejected,uint32_t color)
{
 lv_obj_t *root=lv_settings_box(parent,x,y,28,28,0);lv_obj_set_style_bg_opa(root,0,0);
 lv_obj_t *note=lv_settings_box(root,2,rejected?4:7,21,14,0);lv_obj_set_style_bg_opa(note,0,0);lv_obj_set_style_border_width(note,2,0);lv_obj_set_style_border_color(note,lv_color_hex(color),0);lv_obj_set_style_radius(note,3,0);
 if(rejected){
   lv_obj_t *lens=lv_settings_box(root,12,12,11,11,0xEDF3F8);lv_obj_set_style_border_width(lens,2,0);lv_obj_set_style_border_color(lens,lv_color_hex(color),0);lv_obj_set_style_radius(lens,LV_RADIUS_CIRCLE,0);
   static const lv_point_t pts[]={{0,0},{5,5}};lv_obj_t *line=lv_line_create(root);lv_line_set_points(line,pts,2);lv_obj_set_pos(line,21,21);lv_obj_set_style_line_width(line,2,0);lv_obj_set_style_line_color(line,lv_color_hex(color),0);
 }else{lv_obj_t *stack=lv_settings_box(root,6,2,18,2,color);lv_obj_set_style_radius(stack,1,0);lv_obj_t *dot=lv_settings_box(note,8,4,4,4,color);lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);}
 return root;
}
static void badge_deleted(lv_event_t *e){(void)e;badge=badge_all=badge_reject=NULL;}
void page_collection_main_badge_create(lv_obj_t *parent)
{
 badge=lv_settings_box(parent,755,348,42,44,0xEDF3F8);lv_obj_set_style_radius(badge,12,0);
 lv_obj_set_style_border_width(badge,1,0);lv_obj_set_style_border_color(badge,lv_color_hex(0xCDDBE9),0);lv_obj_add_flag(badge,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_HIDDEN);
 lv_obj_add_event_cb(badge,badge_deleted,LV_EVENT_DELETE,NULL);lv_obj_add_event_cb(badge,badge_click,LV_EVENT_CLICKED,NULL);
 badge_all=capture_icon(badge,7,8,false,0x49799F);badge_reject=capture_icon(badge,7,8,true,0x49799F);
}

static const char* get_data_collect_mode_name(data_collect_mode_t mode)
{
    switch (mode) {
    case DATA_COLLECT_MODE_ALL:
        return ui_tr("All notes");
    case DATA_COLLECT_MODE_FALSE:
        return ui_tr("Rejected notes");
    default:
        return ui_tr("Not selected");
    }
}

static void update_data_collect_btn_style(lv_obj_t* btn, lv_obj_t* label, lv_obj_t* check, bool selected)
{
    if (!btn || !label || !check) {
        return;
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? 0xEDF4FF : 0xF1F4F5), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, selected ? lv_color_hex(0x8CACDA) : color_line(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_text_color(label, selected ? color_primary() : color_text(), 0);
    if(selected)lv_obj_clear_flag(check,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(check,LV_OBJ_FLAG_HIDDEN);
}

void page_collection_refresh(void)
{
    data_collect_mode_t mode;
    bool request_pending;
    int pcs;
    const char *status;
    bool mode_changed;
    bool pcs_changed;
    bool status_changed;

    if (!body || !dc_btn_all || !lv_obj_is_visible(body)) {
        data_collection_dirty = true;
        return;
    }

    mode = data_collection_state_mode();
    request_pending = data_collection_request_pending();
    pcs = data_collection_state_pcs();
    status = data_collection_state_status();
    if (status == NULL) status = "";
    mode_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                   mode != data_collection_last_mode;
    pcs_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                  pcs != data_collection_last_pcs;
    status_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                     strcmp(status, data_collection_last_status) != 0;

    bool ready=work_mode_service_diagnostic_ready();
    bool busy=app_command_runtime_count_start_busy();
    lv_obj_t *actions[]={dc_btn_all,dc_btn_false,dc_btn_start,dc_btn_disable};
    for(unsigned i=0;i<4;i++){
        bool enabled=!request_pending&&!busy&&(i!=2||ready)&&
                     (i<2||mode!=DATA_COLLECT_MODE_NONE);
        if(enabled)settings_detail_action_wait(actions[i], NULL);
        else settings_detail_action_wait(actions[i], request_pending ? UI_N_("Wait for the current collection request to finish.") : busy ? UI_N_("Stop counting before changing collection.") : !ready ? work_mode_service_status_msgid() : UI_N_("Select All notes or Rejected notes first."));
    }
    work_mode_snapshot_t gate;
    work_mode_service_get_snapshot(&gate);
    if(dc_btn_retry){
        if(gate.phase==WORK_MODE_FAILED)lv_obj_clear_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
    }
    if(dc_gate_label&&strcmp(lv_label_get_text(dc_gate_label),work_mode_service_status_text()))
        lv_label_set_text(dc_gate_label,work_mode_service_status_text());

    if (mode_changed) {
        update_data_collect_btn_style(dc_btn_all, dc_label_all, dc_check_all,
                                      mode == DATA_COLLECT_MODE_ALL);
        update_data_collect_btn_style(dc_btn_false, dc_label_false, dc_check_false,
                                      mode == DATA_COLLECT_MODE_FALSE);
        if (dc_mode_value_label) {
            lv_label_set_text(dc_mode_value_label, get_data_collect_mode_name(mode));
        }
    }

    if (pcs_changed && dc_pcs_label) {
        lv_label_set_text_fmt(dc_pcs_label, "%d", pcs);
    }

    if (status_changed && dc_status_label) {
        lv_label_set_text(dc_status_label, status);
    }

    data_collection_last_mode = mode;
    data_collection_last_pcs = pcs;
    snprintf(data_collection_last_status, sizeof(data_collection_last_status),
             "%s", status);
    data_collection_snapshot_valid = true;
    data_collection_dirty = false;
}

static void data_collect_mode_btn_event_cb(lv_event_t *event)
{
    data_collect_mode_t mode = (data_collect_mode_t)(uintptr_t)lv_event_get_user_data(event);
    if (data_collection_request_pending() || app_command_runtime_count_start_busy()) return;
    (void)app_collection_request(mode);
    page_collection_refresh();
}

static void data_collect_start_btn_event_cb(lv_event_t* e)
{
    (void)e;
    if(!work_mode_service_diagnostic_ready()||data_collection_request_pending()||app_command_runtime_count_start_busy())return;

    if (data_collection_state_mode() == DATA_COLLECT_MODE_NONE) {
        data_collection_state_set_status(UI_N_("Please select a collection mode first"));
        page_collection_refresh();
        return;
    }

    if (app_command_runtime_request_diagnostic_run()) {
        data_collection_state_reset_pcs();
        data_collection_state_set_status(UI_N_("Counting command sent. Waiting for controller reply..."));
        page_collection_refresh();
    } else {
        data_collection_state_set_status(UI_N_("Start was not accepted. Check the controller."));
        page_collection_refresh();
    }
}

static void data_collect_disable_btn_event_cb(lv_event_t *event)
{
    (void)event;
    if (app_command_runtime_count_start_busy() ||
        data_collection_state_mode() == DATA_COLLECT_MODE_NONE) return;
    (void)app_collection_request(DATA_COLLECT_MODE_NONE);
    page_collection_refresh();
}

static lv_obj_t* create_dc_mode_button(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                       const char* text, uint8_t sub,
                                       lv_obj_t** out_label, lv_obj_t** out_check)
{
    lv_obj_t* btn = lv_obj_create(parent);
    style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 380, 62);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_settings_action_guard_init(btn);
    lv_obj_add_event_cb(btn, data_collect_mode_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)sub);

    lv_obj_set_style_bg_color(btn,lv_color_hex(0xE2E9EE),LV_STATE_PRESSED);
    lv_obj_t* label = create_label(btn, text, &lv_font_instrument_sans_medium_18, color_text());
    lv_obj_set_width(label, 324);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 18, 0);

    lv_obj_t* check = lv_settings_icon(btn,"Check-active",0,0);
    lv_obj_align(check, LV_ALIGN_RIGHT_MID, -16, 0);

    if (out_label) {
        *out_label = label;
    }
    if (out_check) {
        *out_check = check;
    }

    return btn;
}

static void data_collection_retry(lv_event_t *event)
{
    (void)event;
    work_mode_service_retry();
    page_collection_refresh();
}

void page_collection_create(lv_settings_frame_t *frame)
{
    lv_obj_t *parent=frame->body;
    body=parent;
    lv_obj_add_event_cb(body, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_opa(parent,0,0);lv_obj_set_style_border_width(parent,0,0);
    lv_obj_t *modes=lv_settings_panel(parent,0,0,424,242);
    lv_settings_label(modes,ui_tr("Collect"),22,19,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    dc_btn_all = create_dc_mode_button(modes, 22, 52, ui_tr("All notes"),
                                       0x01, &dc_label_all, &dc_check_all);
    dc_btn_false = create_dc_mode_button(modes, 22, 126, ui_tr("Rejected notes"),
                                         0x02, &dc_label_false, &dc_check_false);
    lv_settings_label(modes,ui_tr("Choose a mode before starting."),22,207,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *card=lv_settings_panel(parent,440,0,792,242);
    lv_settings_label(card,ui_tr("Collection session"),24,19,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    lv_settings_box(card,24,52,744,1,0xE3E9ED);
    lv_settings_label(card,ui_tr("Confirmed mode"),24,73,&lv_font_instrument_sans_medium_14,0x586B78);
    dc_mode_value_label=lv_settings_label(card,ui_tr("Not selected"),24,98,&lv_font_instrument_sans_semibold_24,0x1D2B34);
    lv_settings_label(card,ui_tr("Notes collected"),548,73,&lv_font_instrument_sans_medium_14,0x586B78);
    dc_pcs_label=lv_settings_label(card,"0",548,98,&lv_font_instrument_sans_semibold_40,0x1D2B34);
    lv_settings_box(card,24,160,744,1,0xE3E9ED);
    dc_status_label=lv_settings_label(card,ui_tr("Select a collection mode"),24,184,&lv_font_instrument_sans_medium_16,0x586B78);
    lv_obj_set_width(dc_status_label,744);
    lv_label_set_long_mode(dc_status_label, LV_LABEL_LONG_WRAP);
    dc_btn_disable=lv_settings_button(frame->footer,898,0,164,44,ui_tr("End collection"),false,data_collect_disable_btn_event_cb,NULL);
    dc_btn_start=lv_settings_button(frame->footer,1076,0,156,44,ui_tr("RUN"),true,data_collect_start_btn_event_cb,NULL);
    dc_btn_retry=lv_settings_button(frame->footer,756,0,128,44,ui_tr("Retry"),false,data_collection_retry,NULL);
    lv_obj_add_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
    dc_gate_label=frame->message;
    lv_obj_set_width(dc_gate_label,736);
    lv_label_set_text(dc_gate_label,work_mode_service_status_text());
    lv_obj_update_layout(body);
    page_collection_refresh();
}


void page_collection_poll(void)
{
    static uint32_t last;
    uint32_t now=lv_tick_get();
    if ((uint32_t)(now-last)<100) return;
    last=now;
    if (!badge) return;
    data_collect_mode_t mode=data_collection_state_mode();
    if (mode!=DATA_COLLECT_MODE_NONE && !app_command_runtime_count_start_busy() &&
        smart_island_status_slot_available())
        lv_obj_clear_flag(badge,LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(badge,LV_OBJ_FLAG_HIDDEN);
    bool all=mode==DATA_COLLECT_MODE_ALL;
    lv_obj_add_flag(all?badge_reject:badge_all,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(all?badge_all:badge_reject,LV_OBJ_FLAG_HIDDEN);
}
