#include "page_20_set_print.h"
#include "un260/lv_core/lv_page_manager.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/ui_text.h"
#include "un260/print/print_config.h"
#include "un260/gesture/gesture_service.h"
#include "lv_port_indev.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PRINT_HEAD_MAX_LEN       PRINT_SETTING_HEAD_MAX_LEN
#define PRINT_SPACE_MAX_LINES    PRINT_SETTING_SPACE_MAX_LINES

typedef enum {
    PRINT_FIELD_SPACE_TOP = 0,
    PRINT_FIELD_HEAD1,
    PRINT_FIELD_HEAD2,
    PRINT_FIELD_SPACE_BOTTOM,
} print_field_t;

typedef enum {
    PRINT_CONTENT_LIST = PRINT_SETTING_CONTENT_LIST,
    PRINT_CONTENT_SN = PRINT_SETTING_CONTENT_SN,
    PRINT_CONTENT_LIST_SN = PRINT_SETTING_CONTENT_LIST_SN,
} print_content_t;

static lv_settings_frame_t print_frame;
static bool print_pending, print_saving, print_leave_home;
static print_config_value_t print_draft;
static lv_obj_t *print_save_button,*print_cancel_button;
static void print_apply_next(void);
static bool print_dirty(void)
{
    print_config_value_t actual;print_config_get(&actual);
    return actual.content!=print_draft.content||actual.space_top!=print_draft.space_top||
        actual.space_bottom!=print_draft.space_bottom||strcmp(actual.head1,print_draft.head1)||
        strcmp(actual.head2,print_draft.head2);
}
static lv_obj_t* print_page = NULL;
static lv_obj_t* value_space_top = NULL;
static lv_obj_t* value_head1 = NULL;
static lv_obj_t* value_head2 = NULL;
static lv_obj_t* value_space_bottom = NULL;
static lv_obj_t* field_boxes[4] = { NULL };
static lv_obj_t* status_label = NULL;
static lv_obj_t* content_boxes[3] = { NULL };
static print_field_t active_field = PRINT_FIELD_SPACE_TOP;
static bool active_field_valid = false;
static lv_obj_t *receipt_titles[2], *receipt_content, *space_presets[2][3];
static lv_obj_t *receipt_paper, *receipt_body, *receipt_caption;

static void print_refresh_view(void);

static void print_set_status(const char* text, lv_color_t color)
{
    if (!status_label) return;
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

static void print_set_active_field(bool active, print_field_t field)
{
    if (active_field_valid && field_boxes[active_field])
        lv_obj_clear_state(field_boxes[active_field], LV_STATE_CHECKED);
    active_field = field;
    active_field_valid = active;
    if (active && field_boxes[field]) lv_obj_add_state(field_boxes[field], LV_STATE_CHECKED);
}

static void print_request_started(void)
{
    print_pending = true;
    /* Locked Save/Cancel indicate the transaction without flashing the footer. */
    if (print_page) print_refresh_view();
}

static void print_keyboard_close_cb(void* user_data)
{
    (void)user_data;
    print_set_active_field(false, active_field);
}

static uint8_t print_parse_space(const char* value)
{
    long v;

    if (!value || value[0] == '\0') {
        return 0;
    }

    v = strtol(value, NULL, 10);
    if (v < 0) v = 0;
    if (v > PRINT_SPACE_MAX_LINES) v = PRINT_SPACE_MAX_LINES;
    return (uint8_t)v;
}

static bool print_send_content(print_content_t content,
                               const print_config_value_t* target)
{
    uint8_t payload[2] = { 0x01, (uint8_t)content };
    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    print_request_started();
    return true;
}

static bool print_send_head(uint8_t index, const char* text,
                            const print_config_value_t* target)
{
    uint8_t payload[2 + PRINT_HEAD_MAX_LEN];

    payload[0] = 0x02;
    payload[1] = index;
    memset(&payload[2], ' ', PRINT_HEAD_MAX_LEN);

    if (text) {
        size_t len = strlen(text);
        if (len > PRINT_HEAD_MAX_LEN) {
            len = PRINT_HEAD_MAX_LEN;
        }
        memcpy(&payload[2], text, len);
    }

    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    print_request_started();
    return true;
}

static bool print_send_space(uint8_t index, uint8_t lines,
                             const print_config_value_t* target)
{
    uint8_t payload[3] = { 0x03, index, lines };
    if (!print_config_request(payload[0], payload, sizeof(payload), target)) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_UART_NOT_READY), lv_color_hex(0xC03A2B));
        return false;
    }
    print_request_started();
    return true;
}

