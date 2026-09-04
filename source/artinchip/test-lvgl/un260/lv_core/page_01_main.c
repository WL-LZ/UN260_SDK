#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_01_detail_scroll.h"
#include "un260/lv_core/lv_page_manager.h"
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
static lv_obj_t* page_01_main_scroll_container = NULL;
static ui_element_t page_01_main_obj[];

// 添加数组元素计数变量
static int page_01_main_len = 0;

//          "obj_name", obj_type, img_src,
//          { x, y, w, h, r, g, b },
//          { laebl, r, g, b, font,align },
//          { opacity, radius, border_width, use_custom_style },
//          event_cb, event_code, user_data, obj_ref ,btn_style}
//label 背景默认透明


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
static void page_01_create_main_scrollable_container(void);

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

void page_01_main_mark_dirty(uint32_t flags)
{
    s_main_dirty |= flags;
}

bool page_01_main_defer_refresh(uint32_t flags)
{
    if (!page_01_main_is_visible()) {
        page_01_main_mark_dirty(flags);
        return true;
    }
    s_main_dirty &= ~flags;
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

static void page_01_create_main_scrollable_container(void)
{
    if (page_01_main_scroll_container) return;

    page_01_main_scroll_container = lv_obj_create(main_page);
    lv_obj_set_pos(page_01_main_scroll_container, 720, 54);
    lv_obj_set_size(page_01_main_scroll_container, 300, 240);
    lv_obj_set_style_bg_opa(page_01_main_scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_01_main_scroll_container, 0, 0);
    lv_obj_set_style_pad_all(page_01_main_scroll_container, 0, 0);
    lv_obj_set_scrollbar_mode(page_01_main_scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(page_01_main_scroll_container, LV_DIR_VER);
    lv_obj_add_flag(page_01_main_scroll_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(page_01_main_scroll_container, page_01_detail_area_event_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(page_01_main_scroll_container, page_01_detail_area_event_cb,
                        LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(page_01_main_scroll_container, page_01_detail_area_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(page_01_main_scroll_container, page_01_detail_area_event_cb,
                        LV_EVENT_PRESS_LOST, NULL);

    for (int i = 1; i <= 10; i++) {
        char name[32];
        lv_obj_t* obj;

        snprintf(name, sizeof(name), "denom_%d_label", i);
        obj = find_obj_by_name(name, page_01_main_obj, page_01_main_len);
        if (obj) {
            lv_obj_set_parent(obj, page_01_main_scroll_container);
            lv_obj_set_pos(obj, 8, (i - 1) * 32);
        }

        snprintf(name, sizeof(name), "pcs_%d_label", i);
        obj = find_obj_by_name(name, page_01_main_obj, page_01_main_len);
        if (obj) {
            lv_obj_set_parent(obj, page_01_main_scroll_container);
            lv_obj_set_pos(obj, 106, (i - 1) * 32);
        }

        snprintf(name, sizeof(name), "amount_%d_label", i);
        obj = find_obj_by_name(name, page_01_main_obj, page_01_main_len);
        if (obj) {
            lv_obj_set_parent(obj, page_01_main_scroll_container);
            lv_obj_set_pos(obj, 213, (i - 1) * 32);
        }
    }

    page_01_detail_scroll_attach(main_page, page_01_main_scroll_container);
    perf_profile_watch_invalidation(page_01_main_scroll_container,
                                    "MAIN_DETAIL_SCROLL");
}

void page_01_mode_switch_refre(void)
{
    const char* mode_str = "NONE";

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_MODE)) return;
    switch (machine_state_mode()) {
    case MODE_MDC:
        mode_str = "MDC";
        break;
    case MODE_CNT:
        mode_str = "CNT";
        break;
    case MODE_VER:
        mode_str = "VER";
        break;
    case MODE_SDC:
        mode_str = "SDC";
        break;
    default:
        break;
    }

    update_label_by_name(page_01_main_obj, page_01_main_len,
                         "mix_label", "%s", mode_str);
    update_label_by_name(page_01_main_obj, page_01_main_len,
                         "mode_label", "%s", mode_str);
    page_01_bottom_a_refresh_mode(false);
}

void page_01_add_refre(void)
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_ADD)) return;
    update_label_by_name(page_01_main_obj, page_01_main_len, "add_label", "%s",
                         machine_state_add_enabled() ? "ADD:ON" : "ADD:OFF");
    page_01_bottom_a_refresh_add(false);
}

void page_01_work_refre(void)
{
    static const char* const work[] = { "AUTO", "MANUAL" };

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_WORK)) return;
    update_label_by_name(page_01_main_obj, page_01_main_len, "auto_label", "%s",
                         work[machine_state_work_mode()]);
    page_01_bottom_a_refresh_work(false);
}

