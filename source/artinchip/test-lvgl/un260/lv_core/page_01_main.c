#include "un260/lv_resources/ui_page_background.h"
#include "un260/lv_core/page_01_main.h"
#include "page_01_multi.h"
#include "un260/lv_core/page_01_main_detail.h"
#include "un260/lv_components/lv_loading_orbit.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/currency/currency_metadata.h"
#include "un260/font/main_fonts.h"
#include "un260/lv_core/lv_page_manager.h"
#include "ui_frame_commit.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "lv_page_event.h"
#include <stdio.h>
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/ui_object_utils.h"
#include <string.h>
#include "un260/lv_resources/lv_img_init.h" 
#include "../aic_ui/aic_ui.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_system/lv_str.h" 
#include "un260/lv_system/user_cfg.h"
#include "un260/machine_state/machine_state.h"
#include "un260/currency/currency_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/innovation/page_32_innovation.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include <stdint.h>

static lv_obj_t* main_page = NULL;
/* Names remain stable for the shared action feedback callbacks. Layout and
 * widget lifetime belong to this page, not the counting runtime. */
static ui_element_t page_01_main_obj[40];
static int page_01_main_len;
static lv_obj_t *s_summary_card, *s_detail_card, *s_multi_card, *s_detail_tray;
static lv_obj_t *s_detail_frame;
static lv_obj_t *s_amount_unit_icon, *s_currency_code;
static lv_obj_t *s_start_orbit;
static bool s_multi_layout;
static lv_obj_t* s_time_label = NULL;
static lv_timer_t* s_time_timer = NULL;
static bool s_main_init_protocol_sent = false;
static lv_obj_t* s_bottom_area_a = NULL;
static lv_obj_t* s_bottom_area_b = NULL;
static lv_obj_t* s_bottom_area_c = NULL;
static lv_obj_t* s_bottom_a_btn_mode = NULL;
static lv_obj_t* s_bottom_a_btn_add = NULL;
static lv_obj_t* s_bottom_a_btn_work = NULL;
static lv_obj_t* s_bottom_a_btn_fo = NULL;
static lv_obj_t* s_bottom_a_label_mode = NULL;
static lv_obj_t* s_bottom_a_label_add = NULL;
static lv_obj_t* s_bottom_a_label_work = NULL;
static lv_obj_t* s_bottom_a_label_fo = NULL;
static lv_obj_t* s_bottom_c_btn_batch = NULL;
static lv_obj_t* s_bottom_c_btn_speed = NULL;
static lv_obj_t* s_bottom_c_box_cfd = NULL;
static lv_obj_t* s_bottom_c_label_batch = NULL;
static lv_obj_t* s_bottom_c_label_speed = NULL;
static lv_obj_t* s_bottom_c_label_cfd = NULL;
static lv_obj_t* s_detail_btn_a = NULL;
static lv_obj_t* s_detail_btn_b = NULL;
static lv_obj_t* s_detail_btn_c = NULL;
static page_01_detail_section_t s_detail_section = PAGE_01_DETAIL_SECTION_A;
static uint32_t s_main_dirty = PAGE_01_MAIN_DIRTY_ALL;
/* High bits travel with a coalesced request, not with persistent page dirtiness.
 * A later non-animated refresh must not erase an earlier interaction intent. */
#define PAGE_01_MAIN_BOTTOM_ANIM_SHIFT 16U
#define PAGE_01_MAIN_BOTTOM_ANIM_MASK (PAGE_01_MAIN_DIRTY_MODE | PAGE_01_MAIN_DIRTY_ADD | \
    PAGE_01_MAIN_DIRTY_WORK | PAGE_01_MAIN_DIRTY_FO | PAGE_01_MAIN_DIRTY_SPEED | PAGE_01_MAIN_DIRTY_BATCH)
static uint32_t s_main_commit_animation_flags;
static bool s_main_snapshot_valid = false;
static machine_state_snapshot_t s_main_machine_snapshot;
static char s_main_currency_snapshot[4];
typedef struct {
    denom_t denom[COUNTING_DENOM_MAX_ITEMS];
    uint8_t denom_number;
    int total_pcs;
    float total_amount;
    uint16_t err_num;
    uint16_t err_expected;
    int serial_count;
    int error_detail_count;
} page_01_counting_snapshot_t;
static page_01_counting_snapshot_t s_main_counting_snapshot;
static lv_obj_t *s_curr_img = NULL;
static lv_obj_t *s_curr_label = NULL;
static char s_curr_rendered_code[4];
static char s_curr_rendered_effective[4];
static bool s_curr_rendered_valid = false;
static lv_obj_t *s_total_pcs_label = NULL;
static lv_obj_t *s_total_amount_label = NULL;
static bool s_total_pcs_compact = false;
static bool s_total_amount_compact = false;
static lv_dma_static_skin_t s_main_action_skins[3];
static lv_dma_static_skin_t s_main_detail_tab_skins[3];

static void page_01_detail_section_btn_style_apply(void);
static void page_01_detail_section_btn_event_cb(lv_event_t* e);
static void page_01_detail_section_btn_text_refresh(void);
static void page_01_main_layout_refresh(void);

bool page_01_main_is_visible(void)
{
    return page_01_main_is_created() &&
           !lv_obj_has_flag(main_page, LV_OBJ_FLAG_HIDDEN);
}

static void page_01_main_action_skins_attach(void)
{
    static const char *const object_names[] = {
        "menu_btn", "start_btn", "esc_btn",
    };
    static const char *const cache_keys[] = {
        "MAIN_MENU_BTN_SKIN",
        "MAIN_START_BTN_SKIN",
        "MAIN_ESC_BTN_SKIN",
    };

    for (uint32_t i = 0; i < 3; i++) {
        lv_obj_t *button = find_obj_by_name(object_names[i],
                                            page_01_main_obj,
                                            page_01_main_len);
        if (button != NULL && lv_obj_is_valid(button)) {
            (void)lv_dma_static_skin_attach(&s_main_action_skins[i],
                                             button, cache_keys[i]);
        }
    }
}

static void page_01_main_action_skins_release(void)
{
    for (uint32_t i = 0; i < 3; i++) {
        lv_dma_static_skin_release(&s_main_action_skins[i]);
    }
}

/* Only visual state is merged. Commands, warnings and counting state-machine
 * transitions still execute in original receive order. No object pointers
 * escape the page lifetime into this callback. */
