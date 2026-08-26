#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "un260/lv_resources/lv_img_init.h" 
#include "un260/lv_components/lv_components.h"
#include "un260/boot/boot_service.h"
#include "../aic_ui/aic_ui.h"

#include <string.h>

#define BOOT_SELFTEST_LIST_X 575
#define BOOT_SELFTEST_LIST_Y 76
#define BOOT_SELFTEST_LIST_COUNT 5

typedef struct {
    lv_obj_t *page;
    lv_obj_t *progress_bg;
    lv_obj_t *progress_fill;
    lv_obj_t *progress_percent_label;
    lv_obj_t *progress_loading_label;
    lv_obj_t *selftest_list;
    lv_timer_t *handshake_timer;
    lv_selftest_list_state_t item_states[BOOT_SELFTEST_LIST_COUNT];
    uint8_t progress_percent;
} boot_page_context_t;

static boot_page_context_t g_boot_page;

static void boot_page_context_reset(void)
{
    uint8_t i;

    memset(&g_boot_page, 0, sizeof(g_boot_page));
    for (i = 0; i < BOOT_SELFTEST_LIST_COUNT; i++) {
        g_boot_page.item_states[i] = LV_SELFTEST_LIST_STATE_PENDING;
    }
}

static const char* boot_selftest_base_names[BOOT_SELFTEST_LIST_COUNT] = {
    "Read config parameters",
    "Sensor self-test ",
    "Motor self-test ",
    "Electromagnet self-test ",
    "Image board self-test ",
};
static const char* boot_selftest_running_names[BOOT_SELFTEST_LIST_COUNT] = {
    "Read config parameters ",
    "Sensor self-test ",
    "Motor self-test ",
    "Electromagnet self-test ",
    "Image board self-test ",
};
static const char* boot_selftest_success_names[BOOT_SELFTEST_LIST_COUNT] = {
    "Read config parameters success",
    "Sensor self-test success",
    "Motor self-test success",
    "Electromagnet self-test success",
    "Image board self-test success",
};
static const char* boot_selftest_failed_names[BOOT_SELFTEST_LIST_COUNT] = {
    "Read config parameters failed",
    "Sensor self-test failed",
    "Motor self-test failed",
    "Electromagnet self-test failed",
    "Image board self-test failed",
};
ui_element_t page_08_curr_obj[] = {
    // 背景图

    { "page_08_boot_bg_img.png", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 0, 0, 1280, 400, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },


};

int page_08_curr_len = sizeof(page_08_curr_obj) / sizeof(page_08_curr_obj[0]);

static void boot_selftest_list_create(lv_obj_t* parent); // 创建自检卡片列表
static void boot_selftest_list_apply_state(uint8_t index, lv_selftest_list_state_t state); // 刷新单卡状态
static void boot_selftest_list_mark_pending(void); // 全部恢复待测
static const char *boot_selftest_list_text_get(uint8_t index, lv_selftest_list_state_t state); // 获取自检状态文案
static lv_selftest_list_state_t boot_selftest_list_state_from_result(uint8_t result); // 根据协议结果获取状态

#define BOOT_PROGRESS_X 39
#define BOOT_PROGRESS_Y 380
#define BOOT_PROGRESS_W 1203
#define BOOT_PROGRESS_H 3

static void boot_progress_apply(uint8_t percent)
{
    if (percent > 100) percent = 100;
    g_boot_page.progress_percent = percent;

    if (g_boot_page.progress_fill && lv_obj_is_valid(g_boot_page.progress_fill)) {
        lv_coord_t w = (lv_coord_t)((BOOT_PROGRESS_W * g_boot_page.progress_percent) / 100);
        lv_obj_set_width(g_boot_page.progress_fill, w);
    }

    if (g_boot_page.progress_percent_label &&
        lv_obj_is_valid(g_boot_page.progress_percent_label)) {
        lv_label_set_text_fmt(g_boot_page.progress_percent_label, "%u%%",
                              g_boot_page.progress_percent);
    }
}

static void boot_progress_handshake_timer_cb(lv_timer_t* t)
{
    LV_UNUSED(t);
    if (g_boot_page.progress_percent >= 18) return;

    uint8_t next = (uint8_t)(g_boot_page.progress_percent + 3);
    if (next > 18) next = 18;
    boot_progress_apply(next);
}

static void boot_progress_handshake_tick_start(void)
{
    if (g_boot_page.handshake_timer == NULL) {
        g_boot_page.handshake_timer = lv_timer_create(
            boot_progress_handshake_timer_cb, 1000, NULL);
    } else {
        lv_timer_resume(g_boot_page.handshake_timer);
    }
}

static void boot_progress_handshake_tick_stop(void)
{
    if (g_boot_page.handshake_timer) {
        lv_timer_del(g_boot_page.handshake_timer);
        g_boot_page.handshake_timer = NULL;
    }
}

void boot_progress_set(uint8_t percent)
{
    boot_progress_apply(percent);
    if (percent >= 20) {
        boot_progress_handshake_tick_stop();
    }
    if (percent >= 100) {
        boot_selftest_list_finish();
    }
}