void page_01_batch_refre(void)
{
    static const char* const batch[] = { "BATCH :", "VBATCH :" };
    char buf[12];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_BATCH)) return;
    snprintf(buf, sizeof(buf), "%d", machine_state_batch_num());
    update_label_by_name(page_01_main_obj, page_01_main_len, "bacth_label", "%s",
                         batch[machine_state_batch_mode()]);
    update_label_by_name(page_01_main_obj, page_01_main_len, "bacth_num_label", "%s",
                         machine_state_batch_enabled() ? buf : "OFF");
    page_01_bottom_c_refresh_batch(false);
}

void page_01_face_refre(void)
{
    static const char* const face[] = { "F./O. : OFF", "F.", "O.", "F./O." };

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_FO)) return;
    update_label_by_name(page_01_main_obj, page_01_main_len, "face_label", "%s",
                         face[machine_state_fo_mode()]);
    page_01_bottom_a_refresh_fo(false);
}

void page_01_cfd_refre(void)
{
    static const char* const cfd[] = { "L", "M", "H" };

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CFD)) return;
    update_label_by_name(page_01_main_obj, page_01_main_len,
                         "cfd_value_label", "%s", cfd[machine_state_cfd_mode()]);
#if LV_DEBUG
    printf("cfd:%s\n", cfd[machine_state_cfd_mode()]);
#endif
    page_01_bottom_c_refresh_cfd();
}

void page_01_speed_refre(void)
{
    static const int speed[] = { 600, 800, 1000 };

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_SPEED)) return;
    update_label_by_name(page_01_main_obj, page_01_main_len,
                         "speed_num_label", "%d", speed[machine_state_speed()]);
    page_01_bottom_c_refresh_speed(false);
}

void page_01_err_num_refre(void)
{
    char buf[12];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_ERROR)) return;
    snprintf(buf, sizeof(buf), "%d", counting_data_reject_pcs_count(counting_data_current()));
    update_label_by_name(page_01_main_obj, page_01_main_len,
                         "reject_num_label", "%s", buf);
}

void page_01_curr_img_refre(void)
{
    char curr_code[4];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CURRENCY)) return;

    currency_state_get_effective_code(curr_code);
    if (s_curr_rendered_valid && strcmp(curr_code, s_curr_rendered_code) == 0) return;
    if (s_curr_img == NULL || !lv_obj_is_valid(s_curr_img)) {
        s_curr_img = find_obj_by_name("curr_USD_img", page_01_main_obj,
                                      page_01_main_len);
    }
    if (s_curr_label == NULL || !lv_obj_is_valid(s_curr_label)) {
        s_curr_label = find_obj_by_name("curr_icon_label", page_01_main_obj,
                                        page_01_main_len);
    }
    if (s_curr_img && lv_obj_is_valid(s_curr_img))
        lv_img_set_src(s_curr_img, get_currency_img(curr_code));
    if (s_curr_label && lv_obj_is_valid(s_curr_label))
        lv_label_set_text(s_curr_label, currency_state_display_code(curr_code));
    snprintf(s_curr_rendered_code, sizeof(s_curr_rendered_code), "%s", curr_code);
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
    PAGE_01_BOTTOM_TEXT_ANIM_FADE,
    PAGE_01_BOTTOM_TEXT_ANIM_SLIDE,
    PAGE_01_BOTTOM_TEXT_ANIM_ZOOM
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
    smart_island_refresh_time(); //刷新灵动岛默认时间
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

static void page_01_bottom_text_anim_zoom_cb(void* var, int32_t v) //底部按钮文本缩放动画
{
    lv_obj_set_style_transform_zoom((lv_obj_t*)var, (lv_coord_t)v, 0);
}

static void page_01_bottom_label_anim_stop(lv_obj_t* label)
{
    if (label == NULL || !lv_obj_is_valid(label)) return;
    lv_anim_del(label, NULL);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(label, 0, 0);
    lv_obj_set_style_transform_zoom(label, 256, 0);
}

static void page_01_bottom_animations_stop(void)
{
    page_01_bottom_label_anim_stop(s_bottom_a_label_mode);
    page_01_bottom_label_anim_stop(s_bottom_a_label_add);
    page_01_bottom_label_anim_stop(s_bottom_a_label_work);
    page_01_bottom_label_anim_stop(s_bottom_a_label_fo);
    page_01_bottom_label_anim_stop(s_bottom_c_label_batch);
    page_01_bottom_label_anim_stop(s_bottom_c_label_speed);
    page_01_bottom_label_anim_stop(s_bottom_c_label_cfd);
}