static void page_01_main_commit(void *context, uint32_t flags)
{
    (void)context;
    if (!page_01_main_is_visible()) {
        s_main_dirty |= flags & PAGE_01_MAIN_DIRTY_ALL;
        return;
    }
    s_main_commit_animation_flags = ui_manager_is_transitioning() ? 0 :
        ((flags >> PAGE_01_MAIN_BOTTOM_ANIM_SHIFT) & PAGE_01_MAIN_BOTTOM_ANIM_MASK);
    if (flags & PAGE_01_MAIN_DIRTY_MODE) page_01_mode_switch_refre();
    if (flags & PAGE_01_MAIN_DIRTY_ADD) page_01_add_refre();
    if (flags & PAGE_01_MAIN_DIRTY_WORK) page_01_work_refre();
    if (flags & PAGE_01_MAIN_DIRTY_BATCH) page_01_batch_refre();
    if (flags & PAGE_01_MAIN_DIRTY_FO) page_01_face_refre();
    if (flags & PAGE_01_MAIN_DIRTY_CFD) page_01_cfd_refre();
    if (flags & PAGE_01_MAIN_DIRTY_SPEED) page_01_speed_refre();
    if (flags & PAGE_01_MAIN_DIRTY_ERROR) page_01_err_num_refre();
    if (flags & PAGE_01_MAIN_DIRTY_CURRENCY) page_01_curr_img_refre();
    if (flags & PAGE_01_MAIN_DIRTY_LANGUAGE) page_01_update_language_texts();
    else if (flags & PAGE_01_MAIN_DIRTY_COUNTING) ui_refresh_main_page();
    s_main_commit_animation_flags = 0;
}

void page_01_main_mark_dirty(uint32_t flags)
{
    s_main_dirty |= flags & PAGE_01_MAIN_DIRTY_ALL;
}

bool page_01_main_defer_refresh(uint32_t flags)
{
    if (!page_01_main_is_visible()) {
        page_01_main_mark_dirty(flags);
        return true;
    }
    /* Navigation/create/resume must populate the first frame synchronously. */
    if (!ui_manager_is_transitioning() &&
        ui_frame_commit_defer(page_01_main_commit, NULL, flags)) {
        page_01_main_mark_dirty(flags);
        return true;
    }
    s_main_dirty &= ~(flags & PAGE_01_MAIN_DIRTY_ALL);
    return false;
}

static bool page_01_main_defer_bottom_refresh(uint32_t flag, bool *animate)
{
    uint32_t request = flag;
    if (*animate) request |= (flag & PAGE_01_MAIN_BOTTOM_ANIM_MASK) << PAGE_01_MAIN_BOTTOM_ANIM_SHIFT;
    if (page_01_main_defer_refresh(request)) return true;
    *animate = !ui_manager_is_transitioning() &&
        (*animate || (s_main_commit_animation_flags & flag) != 0);
    return false;
}

static void page_01_main_snapshot_capture(void)
{
    const counting_sim_t *counting = counting_data_current();

    machine_state_get_snapshot(&s_main_machine_snapshot);
    currency_state_get_effective_code(s_main_currency_snapshot);
    memset(&s_main_counting_snapshot, 0, sizeof(s_main_counting_snapshot));
    if (counting != NULL) {
        memcpy(s_main_counting_snapshot.denom, counting->denom,
               sizeof(s_main_counting_snapshot.denom));
        s_main_counting_snapshot.denom_number = counting->denom_number;
        s_main_counting_snapshot.total_pcs = counting->total_pcs;
        s_main_counting_snapshot.total_amount = counting->total_amount;
        s_main_counting_snapshot.err_num = counting->err_num;
        s_main_counting_snapshot.err_expected = counting->err_expected;
        s_main_counting_snapshot.serial_count =
            counting_data_serial_valid_count(counting);
        s_main_counting_snapshot.error_detail_count =
            counting_data_error_detail_count(counting);
    }
    s_main_snapshot_valid = true;
}

static void page_01_main_detect_snapshot_changes(void)
{
    machine_state_snapshot_t machine;
    char currency[4];
    const counting_sim_t *counting = counting_data_current();

    if (!s_main_snapshot_valid) {
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_ALL);
        return;
    }

    machine_state_get_snapshot(&machine);
    if (machine.mode != s_main_machine_snapshot.mode)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_MODE);
    if (machine.add_enabled != s_main_machine_snapshot.add_enabled)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_ADD);
    if (machine.work_mode != s_main_machine_snapshot.work_mode)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_WORK);
    if (machine.batch_enabled != s_main_machine_snapshot.batch_enabled ||
        machine.batch_num != s_main_machine_snapshot.batch_num ||
        machine.batch_mode != s_main_machine_snapshot.batch_mode ||
        machine.batch_amount != s_main_machine_snapshot.batch_amount)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_BATCH);
    if (machine.fo_mode != s_main_machine_snapshot.fo_mode)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_FO);
    if (machine.cfd_mode != s_main_machine_snapshot.cfd_mode)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_CFD);
    if (machine.speed != s_main_machine_snapshot.speed)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_SPEED);

    currency_state_get_effective_code(currency);
    if (strcmp(currency, s_main_currency_snapshot) != 0)
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_CURRENCY);

    if (counting == NULL ||
        counting->denom_number != s_main_counting_snapshot.denom_number ||
        counting->total_pcs != s_main_counting_snapshot.total_pcs ||
        counting->total_amount != s_main_counting_snapshot.total_amount ||
        counting->err_num != s_main_counting_snapshot.err_num ||
        counting->err_expected != s_main_counting_snapshot.err_expected ||
        counting_data_serial_valid_count(counting) !=
            s_main_counting_snapshot.serial_count ||
        counting_data_error_detail_count(counting) !=
            s_main_counting_snapshot.error_detail_count ||
        memcmp(counting->denom, s_main_counting_snapshot.denom,
               sizeof(counting->denom)) != 0) {
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_COUNTING |
                                PAGE_01_MAIN_DIRTY_ERROR);
    }
}


void page_01_mode_switch_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_MODE)) return;
    const char *mode = machine_state_mode() == MODE_CNT ? "CNT" :
        machine_state_mode() == MODE_SDC ? "SDC" : "MDC";
    update_label_by_name(page_01_main_obj, page_01_main_len, "mode_label", "%s", mode);
    page_01_bottom_a_refresh_mode(false);
}

void page_01_add_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_ADD)) return;
    page_01_bottom_a_refresh_add(false);
}

void page_01_work_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_WORK)) return;
    page_01_bottom_a_refresh_work(false);
}

void page_01_batch_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_BATCH)) return;
    page_01_bottom_c_refresh_batch(false);
}

void page_01_face_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_FO)) return;
    page_01_bottom_a_refresh_fo(false);
}

void page_01_cfd_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CFD)) return;
    page_01_bottom_c_refresh_cfd();
}

void page_01_speed_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_SPEED)) return;
    page_01_bottom_c_refresh_speed(false);
}

void page_01_err_num_refre(void)
{
    char text[20];
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_ERROR)) return;
    lv_snprintf(text, sizeof(text), "%d", counting_data_reject_pcs_count(counting_data_current()));
    update_label_by_name(page_01_main_obj, page_01_main_len, "reject_num_label", "%s", text);
}