void boot_progress_reset(void)
{
    boot_progress_apply(0);
}

static void boot_progress_create(lv_obj_t* parent)
{
    g_boot_page.progress_bg = lv_obj_create(parent);
    lv_obj_remove_style_all(g_boot_page.progress_bg);
    lv_obj_set_pos(g_boot_page.progress_bg, BOOT_PROGRESS_X, BOOT_PROGRESS_Y);
    lv_obj_set_size(g_boot_page.progress_bg, BOOT_PROGRESS_W, BOOT_PROGRESS_H);
    lv_obj_set_style_bg_color(g_boot_page.progress_bg, lv_color_hex(0xDDEFF9), 0);
    lv_obj_set_style_bg_opa(g_boot_page.progress_bg, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_boot_page.progress_bg, LV_OBJ_FLAG_SCROLLABLE);

    g_boot_page.progress_fill = lv_obj_create(parent);
    lv_obj_remove_style_all(g_boot_page.progress_fill);
    lv_obj_set_pos(g_boot_page.progress_fill, BOOT_PROGRESS_X, BOOT_PROGRESS_Y);
    lv_obj_set_size(g_boot_page.progress_fill, 0, BOOT_PROGRESS_H);
    lv_obj_set_style_bg_color(g_boot_page.progress_fill, lv_color_hex(0x10A5E9), 0);
    lv_obj_set_style_bg_opa(g_boot_page.progress_fill, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_boot_page.progress_fill, LV_OBJ_FLAG_SCROLLABLE);

    g_boot_page.progress_loading_label = lv_label_create(parent);
    lv_label_set_text(g_boot_page.progress_loading_label, "LOADING");
    lv_obj_set_pos(g_boot_page.progress_loading_label, 44, 361);
    lv_obj_set_style_text_color(g_boot_page.progress_loading_label,
                                lv_color_hex(0x6D92AA), 0);
    lv_obj_set_style_text_font(g_boot_page.progress_loading_label,
                               LV_FONT_DEFAULT, 0);

    g_boot_page.progress_percent_label = lv_label_create(parent);
    lv_label_set_text(g_boot_page.progress_percent_label, "0%");
    lv_obj_set_pos(g_boot_page.progress_percent_label, 1220, 361);
    lv_obj_set_style_text_color(g_boot_page.progress_percent_label,
                                lv_color_hex(0x6D92AA), 0);
    lv_obj_set_style_text_font(g_boot_page.progress_percent_label,
                               LV_FONT_DEFAULT, 0);

    boot_progress_apply(g_boot_page.progress_percent);
}

static void boot_selftest_list_create(lv_obj_t* parent) // 创建自检卡片列表
{
    if (parent == NULL || g_boot_page.selftest_list != NULL) {
        return;
    }

    lv_selftest_list_config_t cfg;

    lv_selftest_list_config_init(&cfg);
    cfg.item_w = 682;
    cfg.item_h = 36;
    cfg.item_gap = 10;
    cfg.item_pad_x = 10;
    cfg.icon_size = 15;
    cfg.spinner_size = 15;
    cfg.name_gap = 7;
    cfg.state_w = 100;
    cfg.success_text_color = lv_color_hex(0x0084FF);
    cfg.loading_text_color = lv_color_hex(0x0084FF);
    cfg.pending_text_color = lv_color_hex(0x0084FF);
    cfg.error_text_color = lv_color_hex(0x0084FF);

    g_boot_page.selftest_list = lv_selftest_list_create_with_config(
        parent, BOOT_SELFTEST_LIST_COUNT, &cfg);
    if (g_boot_page.selftest_list == NULL) {
        return;
    }

    lv_obj_set_pos(g_boot_page.selftest_list,
                   BOOT_SELFTEST_LIST_X, BOOT_SELFTEST_LIST_Y);
    lv_obj_move_foreground(g_boot_page.selftest_list);

    for (uint8_t i = 0; i < BOOT_SELFTEST_LIST_COUNT; i++) {
        lv_selftest_list_set_item(g_boot_page.selftest_list, i,
                                  boot_selftest_base_names[i],
                                  LV_SELFTEST_LIST_STATE_PENDING);
    }
}

static void boot_selftest_list_apply_state(uint8_t index, lv_selftest_list_state_t state) // 刷新单卡状态
{
    if (g_boot_page.selftest_list == NULL ||
        !lv_obj_is_valid(g_boot_page.selftest_list) ||
        index >= BOOT_SELFTEST_LIST_COUNT) {
        return;
    }

    g_boot_page.item_states[index] = state;
    lv_selftest_list_set_item(g_boot_page.selftest_list, index,
                              boot_selftest_list_text_get(index, state), state);
}

static void boot_selftest_list_mark_pending(void) // 全部恢复待测
{
    for (uint8_t i = 0; i < BOOT_SELFTEST_LIST_COUNT; i++) {
        boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_PENDING);
    }
}