static void page_01_bottom_label_anim_run(lv_obj_t* label, const char* text,
    page_01_bottom_text_anim_t anim_type) //刷新底部按钮文本并执行轻量动画
{
    lv_anim_t a;
    const char* old_text;

    if (label == NULL || text == NULL) return;
    old_text = lv_label_get_text(label);

    if (old_text != NULL && strcmp(old_text, text) == 0) {
        return; //文本未变化时不重复做动画，避免闪烁
    }

    lv_anim_del(label, NULL);
    lv_label_set_text(label, text);

    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(label, 0, 0);
    lv_obj_set_style_transform_zoom(label, 256, 0);

    if (anim_type == PAGE_01_BOTTOM_TEXT_ANIM_NONE) {
        return;
    }

    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_time(&a, 160);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

    if (anim_type == PAGE_01_BOTTOM_TEXT_ANIM_FADE) {
        lv_obj_set_style_text_opa(label, LV_OPA_60, 0);
        lv_anim_set_values(&a, LV_OPA_60, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, page_01_bottom_text_anim_opa_cb);
        lv_anim_start(&a);
        return;
    }

    if (anim_type == PAGE_01_BOTTOM_TEXT_ANIM_SLIDE) {
        lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
        lv_obj_set_style_translate_x(label, 12, 0);

        lv_anim_set_values(&a, LV_OPA_70, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, page_01_bottom_text_anim_opa_cb);
        lv_anim_start(&a);

        lv_anim_init(&a);
        lv_anim_set_var(&a, label);
        lv_anim_set_time(&a, 160);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_values(&a, 12, 0);
        lv_anim_set_exec_cb(&a, page_01_bottom_text_anim_x_cb);
        lv_anim_start(&a);
        return;
    }

    if (anim_type == PAGE_01_BOTTOM_TEXT_ANIM_ZOOM) {
        lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
        lv_obj_set_style_transform_zoom(label, 236, 0);

        lv_anim_set_values(&a, LV_OPA_70, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, page_01_bottom_text_anim_opa_cb);
        lv_anim_start(&a);

        lv_anim_init(&a);
        lv_anim_set_var(&a, label);
        lv_anim_set_time(&a, 160);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_values(&a, 236, 256);
        lv_anim_set_exec_cb(&a, page_01_bottom_text_anim_zoom_cb);
        lv_anim_start(&a);
    }
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
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xF8F5F5), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xCECECE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 32, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

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
    lv_obj_set_style_text_color(label, lv_color_hex(0x5D5D5D), 0);
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

static void page_01_bottom_old_bar_hide(void) //隐藏旧底部信息栏对象，避免与新A区重叠
{
    const char* old_obj_name[] = {
        "mix_label", "add_label", "auto_label", "bacth_label", "bacth_num_label",
        "face_label", "cfd_label", "cfd_value_label", "speed_label", "speed_num_label",
        "page_01_collect_icon.png", "page_01_usb_icon.png"
    };
    lv_obj_t* obj;
    uint32_t i;

    for (i = 0; i < sizeof(old_obj_name) / sizeof(old_obj_name[0]); i++) {
        obj = find_obj_by_name(old_obj_name[i], page_01_main_obj, page_01_main_len);
        if (obj) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_obj_t* page_01_bottom_c_btn_create(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
    lv_color_t bg_color, lv_event_cb_t event_cb, bool clickable) //创建主界面底部C区按钮
{
    lv_obj_t* btn = page_01_bottom_box_create(x, y, w, h, bg_color);

    if (clickable) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xCECECE), LV_STATE_PRESSED);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
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

    page_01_bottom_old_bar_hide();

    s_bottom_a_btn_mode = page_01_bottom_btn_create(18, 354, 85, 32, page_01_bottom_mode_btn_event_cb);
    s_bottom_a_btn_add = page_01_bottom_btn_create(109, 354, 95, 32, page_01_add_btn_event_cb);
    s_bottom_a_btn_work = page_01_bottom_btn_create(210, 354, 110, 32, page_01_work_btn_event_cb);
    s_bottom_a_btn_fo = page_01_bottom_btn_create(326, 354, 111, 32, page_01_fo_btn_event_cb);

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

    s_bottom_c_btn_batch = page_01_bottom_c_btn_create(805, 354, 209, 32,
        lv_color_hex(0xF8F5F5), page_01_bottom_batch_btn_event_cb, true);
    s_bottom_c_btn_speed = page_01_bottom_c_btn_create(1020, 354, 113, 32,
        lv_color_hex(0xF8F5F5), page_01_bottom_speed_btn_event_cb, true);
    s_bottom_c_box_cfd = page_01_bottom_c_btn_create(1139, 354, 104, 32,
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

static void page_01_detail_section_btn_update_one(lv_obj_t* btn, bool selected)
{
    lv_color_t base_color = selected ? lv_color_hex(0x48AC50) : lv_color_hex(0xECEFF3);
    lv_color_t pressed_color = selected ? lv_color_hex(0x3F9846) : lv_color_hex(0xDDE3EA);
    lv_color_t border_color = selected ? lv_color_hex(0x48AC50) : lv_color_hex(0xD6DCE4);
    lv_color_t dot_color = selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x7F8894);
    lv_color_t text_color = selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x3F4A56);

    if (btn == NULL || !lv_obj_is_valid(btn)) return;

    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, base_color, 0);
    lv_obj_set_style_bg_color(btn, pressed_color, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, border_color, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x9AA7B5), 0);
    lv_obj_set_style_shadow_width(btn, 5, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);

    lv_obj_t* dot = lv_obj_get_child(btn, 0);
    if (dot != NULL) {
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, 999, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, dot_color, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 14);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* label = lv_obj_get_child(btn, 1);
    if (label != NULL) {
        lv_obj_set_style_text_color(label, text_color, 0);
    }
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
    LV_UNUSED(s_detail_btn_a);
    LV_UNUSED(s_detail_btn_b);
    LV_UNUSED(s_detail_btn_c);
}