void page_01_curr_img_refre(void)
{
    char code[4], effective[4];
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CURRENCY)) return;
    currency_state_get_selected_code(code);
    currency_state_get_effective_code(effective);
    page_01_main_layout_refresh();
    if (s_curr_rendered_valid && strcmp(code, s_curr_rendered_code) == 0 &&
        strcmp(effective, s_curr_rendered_effective) == 0) return;
    const bool special = currency_state_is_special_code(effective);
    if (s_curr_img) {
        lv_img_set_src(s_curr_img, get_currency_img(effective));
        /* Flags are stored at 180x100. Keep their aspect ratio at 54x30. */
        lv_img_set_zoom(s_curr_img, 77);
        lv_img_set_pivot(s_curr_img, 0, 0);
    }
    if (s_currency_code) lv_label_set_text(s_currency_code, currency_state_display_code(effective));
    if (s_curr_label) {
        const char *symbol = currency_metadata_symbol(effective);
        lv_label_set_text(s_curr_label, symbol ? symbol : "");
        lv_obj_set_style_text_font(s_curr_label,
            symbol != NULL && strlen(symbol) > 3 ?
                &lv_font_main_currency_32 : &lv_font_main_currency_56,
            0);
        if (special || !symbol) lv_obj_add_flag(s_curr_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_curr_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_amount_unit_icon) {
        if (!special && !currency_metadata_symbol(effective)) lv_obj_clear_flag(s_amount_unit_icon, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_amount_unit_icon, LV_OBJ_FLAG_HIDDEN);
    }
    snprintf(s_curr_rendered_code, sizeof(s_curr_rendered_code), "%s", code);
    snprintf(s_curr_rendered_effective, sizeof(s_curr_rendered_effective), "%s", effective);
    s_curr_rendered_valid = true;
}

static void page_01_smart_island_action_cb(uint8_t action_id)
{
    if (action_id == SMART_ISLAND_ACTION_TIME_SETTING) {
        ui_export_data_request();
    }
}

typedef enum {
    PAGE_01_BOTTOM_TEXT_ANIM_NONE = 0,
    PAGE_01_BOTTOM_TEXT_ANIM_SLIDE
} page_01_bottom_text_anim_t;

static void main_time_refresh(void)
{
    machine_time_value_t now;
    char buf[16];

    machine_time_get(&now);
    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
        (unsigned)now.hour,
        (unsigned)now.minute,
        (unsigned)now.second);
    if (s_time_label) lv_label_set_text(s_time_label, buf);
}

static void main_time_timer_cb(lv_timer_t* t)
{
    (void)t;
    if (!page_01_main_is_visible()) return;
    main_time_refresh();
    smart_island_refresh_time();
    page_01_main_refresh_start_state();
}

static void page_01_main_send_init_protocol(void) //主界面首次进入时发送初始化协议
{
    uint8_t sub = 0x02;

    if (s_main_init_protocol_sent) return;
    if (!protocol_send_is_ready()) return;

    protocol_send(0xC0, &sub, 1); //FD DF 06 C0 02：通知下位机进入采集误报数据模式

    /* 预留：后续新增主界面首次进入协议时，继续在这里统一发送。 */

    s_main_init_protocol_sent = true;
}

static void page_01_bottom_text_anim_opa_cb(void* var, int32_t v) //底部按钮文本透明度动画
{
    lv_obj_set_style_text_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

static void page_01_bottom_text_anim_x_cb(void* var, int32_t v) //底部按钮文本横向位移动画
{
    lv_obj_set_style_translate_x((lv_obj_t*)var, (lv_coord_t)v, 0);
}


static void page_01_bottom_label_anim_stop(lv_obj_t* label)
{
    if (label == NULL || !lv_obj_is_valid(label)) return;
    lv_anim_del(label, NULL);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(label, 0, 0);
}

static void page_01_bottom_animations_stop(void)
{
    /* Suspend/destroy discard interaction effects, while queued state remains
     * dirty for the synchronous first frame on the next page activation. */
    ui_frame_commit_cancel(page_01_main_commit, NULL);
    s_main_commit_animation_flags = 0;
    page_01_bottom_label_anim_stop(s_bottom_a_label_mode);
    page_01_bottom_label_anim_stop(s_bottom_a_label_add);
    page_01_bottom_label_anim_stop(s_bottom_a_label_work);
    page_01_bottom_label_anim_stop(s_bottom_a_label_fo);
    page_01_bottom_label_anim_stop(s_bottom_c_label_batch);
    page_01_bottom_label_anim_stop(s_bottom_c_label_speed);
    page_01_bottom_label_anim_stop(s_bottom_c_label_cfd);
}

static void page_01_bottom_label_anim_run(lv_obj_t *label, const char *text,
    page_01_bottom_text_anim_t anim_type)
{
    lv_anim_t animation;
    if (!label || !text || strcmp(lv_label_get_text(label), text) == 0) return;
    page_01_bottom_label_anim_stop(label);
    lv_label_set_text(label, text);
    if (anim_type == PAGE_01_BOTTOM_TEXT_ANIM_NONE) return;
    lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
    lv_obj_set_style_translate_x(label, -12, 0);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, label);
    lv_anim_set_time(&animation, 160);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_values(&animation, LV_OPA_70, LV_OPA_COVER);
    lv_anim_set_exec_cb(&animation, page_01_bottom_text_anim_opa_cb);
    lv_anim_start(&animation);
    lv_anim_set_values(&animation, -12, 0);
    lv_anim_set_exec_cb(&animation, page_01_bottom_text_anim_x_cb);
    lv_anim_start(&animation);
}

static const char* page_01_bottom_mode_text_get(uint8_t mode) //获取底部A区模式文本
{
    switch (mode) {
    case MODE_MDC: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_MDC);
    case MODE_SDC: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_SDC);
    case MODE_CNT: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_CNT);
    default: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_MDC);
    }
}

static const char* page_01_bottom_add_text_get(void) //获取底部A区ADD文本
{
    return machine_state_add_enabled() ? ui_text_get(UI_TEXT_PAGE01_BOTTOM_ADD_ON) :
        ui_text_get(UI_TEXT_PAGE01_BOTTOM_ADD_OFF);
}

static const char* page_01_bottom_work_text_get(void) //获取底部A区工作模式文本
{
    return machine_state_work_mode() ? ui_text_get(UI_TEXT_PAGE01_BOTTOM_WORK_MANUAL) :
        ui_text_get(UI_TEXT_PAGE01_BOTTOM_WORK_AUTO);
}

static const char* page_01_bottom_fo_text_get(void) //获取底部A区F/O文本
{
    switch (machine_state_fo_mode()) {
    case 0: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_FO_OFF);
    case 1: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_FO_F);
    case 2: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_FO_O);
    case 3: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_FO_FO);
    default: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_FO_OFF);
    }
}

static const char* page_01_bottom_speed_text_get(void) //获取底部C区速度文本
{
    switch (machine_state_speed()) {
    case 0: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_SPEED_LOW);
    case 1: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_SPEED_MID);
    case 2: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_SPEED_HIGH);
    default: return ui_text_get(UI_TEXT_PAGE01_BOTTOM_SPEED_LOW);
    }
}