static void print_leave(void *data)
{
    (void)data;
    if(print_leave_home){ui_manager_suspend_to_home();}else ui_manager_pop_page();
}
static void print_ask_leave(bool home)
{
    if(print_pending||print_saving)return;
    print_leave_home=home;
    if(print_dirty())settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,"Discard changes?","The receipt changes have not been saved.","Discard","Keep editing",print_leave,NULL,NULL);
    else print_leave(NULL);
}
static void print_esc_cb(lv_event_t *e)
{
    (void)e;
    if(settings_detail_overlay_is_open()){settings_detail_keyboard_hide();return;}
    print_ask_leave(false);
}
static bool print_gesture(gesture_action_t action)
{
    if(print_pending||print_saving||settings_detail_overlay_is_open())return true;
    if(action==GESTURE_ACTION_HOME&&print_dirty()){print_ask_leave(true);return true;}
    return false;
}
static void print_cancel(lv_event_t *e)
{
    (void)e;if(print_pending||print_saving)return;
    print_config_get(&print_draft);print_refresh_view();
    print_set_status("Changes discarded.",lv_color_hex(0x586B78));
}
static void print_apply_next(void)
{
    print_config_value_t next;print_config_get(&next);
    bool sent=false;
    if(strcmp(next.head1,print_draft.head1)){
        memcpy(next.head1,print_draft.head1,sizeof(next.head1));sent=print_send_head(1,next.head1,&next);
    }else if(strcmp(next.head2,print_draft.head2)){
        memcpy(next.head2,print_draft.head2,sizeof(next.head2));sent=print_send_head(2,next.head2,&next);
    }else if(next.space_top!=print_draft.space_top){
        next.space_top=print_draft.space_top;sent=print_send_space(1,next.space_top,&next);
    }else if(next.space_bottom!=print_draft.space_bottom){
        next.space_bottom=print_draft.space_bottom;sent=print_send_space(2,next.space_bottom,&next);
    }else if(next.content!=print_draft.content){
        next.content=print_draft.content;sent=print_send_content(next.content,&next);
    }else{
        print_saving=false;print_refresh_view();print_set_status("All changes confirmed.",lv_color_hex(0x29704D));return;
    }
    if(!sent){print_saving=false;print_refresh_view();print_set_status("Save incomplete. Confirmed changes remain; retry Save.",lv_color_hex(0xA35B12));}
}
static void print_save(lv_event_t *e)
{
    (void)e;if(print_pending||print_saving||!print_dirty())return;
    print_saving=true;print_apply_next();
}

static void print_field_keyboard_done(const char* value, void* user_data)
{
    if(!print_page||print_pending||print_saving)return;
    print_field_t field = (print_field_t)(uintptr_t)user_data;
    print_config_value_t config;

    config = print_draft;

    switch (field) {
    case PRINT_FIELD_SPACE_TOP: {
        uint8_t lines = print_parse_space(value);
        config.space_top = lines;
        print_draft=config;
        break;
    }

    case PRINT_FIELD_HEAD1: {
        char text[PRINT_HEAD_MAX_LEN + 1];
        lv_snprintf(text, sizeof(text), "%s", value ? value : "");
        lv_snprintf(config.head1, sizeof(config.head1), "%s", text);
        print_draft=config;
        break;
    }

    case PRINT_FIELD_HEAD2: {
        char text[PRINT_HEAD_MAX_LEN + 1];
        lv_snprintf(text, sizeof(text), "%s", value ? value : "");
        lv_snprintf(config.head2, sizeof(config.head2), "%s", text);
        print_draft=config;
        break;
    }

    case PRINT_FIELD_SPACE_BOTTOM: {
        uint8_t lines = print_parse_space(value);
        config.space_bottom = lines;
        print_draft=config;
        break;
    }

    default:
        break;
    }

    print_refresh_view();
}