static void page_01_detail_section_btn_set_vertical_text(lv_obj_t* label, const char* text)
{
    char vertical[32];
    size_t out = 0;

    if (label == NULL || text == NULL) return;

    for (size_t i = 0; text[i] != '\0' && out + 2 < sizeof(vertical); i++) {
        if (i > 0) {
            vertical[out++] = '\n';
        }
        vertical[out++] = text[i];
    }
    vertical[out] = '\0';

    lv_label_set_text(label, vertical);
}

static lv_obj_t* page_01_detail_section_btn_create(lv_coord_t x, lv_coord_t y,
    const char* text, page_01_detail_section_t section)
{
    lv_obj_t* btn = lv_btn_create(main_page);
    lv_obj_t* dot;
    lv_obj_t* label;

    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 34, 96);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, page_01_detail_section_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)section);

    dot = lv_obj_create(btn);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x7F8894), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 999, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 14);

    label = lv_label_create(btn);
    page_01_detail_section_btn_set_vertical_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_bold_10, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x3F4A56), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label, -1, 0);
    lv_obj_set_width(label, 16);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 28);

    return btn;
}

static void page_01_detail_section_btn_create_all(void)
{
    if (s_detail_btn_a || s_detail_btn_b || s_detail_btn_c) return;

    // 与右侧menu/start/esc按钮中心线对齐，并给右侧按钮区保留间距。
    s_detail_btn_a = page_01_detail_section_btn_create(1086, 16, "REPORT", PAGE_01_DETAIL_SECTION_A);
    s_detail_btn_b = page_01_detail_section_btn_create(1086, 126, "SERIAL", PAGE_01_DETAIL_SECTION_B);
    s_detail_btn_c = page_01_detail_section_btn_create(1086, 236, "ERROR", PAGE_01_DETAIL_SECTION_C);
    page_01_detail_section_btn_text_refresh();
    page_01_detail_section_btn_style_apply();
}

static void page_01_detail_section_btn_destroy_all(void)
{
    for (uint32_t i = 0; i < 3; i++) {
        lv_dma_static_skin_release(&s_main_detail_tab_skins[i]);
    }
    if (s_detail_btn_a) lv_obj_del(s_detail_btn_a);
    if (s_detail_btn_b) lv_obj_del(s_detail_btn_b);
    if (s_detail_btn_c) lv_obj_del(s_detail_btn_c);

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
    if (section > PAGE_01_DETAIL_SECTION_C) return;
    if (s_detail_section == section && !refresh) return;

    page_01_detail_scroll_before_section_switch();
    s_detail_section = section;
    if (refresh) {
        ui_state_save_page01_detail_section();
    }
    page_01_detail_section_btn_style_apply();
    page_01_detail_scroll_after_section_switch();
    if (refresh) {
        ui_refresh_main_page();
    }
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
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_MODE)) return;
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
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_ADD)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_add, page_01_bottom_add_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_a_refresh_work(bool anim_en) //刷新主界面底部A区工作模式文本
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_WORK)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_work, page_01_bottom_work_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_a_refresh_fo(bool anim_en) //刷新主界面底部A区F/O文本
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_FO)) return;
    page_01_bottom_label_anim_run(s_bottom_a_label_fo, page_01_bottom_fo_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_batch(bool anim_en) //刷新主界面底部C区Batch文本
{
    char text_buf[32];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_BATCH)) return;

    if (machine_state_batch_enabled()) {
        lv_snprintf(text_buf, sizeof(text_buf), ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_VALUE_FMT),
            machine_state_batch_num());
    } else {
        lv_snprintf(text_buf, sizeof(text_buf), "%s", ui_text_get(UI_TEXT_PAGE01_BOTTOM_BATCH_OFF));
    }

    page_01_bottom_label_anim_run(s_bottom_c_label_batch, text_buf,
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_NONE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_speed(bool anim_en) //刷新主界面底部C区速度文本
{
    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_SPEED)) return;
    page_01_bottom_label_anim_run(s_bottom_c_label_speed, page_01_bottom_speed_text_get(),
        anim_en ? PAGE_01_BOTTOM_TEXT_ANIM_SLIDE : PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

void page_01_bottom_c_refresh_cfd(void) //刷新主界面底部C区CFD文本
{
    char text_buf[24];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_CFD)) return;

    lv_snprintf(text_buf, sizeof(text_buf), ui_text_get(UI_TEXT_PAGE01_BOTTOM_CFD_FMT),
        "L");
    page_01_bottom_label_anim_run(s_bottom_c_label_cfd, text_buf, PAGE_01_BOTTOM_TEXT_ANIM_NONE);
}