static lv_obj_t* page_01_bottom_btn_create(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
    lv_event_cb_t event_cb) //创建主界面底部A区按钮
{
    lv_obj_t* btn = lv_obj_create(main_page);

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE7EDF1), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 32, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_damped_button_register(btn, lv_color_hex(0xFFFFFF),
                              lv_color_hex(0xE7EDF1));

    if (event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

static lv_obj_t* page_01_bottom_btn_label_create(lv_obj_t* parent) //创建主界面底部A区按钮文本
{
    lv_obj_t* label = lv_label_create(parent);

    lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0x4C606E), 0);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_18, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    return label;
}

static lv_obj_t* page_01_bottom_box_create(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
    lv_color_t bg_color) //创建主界面底部背景盒子
{
    lv_obj_t* obj = lv_obj_create(main_page);

    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, bg_color, 0);
    lv_obj_set_style_radius(obj, 32, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}


static lv_obj_t* page_01_bottom_c_btn_create(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
    lv_color_t bg_color, lv_event_cb_t event_cb, bool clickable) //创建主界面底部C区按钮
{
    lv_obj_t* btn = page_01_bottom_box_create(x, y, w, h, bg_color);

    if (clickable) {
        lv_damped_button_register(btn, bg_color, lv_color_hex(0xE7EDF1));
        if (event_cb) {
            lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
        }
    } else {
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }

    return btn;
}

static void page_01_bottom_a_create(void) //创建主界面底部A区四个按钮
{
    if (s_bottom_a_btn_mode || s_bottom_a_btn_add || s_bottom_a_btn_work || s_bottom_a_btn_fo) return;

    s_bottom_a_btn_mode = page_01_bottom_btn_create(10, 348, 91, 44, page_01_bottom_mode_btn_event_cb);
    s_bottom_a_btn_add = page_01_bottom_btn_create(101, 348, 104, 44, page_01_add_btn_event_cb);
    s_bottom_a_btn_work = page_01_bottom_btn_create(205, 348, 119, 44, page_01_work_btn_event_cb);
    s_bottom_a_btn_fo = page_01_bottom_btn_create(324, 348, 121, 44, page_01_fo_btn_event_cb);

    s_bottom_a_label_mode = page_01_bottom_btn_label_create(s_bottom_a_btn_mode);
    s_bottom_a_label_add = page_01_bottom_btn_label_create(s_bottom_a_btn_add);
    s_bottom_a_label_work = page_01_bottom_btn_label_create(s_bottom_a_btn_work);
    s_bottom_a_label_fo = page_01_bottom_btn_label_create(s_bottom_a_btn_fo);
}

static void page_01_bottom_a_destroy(void) //销毁主界面底部A区四个按钮
{
    if (s_bottom_a_btn_mode) lv_obj_del(s_bottom_a_btn_mode);
    if (s_bottom_a_btn_add) lv_obj_del(s_bottom_a_btn_add);
    if (s_bottom_a_btn_work) lv_obj_del(s_bottom_a_btn_work);
    if (s_bottom_a_btn_fo) lv_obj_del(s_bottom_a_btn_fo);

    s_bottom_a_btn_mode = NULL;
    s_bottom_a_btn_add = NULL;
    s_bottom_a_btn_work = NULL;
    s_bottom_a_btn_fo = NULL;
    s_bottom_a_label_mode = NULL;
    s_bottom_a_label_add = NULL;
    s_bottom_a_label_work = NULL;
    s_bottom_a_label_fo = NULL;
}

static void page_01_bottom_c_create(void) //创建主界面底部C区三个区域
{
    if (s_bottom_c_btn_batch || s_bottom_c_btn_speed || s_bottom_c_box_cfd) return;

    s_bottom_c_btn_batch = page_01_bottom_c_btn_create(799, 348, 216, 44,
        lv_color_hex(0xFFFFFF), page_01_bottom_batch_btn_event_cb, true);
    s_bottom_c_btn_speed = page_01_bottom_c_btn_create(1015, 348, 119, 44,
        lv_color_hex(0xFFFFFF), page_01_bottom_speed_btn_event_cb, true);
    s_bottom_c_box_cfd = page_01_bottom_c_btn_create(1134, 348, 115, 44,
        lv_color_hex(0xFFFFFF), NULL, false);

    s_bottom_c_label_batch = page_01_bottom_btn_label_create(s_bottom_c_btn_batch);
    s_bottom_c_label_speed = page_01_bottom_btn_label_create(s_bottom_c_btn_speed);
    s_bottom_c_label_cfd = page_01_bottom_btn_label_create(s_bottom_c_box_cfd);
}

static void page_01_bottom_c_destroy(void) //销毁主界面底部C区三个区域
{
    if (s_bottom_c_btn_batch) lv_obj_del(s_bottom_c_btn_batch);
    if (s_bottom_c_btn_speed) lv_obj_del(s_bottom_c_btn_speed);
    if (s_bottom_c_box_cfd) lv_obj_del(s_bottom_c_box_cfd);

    s_bottom_c_btn_batch = NULL;
    s_bottom_c_btn_speed = NULL;
    s_bottom_c_box_cfd = NULL;
    s_bottom_c_label_batch = NULL;
    s_bottom_c_label_speed = NULL;
    s_bottom_c_label_cfd = NULL;
}

static void page_01_detail_section_btn_update_one(lv_obj_t *btn, bool selected)
{
    if (!btn || !lv_obj_is_valid(btn)) return;
    lv_damped_button_set_palette(btn, lv_color_hex(selected ? 0xFFFFFF : 0xE7EDF0),
        lv_color_hex(0xD9E5ED));
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xCBDEE9), 0);
    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if (label) lv_obj_set_style_text_color(label, lv_color_hex(selected ? 0x233B49 : 0x526E80), 0);
}

static void page_01_detail_section_btn_skins_sync(void)
{
    lv_obj_t *const buttons[] = {
        s_detail_btn_a, s_detail_btn_b, s_detail_btn_c,
    };

    for (uint32_t i = 0; i < 3; i++) {
        const bool selected = s_detail_section == (page_01_detail_section_t)i;

        /* All three tabs share the same geometry and visual states.  Cache
         * only the selected/unselected parent decoration; live dot, text,
         * hit testing and pressed-state feedback remain on the LVGL object. */
        lv_dma_static_skin_release(&s_main_detail_tab_skins[i]);
        if (buttons[i] != NULL && lv_obj_is_valid(buttons[i])) {
            (void)lv_dma_static_skin_attach(
                &s_main_detail_tab_skins[i], buttons[i],
                selected ? "MAIN_DETAIL_TAB_SELECTED" :
                           "MAIN_DETAIL_TAB_UNSELECTED");
        }
    }
}

