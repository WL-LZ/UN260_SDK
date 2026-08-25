#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "un260/lv_resources/lv_img_init.h" 
#include "lv_page_event.h"
#include "un260/lv_system/ui_object_utils.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_components/lv_components.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include "../aic_ui/aic_ui.h"
#include <stdlib.h>
#include <string.h>

static lv_obj_t* menu_page = NULL;
static ui_element_t page_03_menu_obj[];
static lv_obj_t* g_batch_tip_label = NULL;
static lv_timer_t* g_batch_tip_timer = NULL;
static lv_obj_t* g_batch_amount_label = NULL;
static lv_obj_t* g_batch_pcs_label = NULL;

static lv_obj_t* g_batch_num_display;
static int g_batch_num_index;
static char g_input_batch_num[9];
static bool g_pcs_batch_num_lock_200;

void page_03_batch_num_edit_reset(void) //清空batch编辑态，只保留已保存的batch值
{
    g_pcs_batch_num_lock_200 = false;
    memset(g_input_batch_num, 0, sizeof(g_input_batch_num));
    g_batch_num_index = 0;

    if (g_batch_num_display && lv_obj_is_valid(g_batch_num_display)) {
        lv_label_set_text(g_batch_num_display, "0");
        lv_obj_set_align(g_batch_num_display, LV_ALIGN_RIGHT_MID);
    }
}

void page_03_batch_num_edit_input(char input_num)
{
    if (input_num < '0' || input_num > '9' ||
        !g_batch_num_display || !lv_obj_is_valid(g_batch_num_display)) {
        return;
    }
    if (machine_state_batch_mode() == PCS_BATCH_MODE &&
        g_pcs_batch_num_lock_200) {
        lv_label_set_text(g_batch_num_display, "200");
        lv_obj_set_align(g_batch_num_display, LV_ALIGN_RIGHT_MID);
        return;
    }

    if (input_num == '0' &&
        (g_batch_num_index == 0 ||
         (g_batch_num_index == 1 && g_input_batch_num[0] == '0'))) {
        g_input_batch_num[0] = '0';
        g_input_batch_num[1] = '\0';
        g_batch_num_index = 1;
    } else if (g_batch_num_index == 1 && input_num != '0' &&
               g_input_batch_num[0] == '0') {
        g_input_batch_num[0] = input_num;
        g_input_batch_num[1] = '\0';
    } else if (g_batch_num_index >= 8) {
        memmove(g_input_batch_num, &g_input_batch_num[1], 7);
        g_input_batch_num[7] = input_num;
        g_input_batch_num[8] = '\0';
    } else {
        g_input_batch_num[g_batch_num_index++] = input_num;
        g_input_batch_num[g_batch_num_index] = '\0';
    }

    if (atoi(g_input_batch_num) > 200) {
        g_pcs_batch_num_lock_200 = true;
        strcpy(g_input_batch_num, "200");
        g_batch_num_index = 3;
    }
    lv_label_set_text(g_batch_num_display, g_input_batch_num);
    lv_obj_set_align(g_batch_num_display, LV_ALIGN_RIGHT_MID);
}

bool page_03_batch_num_edit_value(int* value)
{
    if (!value || g_batch_num_index <= 0) return false;
    *value = atoi(g_input_batch_num);
    return true;
}



// 添加长度变量
static int page_03_menu_len = 0;

// 定义动画和状态变量
static bool is_amount_active = false;  // 初始状态  PCS为激活状态
static lv_anim_t anim_amount;
static lv_anim_t anim_pcs;

typedef enum {
    PAGE_03_FUNCTION_BEEP = 0,
    PAGE_03_FUNCTION_SPEED,
    PAGE_03_FUNCTION_ADD,
    PAGE_03_FUNCTION_SORT,
    PAGE_03_FUNCTION_WORK,
} page_03_function_t;

static lv_obj_t* g_page_03_preview_icon = NULL;
static lv_obj_t* g_page_03_preview_mode = NULL;
static lv_obj_t* g_page_03_preview_state = NULL;
static lv_obj_t* g_page_03_preview_orb = NULL;
static lv_obj_t* g_page_03_preview_flow = NULL;
static lv_timer_t* g_page_03_preview_timer = NULL;
static bool g_page_03_preview_feedback = false;
static bool g_page_03_preview_needs_idle_refresh = false;
static uint32_t g_page_03_preview_started = 0;
static uint32_t g_page_03_preview_feedback_started = 0;
static machine_state_snapshot_t g_page_03_snapshot;
static bool g_page_03_snapshot_valid = false;

static void page_03_create_decor(void);
static void page_03_apply_modern_style(void);
static void page_03_create_preview(void);
static void page_03_create_batch_num_area(void);
static void page_03_create_batch_label_switcher(void);

static bool page_03_get_batch_labels(lv_obj_t** amount, lv_obj_t** pcs)
{
    if (!menu_page || !lv_obj_is_valid(menu_page)) {
        *amount = NULL;
        *pcs = NULL;
        return false;
    }

    if (!g_batch_amount_label || !lv_obj_is_valid(g_batch_amount_label)) {
        g_batch_amount_label = find_obj_by_name(
            "03_amount_batch_label", page_03_menu_obj, page_03_menu_len);
    }
    if (!g_batch_pcs_label || !lv_obj_is_valid(g_batch_pcs_label)) {
        g_batch_pcs_label = find_obj_by_name(
            "03_pcs_batch_label", page_03_menu_obj, page_03_menu_len);
    }
    *amount = g_batch_amount_label;
    *pcs = g_batch_pcs_label;
    return *amount != NULL && lv_obj_is_valid(*amount) &&
           *pcs != NULL && lv_obj_is_valid(*pcs);
}