static void print_input_cb(lv_event_t* e)
{
    print_field_t field;
    char value[24];
    const char* title = "";
    settings_detail_keyboard_mode_t mode = SETTINGS_DETAIL_KEYBOARD_NUM;
    uint16_t max_len = 2;
    print_config_value_t config;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED || print_pending) return;

    field = (print_field_t)(uintptr_t)lv_event_get_user_data(e);
    value[0] = '\0';
    config=print_draft;

    switch (field) {
    case PRINT_FIELD_SPACE_TOP:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_TOP);
        lv_snprintf(value, sizeof(value), "%u", (unsigned)config.space_top);
        break;

    case PRINT_FIELD_HEAD1:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD1);
        lv_snprintf(value, sizeof(value), "%s", config.head1);
        mode = SETTINGS_DETAIL_KEYBOARD_TEXT;
        max_len = PRINT_HEAD_MAX_LEN;
        break;

    case PRINT_FIELD_HEAD2:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD2);
        lv_snprintf(value, sizeof(value), "%s", config.head2);
        mode = SETTINGS_DETAIL_KEYBOARD_TEXT;
        max_len = PRINT_HEAD_MAX_LEN;
        break;

    case PRINT_FIELD_SPACE_BOTTOM:
        title = ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM);
        lv_snprintf(value, sizeof(value), "%u", (unsigned)config.space_bottom);
        break;

    default:
        return;
    }

    print_refresh_view();
    print_set_active_field(true, field);

    if (!settings_detail_keyboard_show_ex(title, value, max_len, mode,
                                          print_field_keyboard_done,
                                          (void*)(uintptr_t)field,
                                          print_keyboard_close_cb, NULL)) {
        print_set_active_field(false, field);
    }
}

static void print_content_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || print_pending) return;
    print_content_t content = (print_content_t)(uintptr_t)lv_event_get_user_data(event);
    print_config_value_t config=print_draft;
    if (config.content == content) return;
    print_set_active_field(false, active_field);
    config.content = (uint8_t)content;
    print_draft=config;print_refresh_view();
}

static void print_space_preset(lv_event_t *event)
{
    if(print_pending)return;
    unsigned key=(unsigned)(uintptr_t)lv_event_get_user_data(event),row=key/3,value=key%3;
    print_config_value_t config=print_draft;
    if((row?config.space_bottom:config.space_top)==value)return;
    if(row)config.space_bottom=value;else config.space_top=value;
    print_draft=config;print_refresh_view();
}