static void page_01_detail_section_btn_style_apply(void)
{
    page_01_detail_section_btn_update_one(s_detail_btn_a, s_detail_section == PAGE_01_DETAIL_SECTION_A);
    page_01_detail_section_btn_update_one(s_detail_btn_b, s_detail_section == PAGE_01_DETAIL_SECTION_B);
    page_01_detail_section_btn_update_one(s_detail_btn_c, s_detail_section == PAGE_01_DETAIL_SECTION_C);
    page_01_detail_section_btn_skins_sync();
}

static void page_01_detail_section_btn_text_refresh(void)
{
    lv_obj_t *buttons[] = {s_detail_btn_a, s_detail_btn_b, s_detail_btn_c};
    const char *titles[] = {"REPORT", "SERIAL", "REJECT"};
    for (unsigned i = 0; i < 3; ++i)
        if (buttons[i]) lv_label_set_text(lv_obj_get_child(buttons[i], 1), titles[i]);
}


static lv_obj_t *page_01_detail_section_btn_create(lv_coord_t x, lv_coord_t y,
    const char *text, page_01_detail_section_t section)
{
    static const uint32_t colors[] = {0x2BD900, 0x0074F8, 0xF85820};
    static const char *letters[] = {"A", "B", "C"};
    lv_obj_t *btn = lv_obj_create(main_page);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 172, 40);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_damped_button_register(btn, lv_color_hex(0xF4F7F8), lv_color_hex(0xD9E5ED));
    lv_obj_add_event_cb(btn, page_01_detail_section_btn_event_cb, LV_EVENT_CLICKED,
        (void *)(uintptr_t)section);
    lv_obj_t *badge = lv_obj_create(btn);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 20, 20);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 30, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(colors[section]), 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *letter = lv_label_create(badge);
    lv_label_set_text(letter, letters[section]);
    lv_obj_set_style_text_font(letter, &lv_font_instrument_sans_bold_14, 0);
    lv_obj_set_style_text_color(letter, lv_color_white(), 0);
    lv_obj_center(letter);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_16, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 58, 0);
    return btn;
}