void page_03_menu_refresh_batch_mode(void)
{
    lv_obj_t* amount;
    lv_obj_t* pcs;
    bool amount_active = machine_state_batch_mode() == AMOUNT_BATCH_MODE;
    lv_color_t active_color = machine_state_batch_enabled()
                              ? lv_color_hex(0x4285F4)
                              : lv_color_hex(0x888888);

    if (!page_03_menu_is_visible() ||
        !page_03_get_batch_labels(&amount, &pcs)) {
        return;
    }

    lv_obj_set_style_text_color(amount, amount_active ? active_color
                                                       : lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_opa(amount, amount_active ? 255 : 40, 0);
    lv_obj_set_style_text_color(pcs, amount_active ? lv_color_hex(0x888888)
                                                   : active_color, 0);
    lv_obj_set_style_text_opa(pcs, amount_active ? 40 : 255, 0);
}

void page_03_menu_refresh_batch_number(void)
{
    if (!page_03_menu_is_visible()) {
        return;
    }

    if (!machine_state_batch_enabled()) {
        update_label_by_name(page_03_menu_obj, page_03_menu_len,
                             "03_batch_num_label", "%s", "OFF");
    } else {
        int batch_num = machine_state_batch_num();
        update_label_by_name(page_03_menu_obj, page_03_menu_len,
                             "03_batch_num_label", "%d",
                             batch_num > 0 ? batch_num : 200);
    }
    page_03_batch_num_edit_reset();
}

static void page_03_create_batch_label_switcher(void)
{
    lv_obj_t* amount;
    lv_obj_t* pcs;
    lv_obj_t* container = lv_obj_create(menu_page);
    bool amount_active = machine_state_batch_mode() == AMOUNT_BATCH_MODE;

    lv_obj_remove_style_all(container);
    lv_obj_set_pos(container, 52, 37);
    lv_obj_set_size(container, 460, 146);
    lv_obj_set_style_bg_color(container, lv_color_hex(0xfffffdd6), 0);
    lv_obj_set_style_radius(container, 10, 0);
    lv_obj_add_flag(container, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

    if (!page_03_get_batch_labels(&amount, &pcs)) {
        LV_LOG_WARN("amount_obj or pcs_obj not found. Check names or UI structure.");
        return;
    }

    is_amount_active = amount_active;
    lv_obj_set_parent(amount, container);
    lv_obj_set_size(amount, 210, 35);
    lv_obj_set_pos(amount, 118, amount_active ? 63 : 95);
    lv_label_set_long_mode(amount, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(amount, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_set_parent(pcs, container);
    lv_obj_set_size(pcs, 160, 35);
    lv_obj_set_pos(pcs, 143, amount_active ? 95 : 63);
    lv_label_set_long_mode(pcs, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(pcs, LV_TEXT_ALIGN_CENTER, 0);

    page_03_menu_refresh_batch_mode();
    lv_obj_add_event_cb(container, page_03_batch_label_input_event_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(container, page_03_batch_label_input_event_cb,
                        LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(container, page_03_batch_label_input_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(container, page_03_batch_label_input_event_cb,
                        LV_EVENT_GESTURE, NULL);
}

static void page_03_delete_batch_tip_cb(lv_timer_t* timer)
{
    lv_obj_t* label = (lv_obj_t*)timer->user_data;

    if (label && lv_obj_is_valid(label)) {
        lv_obj_del(label);
    }
    if (g_batch_tip_label == label) {
        g_batch_tip_label = NULL;
    }
    lv_timer_del(timer);
    g_batch_tip_timer = NULL;
}

void page_03_menu_clear_batch_tip(void)
{
    if (g_batch_tip_timer) {
        lv_timer_del(g_batch_tip_timer);
        g_batch_tip_timer = NULL;
    }
    if (g_batch_tip_label && lv_obj_is_valid(g_batch_tip_label)) {
        lv_obj_del(g_batch_tip_label);
    }
    g_batch_tip_label = NULL;
}

void page_03_menu_show_batch_saved_tip(void)
{
    page_03_menu_clear_batch_tip();
    if (!menu_page || !lv_obj_is_valid(menu_page)) {
        return;
    }

    g_batch_tip_label = lv_label_create(menu_page);
    lv_obj_set_pos(g_batch_tip_label, 90, 182);
    lv_obj_set_size(g_batch_tip_label, 400, 18);
    lv_obj_set_style_text_font(g_batch_tip_label, &lv_font_instrument_sans_semibold_12, 0);
    lv_obj_set_style_text_color(g_batch_tip_label, lv_color_hex(0x28A95B), 0);
    lv_obj_set_style_text_align(g_batch_tip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_batch_tip_label, "Batch num saved successfully!");
    g_batch_tip_timer = lv_timer_create(page_03_delete_batch_tip_cb, 2000,
                                        g_batch_tip_label);
}

static void page_03_create_batch_num_area(void)
{
    lv_obj_t* batch_num_area = lv_obj_create(menu_page);
    lv_obj_set_size(batch_num_area, 338, 49);
    lv_obj_set_pos(batch_num_area, 119, 156);
    lv_obj_set_style_bg_opa(batch_num_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(batch_num_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(batch_num_area, 1, 0);
    lv_obj_set_style_radius(batch_num_area, 48, 0);
    lv_obj_clear_flag(batch_num_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(batch_num_area, LV_SCROLLBAR_MODE_OFF);

    g_batch_num_display = lv_label_create(batch_num_area);
    lv_obj_set_style_text_font(g_batch_num_display, &lv_font_manrope_bold_32, 0);
    lv_obj_set_style_text_color(g_batch_num_display, lv_color_hex(0x000000), 0);
    lv_obj_set_align(g_batch_num_display, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(g_batch_num_display, "0");

    lv_obj_t* amount_obj;
    lv_obj_t* pcs_obj;
    if (page_03_get_batch_labels(&amount_obj, &pcs_obj)) {
        lv_obj_set_style_text_opa(pcs_obj, 255, 0);
        lv_obj_set_style_text_opa(amount_obj, 40, 0);
    }
}

// 定义初始位置常量（相对于容器的坐标）
#define PCS_ACTIVE_Y 63     
#define AMOUNT_INACTIVE_Y 95 

// 动画回调函数 控制amount_label标签Y位置
static void amount_batch_label_anim_cb(void* var, int32_t v)
{
    lv_obj_t* label = (lv_obj_t*)var;
    lv_obj_set_y(label, v);
}

//  控制pcs_label标签Y位置
static void pcs_batch_label_anim_cb(void* var, int32_t v)
{
    lv_obj_t* label = (lv_obj_t*)var;
    lv_obj_set_y(label, v);
}

static ui_element_t page_03_menu_obj[] = {
    { "page_02_menu_bg.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 0, 0, 1280, 400, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

//**************BTN************//
  { "03_home_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 1149, 189, 100, 100, 244, 244, 255 },
        { NULL, 0, 0, 0, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 18, 0, false },
        page_01_back_btn_event_cb, LV_EVENT_CLICKED, NULL, NULL,
        UI_BTN_STYLE_APPLE },

    { "03_beep_on_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 710, 97, 84, 38, 100, 100, 100 },
        { "ON", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_cfd_mode_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_ANDROID },

    { "03_beep_off_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 814, 97, 84, 38, 100, 100, 100 },
        { "OFF", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_cfd_mode_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_ANDROID },

    { "03_speed_800_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 710, 155, 90, 38, 100, 100, 100 },
        { "LOW", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_speed_mode_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_ANDROID },

    { "03_speed_1000_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 820, 155, 90, 38, 100, 100, 100 },
        { "MID", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_speed_mode_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_ANDROID },

    { "03_speed_1200_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 930, 155, 90, 38, 100, 100, 100 },
        { "HIGH", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_speed_mode_event_cb, LV_EVENT_CLICKED, "2", NULL, UI_BTN_STYLE_ANDROID },

    { "03_add_on_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 710, 218, 84, 38, 100, 100, 100 },
        { "ON", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_add_mode_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_ANDROID },

    { "03_add_off_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 814, 218, 84, 38, 100, 100, 100 },
        { "OFF", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_add_mode_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_ANDROID },

    { "03_fo_OFF_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 710, 286, 84, 38, 100, 100, 100 },
        { "OFF", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_fo_mode_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_ANDROID },

    { "03_fo_F_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 814, 286, 84, 38, 100, 100, 100 },
        { "F", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_fo_mode_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_ANDROID },

    { "03_fo_O_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 918, 286, 84, 38, 100, 100, 100 },
        { "O", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_fo_mode_event_cb, LV_EVENT_CLICKED, "2", NULL, UI_BTN_STYLE_ANDROID },

    { "03_fo_FO_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 1022, 286, 84, 38, 100, 100, 100 },
        { "F/O", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_fo_mode_event_cb, LV_EVENT_CLICKED, "3", NULL, UI_BTN_STYLE_ANDROID },

    { "03_work_auto_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 710, 341, 84, 38, 100, 100, 100 },
        { "AUTO", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_work_mode_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_ANDROID },

    { "03_work_manaul_btn", LV_OBJ_TYPE_BUTTON, NULL,
        { 814, 341, 120, 38, 100, 100, 100 },
        { "MANAUL", 255, 255, 255, &lv_font_instrument_sans_medium_24, LV_TEXT_ALIGN_CENTER },
        { 255, 10, 0, false },
        page_03_work_mode_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_ANDROID },
//**********img************//


    { "page_02_home_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1171, 212, 55, 43, 255, 255, 255 },
        { NULL, 0, 0, 0, NULL },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE},
    

    { "03_menu_title_label", LV_OBJ_TYPE_LABEL, NULL,
        { 620, 13, 100, 36, 112, 112, 112 },
        { "MENU", 80, 80, 80, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },
    

    { "03_batch_title", LV_OBJ_TYPE_LABEL, NULL,
        { 178, 59, 210, 36, 255, 255, 255 },
        { "BATCH SETTING", 255, 255, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

    { "03_batch_title", LV_OBJ_TYPE_LABEL, NULL,
        { 564, 59, 693, 36, 255, 255, 255 },
        { "FUNCTION", 255, 255, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },
    
    { "03_pcs_batch_label", LV_OBJ_TYPE_LABEL, NULL,
        { 165, 100, 212, 21, 255, 255, 255 },
        { "PCS BATCH", 47, 103, 242, &lv_font_instrument_sans_semibold_24, 0 },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },
    
    { "03_amount_batch_label", LV_OBJ_TYPE_LABEL, NULL,
        { 170, 132, 200, 15, 255, 255, 255 },
        { "AMOUNT BATCH", 80, 80, 80, &lv_font_instrument_sans_bold_24, 0 },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

    { "03_batch_num_mix_label", LV_OBJ_TYPE_LABEL, NULL,
        { 102, 354, 200, 15, 255, 255, 255 },
        { "BATCH NUM:", 152, 40, 48, &lv_font_instrument_sans_semibold_20, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },
    { "03_batch_num_label", LV_OBJ_TYPE_LABEL, NULL,
        { 252, 352, 200, 20, 255, 255, 255 },
        { "0", 152, 40, 48, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

//function_mode
    { "03_beep_label", LV_OBJ_TYPE_LABEL, NULL,
        { 616, 105, 100, 40, 255, 255, 255 },
        { "BEEP", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },
    { "03_speed_label", LV_OBJ_TYPE_LABEL, NULL,
        { 616, 163, 100, 40, 255, 255, 255 },
        { "SPEED", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

    { "03_add_label", LV_OBJ_TYPE_LABEL, NULL,
        { 616, 226, 100, 40, 255, 255, 255 },
        { "ADD", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },


    { "03_f_o_label", LV_OBJ_TYPE_LABEL, NULL,
        { 616, 294, 100, 40, 255, 255, 255 },
        { "SORT", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

    { "03_word_label", LV_OBJ_TYPE_LABEL, NULL,
        { 616, 349, 100, 40, 255, 255, 255 },
        { "WORK", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_LEFT },
        { 255, 18, 0, false },
        NULL, 0, NULL, NULL,
        UI_BTN_STYLE_NONE },

 //*****KEYPAD*******//
{ "key_1", LV_OBJ_TYPE_BUTTON, NULL,
    {  93, 225, 90, 35, 100,100,100 }, // 254 - 29 = 225
    { "1", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
    { 255,10,0,true },
    page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "1", NULL, UI_BTN_STYLE_NONE },

{ "key_2", LV_OBJ_TYPE_BUTTON, NULL,
    { 188, 225, 90, 35, 100,100,100 },
    { "2", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
    { 255,10,0,true },
    page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "2", NULL, UI_BTN_STYLE_NONE },

{ "key_3", LV_OBJ_TYPE_BUTTON, NULL,
    { 283, 225, 90, 35, 100,100,100 },
    { "3", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
    { 255,10,0,true },
    page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "3", NULL, UI_BTN_STYLE_NONE },

{ "key_4", LV_OBJ_TYPE_BUTTON, NULL,
    { 378, 225, 90, 35, 100,100,100 },
    { "4", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
    { 255,10,0,true },
    page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "4", NULL, UI_BTN_STYLE_NONE },

    // 第二行：5 6 7 8
    { "key_5", LV_OBJ_TYPE_BUTTON, NULL,
        {  93, 265, 90, 35, 100,100,100 },
        { "5", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
        { 255,10,0,true },
        page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "5", NULL, UI_BTN_STYLE_NONE },

    { "key_6", LV_OBJ_TYPE_BUTTON, NULL,
        { 188, 265, 90, 35, 100,100,100 },
        { "6", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
        { 255,10,0,true },
        page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "6", NULL, UI_BTN_STYLE_NONE },

    { "key_7", LV_OBJ_TYPE_BUTTON, NULL,
        { 283, 265, 90, 35, 100,100,100 },
        { "7",255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
        { 255,10,0,true },
        page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "7", NULL, UI_BTN_STYLE_NONE },

    { "key_8", LV_OBJ_TYPE_BUTTON, NULL,
        { 378, 265, 90, 35, 100,100,100 },
        { "8", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
        { 255,10,0,true },
        page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "8", NULL, UI_BTN_STYLE_NONE },

        // 第三行：9 0 Del Enter
        { "key_9", LV_OBJ_TYPE_BUTTON, NULL,
            {  93, 305, 90, 35, 100,100,100 },
            { "9", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
            { 255,10,0,true },
            page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "9", NULL, UI_BTN_STYLE_NONE },

        { "key_0", LV_OBJ_TYPE_BUTTON, NULL,
            { 188, 305, 90, 35, 100,100,100 },
            { "0", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
            { 255,10,0,true },
            page_03_batch_num_keypad_event_cb, LV_EVENT_CLICKED, "0", NULL, UI_BTN_STYLE_NONE },

        { "key_del", LV_OBJ_TYPE_BUTTON, NULL,
            { 283, 305, 90, 35, 120,120, 120 },
            { " ", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
            { 255,10,0,true },
            page_03_batch_num_keypad_clear_event_cb, LV_EVENT_CLICKED, NULL, NULL, UI_BTN_STYLE_NONE },

        { "key_enter", LV_OBJ_TYPE_BUTTON, NULL,
            { 378, 305, 90, 35, 72,172,  80 },
            { " ", 255,255,255, &lv_font_instrument_sans_bold_24, LV_TEXT_ALIGN_CENTER },
            { 255,10,0,true },
            page_03_batch_num_keypad_enter_event_cb, LV_EVENT_CLICKED, NULL, NULL, UI_BTN_STYLE_NONE },

        { "page_03_del_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 410, 312, 40, 40, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

        { "page_03_ok_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
            { 315, 315, 40, 40, 0, 0, 0 },
            { NULL, 0, 0, 0, NULL },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },
};

lv_obj_t *page_03_menu_find_obj(const char *name)
{
    return find_obj_by_name(name, page_03_menu_obj, page_03_menu_len);
}

bool page_03_menu_is_created(void)
{
    return menu_page != NULL && lv_obj_is_valid(menu_page);
}

bool page_03_menu_is_visible(void)
{
    return page_03_menu_is_created() &&
           !lv_obj_has_flag(menu_page, LV_OBJ_FLAG_HIDDEN);
}

void page_03_menu_icon_feedback(const char *name)
{
    icon_feedback_comp(name, page_03_menu_obj, page_03_menu_len);
}

static lv_obj_t* page_03_find(const char* name)
{
    return page_03_menu_find_obj(name);
}

static void page_03_bg_to_back(void)
{
    lv_obj_t* bg = page_03_find("page_02_menu_bg.png");

    if (bg && lv_obj_is_valid(bg)) {
        lv_obj_move_background(bg);
    }
}

static lv_obj_t* page_03_create_plain(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h, uint32_t color,
                                      lv_coord_t radius)
{
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t* page_03_create_text(lv_obj_t* parent, const char* text,
                                     const lv_font_t* font, uint32_t color,
                                     lv_coord_t x, lv_coord_t y)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void page_03_style_card(lv_obj_t* card)
{
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE3E7EC), 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x9AA7B5), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_width(card, 14, 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);
}

static void page_03_create_decor(void)
{
    static const ui_text_id_t row_text[] = {
        UI_TEXT_MENU_FUNCTION_BEEP,
        UI_TEXT_MENU_FUNCTION_SPEED,
        UI_TEXT_MENU_FUNCTION_ADD,
        UI_TEXT_MENU_FUNCTION_SORT,
        UI_TEXT_MENU_FUNCTION_WORK,
    };
    static const char* row_icon[] = {
        LV_SYMBOL_VOLUME_MAX,
        LV_SYMBOL_CHARGE,
        LV_SYMBOL_PLUS,
        LV_SYMBOL_SHUFFLE,
        LV_SYMBOL_PLAY,
    };

    lv_obj_t* batch_card = page_03_create_plain(menu_page, 33, 16, 509, 372,
                                                0xFFFFFF, 12);
    page_03_style_card(batch_card);

    lv_obj_t* batch_accent = page_03_create_plain(menu_page, 68, 28, 7, 38,
                                                  0x24C7DD, 4);
    page_03_create_text(menu_page, ui_text_get(UI_TEXT_MENU_BATCH_TITLE),
                        &lv_font_instrument_sans_bold_20, 0x101114, 90, 25);
    page_03_create_text(menu_page, ui_text_get(UI_TEXT_MENU_BATCH_SUBTITLE),
                        &lv_font_instrument_sans_medium_12, 0x7992C5, 90, 51);

    lv_obj_t* batch_line = page_03_create_plain(menu_page, 33, 83, 509, 1,
                                                0xE9ECEF, 0);
    lv_obj_t* input_line = page_03_create_plain(menu_page, 85, 174, 405, 2,
                                                0x0B69FF, 0);

    lv_obj_t* function_card = page_03_create_plain(menu_page, 583, 16, 568, 372,
                                                   0xFFFFFF, 12);
    page_03_style_card(function_card);

    for (int i = 0; i < 4; i++) {
        lv_coord_t x = 600 + (i % 2) * 12;
        lv_coord_t y = 29 + (i / 2) * 12;
        page_03_create_plain(menu_page, x, y, 9, 9, 0x2E6BFF, 0);
    }
    page_03_create_text(menu_page, ui_text_get(UI_TEXT_MENU_FUNCTION_TITLE),
                        &lv_font_instrument_sans_bold_20, 0x101114, 635, 25);
    page_03_create_text(menu_page, ui_text_get(UI_TEXT_MENU_FUNCTION_SUBTITLE),
                        &lv_font_instrument_sans_medium_12, 0x7992C5, 635, 51);

    lv_obj_t* function_line = page_03_create_plain(menu_page, 583, 83, 568, 1,
                                                   0xE9ECEF, 0);
    for (int i = 0; i < 5; i++) {
        lv_coord_t y = 94 + i * 59;
        lv_obj_t* row = page_03_create_plain(menu_page, 604, y, 526, 45,
                                             0xFFFFFF, 5);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xE4E7EB), 0);
        lv_obj_set_style_shadow_color(row, lv_color_hex(0xC8CDD5), 0);
        lv_obj_set_style_shadow_opa(row, LV_OPA_10, 0);
        lv_obj_set_style_shadow_width(row, 4, 0);
        lv_obj_set_style_shadow_ofs_y(row, 1, 0);

        lv_obj_t* icon_box = page_03_create_plain(menu_page, 618, y + 11, 22, 22,
                                                  0xE8F1FF, 5);
        lv_obj_t* icon = page_03_create_text(icon_box, row_icon[i],
                                             &lv_font_montserrat_14,
                                             0x0B69FF, 0, 0);
        lv_obj_center(icon);
        page_03_create_text(menu_page, ui_text_get(row_text[i]),
                            &lv_font_instrument_sans_bold_14,
                            0x26354D, 654, y + 13);

        lv_obj_move_background(row);
    }

    lv_obj_move_background(function_line);
    lv_obj_move_background(input_line);
    lv_obj_move_background(batch_line);
    lv_obj_move_background(function_card);
    lv_obj_move_background(batch_accent);
    lv_obj_move_background(batch_card);
    page_03_bg_to_back();
}

static void page_03_style_button(lv_obj_t* btn, lv_coord_t x, lv_coord_t y,
                                 lv_coord_t w, lv_coord_t h)
{
    if (!btn) return;

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xEEF2F7), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xD7E6FF), LV_STATE_PRESSED);
    lv_obj_set_style_transform_zoom(btn, 256, LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 1, LV_STATE_PRESSED);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) {
        lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x747B84), 0);
    }
}

static void page_03_style_function_button(lv_obj_t* btn, lv_coord_t x, lv_coord_t y,
                                          lv_coord_t w, lv_coord_t h)
{
    page_03_style_button(btn, x, y, w, h);
    if (!btn) return;

    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xE1E6ED), 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x9AA8BA), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_width(btn, 7, 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 2, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xDFEAFA), LV_STATE_PRESSED);

    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) {
        lv_obj_set_style_text_font(label, &lv_font_instrument_sans_bold_14, 0);
    }
}

static void page_03_apply_modern_style(void)
{
    static const char* key_name[] = {
        "key_1", "key_2", "key_3", "key_4",
        "key_5", "key_6", "key_7", "key_8",
        "key_9", "key_0", "key_del", "key_enter",
    };
    static const char* function_name[] = {
        "03_beep_off_btn", "03_beep_on_btn",
        "03_speed_800_btn", "03_speed_1000_btn", "03_speed_1200_btn",
        "03_add_off_btn", "03_add_on_btn",
        "03_fo_OFF_btn", "03_fo_F_btn", "03_fo_O_btn", "03_fo_FO_btn",
        "03_work_auto_btn", "03_work_manaul_btn",
    };
    static const lv_coord_t function_x[] = {
        782, 956,
        782, 895, 1008,
        782, 956,
        782, 867, 952, 1037,
        782, 956,
    };
    static const lv_coord_t function_y[] = {
        102, 102,
        161, 161, 161,
        220, 220,
        279, 279, 279, 279,
        338, 338,
    };
    static const lv_coord_t function_w[] = {
        166, 166,
        108, 108, 114,
        166, 166,
        78, 78, 78, 85,
        166, 166,
    };

    for (int i = 0; i < page_03_menu_len; i++) {
        if (strcmp(page_03_menu_obj[i].obj_name, "03_menu_title_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_batch_title") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_amount_batch_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_beep_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_speed_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_add_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_f_o_label") == 0 ||
            strcmp(page_03_menu_obj[i].obj_name, "03_word_label") == 0) {
            if (page_03_menu_obj[i].obj_ref) {
                lv_obj_add_flag(page_03_menu_obj[i].obj_ref, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    lv_obj_t* home = page_03_find("03_home_btn");
    if (home) {
        lv_obj_set_pos(home, 1169, 16);
        lv_obj_set_size(home, 97, 59);
        lv_obj_set_style_radius(home, 18, 0);
        lv_obj_set_style_bg_color(home, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(home, lv_color_hex(0xAEB5BE), 0);
        lv_obj_set_style_shadow_opa(home, LV_OPA_20, 0);
        lv_obj_set_style_shadow_width(home, 10, 0);
        lv_obj_set_style_shadow_ofs_y(home, 3, 0);
        lv_obj_set_style_bg_color(home, lv_color_hex(0xEEF3FA), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(home, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_transform_zoom(home, 256, LV_STATE_PRESSED);
        lv_obj_set_style_translate_y(home, 1, LV_STATE_PRESSED);
    }

    lv_obj_t* home_icon = page_03_find("page_02_home_icon.png");
    if (home_icon && home) {
        lv_img_set_zoom(home_icon, 170);
        lv_obj_align_to(home_icon, home, LV_ALIGN_CENTER, 0, 0);
        lv_obj_move_foreground(home_icon);
    }

    lv_obj_t* pcs = page_03_find("03_pcs_batch_label");
    if (pcs) {
        lv_label_set_text(pcs, ui_text_get(UI_TEXT_MENU_BATCH_PCS));
        lv_obj_set_pos(pcs, 20, 12);
        lv_obj_set_size(pcs, 120, 22);
        lv_obj_set_style_text_font(pcs, &lv_font_instrument_sans_medium_14, 0);
        lv_obj_set_style_text_color(pcs, lv_color_hex(0x101114), 0);
        lv_obj_set_style_text_align(pcs, LV_TEXT_ALIGN_LEFT, 0);
    }

    if (g_batch_num_display) {
        lv_obj_t* input = lv_obj_get_parent(g_batch_num_display);
        lv_obj_set_pos(input, 85, 99);
        lv_obj_set_size(input, 405, 77);
        lv_obj_set_style_bg_color(input, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(input, lv_color_hex(0xE2E6EB), 0);
        lv_obj_set_style_border_opa(input, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(input, 1, 0);
        lv_obj_set_style_radius(input, 6, 0);
        lv_obj_set_style_pad_right(input, 24, 0);
        lv_obj_set_style_text_font(g_batch_num_display, &lv_font_manrope_bold_40, 0);
        lv_obj_set_style_text_color(g_batch_num_display, lv_color_hex(0x000000), 0);
    }

    lv_obj_t* batch_mode = pcs ? lv_obj_get_parent(pcs) : NULL;
    if (batch_mode) {
        lv_obj_set_pos(batch_mode, 85, 99);
        lv_obj_set_size(batch_mode, 405, 77);
        lv_obj_set_style_bg_opa(batch_mode, LV_OPA_TRANSP, 0);
    }

    for (int i = 0; i < 12; i++) {
        int col = i % 4;
        int row = i / 4;
        lv_obj_t* key = page_03_find(key_name[i]);
        page_03_style_button(key, 90 + col * 102, 206 + row * 52, 92, 32);
        if (key) {
            bool action = i >= 10;
            lv_obj_set_style_radius(key, 8, 0);
            lv_obj_set_style_border_width(key, 0, 0);
            lv_obj_set_style_shadow_color(key, lv_color_hex(0x8A95A5), 0);
            lv_obj_set_style_shadow_opa(key, LV_OPA_20, 0);
            lv_obj_set_style_shadow_width(key, 5, 0);
            lv_obj_set_style_shadow_ofs_y(key, 2, 0);
            lv_obj_set_style_shadow_width(key, 1, LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(key,
                                     i == 11 ? lv_color_hex(0x0B69FF) :
                                     action ? lv_color_hex(0x263246) :
                                              lv_color_hex(0x202A3B), 0);
            lv_obj_set_style_bg_color(key,
                                     i == 11 ? lv_color_hex(0x0755D5) :
                                              lv_color_hex(0x111827), LV_STATE_PRESSED);
            lv_obj_t* label = lv_obj_get_child(key, 0);
            if (label) {
                if (i == 10) {
                    lv_label_set_text(label, ui_text_get(UI_TEXT_MENU_KEY_DELETE));
                } else if (i == 11) {
                    lv_label_set_text(label, ui_text_get(UI_TEXT_MENU_KEY_OK));
                }
                lv_obj_set_style_text_font(label,
                                           action ? &lv_font_instrument_sans_bold_14 :
                                                    &lv_font_instrument_sans_bold_16, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            }
        }
    }

    lv_obj_t* del_icon = page_03_find("page_03_ok_icon.png");
    if (del_icon) {
        lv_obj_add_flag(del_icon, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t* ok_icon = page_03_find("page_03_del_icon.png");
    if (ok_icon) {
        lv_obj_add_flag(ok_icon, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* batch_num_title = page_03_find("03_batch_num_mix_label");
    if (batch_num_title) {
        lv_label_set_text(batch_num_title, ui_text_get(UI_TEXT_MENU_BATCH_NUM));
        lv_obj_set_pos(batch_num_title, 90, 353);
        lv_obj_set_size(batch_num_title, 180, 26);
        lv_obj_set_style_text_font(batch_num_title, &lv_font_instrument_sans_medium_20, 0);
        lv_obj_set_style_text_color(batch_num_title, lv_color_hex(0x0B69FF), 0);
    }

    lv_obj_t* batch_num_value = page_03_find("03_batch_num_label");
    if (batch_num_value) {
        lv_obj_set_pos(batch_num_value, 293, 353);
        lv_obj_set_size(batch_num_value, 80, 26);
        lv_obj_set_style_text_font(batch_num_value, &lv_font_instrument_sans_bold_20, 0);
        lv_obj_set_style_text_color(batch_num_value, lv_color_hex(0x0B69FF), 0);
    }

    for (int i = 0; i < 13; i++) {
        page_03_style_function_button(page_03_find(function_name[i]), function_x[i],
                                      function_y[i], function_w[i], 29);
    }
}

static int32_t page_03_preview_wave(uint32_t elapsed, uint32_t period)
{
    uint32_t half = period / 2;
    uint32_t pos = elapsed % period;
    uint32_t linear = pos <= half ? pos * 1000 / half :
                                    (period - pos) * 1000 / half;
    int64_t smooth = (int64_t)linear * linear * (3000 - 2 * linear);
    return (int32_t)(smooth / 1000000);
}

static uint32_t page_03_preview_mix_color(uint32_t a, uint32_t b, int32_t t)
{
    uint8_t ar = (a >> 16) & 0xFF;
    uint8_t ag = (a >> 8) & 0xFF;
    uint8_t ab = a & 0xFF;
    uint8_t br = (b >> 16) & 0xFF;
    uint8_t bg = (b >> 8) & 0xFF;
    uint8_t bb = b & 0xFF;
    uint8_t r = ar + (br - ar) * t / 1000;
    uint8_t g = ag + (bg - ag) * t / 1000;
    uint8_t bl = ab + (bb - ab) * t / 1000;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

#define PAGE_03_PREVIEW_DOT_SIZE       17
#define PAGE_03_PREVIEW_ARC_VALUE      38
#define PAGE_03_PREVIEW_FEEDBACK_MS    2200
#define PAGE_03_PREVIEW_DOT_LIGHT      0x45D7E8
#define PAGE_03_PREVIEW_DOT_DEEP       0x0651D8

static void page_03_preview_set_labels(const char* mode, const char* state)
{
    if (g_page_03_preview_mode) {
        lv_label_set_text(g_page_03_preview_mode, mode ? mode : "");
    }

    if (g_page_03_preview_state) {
        lv_label_set_text(g_page_03_preview_state, state ? state : "");
    }
}

static const char* page_03_preview_state_text(uint8_t function, uint8_t value)
{
    if (function == PAGE_03_FUNCTION_SPEED) {
        return ui_text_get(value == 0 ? UI_TEXT_MENU_STATE_LOW :
                           value == 1 ? UI_TEXT_MENU_STATE_MID :
                                        UI_TEXT_MENU_STATE_HIGH);
    }

    if (function == PAGE_03_FUNCTION_SORT) {
        return value == 1 ? "F" : value == 2 ? "O" : value == 3 ? "F/O" :
               ui_text_get(UI_TEXT_MENU_STATE_OFF);
    }

    if (function == PAGE_03_FUNCTION_WORK) {
        return ui_text_get(value == 0 ? UI_TEXT_MENU_STATE_AUTO :
                                        UI_TEXT_MENU_STATE_MANUAL);
    }

    return ui_text_get(value ? UI_TEXT_MENU_STATE_ON : UI_TEXT_MENU_STATE_OFF);
}

static void page_03_preview_set_system(uint32_t color, uint16_t rotation)
{
    if (g_page_03_preview_icon) {
        lv_obj_set_style_bg_color(g_page_03_preview_icon, lv_color_hex(color), 0);
        lv_obj_set_style_shadow_color(g_page_03_preview_icon, lv_color_hex(color), 0);
    }

    if (g_page_03_preview_flow) {
        lv_arc_set_rotation(g_page_03_preview_flow, (int16_t)rotation);
    }
}

static void page_03_preview_idle(void)
{
    if (!g_page_03_preview_orb || !lv_obj_is_valid(g_page_03_preview_orb)) return;

    g_page_03_preview_feedback = false;
    page_03_preview_set_labels(ui_text_get(UI_TEXT_MENU_PREVIEW_SYNCING),
                               ui_text_get(UI_TEXT_MENU_PREVIEW_REALTIME));
}

static void page_03_preview_timer_cb(lv_timer_t* timer)
{
    LV_UNUSED(timer);
    if (!g_page_03_preview_orb || !lv_obj_is_valid(g_page_03_preview_orb)) return;

    uint32_t elapsed = lv_tick_elaps(g_page_03_preview_started);
    int32_t breath = page_03_preview_wave(elapsed, 1800);
    uint16_t rotation = (uint16_t)((elapsed / 7) % 360);
    uint32_t glow = page_03_preview_mix_color(PAGE_03_PREVIEW_DOT_LIGHT,
                                              PAGE_03_PREVIEW_DOT_DEEP,
                                              breath);

    if (g_page_03_preview_feedback &&
        lv_tick_elaps(g_page_03_preview_feedback_started) >= PAGE_03_PREVIEW_FEEDBACK_MS) {
        page_03_preview_idle();
    }

    page_03_preview_set_system(glow, (uint16_t)((270 + rotation) % 360));
}

static void page_03_create_preview(void)
{
    lv_obj_t* card = page_03_create_plain(menu_page, 1169, 83, 97, 305,
                                          0xF8FBFF, 6);
    page_03_style_card(card);

    lv_obj_t* title = page_03_create_text(card, ui_text_get(UI_TEXT_MENU_PREVIEW_SYSTEM),
                                          &lv_font_instrument_sans_bold_16,
                                          0x0B69FF, 0, 24);
    lv_obj_set_width(title, 97);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);

    g_page_03_preview_flow = lv_arc_create(card);
    lv_obj_remove_style(g_page_03_preview_flow, NULL, LV_PART_KNOB);
    lv_obj_set_pos(g_page_03_preview_flow, 14, 79);
    lv_obj_set_size(g_page_03_preview_flow, 69, 69);
    lv_arc_set_range(g_page_03_preview_flow, 0, 100);
    lv_arc_set_bg_angles(g_page_03_preview_flow, 0, 360);
    lv_arc_set_rotation(g_page_03_preview_flow, 270);
    lv_arc_set_value(g_page_03_preview_flow, PAGE_03_PREVIEW_ARC_VALUE);
    lv_obj_set_style_arc_width(g_page_03_preview_flow, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_page_03_preview_flow, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_page_03_preview_flow, lv_color_hex(0xE7EEF8), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_page_03_preview_flow, lv_color_hex(0x0B69FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(g_page_03_preview_flow, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(g_page_03_preview_flow, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_page_03_preview_flow, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g_page_03_preview_flow, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(g_page_03_preview_flow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_page_03_preview_flow, LV_OBJ_FLAG_SCROLLABLE);

    g_page_03_preview_orb = page_03_create_plain(card, 23, 88, 51, 51,
                                                 0x4AD5EE, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(g_page_03_preview_orb, LV_OPA_20, 0);
    lv_obj_set_style_border_width(g_page_03_preview_orb, 1, 0);
    lv_obj_set_style_border_color(g_page_03_preview_orb, lv_color_hex(0xCFE7FF), 0);
    lv_obj_set_style_border_opa(g_page_03_preview_orb, LV_OPA_70, 0);
    lv_obj_set_style_shadow_color(g_page_03_preview_orb, lv_color_hex(0x0B69FF), 0);
    lv_obj_set_style_shadow_opa(g_page_03_preview_orb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(g_page_03_preview_orb, 14, 0);
    lv_obj_set_style_shadow_ofs_y(g_page_03_preview_orb, 0, 0);

    g_page_03_preview_icon = page_03_create_plain(card, 40, 105, 17, 17,
                                                  0x32D7E8, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(g_page_03_preview_icon, 2, 0);
    lv_obj_set_style_border_color(g_page_03_preview_icon, lv_color_hex(0xE8FBFF), 0);
    lv_obj_set_style_border_opa(g_page_03_preview_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(g_page_03_preview_icon, lv_color_hex(0x32D7E8), 0);
    lv_obj_set_style_shadow_opa(g_page_03_preview_icon, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(g_page_03_preview_icon, 7, 0);

    g_page_03_preview_mode = page_03_create_text(card, "",
                                                 &lv_font_instrument_sans_bold_12,
                                                 0x111827, 5, 181);
    lv_obj_set_size(g_page_03_preview_mode, 87, 16);
    lv_obj_set_style_text_align(g_page_03_preview_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(g_page_03_preview_mode, 1, 0);

    g_page_03_preview_state = page_03_create_text(card, "",
                                                  &lv_font_instrument_sans_medium_10,
                                                  0x8A99AD, 5, 199);
    lv_obj_set_size(g_page_03_preview_state, 87, 14);
    lv_obj_set_style_text_align(g_page_03_preview_state, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(g_page_03_preview_state, 1, 0);

    page_03_create_plain(card, 21, 255, 54, 6, 0xE9EEF5, 3);
    page_03_create_plain(card, 21, 269, 54, 6, 0xE9EEF5, 3);
    page_03_create_plain(card, 21, 283, 54, 6, 0xE9EEF5, 3);

    for (int i = 0; i < 3; i++) {
        lv_coord_t bar_w = 54 - i * 10;
        page_03_create_plain(card, 21, 255 + i * 14, bar_w, 6,
                             i == 0 ? 0x0B69FF :
                             i == 1 ? 0x47D6E5 : 0x79B4FF, 3);
    }

    g_page_03_preview_started = lv_tick_get();
    g_page_03_preview_timer = lv_timer_create(page_03_preview_timer_cb, 30, NULL);
    page_03_preview_idle();
}

void page_03_menu_function_feedback(uint8_t function, uint8_t value)
{
    static const ui_text_id_t title[] = {
        UI_TEXT_MENU_FUNCTION_BEEP,
        UI_TEXT_MENU_FUNCTION_SPEED,
        UI_TEXT_MENU_FUNCTION_ADD,
        UI_TEXT_MENU_FUNCTION_SORT,
        UI_TEXT_MENU_FUNCTION_WORK,
    };

    if (!page_03_menu_is_visible() ||
        function > PAGE_03_FUNCTION_WORK ||
        !g_page_03_preview_orb || !lv_obj_is_valid(g_page_03_preview_orb)) {
        return;
    }

    g_page_03_preview_feedback = true;
    g_page_03_preview_feedback_started = lv_tick_get();
    page_03_preview_set_labels(ui_text_get(title[function]),
                               page_03_preview_state_text(function, value));

    page_03_preview_timer_cb(g_page_03_preview_timer);
}

void page_03_menu_function_focus(uint8_t function)
{
    uint8_t value = 0;

    if (function == PAGE_03_FUNCTION_BEEP) value = machine_state_buzzer_enabled();
    else if (function == PAGE_03_FUNCTION_SPEED) value = machine_state_speed();
    else if (function == PAGE_03_FUNCTION_ADD) value = machine_state_add_enabled();
    else if (function == PAGE_03_FUNCTION_SORT) value = machine_state_fo_mode();
    else if (function == PAGE_03_FUNCTION_WORK) value = machine_state_work_mode();

    page_03_menu_function_feedback(function, value);
}

void page_03_menu_preview_refresh(void)
{
    if (!g_page_03_preview_feedback) {
        page_03_preview_idle();
    }
}

// 切换到AMOUNT BATCH激活状态
void switch_to_amount_batch(void)
{
    /* Amount batch 暂未启用：先停用切换与动画逻辑，后续直接取消注释恢复 */
    return;
#if 0
    if (is_amount_active) return;

    is_amount_active = true;  
    
    lv_obj_t* amount_obj = find_obj_by_name("03_amount_batch_label", page_03_menu_obj, page_03_menu_len);
    
    // AMOUNT BATCH 向上移动动画
    lv_anim_init(&anim_amount);
    lv_anim_set_var(&anim_amount, amount_obj);
    lv_anim_set_exec_cb(&anim_amount, amount_batch_label_anim_cb);
    lv_anim_set_values(&anim_amount, AMOUNT_INACTIVE_Y, PCS_ACTIVE_Y);
    lv_anim_set_path_cb(&anim_amount, lv_anim_path_ease_out);  // 缓出动画
    lv_anim_start(&anim_amount);

    lv_obj_t* pcs_obj = find_obj_by_name("03_pcs_batch_label", page_03_menu_obj, page_03_menu_len);
    
    // PCS BATCH 向下移动动画
    lv_anim_init(&anim_pcs);
    lv_anim_set_var(&anim_pcs, pcs_obj);
    lv_anim_set_exec_cb(&anim_pcs, pcs_batch_label_anim_cb);
    lv_anim_set_values(&anim_pcs, PCS_ACTIVE_Y, AMOUNT_INACTIVE_Y);
    lv_anim_set_time(&anim_pcs, 300);
    lv_anim_set_path_cb(&anim_pcs, lv_anim_path_ease_out);
    lv_anim_start(&anim_pcs);

    lv_obj_set_style_text_color(amount_obj, lv_color_hex(0x4285F4), 0); 
    lv_obj_set_style_text_color(pcs_obj, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_opa(pcs_obj, 40, 0);  // 半透明
    lv_obj_set_style_text_opa(amount_obj, 255, 0);  
#endif

}

void switch_to_pcs_batch(void)
{
    lv_obj_t* amount_obj;
    lv_obj_t* pcs_obj;

    if (!is_amount_active) return;
    if (!page_03_get_batch_labels(&amount_obj, &pcs_obj)) return;

    is_amount_active = false;  
    
    lv_anim_init(&anim_pcs);
    lv_anim_set_var(&anim_pcs, pcs_obj);
    lv_anim_set_exec_cb(&anim_pcs, pcs_batch_label_anim_cb);
    lv_anim_set_values(&anim_pcs, AMOUNT_INACTIVE_Y, PCS_ACTIVE_Y);
    lv_anim_set_time(&anim_pcs, 300);
    lv_anim_set_path_cb(&anim_pcs, lv_anim_path_ease_out);
    lv_anim_start(&anim_pcs);

    lv_anim_init(&anim_amount);
    lv_anim_set_var(&anim_amount, amount_obj);
    lv_anim_set_exec_cb(&anim_amount, amount_batch_label_anim_cb);
    lv_anim_set_values(&anim_amount, PCS_ACTIVE_Y, AMOUNT_INACTIVE_Y); 
    lv_anim_set_time(&anim_amount, 300);
    lv_anim_set_path_cb(&anim_amount, lv_anim_path_ease_out);
    lv_anim_start(&anim_amount);

    lv_obj_set_style_text_color(pcs_obj, lv_color_hex(0x4285F4), 0);  
    lv_obj_set_style_text_color(amount_obj, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_opa(amount_obj, 40, 0);  
    lv_obj_set_style_text_opa(pcs_obj, 255, 0);  

    int num = atoi(g_input_batch_num);
    if (num > 200) {
        g_pcs_batch_num_lock_200 = true;
        strcpy(g_input_batch_num, "200");
        g_batch_num_index = 3;
        lv_label_set_text(g_batch_num_display, "200");
        lv_obj_set_align(g_batch_num_display, LV_ALIGN_RIGHT_MID);
    }

}





void toggle_batch_mode(void)
{
    /* Amount batch 暂未启用：菜单页固定 PCS 模式 */
    if (machine_state_batch_mode() != PCS_BATCH_MODE) {
        machine_state_confirm_batch_mode(PCS_BATCH_MODE);
        switch_to_pcs_batch();
    }
#if LV_DEBUG
    printf("machine batch mode: PCS MODE\n");
#endif
}

//BATCH SWITCH
// 创建批次标签切换器


// 创建页面UI
void ui_page_03_menu_create(lv_obj_t* parent)
{
    if (menu_page && lv_obj_is_valid(menu_page)) return;
    ui_page_03_menu_destroy();
    machine_state_confirm_batch_mode(PCS_BATCH_MODE);
    is_amount_active = false;
    page_03_batch_num_edit_reset();
    menu_page = lv_obj_create(parent ? parent : lv_scr_act());
    lv_obj_remove_style_all(menu_page);
    lv_obj_set_pos(menu_page, 0, 0);
    lv_obj_set_size(menu_page, 1280, 400);
    lv_obj_set_style_bg_color(menu_page, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(menu_page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(menu_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(menu_page, LV_SCROLLBAR_MODE_OFF);

    page_03_menu_len = sizeof(page_03_menu_obj) / sizeof(ui_element_t);
    lv_ui_obj_init(menu_page, page_03_menu_obj, page_03_menu_len);
    page_03_create_decor();
    page_03_create_batch_num_area();//BATCH_NUM_MIX
    page_03_create_batch_label_switcher();
    //刷新batch_num
    page_03_menu_refresh_batch_number();
    //batch开关
    create_batch_num_switch(menu_page);
    lv_obj_t* obj = get_batch_switch_container();
    lv_obj_set_pos(obj, 411, 32);
    
    // 确保开关状态与已确认 Batch enable 一致
    set_batch_switch_state(machine_state_batch_enabled());

    page_03_apply_modern_style();
    page_03_create_plain(menu_page, 85, 174, 405, 2, 0x0B69FF, 0);
    page_03_create_preview();
    page_03_bg_to_back();

    //初始化fuction开关状态
    page_03_update_menu_button_states_refresh();
    machine_state_get_snapshot(&g_page_03_snapshot);
    g_page_03_snapshot_valid = true;

}

bool ui_page_03_menu_resume(void)
{
    machine_state_snapshot_t snapshot;
    bool batch_dirty;
    bool function_dirty;
    bool profile_enabled;
    uint64_t profile_started_us = 0;
    const char* profile_event;

    if (!page_03_menu_is_created()) {
        return false;
    }

    machine_state_get_snapshot(&snapshot);
    batch_dirty = !g_page_03_snapshot_valid ||
                  snapshot.batch_enabled != g_page_03_snapshot.batch_enabled ||
                  snapshot.batch_num != g_page_03_snapshot.batch_num ||
                  snapshot.batch_mode != g_page_03_snapshot.batch_mode ||
                  snapshot.batch_amount != g_page_03_snapshot.batch_amount;
    function_dirty = !g_page_03_snapshot_valid ||
                     snapshot.buzzer_enabled != g_page_03_snapshot.buzzer_enabled ||
                     snapshot.speed != g_page_03_snapshot.speed ||
                     snapshot.add_enabled != g_page_03_snapshot.add_enabled ||
                     snapshot.fo_mode != g_page_03_snapshot.fo_mode ||
                     snapshot.work_mode != g_page_03_snapshot.work_mode;
    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
    }

    lv_obj_clear_flag(menu_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menu_page);
    page_03_batch_num_edit_reset();
    if (batch_dirty) {
        page_03_menu_refresh_batch_mode();
        page_03_menu_refresh_batch_number();
        set_batch_switch_state(snapshot.batch_enabled);
    }
    if (function_dirty) {
        page_03_update_menu_button_states_refresh();
    }
    if (g_page_03_preview_needs_idle_refresh) {
        page_03_menu_preview_refresh();
        g_page_03_preview_needs_idle_refresh = false;
    }
    g_page_03_snapshot = snapshot;
    g_page_03_snapshot_valid = true;
    g_page_03_preview_started = lv_tick_get();
    if (g_page_03_preview_timer) {
        lv_timer_resume(g_page_03_preview_timer);
    }
    if (profile_enabled) {
        profile_event = batch_dirty && function_dirty ? "RESUME_MULTI" :
                        batch_dirty ? "RESUME_BATCH" :
                        function_dirty ? "RESUME_FUNCTION" : "RESUME_CLEAN";
        perf_profile_report_event_us(
            "MENU", profile_event,
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_03_menu_suspend(void)
{
    if (!page_03_menu_is_created()) {
        return;
    }

    page_03_menu_clear_batch_tip();
    g_page_03_preview_needs_idle_refresh = g_page_03_preview_feedback;
    g_page_03_preview_feedback = false;
    if (g_page_03_preview_timer) {
        lv_timer_pause(g_page_03_preview_timer);
    }
    machine_state_get_snapshot(&g_page_03_snapshot);
    g_page_03_snapshot_valid = true;
    lv_obj_add_flag(menu_page, LV_OBJ_FLAG_HIDDEN);
}

void ui_page_03_menu_destroy(void)
{
    page_03_function_button_cache_reset();
    if (g_page_03_preview_timer) {
        lv_timer_del(g_page_03_preview_timer);
        g_page_03_preview_timer = NULL;
    }
    page_03_menu_clear_batch_tip();
    if (menu_page && lv_obj_is_valid(menu_page)) {
        lv_obj_del(menu_page);
    }
    menu_page = NULL;
    g_batch_num_display = NULL;
    g_batch_amount_label = NULL;
    g_batch_pcs_label = NULL;
    g_page_03_preview_icon = NULL;
    g_page_03_preview_mode = NULL;
    g_page_03_preview_state = NULL;
    g_page_03_preview_orb = NULL;
    g_page_03_preview_flow = NULL;
    g_page_03_preview_feedback = false;
    g_page_03_preview_needs_idle_refresh = false;
    g_page_03_snapshot_valid = false;
}