void boot_selftest_list_reset(void) // 重置自检卡片显示
{
    if (g_boot_page.selftest_list == NULL ||
        !lv_obj_is_valid(g_boot_page.selftest_list)) {
        return;
    }

    boot_selftest_list_mark_pending();
}

void boot_selftest_list_sync_step(uint8_t step) // 根据步骤同步自检卡片
{
    if (g_boot_page.selftest_list == NULL ||
        !lv_obj_is_valid(g_boot_page.selftest_list)) {
        return;
    }

    if (step >= BOOT_SELFTEST_LIST_COUNT) {
        boot_selftest_list_finish();
        return;
    }

    for (uint8_t i = 0; i < BOOT_SELFTEST_LIST_COUNT; i++) {
        if (g_boot_page.item_states[i] == LV_SELFTEST_LIST_STATE_ERROR) {
            boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_ERROR);
        } else if (g_boot_page.item_states[i] == LV_SELFTEST_LIST_STATE_SUCCESS) {
            boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_SUCCESS);
        } else if (i < step) {
            boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_SUCCESS);
        } else if (i == step) {
            boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_LOADING);
        } else {
            boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_PENDING);
        }
    }
}

void boot_selftest_list_finish(void) // 自检完成后全部置成功
{
    if (g_boot_page.selftest_list == NULL ||
        !lv_obj_is_valid(g_boot_page.selftest_list)) {
        return;
    }

    for (uint8_t i = 0; i < BOOT_SELFTEST_LIST_COUNT; i++) {
        boot_selftest_list_apply_state(i, LV_SELFTEST_LIST_STATE_SUCCESS);
    }
}

void boot_selftest_list_set_result(uint8_t index, uint8_t result) // 按协议结果更新单项状态
{
    if (g_boot_page.selftest_list == NULL ||
        !lv_obj_is_valid(g_boot_page.selftest_list)) {
        return;
    }

    if (index >= BOOT_SELFTEST_LIST_COUNT) {
        return;
    }

    boot_selftest_list_apply_state(index, boot_selftest_list_state_from_result(result));
}

static lv_selftest_list_state_t boot_selftest_list_state_from_result(uint8_t result) // 根据协议结果获取状态
{
    if (result == 0x01) {
        return LV_SELFTEST_LIST_STATE_SUCCESS;
    }

    return LV_SELFTEST_LIST_STATE_ERROR;
}

static const char *boot_selftest_list_text_get(uint8_t index, lv_selftest_list_state_t state) // 获取自检状态文案
{
    if (index >= BOOT_SELFTEST_LIST_COUNT) {
        return "";
    }

    switch (state) {
    case LV_SELFTEST_LIST_STATE_LOADING:
        return boot_selftest_running_names[index];
    case LV_SELFTEST_LIST_STATE_SUCCESS:
        return boot_selftest_success_names[index];
    case LV_SELFTEST_LIST_STATE_ERROR:
        return boot_selftest_failed_names[index];
    case LV_SELFTEST_LIST_STATE_PENDING:
    default:
        return boot_selftest_base_names[index];
    }
}

void ui_page_08_curr_create(lv_obj_t* parent)
{
    if (g_boot_page.page && lv_obj_is_valid(g_boot_page.page)) return;

    ui_page_08_curr_destroy();
    if (parent == NULL) {
        parent = lv_scr_act();
    }

    g_boot_page.page = lv_obj_create(parent);
    lv_obj_remove_style_all(g_boot_page.page);
    lv_obj_set_pos(g_boot_page.page, 0, 0);
    lv_obj_set_size(g_boot_page.page, 1280, 400);
    lv_obj_clear_flag(g_boot_page.page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_boot_page.page, LV_SCROLLBAR_MODE_OFF);
    lv_ui_obj_init(g_boot_page.page, page_08_curr_obj, page_08_curr_len);
    boot_progress_create(g_boot_page.page);
    boot_selftest_list_create(g_boot_page.page);
    boot_selftest_list_reset();
    boot_selftest_list_sync_step(boot_service_self_test_sequence_index());

    if (boot_service_get_stage() == BOOT_STAGE_HANDSHAKE &&
        boot_service_handshake_state() != HANDSHAKE_OK) {
        boot_progress_set(0);
        boot_progress_handshake_tick_start();
    } else if (boot_service_get_stage() >= BOOT_STAGE_SENSOR && boot_service_get_stage() <= BOOT_STAGE_IMAGE) {
        uint8_t selftest_ok_cnt = (uint8_t)(boot_service_get_stage() - BOOT_STAGE_SENSOR);
        boot_progress_set((uint8_t)(20 + selftest_ok_cnt * 10));
    } else if (boot_service_get_stage() == BOOT_STAGE_DONE) {
        boot_progress_set(100);
    } else {
        boot_progress_handshake_tick_stop();
    }

};

void ui_page_08_curr_destroy(void)
{
    boot_progress_handshake_tick_stop();

    if (g_boot_page.page && lv_obj_is_valid(g_boot_page.page)) {
        lv_obj_del(g_boot_page.page);
    }

    boot_page_context_reset();
}