static void page_01_detail_section_btn_create_all(void)
{
    s_detail_tray = lv_obj_create(main_page);
    lv_obj_remove_style_all(s_detail_tray);
    lv_obj_set_pos(s_detail_tray, 620, 26);
    lv_obj_set_size(s_detail_tray, 518, 40);
    lv_obj_set_style_bg_color(s_detail_tray, lv_color_hex(0xE7EDF0), 0);
    lv_obj_set_style_bg_opa(s_detail_tray, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_detail_tray, 12, 0);
    lv_obj_clear_flag(s_detail_tray, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    /* 3 equal 172px targets + two 1px seams fill the unchanged 518px tray. */
    s_detail_btn_a = page_01_detail_section_btn_create(620, 26, "REPORT", PAGE_01_DETAIL_SECTION_A);
    s_detail_btn_b = page_01_detail_section_btn_create(793, 26, "SERIAL", PAGE_01_DETAIL_SECTION_B);
    s_detail_btn_c = page_01_detail_section_btn_create(966, 26, "REJECT", PAGE_01_DETAIL_SECTION_C);
    page_01_detail_section_btn_style_apply();
    /* A non-interactive stroke above all three flush selected surfaces. */
    s_detail_frame = lv_obj_create(main_page);
    lv_obj_remove_style_all(s_detail_frame);
    lv_obj_set_pos(s_detail_frame, 620, 26);
    lv_obj_set_size(s_detail_frame, 518, 40);
    lv_obj_set_style_radius(s_detail_frame, 12, 0);
    lv_obj_set_style_border_width(s_detail_frame, 3, 0);
    lv_obj_set_style_border_color(s_detail_frame, lv_color_hex(0xECF0F3), 0);
    lv_obj_set_style_border_opa(s_detail_frame, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_detail_frame, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static void page_01_detail_section_btn_destroy_all(void)
{
    for (uint32_t i = 0; i < 3; i++) {
        lv_dma_static_skin_release(&s_main_detail_tab_skins[i]);
    }
    if (s_detail_btn_a) lv_obj_del(s_detail_btn_a);
    if (s_detail_btn_b) lv_obj_del(s_detail_btn_b);
    if (s_detail_btn_c) lv_obj_del(s_detail_btn_c);
    if (s_detail_tray) lv_obj_del(s_detail_tray);
    if (s_detail_frame) lv_obj_del(s_detail_frame);
    s_detail_frame = NULL;
    s_detail_tray = NULL;

    s_detail_btn_a = NULL;
    s_detail_btn_b = NULL;
    s_detail_btn_c = NULL;
    s_detail_section = PAGE_01_DETAIL_SECTION_A;
}

static void page_01_detail_section_btn_event_cb(lv_event_t* e)
{
    page_01_detail_section_t section;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    section = (page_01_detail_section_t)(uintptr_t)lv_event_get_user_data(e);
    page_01_detail_section_set(section, true);
}

void page_01_detail_section_set(page_01_detail_section_t section, bool refresh)
{
    if ((unsigned)section > PAGE_01_DETAIL_SECTION_C) return;
    const bool changed = s_detail_section != section;
    s_detail_section = section;
    if (changed && refresh) ui_state_save_page01_detail_section();
    if (changed) page_01_detail_section_btn_style_apply();
    if (refresh) page_01_main_detail_refresh(section);
}

page_01_detail_section_t page_01_detail_section_get(void)
{
    return s_detail_section;
}

static lv_obj_t* page_01_bottom_bg_create(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
    lv_color_t bg_color) //创建主界面底部背景块
{
    lv_obj_t* obj = lv_obj_create(main_page);

    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, bg_color, 0);
    lv_obj_set_style_radius(obj, 40, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}

static void page_01_bottom_bg_create_all(void) //创建主界面底部三个背景区域
{
    if (s_bottom_area_a || s_bottom_area_b || s_bottom_area_c) return;

    s_bottom_area_a = page_01_bottom_bg_create(10, 348, 435, 44, lv_color_make(255, 255, 255));
    s_bottom_area_b = page_01_bottom_bg_create(492, 348, 261, 44, lv_color_hex(0x656565));
    s_bottom_area_c = page_01_bottom_bg_create(799, 348, 450, 44, lv_color_make(255, 255, 255));
}

static void page_01_bottom_bg_destroy_all(void) //销毁主界面底部三个背景区域
{
    if (s_bottom_area_a) lv_obj_del(s_bottom_area_a);
    if (s_bottom_area_b) lv_obj_del(s_bottom_area_b);
    if (s_bottom_area_c) lv_obj_del(s_bottom_area_c);

    s_bottom_area_a = NULL;
    s_bottom_area_b = NULL;
    s_bottom_area_c = NULL;
}

void page_01_bottom_a_refresh_mode(bool anim_en) //刷新主界面底部A区模式文本
{
    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_MODE, &anim_en)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_mode, page_01_bottom_mode_text_get(machine_state_mode()),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_a_refresh_mode_preview(uint8_t mode) //预刷新主界面底部A区模式文本
{
    if (!page_01_main_is_visible()) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_mode, page_01_bottom_mode_text_get(mode),
        PAGE_01_BOTTOM_TEXT_ANIM_SLIDE);
}

void page_01_bottom_a_refresh_add(bool anim_en) //刷新主界面底部A区ADD文本
{
    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_ADD, &anim_en)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_add, page_01_bottom_add_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_a_refresh_work(bool anim_en) //刷新主界面底部A区工作模式文本
{
    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_WORK, &anim_en)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_work, page_01_bottom_work_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_a_refresh_fo(bool anim_en) //刷新主界面底部A区F/O文本
{
    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_FO, &anim_en)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_fo, page_01_bottom_fo_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_batch(bool anim_en) //刷新主界面底部C区Batch文本
{
    char text_buf[32];

    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_BATCH, &anim_en)) return;

    if (machine_state_batch_enabled()) {
        lv_snprintf(text_buf, sizeof(text_buf), ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_VALUE_FMT),
            machine_state_batch_num());
    } else {
        lv_snprintf(text_buf, sizeof(text_buf), "%s", ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_OFF));
    }

    page_01_bottom_label_anim_run(s_bottom_c_label_batch, text_buf,
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_speed(bool anim_en) //刷新主界面底部C区速度文本
{
    if (page_01_main_defer_bottom_refresh(PAGE_01_MAIN_DIRTY_SPEED, &anim_en)) return;
    page_01_bottom_label_anim_run(s_bottom_c_label_speed, page_01_bottom_speed_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_cfd(void) //刷新主界面底部C区CFD文本
{
    char text_buf[24];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CFD)) return;

    lv_snprintf(text_buf, sizeof(text_buf), ui_text_get(UI_TEXT_PAGE01_BOTTOM_CFD_FMT),
        machine_state_cfd_mode() == 1 ? "M" : machine_state_cfd_mode() == 2 ? "H" : "L");
    page_01_bottom_label_anim_run(s_bottom_c_label_cfd, text_buf, PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

static void main_register(const char *name, lv_obj_t *obj)
{
    if (page_01_main_len >= (int)(sizeof(page_01_main_obj) / sizeof(page_01_main_obj[0]))) return;
    page_01_main_obj[page_01_main_len++] = (ui_element_t){.obj_name = name, .obj_ref = obj};
}

static lv_obj_t *main_box(lv_obj_t *parent, int x, int y, int w, int h,
    uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *main_label(lv_obj_t *parent, const char *name, const char *text,
    int x, int y, int w, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    if (name) main_register(name, label);
    return label;
}

static lv_obj_t *main_icon(lv_obj_t *parent, const char *name, const char *source,
    int x, int y)
{
    lv_obj_t *icon = lv_img_create(parent);
    lv_img_set_src(icon, source);
    lv_obj_set_pos(icon, x, y);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    if (name) main_register(name, icon);
    return icon;
}

static lv_obj_t *main_action(const char *name, const char *icon_name, const char *source,
    const char *label_name, const char *text, int x, int y, int w, int h,
    lv_event_cb_t callback, bool primary)
{
    lv_obj_t *button = main_box(main_page, x, y, w, h, primary ? 0xDCEFD5 : 0xFFFFFF, 14);
    main_register(name, button);
    lv_damped_button_register(button, lv_color_hex(primary ? 0xDCEFD5 : 0xFFFFFF),
        lv_color_hex(primary ? 0xD6EDC2 : 0xE7EDF1));
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    main_icon(button, icon_name, source, (w - 28) / 2, h / 2 - 24);
    lv_obj_t *label = main_label(button, label_name, text, 0, h / 2 + 11, w,
        &lv_font_instrument_sans_medium_14, primary ? 0x246A22 : x < 108 ? 0x4C606E : 0x496574);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return button;
}

static void main_currency_target(lv_obj_t *parent)
{
    lv_obj_t *target = main_box(parent, 18, 16, 278, 66, 0xFFFFFF, 8);
    lv_damped_button_register(target, lv_color_white(), lv_color_hex(0xF1F5F7));
    lv_obj_add_event_cb(target, page_01_curr_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *icon = main_icon(target, "curr_USD_img",
        LVGL_DIR"main_icons/currencies_32.png", 4, 15);
    main_label(target, NULL, "Currency", 80, 3, 120, &lv_font_instrument_sans_medium_12, 0x7A8D9B);
    lv_obj_t *code = main_label(target, NULL, "AUTO", 80, 24, 112,
        &lv_font_instrument_sans_semibold_24, 0x17212A);
    main_icon(target, NULL, LVGL_DIR"main_icons/caret_down_16.png", 206, 33);
    s_curr_img = icon; s_currency_code = code;
}

static void page_01_main_build_content(void)
{
    main_icon(main_page, "page_01_back.png", UI_USER_BACKGROUND_SRC, 0, 0);
    main_action("mode_btn", "page_01_mode_icon.png", LVGL_DIR"main_icons/cube_28.png",
        "mode_label", "MDC", 16, 12, 80, 74, page_01_mode_btn_event_cb, false);
    main_action("setting_btn", "page_01_set_icon.png", LVGL_DIR"main_icons/gear_28.png",
        "setting_label", "SETTING", 16, 94, 80, 74, page_01_set_btn_event_cb, false);
    main_action("list_btn", "page_01_list_icon.png", LVGL_DIR"main_icons/list_28.png",
        "list_label", "LIST", 16, 176, 80, 74, page_01_list_btn_event_cb, false);
    main_action("print_btn", "page_01_print_icon.png", LVGL_DIR"main_icons/printer_28.png",
        "print_label", "PRINT", 16, 258, 80, 74, page_01_print_btn_event_cb, false);
    main_action("menu_btn", "page_01_menu_icon.png", LVGL_DIR"main_icons/menu_28.png",
        "menu_label", "MENU", 1168, 12, 96, 98, page_01_menu_btn_event_cb, false);
    main_action("start_btn", "page_01_start_icon.png", LVGL_DIR"main_icons/play_28.png",
        "start_label", "START", 1168, 123, 96, 98, page_01_start_btn_event_cb, true);
    main_action("esc_btn", "page_01_esc_icon.png", LVGL_DIR"main_icons/clear_28.png",
        "esc_label", "CLEAR", 1168, 234, 96, 98, page_01_esc_btn_event_cb, false);
    s_summary_card = main_box(main_page, 108, 12, 482, 320, 0xFFFFFF, 16);
    lv_obj_set_style_border_width(s_summary_card, 1, 0);
    lv_obj_set_style_border_color(s_summary_card, lv_color_hex(0xECF0F3), 0);
    lv_obj_set_style_border_opa(s_summary_card, LV_OPA_COVER, 0);
    main_currency_target(s_summary_card);
    lv_obj_t *reject = main_box(s_summary_card, 346, 25, 114, 38, 0xF4F7F8, 8);
    main_label(reject, NULL, "REJECT", 10, 13, 56, &lv_font_instrument_sans_medium_10, 0x657F90);
    lv_obj_t *reject_num = main_label(reject, "reject_num_label", "0", 66, 8, 39,
        &lv_font_instrument_sans_semibold_20, 0x4C606E);
    lv_obj_set_style_text_align(reject_num, LV_TEXT_ALIGN_RIGHT, 0);
    main_box(s_summary_card, 22, 90, 438, 1, 0xEFF3F5, 0);
    main_icon(s_summary_card, NULL, LVGL_DIR"main_icons/stack_20.png", 22, 133);
    main_label(s_summary_card, NULL, "PCS", 52, 133, 64, &lv_font_instrument_sans_medium_20, 0x4C606E);
    s_total_pcs_label = main_label(s_summary_card, "01_pcs_label", "0", 120, 108, 340,
        &lv_font_manrope_bold_48, 0x17212A);
    main_box(s_summary_card, 22, 190, 438, 1, 0xEFF3F5, 0);
    main_label(s_summary_card, NULL, "AMOUNT", 22, 230, 100, &lv_font_instrument_sans_medium_12, 0x7A8D9B);
    s_curr_label = main_label(s_summary_card, "curr_icon_label", "", 22, 244, 92,
        &lv_font_main_currency_56, 0x0074F8);
    s_amount_unit_icon = main_icon(s_summary_card, NULL, LVGL_DIR"main_icons/currencies_56.png", 22, 248);
    s_total_amount_label = main_label(s_summary_card, "01_amount_label", "0", 120, 223, 340,
        &lv_font_manrope_extrabold_48, 0x17212A);
    s_detail_card = main_box(main_page, 602, 12, 554, 320, 0xFFFFFF, 16);
    lv_obj_set_style_border_width(s_detail_card, 1, 0);
    lv_obj_set_style_border_color(s_detail_card, lv_color_hex(0xECF0F3), 0);
    lv_obj_set_style_border_opa(s_detail_card, LV_OPA_COVER, 0);
    page_01_main_detail_create(main_page, 620, 76, 518, 244);
    page_01_detail_section_btn_create_all();

    s_multi_card = page_01_multi_create(main_page);
}

static void page_01_main_layout_refresh(void)
{
    const bool multi = currency_state_multi_selected() ||
        !counting_data_monetary_result_supported(counting_data_current());
    if (!s_summary_card || !s_multi_card) return;
    lv_obj_t *single[] = {s_summary_card, s_detail_card, s_detail_btn_a, s_detail_btn_b, s_detail_btn_c, s_detail_tray, s_detail_frame};
    for (unsigned i = 0; i < sizeof(single) / sizeof(single[0]); ++i) {
        if (!single[i]) continue;
        if (multi) lv_obj_add_flag(single[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(single[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (multi) lv_obj_clear_flag(s_multi_card, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_multi_card, LV_OBJ_FLAG_HIDDEN);
    page_01_main_detail_set_visible(!multi && page_01_main_is_visible());
    s_multi_layout = multi;
    page_01_multi_visible(multi && page_01_main_is_visible());
}

void page_01_main_refresh_start_state(void)
{
    if (!page_01_main_is_created()) return;
    lv_obj_t *icon = page_01_main_find_obj("page_01_start_icon.png");
    lv_obj_t *button = page_01_main_find_obj("start_btn");
    const bool busy = app_command_runtime_count_start_busy();
    if (busy && page_01_main_is_visible()) {
        if (icon) lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
        if (!s_start_orbit && button) {
            s_start_orbit = lv_loading_orbit_create_sized(button, 34);
            lv_loading_orbit_set_indicator_color(s_start_orbit, lv_color_hex(0x26810A));
            lv_obj_set_pos(s_start_orbit, 31, 21);
        }
    } else {
        if (s_start_orbit) { lv_obj_del(s_start_orbit); s_start_orbit = NULL; }
        if (icon) lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_refresh_main_page(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_COUNTING)) return;
    const counting_sim_t *data = counting_data_current();
    char amount[64];
    format_amount_with_comma(amount, sizeof(amount), data ? data->total_amount : 0);
    page_01_curr_img_refre();
    page_01_main_refresh_totals(data ? data->total_pcs : 0, amount);
    page_01_err_num_refre();
    if (!s_multi_layout) page_01_main_detail_refresh(s_detail_section);
    page_01_main_refresh_start_state();
    page_01_main_snapshot_capture();
}

void page_01_main_detail_refresh_rows_only(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_COUNTING)) return;
    if (!s_multi_layout) page_01_main_detail_refresh(s_detail_section);
}

static void page_01_top_strip_tap(const lv_point_t *point)
{
    if (!point || !page_01_main_is_visible() || ui_manager_is_transitioning()) return;
    lv_obj_t *targets[] = {page_01_main_find_obj("menu_btn"),
        s_detail_btn_a, s_detail_btn_b, s_detail_btn_c};
    lv_area_t area;
    /* The raised tabs now partly overlap the transparent pull-down hot zone.
     * Route an ordinary tap to the visible control before the card fallback. */
    for (unsigned i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        lv_obj_t *target = targets[i];
        if (!target || !lv_obj_is_visible(target)) continue;
        lv_obj_get_coords(target, &area);
        if (point->x >= area.x1 && point->x <= area.x2 &&
            point->y >= area.y1 && point->y <= area.y2) {
            lv_event_send(target, LV_EVENT_CLICKED, NULL);
            return;
        }
    }
    /* MULTI header taps never navigate to the unrelated single-currency List. */
}

void ui_main_create(lv_obj_t *parent)
{
    if (page_01_main_is_created()) return;
    ui_main_destroy();
    main_page = lv_obj_create(parent ? parent : lv_scr_act());
    lv_obj_remove_style_all(main_page);
    lv_obj_set_pos(main_page, 0, 0);
    lv_obj_set_size(main_page, 1280, 400);
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_SCROLLABLE);
    s_detail_section = (page_01_detail_section_t)ui_state_page01_detail_section_get();
    if ((unsigned)s_detail_section > PAGE_01_DETAIL_SECTION_C)
        s_detail_section = PAGE_01_DETAIL_SECTION_A;
    page_01_main_build_content();
    page_01_main_action_skins_attach();
    page_01_bottom_bg_create_all();
    page_01_bottom_a_create();
    page_01_bottom_c_create();
    page_01_mode_switch_refre();
    page_01_add_refre();
    page_01_work_refre();
    page_01_batch_refre();
    page_01_face_refre();
    page_01_cfd_refre();
    page_01_speed_refre();
    ui_refresh_main_page();
    machine_time_init();
    s_time_timer = lv_timer_create(main_time_timer_cb, 1000, NULL);
    lv_print_toast_create();
    ui_state_apply_common_runtime();
    if (!ui_manager_is_prewarming_page(UI_PAGE_MAIN))
        page_01_main_send_init_protocol();
    smart_island_create(main_page);
    smart_island_register_action_cb(page_01_smart_island_action_cb);
    smart_island_refresh_time();
    page_32_innovation_handle_attach(main_page);
    page_32_innovation_handle_set_tap_handler(page_01_top_strip_tap);
    s_main_dirty = 0;
    page_01_main_snapshot_capture();
}

void ui_main_destroy(void)
{
    page_01_bottom_animations_stop();
    if (s_time_timer) { lv_timer_del(s_time_timer); s_time_timer = NULL; }
    s_time_label = NULL;
    if (page_01_main_is_created()) {
        /* The counting store outlives a cached page. UI destruction must never
         * clear a customer's count or pending history transaction. */
        page_01_multi_destroy();
        page_01_main_detail_destroy();
        page_01_main_action_skins_release();
        page_01_bottom_a_destroy();
        page_01_bottom_c_destroy();
        page_01_detail_section_btn_destroy_all();
        page_01_bottom_bg_destroy_all();
        smart_island_destroy();
        page_32_innovation_handle_detach();
        lv_obj_del(main_page);
    }
    main_page = NULL;
    memset(page_01_main_obj, 0, sizeof(page_01_main_obj));
    page_01_main_len = 0;
    s_summary_card = s_detail_card = s_multi_card = s_detail_tray = NULL;
    s_detail_frame = NULL;
    s_start_orbit = NULL;
    s_amount_unit_icon = s_currency_code = NULL;
    s_curr_img = s_curr_label = NULL;
    s_total_pcs_label = s_total_amount_label = NULL;
    s_curr_rendered_valid = s_main_snapshot_valid = s_multi_layout = false;
    s_total_pcs_compact = s_total_amount_compact = false;
    s_curr_rendered_code[0] = '\0';
    s_main_dirty = PAGE_01_MAIN_DIRTY_ALL;
}

bool page_01_main_is_created(void)
{
    return main_page != NULL && lv_obj_is_valid(main_page);
}

lv_obj_t *page_01_main_find_obj(const char *name)
{
    return find_obj_by_name(name, page_01_main_obj, page_01_main_len);
}

lv_obj_t *page_01_main_scroll_obj(void)
{
    if (s_multi_layout) return page_01_multi_scroll();
    return page_01_main_detail_scroll_obj();
}

void page_01_main_scroll_reset(void)
{
    page_01_main_detail_reset();
}

static void page_01_main_summary_label_set(lv_obj_t **cached_label,
    bool *geometry_compact, const char *name, const char *text)
{
    enum { SUMMARY_RIGHT_X = 460, SUMMARY_MAX_WIDTH = 342 };
    if (!*cached_label || !lv_obj_is_valid(*cached_label)) {
        *cached_label = page_01_main_find_obj(name);
        *geometry_compact = false;
    }
    lv_obj_t *label = *cached_label;
    if (!label) return;
    if (strcmp(lv_label_get_text(label), text) == 0 && *geometry_compact) return;
    lv_label_set_text(label, text);
    const lv_font_t *large = &lv_font_main_numeric_64;
    lv_obj_set_style_text_font(label, large, 0);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_update_layout(label);
    if (lv_obj_get_width(label) > SUMMARY_MAX_WIDTH) {
        lv_obj_set_style_text_font(label, &lv_font_manrope_bold_32, 0);
        lv_obj_update_layout(label);
    }
    lv_coord_t width = lv_obj_get_width(label);
    if (width > SUMMARY_MAX_WIDTH) width = SUMMARY_MAX_WIDTH;
    if (width < 1) width = 1;
    lv_obj_set_width(label, width);
    lv_obj_set_x(label, SUMMARY_RIGHT_X - width);
    *geometry_compact = true;
}

void page_01_main_refresh_totals(int total_pcs, const char *amount_text)
{
    char pcs_text[32];
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_COUNTING)) return;
    lv_snprintf(pcs_text, sizeof(pcs_text), "%d", total_pcs);
    if (s_multi_layout) { page_01_multi_refresh(); return; }
    page_01_main_summary_label_set(&s_total_pcs_label, &s_total_pcs_compact,
        "01_pcs_label", pcs_text);
    page_01_main_summary_label_set(&s_total_amount_label, &s_total_amount_compact,
        "01_amount_label", amount_text ? amount_text : "0");
}

void page_01_main_suspend(void)
{
    if (!page_01_main_is_created()) return;
    pause_counting_sim();
    smart_island_set_suspended(true);
    if (s_time_timer) lv_timer_pause(s_time_timer);
    page_01_bottom_animations_stop();
    page_01_main_detail_set_visible(false);
    page_01_multi_visible(false);
    lv_obj_add_flag(main_page, LV_OBJ_FLAG_HIDDEN);
    page_01_main_refresh_start_state();
}

bool page_01_main_resume(void)
{
    if (!page_01_main_is_created()) return false;
    const uint64_t started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    page_01_main_detect_snapshot_changes();
    const uint32_t dirty = s_main_dirty;
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
    resume_counting_sim();
    smart_island_create(main_page);
    smart_island_set_suspended(false);
    page_01_main_send_init_protocol();
    main_time_timer_cb(NULL);
    if (s_time_timer) lv_timer_resume(s_time_timer);
    page_01_main_layout_refresh();
    if (dirty & PAGE_01_MAIN_DIRTY_MODE) page_01_mode_switch_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_ADD) page_01_add_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_WORK) page_01_work_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_BATCH) page_01_batch_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_FO) page_01_face_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_CFD) page_01_cfd_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_SPEED) page_01_speed_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_ERROR) page_01_err_num_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_CURRENCY) page_01_curr_img_refre();
    if (dirty & PAGE_01_MAIN_DIRTY_COUNTING) ui_refresh_main_page();
    if (dirty & PAGE_01_MAIN_DIRTY_LANGUAGE) page_01_update_language_texts();
    page_01_main_refresh_start_state();
    page_01_main_snapshot_capture();
    if (started_us) perf_profile_report_event_us("MAIN",
        dirty ? "RESUME_DIRTY" : "RESUME_CLEAN",
        app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    return true;
}

void page_01_main_reveal_for_transition(void)
{
    if (!page_01_main_is_created()) return;
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
    /* Innovation exposes Main before normal resume. Restore the static detail
     * tree for that first frame; motion was stopped by suspend. */
    page_01_main_layout_refresh();
    lv_obj_move_background(main_page);
}

//// 更新页面上的所有多语言文本
void page_01_update_language_texts(void) //刷新主界面多语言文本
{
    if (!page_01_main_is_created()) return;

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_LANGUAGE |
                                   PAGE_01_MAIN_DIRTY_ALL)) return;

    page_01_detail_section_btn_text_refresh();
    page_01_bottom_a_refresh_mode(false);
    page_01_bottom_a_refresh_add(false);
    page_01_bottom_a_refresh_work(false);
    page_01_bottom_a_refresh_fo(false);
    page_01_bottom_c_refresh_batch(false);
    page_01_bottom_c_refresh_speed(false);
    page_01_bottom_c_refresh_cfd();
    ui_refresh_main_page();
    smart_island_refresh_language_texts();
}