static void print_refresh_view(void)
{
    print_config_value_t config=print_draft;
    if(receipt_titles[0]){
        /* Keep the content readable; compress only very large blank margins. */
        unsigned lines=(unsigned)config.space_top+config.space_bottom;
        unsigned blank=lines>5?40:lines*8;
        unsigned top=lines?blank*config.space_top/lines:0;
        if(config.space_top&&top==0)top=1;
        if(config.space_bottom&&top==blank&&blank)top--;
        lv_obj_set_height(receipt_paper,176+blank);
        lv_obj_set_y(receipt_body,top);
        lv_label_set_text(receipt_caption,lines>5?
            "Example / blank spacing scaled":"Receipt preview / Example data");
        lv_label_set_text(receipt_titles[0],config.head1[0]?config.head1:"UN260");
        lv_label_set_text(receipt_titles[1],config.head2[0]?config.head2:"COUNT REPORT");
        lv_label_set_text(receipt_content,config.content==PRINT_CONTENT_LIST?"Denomination summary":config.content==PRINT_CONTENT_SN?"Serial number records":"Summary + serial numbers");
    }
    for(unsigned row=0;row<2;row++)for(unsigned i=0;i<3;i++)if(space_presets[row][i]){
        lv_obj_t *o=space_presets[row][i];
        if((row?config.space_bottom:config.space_top)==i)lv_obj_add_state(o,LV_STATE_CHECKED);else lv_obj_clear_state(o,LV_STATE_CHECKED);
        if(print_pending)settings_detail_action_block(o, print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");else settings_detail_action_block(o, NULL);
    }
    if(print_save_button){
        if(print_pending||print_saving||!print_dirty())settings_detail_action_block(print_save_button, print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");else settings_detail_action_block(print_save_button, NULL);
        if(print_pending||print_saving)settings_detail_action_block(print_cancel_button, print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");else settings_detail_action_block(print_cancel_button, NULL);
        if(print_pending||print_saving)settings_detail_action_block(print_frame.back, print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");else settings_detail_action_block(print_frame.back, NULL);
        if(!print_pending&&!print_saving)lv_label_set_text(print_frame.message,print_dirty()?"Unsaved changes. Scroll for more options.":"5 settings. Scroll for more options.");
    }
    if (value_space_top) lv_label_set_text_fmt(value_space_top, "%u", (unsigned)config.space_top);
    if (value_head1) lv_label_set_text(value_head1, config.head1[0] ? config.head1 : "Not set");
    if (value_head2) lv_label_set_text(value_head2, config.head2[0] ? config.head2 : "Not set");
    if (value_space_bottom) lv_label_set_text_fmt(value_space_bottom, "%u", (unsigned)config.space_bottom);
    for (unsigned i = 0; i < 3; ++i) {
        if (!content_boxes[i]) continue;
        if (config.content == i + 1) lv_obj_add_state(content_boxes[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(content_boxes[i], LV_STATE_CHECKED);
        if (print_pending) settings_detail_action_block(content_boxes[i], print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");
        else settings_detail_action_block(content_boxes[i], NULL);
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (!field_boxes[i]) continue;
        if (print_pending) settings_detail_action_block(field_boxes[i], print_pending || print_saving ? "Wait for the current print settings request to finish." : "No changes to save.");
        else settings_detail_action_block(field_boxes[i], NULL);
    }
}

static void print_create_field(lv_obj_t *parent, int y, const char *title,
                               lv_obj_t **value, print_field_t field)
{
    parent=lv_settings_control_row(parent,0,y,758,44,true);
    y=8;
    lv_settings_label(parent, title, 20, y + 6,
        &lv_font_instrument_sans_medium_16, 0x1D2B34);
    bool numeric=field==PRINT_FIELD_SPACE_TOP||field==PRINT_FIELD_SPACE_BOTTOM;
    lv_settings_label(parent,numeric?"Blank lines / tap value for keyboard":"Tap to edit with keyboard",20,y+29,&lv_font_instrument_sans_medium_12,0x586B78);
    lv_obj_t *button = lv_settings_button(parent, numeric?646:350, y, numeric?92:388, 44,
        "", false, print_input_cb, (void *)(uintptr_t)field);
    field_boxes[field] = button;
    *value = lv_settings_label(button, "", 18, 13,
        &lv_font_instrument_sans_medium_16, 0x1D2B34);
    lv_obj_set_width(*value, numeric?56:350);
    lv_label_set_long_mode(*value, LV_LABEL_LONG_DOT);
    if(numeric){
        unsigned row=field==PRINT_FIELD_SPACE_BOTTOM;
        lv_obj_t *base=lv_settings_segment_base(parent,350,y,284,44);
        for(unsigned i=0;i<3;i++){
            char label[2]={(char)('0'+i),0};
            space_presets[row][i]=lv_settings_segment(base,i,3,label,print_space_preset,(void*)(uintptr_t)(row*3+i));
        }
    }
}

void ui_page_20_set_print_create(lv_obj_t *parent)
{
    if (print_page) return;
    active_field_valid = false;
    print_config_get(&print_draft);print_saving=false;print_leave_home=false;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_PRINT_TITLE), .subtitle="Device / Receipt configuration", .icon = "Settings", .back = print_esc_cb
    };
    print_frame = lv_settings_frame_create(parent, &header);
    print_page = print_frame.root;
    lv_obj_t *body = print_frame.body;
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);

    receipt_paper=lv_settings_panel(body,70,6,302,176);
    receipt_body=lv_obj_create(receipt_paper);
    lv_obj_remove_style_all(receipt_body);
    lv_obj_set_size(receipt_body,302,176);
    lv_obj_clear_flag(receipt_body,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *receipt=receipt_body;
    for(unsigned i=0;i<2;i++){
        receipt_titles[i]=lv_settings_label(receipt,"",18,12+i*28,i?&lv_font_instrument_sans_medium_14:&lv_font_instrument_sans_semibold_20,0x1D2B34);
        lv_obj_set_width(receipt_titles[i],266);lv_obj_set_style_text_align(receipt_titles[i],LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_long_mode(receipt_titles[i],LV_LABEL_LONG_DOT);
    }
    lv_settings_box(receipt,20,66,262,1,0xDFE7ED);
    lv_settings_label(receipt,"CNY\nPCS\nAMOUNT",20,74,&lv_font_instrument_sans_medium_14,0x1D2B34);
    lv_obj_t *sample=lv_settings_label(receipt,"Example\n85\n425",207,74,&lv_font_instrument_sans_medium_14,0x1D2B34);
    lv_obj_set_width(sample,74);lv_obj_set_style_text_align(sample,LV_TEXT_ALIGN_RIGHT,0);
    lv_settings_box(receipt,20,134,262,1,0xDFE7ED);
    receipt_content=lv_settings_label(receipt,"",20,150,&lv_font_instrument_sans_medium_12,0x536B79);
    receipt_caption=lv_settings_label(body,"",82,225,&lv_font_instrument_sans_medium_14,0x536B79);
    lv_obj_t *rows=lv_settings_panel(body,454,0,778,300);
    lv_obj_add_flag(rows,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(rows,LV_DIR_VER);lv_obj_set_scrollbar_mode(rows,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(rows,4,LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(rows,LV_OPA_COVER,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(rows,lv_color_hex(0x9AAEBB),LV_PART_SCROLLBAR);lv_obj_set_style_radius(rows,2,LV_PART_SCROLLBAR);
    lv_port_indev_set_drag_obj(rows,true);
    lv_settings_label(rows,"Report content",20,10,&lv_font_instrument_sans_medium_16,0x1D2B34);
    lv_settings_label(rows,"Choose the printed records",20,34,&lv_font_instrument_sans_medium_12,0x586B78);
    lv_obj_t *base=lv_settings_segment_base(rows,350,8,388,44);
    const char *names[] = { "Summary", "Serial", "Both" };
    for (unsigned i = 0; i < 3; ++i) {
        content_boxes[i] = lv_settings_segment(base,i,3,names[i],print_content_cb,(void *)(uintptr_t)(i+1));
    }
    print_create_field(rows, 61, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD1), &value_head1, PRINT_FIELD_HEAD1);
    print_create_field(rows, 122, ui_text_get(UI_TEXT_SETTINGS_PRINT_HEAD2), &value_head2, PRINT_FIELD_HEAD2);
    print_create_field(rows, 183, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_TOP), &value_space_top, PRINT_FIELD_SPACE_TOP);
    print_create_field(rows, 244, ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM), &value_space_bottom, PRINT_FIELD_SPACE_BOTTOM);
    print_cancel_button=lv_settings_button(print_frame.footer,964,0,124,46,"Cancel",false,print_cancel,NULL);
    print_save_button=lv_settings_button(print_frame.footer,1100,0,132,46,"Save",true,print_save,NULL);
    gesture_service_set_page_policy(UI_PAGE_PRINT_SETTING,NULL,print_gesture);
    status_label = print_frame.message;
    lv_label_set_text(status_label, print_pending ? "Waiting for controller." : "Values update after controller confirmation.");
    print_refresh_view();
}

void ui_page_20_set_print_destroy(void)
{
    settings_detail_keyboard_hide();settings_detail_dialog_hide();
    gesture_service_clear_page_policy(UI_PAGE_PRINT_SETTING);print_saving=false;

    if (print_page && lv_obj_is_valid(print_page)) {
        lv_obj_del(print_page);
    }

    print_page = NULL;
    memset(&print_frame, 0, sizeof(print_frame));
    value_space_top = NULL;
    value_head1 = NULL;
    value_head2 = NULL;
    value_space_bottom = NULL;
    status_label = NULL;print_save_button=print_cancel_button=NULL;
    memset(receipt_titles,0,sizeof(receipt_titles));receipt_content=NULL;
    receipt_paper=receipt_body=receipt_caption=NULL;
    memset(space_presets,0,sizeof(space_presets));

    for (int i = 0; i < 3; i++) {
        content_boxes[i] = NULL;
    }
    for (int i = 0; i < 4; i++) {
        field_boxes[i] = NULL;
    }
    active_field_valid = false;
}

void ui_page_20_set_print_on_boot_setting(const uint8_t* data, uint16_t len)
{
    uint8_t sub;
    print_config_value_t config;

    if (!data || len < 2) return;

    print_config_cancel_request();
    print_pending = false;print_saving=false;
    sub = data[0];
    print_config_get(&config);
    switch (sub) {
    case 0x01:
        if (data[1] >= PRINT_SETTING_CONTENT_LIST &&
            data[1] <= PRINT_SETTING_CONTENT_LIST_SN) {
            config.content = data[1];
            print_config_confirm(&config);
        }
        break;

    case 0x02:
        if (len >= (uint16_t)(2 + PRINT_HEAD_MAX_LEN)) {
            char text[PRINT_HEAD_MAX_LEN + 1];
            size_t copy_len = PRINT_HEAD_MAX_LEN;

            memcpy(text, &data[2], PRINT_HEAD_MAX_LEN);
            while (copy_len > 0 &&
                   (text[copy_len - 1] == ' ' || text[copy_len - 1] == '\0')) {
                copy_len--;
            }
            text[copy_len] = '\0';

            if (data[1] == 0x01) {
                lv_snprintf(config.head1, sizeof(config.head1), "%s", text);
                print_config_confirm(&config);
            } else if (data[1] == 0x02) {
                lv_snprintf(config.head2, sizeof(config.head2), "%s", text);
                print_config_confirm(&config);
            }
        }
        break;

    case 0x03:
        if (len >= 3) {
            uint8_t lines = data[2];
            if (lines > PRINT_SPACE_MAX_LINES) lines = PRINT_SPACE_MAX_LINES;

            if (data[1] == 0x01) {
                config.space_top = lines;
                print_config_confirm(&config);
            } else if (data[1] == 0x02) {
                config.space_bottom = lines;
                print_config_confirm(&config);
            }
        }
        break;

    default:
        break;
    }

    if (print_page) {
        print_config_get(&print_draft);print_refresh_view();
        print_set_status("Configuration received.", lv_color_hex(0x586B78));
    }
}

void ui_page_20_set_print_on_reply(const print_config_request_result_t* result)
{
    if (!result) return;
    print_pending = false;
    if (print_page) print_refresh_view();
    if (!print_page) return;

    if(print_saving){
        if(result->success){print_apply_next();return;}
        print_saving=false;print_refresh_view();
        print_set_status("Save incomplete. Confirmed changes remain; retry Save.",lv_color_hex(0xA35B12));return;
    }
    if (!result->success) {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_FAIL), lv_color_hex(0xC03A2B));
    } else {
        print_set_status(ui_text_get(UI_TEXT_SETTINGS_PRINT_SUCCESS), lv_color_hex(0x1462CC));
    }
}