static ui_element_t page_01_main_obj[] = {
    //////////////////////////////////////////////////////
  //***************    BG_IMG_LIST  *******************//
////////////////////////////////////////////////////////

    { "page_01_back.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 0, 0, 1280, 400, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

        //////////////////////////////////////////////////////
    //***************    BTN_LIST   *********************//
    //////////////////////////////////////////////////////

        { "menu_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 1125, 18, 119, 92, 244, 244, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_menu_btn_event_cb, 0, NULL , NULL ,UI_BTN_STYLE_APPLE},

        { "start_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 1125, 128, 119, 92, 244, 244, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_start_btn_event_cb , LV_EVENT_CLICKED, NULL, NULL ,UI_BTN_STYLE_APPLE},

        { "esc_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 1125, 238, 119, 92, 244, 244, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_esc_btn_event_cb, LV_EVENT_CLICKED, NULL, NULL ,UI_BTN_STYLE_APPLE},

        { "mode_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 15, 36, 75, 66, 255, 255, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_mode_btn_event_cb, LV_EVENT_CLICKED, NULL, NULL ,UI_BTN_STYLE_PRESS_FEEL },

        { "setting_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 15, 109, 75, 60, 255, 255, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_set_btn_event_cb, LV_EVENT_CLICKED, NULL, NULL ,UI_BTN_STYLE_PRESS_FEEL },

        { "list_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 15, 176, 75, 60, 255, 255, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_list_btn_event_cb, 0, (void*)(uintptr_t)UI_PAGE_LIST, NULL ,
            UI_BTN_STYLE_PRESS_FEEL},
        { "print_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 15, 243, 75, 73, 255, 255, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 18, 0, true },
            page_01_print_btn_event_cb, 0, NULL, NULL ,UI_BTN_STYLE_PRESS_FEEL },

        { "curr_img_btn", LV_OBJ_TYPE_BUTTON, NULL,
            { 159, 33, 184, 100, 255, 255, 255 },
            { NULL, 0, 0, 0, NULL },
            { 255, 7, 0, true },
            page_01_curr_btn_event_cb, 0, NULL, NULL ,UI_BTN_STYLE_PRESS_FEEL },
  //////////////////////////////////////////////////////
 //***************  IMAGE_LIST **********************//
//////////////////////////////////////////////////////

    { "page_01_collect_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1025, 353, 29, 30, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "curr_USD_img", LV_OBJ_TYPE_IMAGE, &page_01_curr_USD,
        { 156, 31, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_esc_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1167, 254, 38, 38, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_list_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 31, 184, 39, 24, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_menu_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1167, 31, 35, 35, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_mode_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 30, 43, 42, 34, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_print_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 32, 249, 38, 35, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_set_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 33, 113, 35, 35, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_start_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 1167, 142, 41, 35, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "page_01_usb_icon.png", LV_OBJ_TYPE_IMAGE, NULL,
        { 985, 353, 22, 30, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },
  //////////////////////////////////////////////////////
 //***************  LABEL_LIST **********************//
//////////////////////////////////////////////////////
    { "01_pcs_label", LV_OBJ_TYPE_LABEL, NULL,
        { 280, 144, 350, 40, 0, 0, 0 },
        { "0", 170, 23, 0, &lv_font_manrope_bold_44, LV_TEXT_ALIGN_RIGHT },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },
    { "01_amount_label", LV_OBJ_TYPE_LABEL, NULL,
        { 280, 226, 350, 40, 0, 0, 0 },
        { "0", 170, 23, 0, &lv_font_manrope_extrabold_48, LV_TEXT_ALIGN_RIGHT },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },
    { "reject_label", LV_OBJ_TYPE_LABEL, NULL,
        { 382, 67, 110, 27, 0, 0, 0 },
        { "REJECT:", 170, 23, 0, &lv_font_instrument_sans_semibold_20, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "mode_label", LV_OBJ_TYPE_LABEL, NULL,
        { 10, 79, 81, 17, 0, 0, 0 },
        { "MODE", 87, 87, 87, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "setting_label", LV_OBJ_TYPE_LABEL, NULL,
        { 10, 151, 81, 17, 0, 0, 0 },
        { "SETTING", 87, 87, 87, &lv_font_instrument_sans_medium_14, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "list_label", LV_OBJ_TYPE_LABEL, NULL,
        { 10, 215, 81, 17, 0, 0, 0 },
        { "LIST", 87, 87, 87, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "print_label", LV_OBJ_TYPE_LABEL, NULL,
        { 10, 292, 81, 17, 0, 0, 0 },
        { "PRINT", 87, 87, 87, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "mix_label", LV_OBJ_TYPE_LABEL, NULL,
        { 12, 357, 96, 17, 0, 0, 0 },
        { "MIX", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "add_label", LV_OBJ_TYPE_LABEL, NULL,
        { 110, 357, 105, 17, 0, 0, 0 },
        { "ADD", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "auto_label", LV_OBJ_TYPE_LABEL, NULL,
        { 217, 357, 122, 17, 0, 0, 0 },
        { "AUTO", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "bacth_label", LV_OBJ_TYPE_LABEL, NULL,
        { 341, 357, 89, 17, 0, 0, 0 },
        { "BATCH:", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "bacth_num_label", LV_OBJ_TYPE_LABEL, NULL,
        { 434, 357, 128, 17, 0, 0, 0 },
        { "10000", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_LEFT },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "face_label", LV_OBJ_TYPE_LABEL, NULL,
        { 566, 357, 111, 17, 0, 0, 0 },
        { "FACE", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "cfd_label", LV_OBJ_TYPE_LABEL, NULL,
        { 698, 357, 50, 17, 0, 0, 0 },
        { "CFD:", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "cfd_value_label", LV_OBJ_TYPE_LABEL, NULL,
        { 748, 357, 20, 17, 0, 0, 0 },
        { "H", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "speed_label", LV_OBJ_TYPE_LABEL, NULL,
        { 827, 357, 33, 17, 0, 0, 0 },
        { "SP:", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "speed_num_label", LV_OBJ_TYPE_LABEL, NULL,
        { 862, 357, 54, 17, 0, 0, 0 },
        { "1000", 61, 61, 61, &lv_font_instrument_sans_medium_18, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "pcs_label", LV_OBJ_TYPE_LABEL, NULL,
        { 145, 175, 54, 23, 0, 0, 0 },
        { "PCS", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "reject_num_label", LV_OBJ_TYPE_LABEL, NULL,
        { 484, 67, 81, 27, 0, 0, 0 },
        { "0", 170, 23, 0, &lv_font_manrope_bold_20, LV_TEXT_ALIGN_RIGHT },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "curr_icon_label", LV_OBJ_TYPE_LABEL, NULL,
        { 145, 254, 75, 27, 0, 0, 0 },
        { "USD", 0, 115, 255, &lv_font_instrument_sans_semibold_24, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "list_demo_label", LV_OBJ_TYPE_LABEL, NULL,
        { 728, 24, 70, 23, 0, 0, 0 },
        { "DENOM", 121, 150, 0, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "list_pcs_label", LV_OBJ_TYPE_LABEL, NULL,
        { 826, 24, 54, 23, 0, 0, 0 },
        { "PCS", 121, 150, 0, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },

        NULL, 0, NULL, NULL },

    { "list_amount_label", LV_OBJ_TYPE_LABEL, NULL,
        { 933, 24, 80, 23, 0, 0, 0 },
        { "AMOUNT", 121, 150, 0, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "menu_btn_label", LV_OBJ_TYPE_LABEL, NULL,
        { 1144, 77, 80, 23, 0, 0, 0 },
        { "MENU", 71, 87, 87, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "start_btn_label", LV_OBJ_TYPE_LABEL, NULL,
        { 1144, 188, 80, 23, 0, 0, 0 },
        { "START", 71, 87, 87, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "esc_btn_label", LV_OBJ_TYPE_LABEL, NULL,
        { 1144, 298, 80, 23, 0, 0, 0 },
        { "ESC", 71, 87, 87, &lv_font_instrument_sans_medium_20, LV_TEXT_ALIGN_CENTER },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },
//list_label
        { "denom_1_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 54, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_1_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 54, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_1_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 54, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_2_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 84, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_2_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 84, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_2_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 84, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_3_label", LV_OBJ_TYPE_LABEL, NULL,
            { 0, 114, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_3_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 114, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_3_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 114, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_4_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 144, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_4_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 144, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_4_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 144, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_5_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 174, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_5_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 174, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_5_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 174, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_6_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 204, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_6_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 204, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_6_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 204, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_7_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 234, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_7_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 234, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_7_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 234, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_8_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 264, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_8_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 264, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_8_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 264, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_9_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 294, 77, 23, 0, 0, 0 },
            { "---", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_9_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 294, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_9_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 294, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "denom_10_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 324, 77, 23, 0, 0, 0 },
            { "---", 86, 86, 86, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "pcs_10_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 324, 54, 23, 0, 0, 0 },
            { "0", 86, 86, 86, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "amount_10_label", LV_OBJ_TYPE_LABEL, NULL,
            { 0, 324, 80, 23, 0, 0, 0 },
            { "0.00", 86, 86, 86, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "total_label", LV_OBJ_TYPE_LABEL, NULL,
            { 714, 305, 77, 23, 0, 0, 0 },
            { "TOTAL", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "total_pcs_label", LV_OBJ_TYPE_LABEL, NULL,
            { 826, 305, 54, 23, 0, 0, 0 },
            { "0", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

        { "total_amount_label", LV_OBJ_TYPE_LABEL, NULL,
            { 933, 305, 80, 23, 0, 0, 0 },
            { "0.00", 61, 61, 61, &lv_font_instrument_sans_medium_16, LV_TEXT_ALIGN_CENTER },
            { 255, 0, 0, false },
            NULL, 0, NULL, NULL },

};

// 添加数组元素计数变量
void ui_main_create(lv_obj_t* parent)
{
    //creat page_main 
    if (page_01_main_is_created()) return;
    ui_main_destroy();
    main_page = lv_obj_create(parent ? parent : lv_scr_act());
    lv_obj_remove_style_all(main_page);
    lv_obj_set_pos(main_page, 0, 0);
    lv_obj_set_size(main_page, 1280, 400);
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_scrollbar_mode(main_page, LV_SCROLLBAR_MODE_OFF); // 滚动条关闭
    static bool frist_creat = true;

    // 计算数组长度
    page_01_main_len = sizeof(page_01_main_obj) / sizeof(ui_element_t);
    
    //创建图片和其他UI元素
    lv_ui_obj_init(main_page, page_01_main_obj, page_01_main_len);
    page_01_main_action_skins_attach();
    

    if (frist_creat)
    {
        /* Main page denomination data is master-driven (0x0B), no local fallback here. */
        page_01_mode_switch_refre();

        frist_creat = false;
    }
    page_01_detail_section_set((page_01_detail_section_t)ui_state_page01_detail_section_get(), false);
    // 创建滚动容器并将标签放入容器
   // sim_data_init();           // 初始化面额列表、count=0、amount=0
    page_01_create_main_scrollable_container();
    ui_refresh_main_page();    // 创建滚动容器后再按当前分区布局并写入数据
    page_01_add_refre();
    page_01_work_refre();
    page_01_batch_refre();
    page_01_face_refre();
    page_01_cfd_refre();
    page_01_speed_refre();
    page_01_err_num_refre();
    page_01_curr_img_refre();
    page_01_bottom_bg_create_all();
    page_01_bottom_a_create();
    page_01_bottom_c_create();
    page_01_bottom_a_refresh_mode(false);
    page_01_bottom_a_refresh_add(false);
    page_01_bottom_a_refresh_work(false);
    page_01_bottom_a_refresh_fo(false);
    page_01_bottom_c_refresh_batch(false);
    page_01_bottom_c_refresh_speed(false);
    page_01_bottom_c_refresh_cfd();
    page_01_detail_section_btn_create_all();
    s_time_label = NULL;
    machine_time_init();
    main_time_refresh();
    if (!s_time_timer) {
        s_time_timer = lv_timer_create(main_time_timer_cb, 1000, NULL);
    } else {
        lv_timer_resume(s_time_timer);
    }
    lv_print_toast_create();
    ui_state_apply_common_runtime();
    /* Object construction is also used by boot prewarm.  Keep hardware
     * protocol effects tied to actual page activation, not cache creation. */
    if (!ui_manager_is_prewarming_page(UI_PAGE_MAIN)) {
        page_01_main_send_init_protocol();
    }
    smart_island_create(main_page); //创建主界面B区灵动岛
    smart_island_register_action_cb(page_01_smart_island_action_cb);
    smart_island_refresh_time(); //初始化时间显示
    page_32_innovation_handle_attach(main_page);
    s_main_dirty = 0;
    page_01_main_snapshot_capture();

}

void ui_main_destroy(void)
{
    if (s_time_timer) {
        lv_timer_del(s_time_timer);
        s_time_timer = NULL;
    }
    s_time_label = NULL;

    if (page_01_main_is_created()) {
        // 安全清理所有资源和定时器
        page_01_main_action_skins_release();
        cleanup_counting_sim();
        page_01_bottom_a_destroy();
        page_01_bottom_c_destroy();
        page_01_detail_section_btn_destroy_all();
        page_01_bottom_bg_destroy_all();
        smart_island_destroy(); //销毁灵动岛
        page_32_innovation_handle_detach();
        perf_profile_unwatch_invalidation(page_01_main_scroll_container);
        lv_obj_del(main_page);
        main_page = NULL;
        page_01_main_scroll_container = NULL;
    }
    s_main_dirty = PAGE_01_MAIN_DIRTY_ALL;
    s_main_snapshot_valid = false;
    s_curr_img = NULL;
    s_curr_label = NULL;
    s_curr_rendered_valid = false;
    s_curr_rendered_code[0] = '\0';
    s_total_pcs_label = NULL;
    s_total_amount_label = NULL;
    s_total_pcs_compact = false;
    s_total_amount_compact = false;
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
    return page_01_main_scroll_container;
}

void page_01_main_scroll_reset(void)
{
    if (page_01_main_scroll_container &&
        lv_obj_is_valid(page_01_main_scroll_container)) {
        lv_obj_scroll_to_y(page_01_main_scroll_container, 0, LV_ANIM_OFF);
    }
}

void page_01_main_icon_feedback(const char *name)
{
    if (!page_01_main_is_visible()) return;
    icon_feedback_comp(name, page_01_main_obj, page_01_main_len);
}

static void page_01_main_summary_label_set(lv_obj_t **cached_label,
                                           bool *geometry_compact,
                                           const char *name,
                                           const char *text)
{
    enum { SUMMARY_RIGHT_X = 630, SUMMARY_MAX_WIDTH = 350 };
    lv_obj_t *label;
    lv_coord_t width;
    const char *old_text;

    if (*cached_label == NULL || !lv_obj_is_valid(*cached_label)) {
        *cached_label = page_01_main_find_obj(name);
        *geometry_compact = false;
    }
    label = *cached_label;
    if (label == NULL || !lv_obj_is_valid(label)) return;

    old_text = lv_label_get_text(label);
    if (old_text == NULL || strcmp(old_text, text) != 0) {
        lv_label_set_text(label, text);
    } else if (*geometry_compact) {
        return;
    }

    /* The generated layout used a fixed 350x40 box.  A changing count then
     * invalidated all 14k pixels even for a one-digit value.  Keep the same
     * right edge, but make the object only as wide as its rendered text. */
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_update_layout(label);
    width = lv_obj_get_width(label);
    if (width < 1) width = 1;
    if (width > SUMMARY_MAX_WIDTH) width = SUMMARY_MAX_WIDTH;
    lv_obj_set_width(label, width);
    lv_obj_set_x(label, SUMMARY_RIGHT_X - width);
    *geometry_compact = true;
}

void page_01_main_refresh_totals(int total_pcs, const char *amount_text)
{
    char pcs_text[32];

    if (page_01_main_defer_refresh(PAGE_01_MAIN_DIRTY_COUNTING)) return;
    lv_snprintf(pcs_text, sizeof(pcs_text), "%d", total_pcs);
    page_01_main_summary_label_set(&s_total_pcs_label,
                                   &s_total_pcs_compact,
                                   "01_pcs_label", pcs_text);
    page_01_main_summary_label_set(&s_total_amount_label,
                                   &s_total_amount_compact,
                                   "01_amount_label",
                                   amount_text != NULL ? amount_text : "0");
}

void page_01_main_suspend(void)
{
    if (!page_01_main_is_created()) {
        return;
    }

    /* Keep the snapshot tied to the last completed UI refresh.  Capturing
     * model data while hiding a cached page can conceal an update that has
     * not reached the objects yet. */
    pause_counting_sim();
    smart_island_set_suspended(true);
    if (s_time_timer) {
        lv_timer_pause(s_time_timer);
    }
    page_01_bottom_animations_stop();
    if (page_01_main_scroll_container &&
        lv_obj_is_valid(page_01_main_scroll_container)) {
        lv_anim_del(page_01_main_scroll_container, NULL);
    }
    lv_obj_add_flag(main_page, LV_OBJ_FLAG_HIDDEN);
}

bool page_01_main_resume(void)
{
    uint64_t started_us;
    uint32_t dirty;

    if (!page_01_main_is_created()) {
        return false;
    }

    started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    page_01_main_detect_snapshot_changes();
    dirty = s_main_dirty;
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
    resume_counting_sim();
    /* PURE can retain the shared island under its hidden page.  Move it to
     * the visible MAIN tree before applying deferred state. */
    smart_island_create(main_page);
    smart_island_set_suspended(false);
    page_01_main_send_init_protocol();
    main_time_timer_cb(NULL);
    if (s_time_timer) {
        lv_timer_resume(s_time_timer);
    }
    if (page_01_main_scroll_container &&
        lv_obj_is_valid(page_01_main_scroll_container) &&
        lv_obj_get_scroll_y(page_01_main_scroll_container) != 0) {
        lv_obj_scroll_to_y(page_01_main_scroll_container, 0, LV_ANIM_OFF);
    }
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
    page_01_scroll_hint_on_enter();
    page_01_main_snapshot_capture();
    if (started_us != 0) {
        perf_profile_report_event_us("MAIN",
            dirty == 0 ? "RESUME_CLEAN" : "RESUME_DIRTY",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return true;
}

void page_01_main_reveal_for_transition(void)
{
    if (!page_01_main_is_created()) return;
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
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
