#include "un260/lv_core/page_07_curr.h"
#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_01_detail_scroll.h"
#include "un260/lv_resources/lv_image_declear.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_service.h"
#include "un260/counting/counting_action_service.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_setting_runtime.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "lv_page_event.h"
#include "aic_ui/aic_ui.h"
#include "lv_port_indev.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"

static lv_obj_t* curr_page = NULL;
static currency_state_snapshot_t g_curr_page_snapshot;
static bool g_curr_page_snapshot_valid;
static char g_curr_page_selected_code[4];
static lv_timer_t *g_curr_snapshot_prewarm_timer;

static void curr_snapshot_prewarm_timer_cb(lv_timer_t *timer);

ui_element_t page_07_curr_obj[] = {
    // 背景图
    {
        .obj_name = "page_07_bg.png",
        .obj_type = LV_OBJ_TYPE_IMAGE,
        .obj_item = { .x = 0, .y = 0, .w = 1280, .h = 400 },
        .obj_style = { .opacity = 255 },
    },

    /*{ "07_curr_01", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 167, 110, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_02", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 421, 110, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_03", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 675, 110, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_04", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 929, 110, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_05", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 167, 248, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_06", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 421, 248, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_07", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 675, 248, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },

    { "07_curr_08", LV_OBJ_TYPE_IMAGE, &page_07_currency_bg_img,
        { 929, 248, 182, 103, 0, 0, 0 },
        { NULL, 0, 0, 0, NULL },
        { 255, 0, 0, false },
        NULL, 0, NULL, NULL },*/

};

int page_07_curr_len = sizeof(page_07_curr_obj) / sizeof(page_07_curr_obj[0]);

void ui_page_07_curr_create(lv_obj_t* parent)
{
    (void)parent;
    if (curr_page) return;
    g_curr_page_snapshot_valid = false;
    memset(g_curr_page_selected_code, 0, sizeof(g_curr_page_selected_code));
    curr_page = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(curr_page);
    lv_obj_set_pos(curr_page, 0, 0);
    lv_obj_set_size(curr_page, 1280, 400);
    lv_obj_clear_flag(curr_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(curr_page, LV_SCROLLBAR_MODE_OFF);
    lv_ui_obj_init(curr_page, page_07_curr_obj, page_07_curr_len);
    page_07_curr_img_refre();
    if (g_curr_snapshot_prewarm_timer == NULL) {
        g_curr_snapshot_prewarm_timer =
            lv_timer_create(curr_snapshot_prewarm_timer_cb, 120, NULL);
    }

};

void ui_page_07_curr_destroy(void)
{
    if (g_curr_snapshot_prewarm_timer != NULL) {
        lv_timer_del(g_curr_snapshot_prewarm_timer);
        g_curr_snapshot_prewarm_timer = NULL;
    }
    if (curr_page)
    {
        page_07_curr_img_reset();
        lv_obj_del(curr_page);
        curr_page = NULL;
    }
    g_curr_page_snapshot_valid = false;

}

#define CURR_SEL_W               288
#define CURR_SEL_H               400
#define CURR_VIEW_X              288
#define CURR_VIEW_Y              0
#define CURR_VIEW_W              992
#define CURR_VIEW_H              400

#define CURR_CARD_W              200
#define CURR_CARD_H              265
#define CURR_CARD_GAP            24
#define CURR_CARD_STRIDE         (CURR_CARD_W + CURR_CARD_GAP)
#define CURR_CARD_FIRST_X        36
#define CURR_CARD_Y              46
#define CURR_CARD_PAD_RIGHT      580
#define CURR_SEL_NEXT_EXTRA_GAP  10
#define CURR_LEFT_PEEK_W         ((CURR_CARD_W * 2) / 3)

/* The selected card renderer includes its shadow/background padding and is
 * taller than the interactive card itself.  Keep the scroll invalidation
 * layer only as tall as that real visual footprint; the progress track below
 * is a sibling and must not make every horizontal drag repaint empty rows. */
#define CURR_CARD_SELECTED_RENDER_W 256
#define CURR_CARD_SELECTED_RENDER_H 328
#define CURR_CARD_SELECTED_RENDER_Y \
    (CURR_CARD_Y - (CURR_CARD_SEL_H - CURR_CARD_H) / 2 - \
     CURR_CARD_SELECTED_BG_OFS_TOP)
#define CURR_CARD_SCROLL_H       \
    (CURR_CARD_SELECTED_RENDER_Y + CURR_CARD_SELECTED_RENDER_H + 1)

#define CURR_TRACK_Y             365
#define CURR_TRACK_H             6
#define CURR_DRAG_THRESHOLD      14
#define CURR_DRAG_APPLY_MIN_PX   3
#define CURR_FLING_FACTOR        1
#define CURR_FLING_TRIGGER       6
#define CURR_FLING_MAX           (CURR_CARD_STRIDE / 2)
#define CURR_SNAP_IDLE_MS        500
#define CURR_OVERSCROLL_SNAP_MS  300
#define CURR_SNAP_COMMIT_PX      (CURR_CARD_STRIDE / 4)

#define CURR_LEFT_BG_COLOR       0xEDF0F4
#define CURR_RIGHT_BG_COLOR      0xF4F5F7
#define CURR_TEXT_SEL            0xFC4000
#define CURR_TEXT_UNSEL          0xBEBFC0
#define CURR_IMG_UNSEL           0xCDCED0
#define CURR_TRACK_BG            0xEBECED
#define CURR_TRACK_FG            0x75A2DF

#define CURR_CARD_SELECTED_BG_PATH      "L:/usr/local/share/lvgl_data/selected_card.png"
#define CURR_CARD_UNSELECTED_BG_PATH    "L:/usr/local/share/lvgl_data/unselected_card.png"
#define CURR_GRID_SELECTED_MARK_PATH    "L:/usr/local/share/lvgl_data/view_selected.png"
#define CURR_CARD_SELECTED_BG_OFS_X     18
#define CURR_CARD_SELECTED_BG_OFS_TOP   16

#define CURR_CARD_SEL_W          ((CURR_CARD_W * 11) / 10)
#define CURR_CARD_SEL_H          ((CURR_CARD_H * 11) / 10)

#define CURR_BTN_W               70
#define CURR_BTN_H               36
#define CURR_BTN_Y               358
#define CURR_VIEW_BTN_X          18
#define CURR_FAV_BTN_X           113
#define CURR_BACK_BTN_X          207

#define CURR_FOCUS_BOX_X         24
#define CURR_FOCUS_BOX_Y         54
#define CURR_FOCUS_BOX_W         245
#define CURR_FOCUS_BOX_H         279

#define CURR_LEFT_IMG_ALIGN_Y    72
#define CURR_LEFT_IMG_ALIGN_X    2
#define CURR_LEFT_CODE_X         54
#define CURR_LEFT_CODE_Y         214
#define CURR_LEFT_CODE_DECOR_X   128
#define CURR_LEFT_CODE_DECOR_Y   228
#define CURR_LEFT_NO_X           184
#define CURR_LEFT_NO_Y           300

#define CURR_GRID_COLS           6
#define CURR_GRID_CELL_W         158
#define CURR_GRID_CELL_H         92
#define CURR_GRID_ITEM_W         132
#define CURR_GRID_START_X        8
#define CURR_GRID_START_Y        10
#define CURR_GRID_ROW_STEP       92
#define CURR_GRID_TEXT_BOTTOM    -4
#define CURR_GRID_GROUP_OFS_X    25
#define CURR_GRID_FLAG_Y         (-6)
#define CURR_GRID_FAV_X          -3
#define CURR_GRID_FAV_Y          -2

#define CURR_FAV_BTN_IN_CARD_X   139
#define CURR_FAV_BTN_IN_CARD_Y   13
#define CURR_FAV_BTN_IN_CARD_W   49
#define CURR_FAV_BTN_IN_CARD_H   49

#define CURR_FLAG_TARGET_W       82
#define CURR_LEFT_FLAG_TARGET_W  121
#define CURR_FLAG_Y_IN_CARD      19

page07_curr_context_t g_page07_curr = {
    .model.view_mode = PAGE07_CURR_VIEW_CARD,
    .gesture.snap_target_visible_idx = -1,
};

typedef enum {
    CURR_MODE_TRANSITION_NONE = 0,
    CURR_MODE_TRANSITION_TO_AUTO,
    CURR_MODE_TRANSITION_TO_MANUAL,
    CURR_MODE_TRANSITION_WAIT_MANUAL_CURRENCY,
} curr_mode_transition_t;

static struct {
    curr_mode_transition_t kind;
    uint8_t target_index;
    uint8_t requested_mode;
    char target_code[4];
} g_curr_mode_transition;

static int g_curr_track_x = -1;
static int g_curr_track_w = -1;
static int g_curr_card_styled_visible_idx = -1;
static int g_curr_grid_styled_abs_idx = -1;

static void curr_refresh_right_views(void);
static void curr_apply_selected_style(void);
static void curr_apply_card_visual(int i, bool selected,
                                   int pos_x, int pos_y);
static void curr_style_back_button(void);
static int curr_abs_i32(int v)
{
    return (v >= 0) ? v : -v;
}

static void curr_set_img_target_width(lv_obj_t* img, const char* code, int target_w)
{
    lv_img_header_t info;
    if (lv_img_decoder_get_info(get_currency_img(code), &info) == LV_RES_OK && info.w > 0) {
        int zoom = (target_w * 256) / (int)info.w;
        if (zoom < 32) zoom = 32;
        lv_img_set_zoom(img, zoom);
    }
}

static void curr_set_path_target_width(lv_obj_t* img, const char* path, int target_w)
{
    lv_img_header_t info;

    if (img == NULL || path == NULL || target_w <= 0) return;
    if (lv_img_decoder_get_info(path, &info) != LV_RES_OK || info.w <= 0) return;

    int zoom = (target_w * 256) / (int)info.w;
    if (zoom < 32) zoom = 32;
    lv_img_set_zoom(img, zoom);
}

static void curr_set_image_unselected_style(lv_obj_t* img)
{
    lv_obj_set_style_img_recolor(img, lv_color_hex(CURR_IMG_UNSEL), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img, LV_OPA_40, 0);
}

static void curr_set_image_selected_style(lv_obj_t* img)
{
    lv_obj_set_style_img_recolor(img, lv_color_hex(CURR_IMG_UNSEL), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img, LV_OPA_COVER, 0);
}

static int curr_scroll_x_abs(void)
{
    if (g_page07_curr.objects.list == NULL) return 0;
    return curr_abs_i32(lv_obj_get_scroll_x(g_page07_curr.objects.list));
}

static int curr_get_max_scroll(void)
{
    int cnt = g_page07_curr.model.visible_count;
    if (cnt <= 0) return 0;
    int content_w = CURR_CARD_FIRST_X
                  + (cnt - 1) * CURR_CARD_STRIDE
                  + CURR_CARD_W
                  + CURR_CARD_PAD_RIGHT
                  + CURR_SEL_NEXT_EXTRA_GAP;
    int max_scroll = content_w - CURR_VIEW_W;
    if (max_scroll < 0) max_scroll = 0;
    return max_scroll;
}

static int curr_highlight_idx_from_scroll(int sx)
{
    if (g_page07_curr.model.visible_count <= 0) return 0;
    if (g_page07_curr.model.visible_count == 1) return 0;

    int max_scroll = curr_get_max_scroll();
    if (sx <= 2) return 0;
    if (sx >= max_scroll - 2) return g_page07_curr.model.visible_count - 1;

    int idx = 1 + (sx + CURR_CARD_STRIDE / 2) / CURR_CARD_STRIDE;
    if (idx < 1) idx = 1;
    if (idx >= g_page07_curr.model.visible_count) idx = g_page07_curr.model.visible_count - 1;
    return idx;
}

static int curr_scroll_from_highlight_idx(int vis_idx)
{
    if (g_page07_curr.model.visible_count <= 0) return 0;

    int max_scroll = curr_get_max_scroll();
    if (vis_idx <= 0) return 0;
    if (vis_idx >= g_page07_curr.model.visible_count - 1) return max_scroll;

    /* Keep only one left-side card visible, with about 2/3 of that unselected card peeking in. */
    int sx = CURR_CARD_FIRST_X
           + (vis_idx - 1) * CURR_CARD_STRIDE
           + (CURR_CARD_W - CURR_LEFT_PEEK_W);

    if (sx < 0) sx = 0;
    if (sx > max_scroll) sx = max_scroll;
    return sx;
}

static int curr_get_track_base_width(void)
{
    int thumb_w;

    if (g_page07_curr.model.visible_count <= 0) return CURR_VIEW_W;

    thumb_w = CURR_VIEW_W / g_page07_curr.model.visible_count;
    if (thumb_w < 36) thumb_w = 36;
    if (thumb_w > CURR_VIEW_W) thumb_w = CURR_VIEW_W;
    return thumb_w;
}

static void curr_update_track_by_scroll(int sx)
{
    if (g_page07_curr.objects.thumb == NULL || g_page07_curr.model.visible_count <= 0) return;

    int thumb_w = CURR_VIEW_W / g_page07_curr.model.visible_count;
    if (thumb_w < 36) thumb_w = 36;
    if (thumb_w > CURR_VIEW_W) thumb_w = CURR_VIEW_W;

    int max_scroll = curr_get_max_scroll();
    if (max_scroll <= 0 || g_page07_curr.model.visible_count <= 1) {
        if (g_curr_track_w != CURR_VIEW_W) {
            lv_obj_set_size(g_page07_curr.objects.thumb, CURR_VIEW_W, CURR_TRACK_H);
            g_curr_track_w = CURR_VIEW_W;
        }
        if (g_curr_track_x != 0) {
            lv_obj_set_pos(g_page07_curr.objects.thumb, 0, CURR_TRACK_Y);
            g_curr_track_x = 0;
        }
        return;
    }

    if (sx < 0) sx = 0;
    if (sx > max_scroll) sx = max_scroll;

    int idx = g_page07_curr.model.selected_visible_idx;
    if (idx < 0) idx = 0;
    if (idx >= g_page07_curr.model.visible_count) idx = g_page07_curr.model.visible_count - 1;

    // 按高亮卡片索引均分滑块位置，避免尾部两档挤在一起
    int x = (idx * (CURR_VIEW_W - thumb_w)) / (g_page07_curr.model.visible_count - 1);

    if (g_curr_track_w != thumb_w) {
        lv_obj_set_size(g_page07_curr.objects.thumb, thumb_w, CURR_TRACK_H);
        g_curr_track_w = thumb_w;
    }
    if (g_curr_track_x != x) {
        lv_obj_set_pos(g_page07_curr.objects.thumb, x, CURR_TRACK_Y);
        g_curr_track_x = x;
    }
}

static void curr_apply_overscroll_visual(int overscroll_px)
{
    int base_w;
    int shrink;
    int thumb_w;
    int thumb_x;

    if (g_page07_curr.objects.list == NULL) return;

    if (lv_obj_get_x(g_page07_curr.objects.list) != overscroll_px) {
        lv_obj_set_x(g_page07_curr.objects.list, overscroll_px);
    }

    if (g_page07_curr.objects.thumb == NULL) return;

    if (overscroll_px == 0) {
        curr_update_track_by_scroll(curr_scroll_x_abs());
        return;
    }

    base_w = curr_get_track_base_width();
    shrink = curr_abs_i32(overscroll_px) / 2;
    if (shrink > base_w - 18) shrink = base_w - 18;
    if (shrink < 0) shrink = 0;
    thumb_w = base_w - shrink;
    if (thumb_w < 18) thumb_w = 18;

    thumb_x = (overscroll_px > 0) ? 0 : (CURR_VIEW_W - thumb_w);
    if (g_curr_track_w != thumb_w) {
        lv_obj_set_size(g_page07_curr.objects.thumb, thumb_w, CURR_TRACK_H);
        g_curr_track_w = thumb_w;
    }
    if (g_curr_track_x != thumb_x) {
        lv_obj_set_pos(g_page07_curr.objects.thumb, thumb_x, CURR_TRACK_Y);
        g_curr_track_x = thumb_x;
    }
}

static void curr_reset_overscroll_visual(void)
{
    curr_apply_overscroll_visual(0);
}

static void curr_overscroll_anim_x_cb(void* var, int32_t v)
{
    (void)var;
    curr_apply_overscroll_visual((int)v);
}

static void curr_animate_overscroll_back(void)
{
    int start;
    lv_anim_t a;

    if (g_page07_curr.objects.list == NULL) return;
    start = lv_obj_get_x(g_page07_curr.objects.list);
    if (start == 0) {
        curr_reset_overscroll_visual();
        return;
    }

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_page07_curr.objects.list);
    lv_anim_set_exec_cb(&a, curr_overscroll_anim_x_cb);
    lv_anim_set_values(&a, start, 0);
    lv_anim_set_time(&a, 320);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

static void curr_set_left_info_by_abs(int abs_idx)
{
    char curr_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, curr_code)) return;
    lv_img_set_src(g_page07_curr.objects.left_img, get_currency_img(curr_code));
    curr_set_img_target_width(g_page07_curr.objects.left_img, curr_code,
                              CURR_LEFT_FLAG_TARGET_W);
    lv_obj_align(g_page07_curr.objects.left_img, LV_ALIGN_TOP_MID, CURR_LEFT_IMG_ALIGN_X, CURR_LEFT_IMG_ALIGN_Y);
    lv_label_set_text(g_page07_curr.objects.left_code,
                      currency_state_display_code(curr_code));
    if (g_page07_curr.objects.left_code_decor) {
        lv_label_set_text(g_page07_curr.objects.left_code_decor,
                          currency_state_display_code(curr_code));
    }
    lv_label_set_text_fmt(g_page07_curr.objects.left_no, "NO.%02d", abs_idx + 1);
}

static void curr_style_view_button(void)
{
    if (g_page07_curr.objects.btn_view == NULL || g_page07_curr.objects.btn_view_label == NULL) return;

    lv_obj_set_style_radius(g_page07_curr.objects.btn_view, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_view, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_page07_curr.objects.btn_view, lv_color_hex(0x0073FF), 0);
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_view, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_view, 12, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_view, LV_OPA_10, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_view_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g_page07_curr.objects.btn_view_label,
                      (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) ? "CARD" : "VIEW");
    lv_obj_center(g_page07_curr.objects.btn_view_label);
}

static void curr_style_fav_button(void)
{
    if (g_page07_curr.objects.btn_favorite == NULL || g_page07_curr.objects.btn_favorite_label == NULL) return;

    lv_obj_set_style_radius(g_page07_curr.objects.btn_favorite, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_favorite, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_page07_curr.objects.btn_favorite,
                              g_page07_curr.model.favorite_only ? lv_color_hex(0xE9DEBD) : lv_color_hex(0x8F8F8F), 0);
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_favorite, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_favorite, 0, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_favorite, LV_OPA_0, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_favorite_label,
                                g_page07_curr.model.favorite_only ? lv_color_hex(0x8A6A11) : lv_color_hex(0x5F5F5F), 0);
    lv_label_set_text(g_page07_curr.objects.btn_favorite_label, "FAV");
    lv_obj_center(g_page07_curr.objects.btn_favorite_label);
}

static void curr_style_back_button(void)
{
    if (g_page07_curr.objects.btn_back == NULL || g_page07_curr.objects.btn_back_label == NULL) return;

    lv_obj_set_style_radius(g_page07_curr.objects.btn_back, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_back, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_page07_curr.objects.btn_back, lv_color_hex(0xD9D9D9), 0);
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_back, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_back, 0, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_back, LV_OPA_0, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_back_label, lv_color_hex(0x000000), 0);
    lv_label_set_text(g_page07_curr.objects.btn_back_label, "BACK");
    lv_obj_center(g_page07_curr.objects.btn_back_label);
}

static void curr_refresh_left_buttons(void)
{
    curr_style_view_button();
    curr_style_fav_button();
    curr_style_back_button();
}

static void curr_back_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_switch(UI_PAGE_MAIN);
}

static void curr_scroll_to_raw(int x, bool anim, bool apply_style)
{
    int max_scroll = curr_get_max_scroll();
    int current_scroll;

    if (g_page07_curr.objects.list == NULL) return;
    if (x < 0) x = 0;
    if (x > max_scroll) x = max_scroll;

    if (lv_obj_get_x(g_page07_curr.objects.list) != 0) {
        curr_reset_overscroll_visual();
    }
    current_scroll = curr_scroll_x_abs();
    if (current_scroll != x) {
        lv_obj_scroll_to_x(g_page07_curr.objects.list, x,
                           anim ? LV_ANIM_ON : LV_ANIM_OFF);
    }

    if (g_page07_curr.model.visible_count > 0) {
        int vis_idx = curr_highlight_idx_from_scroll(x);
        int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
        if (abs_idx != g_page07_curr.model.selected_abs_idx) {
            g_page07_curr.model.selected_abs_idx = abs_idx;
            g_page07_curr.model.selected_visible_idx = vis_idx;
            if (apply_style) {
                curr_apply_selected_style();
            }
        }
    }
    curr_update_track_by_scroll(x);
}

static void curr_scroll_to_visible_idx(int vis_idx, bool anim, bool apply_style)
{
    if (g_page07_curr.model.visible_count <= 0) return;
    if (vis_idx < 0) vis_idx = 0;
    if (vis_idx >= g_page07_curr.model.visible_count) vis_idx = g_page07_curr.model.visible_count - 1;
    curr_scroll_to_raw(curr_scroll_from_highlight_idx(vis_idx), anim, apply_style);
}

static int curr_pick_nearest_visible_idx(void)
{
    return curr_highlight_idx_from_scroll(curr_scroll_x_abs());
}

static int curr_get_release_direction(void)
{
    int scroll_delta;

    scroll_delta = curr_scroll_x_abs() - g_page07_curr.gesture.start_scroll;
    if (scroll_delta > 0) return 1;
    if (scroll_delta < 0) return -1;
    if (g_page07_curr.gesture.last_dx < 0) return 1;
    if (g_page07_curr.gesture.last_dx > 0) return -1;
    return 0;
}

static int curr_pick_release_visible_idx(void)
{
    int step;
    int vis_idx;
    int drag_px;
    int dir;

    if (g_page07_curr.model.visible_count <= 0) return 0;

    drag_px = curr_abs_i32(curr_scroll_x_abs() - g_page07_curr.gesture.start_scroll);
    dir = curr_get_release_direction();
    if (dir == 0) return g_page07_curr.gesture.start_visible_idx;

    step = (drag_px >= CURR_CARD_STRIDE * 2) ? 2 : 1;
    vis_idx = g_page07_curr.gesture.start_visible_idx + dir * step;

    if (vis_idx < 0) vis_idx = 0;
    if (vis_idx >= g_page07_curr.model.visible_count) vis_idx = g_page07_curr.model.visible_count - 1;
    return vis_idx;
}

static int curr_calc_release_scroll_target(void)
{
    int drag_px;
    int dir;
    int target_scroll;

    drag_px = curr_abs_i32(curr_scroll_x_abs() - g_page07_curr.gesture.start_scroll);
    dir = curr_get_release_direction();
    if (dir == 0) return curr_scroll_x_abs();

    if (drag_px >= CURR_CARD_STRIDE * 2) {
        target_scroll = curr_scroll_x_abs() + dir * (CURR_CARD_STRIDE * 2);
    } else if (drag_px >= CURR_CARD_STRIDE) {
        target_scroll = curr_scroll_x_abs() + dir * CURR_CARD_STRIDE;
    } else {
        target_scroll = curr_scroll_from_highlight_idx(curr_pick_release_visible_idx());
    }

    if (target_scroll < 0) target_scroll = 0;
    if (target_scroll > curr_get_max_scroll()) target_scroll = curr_get_max_scroll();
    return target_scroll;
}

static void curr_snap_timer_cb(lv_timer_t* t)
{
    int vis_idx;

    (void)t;
    g_page07_curr.gesture.snap_timer = NULL;
    if (g_page07_curr.model.visible_count <= 0) return;

    vis_idx = curr_pick_nearest_visible_idx();
    g_page07_curr.gesture.snap_target_visible_idx = -1;
    g_page07_curr.model.selected_visible_idx = vis_idx;
    g_page07_curr.model.selected_abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    curr_apply_selected_style();
    curr_scroll_to_visible_idx(vis_idx, true, true);
}

static void curr_start_snap_timer(uint32_t ms)
{
    if (g_page07_curr.gesture.snap_timer) {
        lv_timer_del(g_page07_curr.gesture.snap_timer);
        g_page07_curr.gesture.snap_timer = NULL;
    }
    g_page07_curr.gesture.snap_timer = lv_timer_create(curr_snap_timer_cb, ms, NULL);
    if (g_page07_curr.gesture.snap_timer) lv_timer_set_repeat_count(g_page07_curr.gesture.snap_timer, 1);
}

static void curr_select_and_exit_abs(int abs_idx)
{
    char curr_code[4];
    char target_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, target_code)) return;
    if (currency_service_switch_pending() || setting_service_mode_is_pending() ||
        g_curr_mode_transition.kind != CURR_MODE_TRANSITION_NONE) return;
    currency_state_get_selected_code(curr_code);
    if (page07_curr_model_code_equal(curr_code, target_code)) {
        ui_manager_switch(UI_PAGE_MAIN);
        return;
    }

    if (currency_state_is_auto_code(target_code)) {
        memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        g_curr_mode_transition.kind = CURR_MODE_TRANSITION_TO_AUTO;
        g_curr_mode_transition.target_index = (uint8_t)abs_idx;
        memcpy(g_curr_mode_transition.target_code, target_code, 4);
        if (!setting_service_request_auto_currency()) {
            memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        }
        return;
    }

    if (currency_state_is_auto_code(curr_code)) {
        uint8_t restore_mode = machine_state_mode();

        if (restore_mode != MODE_MDC && restore_mode != MODE_SDC &&
            restore_mode != MODE_CNT) {
            restore_mode = MODE_MDC;
        }
        memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        g_curr_mode_transition.kind = CURR_MODE_TRANSITION_TO_MANUAL;
        g_curr_mode_transition.target_index = (uint8_t)abs_idx;
        g_curr_mode_transition.requested_mode = restore_mode;
        memcpy(g_curr_mode_transition.target_code, target_code, 4);
        if (!setting_service_request_mode(restore_mode)) {
            memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        }
        return;
    }

    if (!currency_service_request_switch((uint8_t)abs_idx, target_code)) return;
    if (protocol_send(0x03, (const uint8_t*)target_code, 3) < 0) {
        currency_switch_result_t result;

        if (currency_service_take_switch_result(0x02, &result)) {
            page_07_curr_apply_switch_result(&result);
        }
    }
}

void page_07_curr_apply_mode_result(uint8_t requested_mode, bool success)
{
    curr_mode_transition_t transition = g_curr_mode_transition.kind;
    uint8_t target_index = g_curr_mode_transition.target_index;

    if ((transition == CURR_MODE_TRANSITION_TO_AUTO &&
         requested_mode != SETTING_MODE_TARGET_AUTO_CURRENCY) ||
        (transition == CURR_MODE_TRANSITION_TO_MANUAL &&
         requested_mode != g_curr_mode_transition.requested_mode) ||
        transition == CURR_MODE_TRANSITION_NONE) {
        return;
    }
    if (!success) {
        memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        if (curr_page != NULL) show_currency_set_fail_popup();
        return;
    }

    if (transition == CURR_MODE_TRANSITION_TO_AUTO) {
        memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
        if (!currency_state_confirm_auto_selection()) {
            if (curr_page != NULL) show_currency_set_fail_popup();
            return;
        }
        g_page07_curr.model.selected_abs_idx = target_index;
        g_page07_curr.model.selected_visible_idx =
            page07_curr_model_find_visible_pos(target_index);
        page07_curr_model_save();
        page_01_curr_img_refre();
        smart_island_refresh_summary();
        if (curr_page != NULL) {
            ui_manager_switch(UI_PAGE_MAIN);
            page_01_scroll_hint_force_hide();
            page_01_main_scroll_reset();
        }
        return;
    }

    g_curr_mode_transition.kind = CURR_MODE_TRANSITION_WAIT_MANUAL_CURRENCY;
}

void page_07_curr_poll_selection(void)
{
    uint8_t target_index;
    char target_code[4];

    if (g_curr_mode_transition.kind !=
            CURR_MODE_TRANSITION_WAIT_MANUAL_CURRENCY ||
        app_setting_runtime_mode_clear_pending() ||
        counting_action_clear_pending() ||
        currency_service_switch_pending()) {
        return;
    }

    target_index = g_curr_mode_transition.target_index;
    memcpy(target_code, g_curr_mode_transition.target_code, 4);
    memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
    if (!currency_service_request_switch(target_index, target_code) ||
        protocol_send(0x03, (const uint8_t *)target_code, 3) < 0) {
        currency_switch_result_t result;

        if (currency_service_take_switch_result(0x02, &result)) {
            page_07_curr_apply_switch_result(&result);
        } else if (curr_page != NULL) {
            show_currency_set_fail_popup();
        }
    }
}

void page_07_curr_cancel_pending_selection(void)
{
    if (g_curr_mode_transition.kind == CURR_MODE_TRANSITION_NONE) return;
    memset(&g_curr_mode_transition, 0, sizeof(g_curr_mode_transition));
    if (curr_page != NULL) show_currency_set_fail_popup();
}

void page_07_curr_apply_switch_result(const currency_switch_result_t* result)
{
    char curr_code[4];

    if (!result) return;
    if (result->success) {
        g_page07_curr.model.selected_abs_idx = result->target_index;
        g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
        page07_curr_model_save();
        if (curr_page == NULL) return;
        ui_manager_switch(UI_PAGE_MAIN);
        page_01_scroll_hint_force_hide();
        // 切换币种成功后再次归零，确保不会出现首行被遮挡
        page_01_main_scroll_reset();
        return;
    }

    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
    if (curr_page == NULL) return;
    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_apply_selected_style();
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx, true, true);
    } else {
        curr_refresh_right_views();
    }
    show_currency_set_fail_popup();
}

static void curr_update_card_fav_ui(int i)
{
    bool sel = (g_page07_curr.cards[i].abs_idx == g_page07_curr.model.selected_abs_idx);
    bool fav = page07_curr_model_is_favorite(g_page07_curr.cards[i].abs_idx);

    if (g_page07_curr.cards[i].fav_btn == NULL || g_page07_curr.cards[i].fav_icon == NULL) return;

    if (!sel || g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) {
        lv_obj_add_flag(g_page07_curr.cards[i].fav_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(g_page07_curr.cards[i].fav_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(g_page07_curr.cards[i].fav_btn, fav ? lv_color_hex(0xBFDFFF) : lv_color_hex(0xE5E5E6), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.cards[i].fav_btn, LV_OPA_COVER, 0);
    lv_img_set_src(g_page07_curr.cards[i].fav_icon, fav ? "L:/usr/local/share/lvgl_data/fav.png" : "L:/usr/local/share/lvgl_data/unfav.png");
}

static void curr_update_grid_fav_ui(int i)
{
    bool fav = page07_curr_model_is_favorite(g_page07_curr.grid_items[i].abs_idx);

    if (g_page07_curr.grid_items[i].fav_btn == NULL || g_page07_curr.grid_items[i].fav_icon == NULL) return;

    lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_TRANSP, 0);
    lv_img_set_src(g_page07_curr.grid_items[i].fav_icon, fav ? "L:/usr/local/share/lvgl_data/fav.png" : "L:/usr/local/share/lvgl_data/unfav.png");
}

static void curr_fav_press_feedback_cb(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    if (btn == NULL) return;

    lv_obj_t* icon = lv_obj_get_child(btn, 0);
    if (icon == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_opa(btn, 220, 0);
        lv_img_set_zoom(icon, 235);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
        lv_img_set_zoom(icon, 256);
    }
}

static void curr_set_card_render_state(int i, bool selected,
                                       int pos_x, int pos_y)
{
    page07_curr_card_t *card;

    if (i < 0 || i >= g_page07_curr.model.visible_count) return;
    card = &g_page07_curr.cards[i];
    if (card->render_root == NULL || card->background == NULL ||
        card->visual_card == NULL) return;

    if (selected) {
        lv_obj_set_size(card->render_root,
                        CURR_CARD_SELECTED_RENDER_W,
                        CURR_CARD_SELECTED_RENDER_H);
        lv_obj_set_pos(card->render_root,
                       pos_x - (CURR_CARD_SEL_W - CURR_CARD_W) / 2 -
                           CURR_CARD_SELECTED_BG_OFS_X,
                       pos_y - (CURR_CARD_SEL_H - CURR_CARD_H) / 2 -
                           CURR_CARD_SELECTED_BG_OFS_TOP);
        lv_img_set_src(card->background, CURR_CARD_SELECTED_BG_PATH);
        lv_img_set_zoom(card->background, LV_IMG_ZOOM_NONE);
        lv_obj_set_pos(card->background, 0, 0);
        lv_obj_set_size(card->visual_card, CURR_CARD_SEL_W,
                        CURR_CARD_SEL_H);
        lv_obj_set_pos(card->visual_card,
                       CURR_CARD_SELECTED_BG_OFS_X,
                       CURR_CARD_SELECTED_BG_OFS_TOP);
        lv_obj_set_style_text_color(card->name,
                                    lv_color_hex(CURR_TEXT_SEL), 0);
        lv_obj_set_style_text_color(card->no, lv_color_hex(0x202020), 0);
        curr_set_image_selected_style(card->img);
    } else {
        lv_obj_set_size(card->render_root, CURR_CARD_W, CURR_CARD_H);
        lv_obj_set_pos(card->render_root, pos_x, pos_y);
        lv_img_set_src(card->background, CURR_CARD_UNSELECTED_BG_PATH);
        curr_set_path_target_width(card->background,
                                   CURR_CARD_UNSELECTED_BG_PATH,
                                   CURR_CARD_W);
        lv_obj_set_pos(card->background, 0, 0);
        lv_obj_set_size(card->visual_card, CURR_CARD_W, CURR_CARD_H);
        lv_obj_set_pos(card->visual_card, 0, 0);
        lv_obj_set_style_text_color(card->name, lv_color_hex(0x7E7E7E), 0);
        lv_obj_set_style_text_color(card->no,
                                    lv_color_hex(CURR_TEXT_UNSEL), 0);
        curr_set_image_unselected_style(card->img);
    }
}

static bool curr_card_snapshot_key(int i, bool selected,
                                   char key[48])
{
    page07_curr_card_t *card;
    char curr_code[4];

    if (i < 0 || i >= g_page07_curr.model.visible_count || key == NULL) {
        return false;
    }
    card = &g_page07_curr.cards[i];
    if (!currency_state_get_code((uint8_t)card->abs_idx, curr_code)) {
        return false;
    }

    /* The sequence number is rendered into the card. Keep it in the visual
     * identity so a controller-provided catalog reorder cannot reuse a stale
     * NO.xx image for the same currency code. */
    snprintf(key, 48, "CURR_CARD_%s_%02d_%s", curr_code,
             card->abs_idx + 1, selected ? "SELECTED" : "NORMAL");
    return true;
}

static bool curr_acquire_card_snapshot(int i, bool selected,
                                       bool create_on_miss)
{
    page07_curr_card_t *card;
    lv_dma_snapshot_t **target;
    char cache_key[48];

    if (i < 0 || i >= g_page07_curr.model.visible_count) return false;
    card = &g_page07_curr.cards[i];
    if (card->render_root == NULL ||
        !curr_card_snapshot_key(i, selected, cache_key)) {
        return false;
    }

    target = selected ? &card->selected_cache :
                        &card->unselected_cache;
    if (*target != NULL) return true;

    if (create_on_miss) {
        curr_set_card_render_state(i, selected,
                                   card->base_x, card->base_y);
        *target = lv_dma_snapshot_cache_acquire_or_create(
            card->render_root, cache_key);
    } else {
        *target = lv_dma_snapshot_cache_acquire(cache_key);
    }
    return *target != NULL;
}

static void curr_attach_cached_card_snapshots(int i)
{
    (void)curr_acquire_card_snapshot(i, false, false);
    (void)curr_acquire_card_snapshot(i, true, false);
}

static bool curr_build_one_missing_card_snapshot(void)
{
    /* Prioritize NORMAL snapshots across the complete catalog.  With a
     * bounded cache, creating NORMAL+SELECTED pairs per card exhausted the
     * budget after only a few currencies and left every later card on the
     * expensive live-render path. */
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        page07_curr_card_t *card = &g_page07_curr.cards[i];
        bool selected;
        int pos_x;
        bool created;

        if (card->render_root == NULL ||
            card->unselected_cache != NULL) {
            continue;
        }

        selected = i == g_page07_curr.model.selected_visible_idx;
        pos_x = card->base_x;
        if (i > g_page07_curr.model.selected_visible_idx) {
            pos_x += CURR_SEL_NEXT_EXTRA_GAP;
        }

        created = curr_acquire_card_snapshot(i, false, true);
        curr_apply_card_visual(i, selected, pos_x, card->base_y);
        return created;
    }

    /* Only after normal scrolling coverage is complete, use remaining
     * capacity for selected-state snapshots. */
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        page07_curr_card_t *card = &g_page07_curr.cards[i];
        bool selected;
        int pos_x;
        bool created;

        if (card->render_root == NULL ||
            card->selected_cache != NULL) {
            continue;
        }

        selected = i == g_page07_curr.model.selected_visible_idx;
        pos_x = card->base_x;
        if (i > g_page07_curr.model.selected_visible_idx) {
            pos_x += CURR_SEL_NEXT_EXTRA_GAP;
        }

        created = curr_acquire_card_snapshot(i, true, true);
        curr_apply_card_visual(i, selected, pos_x, card->base_y);
        return created;
    }

    return false;
}

static void curr_release_card_snapshots(void)
{
    for (int i = 0; i < PAGE07_CURR_MAX_ITEMS; i++) {
        lv_dma_snapshot_cache_release(
            g_page07_curr.cards[i].unselected_cache);
        lv_dma_snapshot_cache_release(
            g_page07_curr.cards[i].selected_cache);
        g_page07_curr.cards[i].unselected_cache = NULL;
        g_page07_curr.cards[i].selected_cache = NULL;
    }
}

static void curr_apply_card_visual(int i, bool selected,
                                   int pos_x, int pos_y)
{
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    lv_dma_snapshot_t *snapshot = selected ? card->selected_cache
                                           : card->unselected_cache;

    if (snapshot != NULL && card->composite != NULL) {
        const lv_img_dsc_t *image = lv_dma_snapshot_image(snapshot);

        lv_obj_add_flag(card->render_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card->composite, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(card->composite, image);
        lv_img_set_zoom(card->composite, LV_IMG_ZOOM_NONE);
        if (selected) {
            lv_obj_set_pos(card->composite,
                           pos_x - (CURR_CARD_SEL_W - CURR_CARD_W) / 2 -
                               CURR_CARD_SELECTED_BG_OFS_X,
                           pos_y - (CURR_CARD_SEL_H - CURR_CARD_H) / 2 -
                               CURR_CARD_SELECTED_BG_OFS_TOP);
        } else {
            lv_obj_set_pos(card->composite, pos_x, pos_y);
        }
    } else {
        if (card->composite != NULL) {
            lv_obj_add_flag(card->composite, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(card->render_root, LV_OBJ_FLAG_HIDDEN);
        curr_set_card_render_state(i, selected, pos_x, pos_y);
    }

    if (selected) {
        lv_obj_set_size(card->card, CURR_CARD_SEL_W, CURR_CARD_SEL_H);
        lv_obj_set_pos(card->card,
                       pos_x - (CURR_CARD_SEL_W - CURR_CARD_W) / 2,
                       pos_y - (CURR_CARD_SEL_H - CURR_CARD_H) / 2);
    } else {
        lv_obj_set_size(card->card, CURR_CARD_W, CURR_CARD_H);
        lv_obj_set_pos(card->card, pos_x, pos_y);
    }
}

static void curr_apply_selected_style(void)
{
    bool selection_changed;
    int previous_selected_idx;

    if (g_page07_curr.objects.card_layer == NULL || g_page07_curr.model.visible_count <= 0) return;

    previous_selected_idx = g_curr_card_styled_visible_idx;
    selection_changed =
        previous_selected_idx !=
        g_page07_curr.model.selected_visible_idx;

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        bool sel = (i == g_page07_curr.model.selected_visible_idx);
        int pos_x = g_page07_curr.cards[i].base_x;
        int pos_y = g_page07_curr.cards[i].base_y;
        int previous_pos_x = g_page07_curr.cards[i].base_x;
        bool previous_sel = i == previous_selected_idx;
        bool visual_changed;

        if (i > g_page07_curr.model.selected_visible_idx) pos_x += CURR_SEL_NEXT_EXTRA_GAP;
        if (i > previous_selected_idx) previous_pos_x += CURR_SEL_NEXT_EXTRA_GAP;

        visual_changed = previous_selected_idx < 0 ||
                         previous_sel != sel ||
                         previous_pos_x != pos_x;

        if (!selection_changed || !visual_changed) {
            curr_update_card_fav_ui(i);
            continue;
        }

        curr_apply_card_visual(i, sel, pos_x, pos_y);

        curr_update_card_fav_ui(i);
    }

    g_curr_card_styled_visible_idx =
        g_page07_curr.model.selected_visible_idx;
}

static void curr_right_drag_cb(lv_event_t* e)
{
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_indev_t* indev = lv_indev_get_act();
        if (indev == NULL) return;

        lv_indev_get_point(indev, &g_page07_curr.gesture.start_point);
        g_page07_curr.gesture.last_point = g_page07_curr.gesture.start_point;
        g_page07_curr.gesture.last_dx = 0;
        g_page07_curr.gesture.active = true;
        g_page07_curr.gesture.dragging = false;
        g_page07_curr.gesture.start_scroll = curr_scroll_x_abs();
        g_page07_curr.gesture.start_visible_idx = curr_pick_nearest_visible_idx();
        curr_reset_overscroll_visual();

        if (g_page07_curr.gesture.snap_timer) {
            lv_timer_del(g_page07_curr.gesture.snap_timer);
            g_page07_curr.gesture.snap_timer = NULL;
        }
        g_page07_curr.gesture.snap_target_visible_idx = -1;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (!g_page07_curr.gesture.active) return;

        lv_indev_t* indev = lv_indev_get_act();
        if (indev == NULL) return;

        lv_point_t p;
        lv_indev_get_point(indev, &p);

        int dx = p.x - g_page07_curr.gesture.start_point.x;
        int dy = p.y - g_page07_curr.gesture.start_point.y;
        g_page07_curr.gesture.last_dx = p.x - g_page07_curr.gesture.last_point.x;
        g_page07_curr.gesture.last_point = p;

        if (!g_page07_curr.gesture.dragging) {
            if (curr_abs_i32(dx) > CURR_DRAG_THRESHOLD && curr_abs_i32(dx) >= curr_abs_i32(dy)) {
                g_page07_curr.gesture.dragging = true;
            }
        }

        if (g_page07_curr.gesture.dragging) {
            int desired_scroll = g_page07_curr.gesture.start_scroll - dx;
            int max_scroll = curr_get_max_scroll();
            int current_scroll = curr_scroll_x_abs();

            if (desired_scroll < 0) {
                curr_scroll_to_raw(0, false, false);
                curr_apply_overscroll_visual((-desired_scroll) / 3);
            } else if (desired_scroll > max_scroll) {
                curr_scroll_to_raw(max_scroll, false, false);
                curr_apply_overscroll_visual(-(desired_scroll - max_scroll) / 3);
            } else if (curr_abs_i32(desired_scroll - current_scroll) >=
                       CURR_DRAG_APPLY_MIN_PX) {
                curr_scroll_to_raw(desired_scroll, false, false);
            }
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (!g_page07_curr.gesture.active) return;
        g_page07_curr.gesture.active = false;

        if (g_page07_curr.gesture.dragging) {
            g_page07_curr.gesture.dragging = false;

            if (lv_obj_get_x(g_page07_curr.objects.list) != 0) {
                curr_animate_overscroll_back();
                curr_start_snap_timer(CURR_OVERSCROLL_SNAP_MS);
                g_page07_curr.gesture.last_drag_tick = lv_tick_get();
                return;
            }

            int release_target_scroll;

            release_target_scroll = curr_calc_release_scroll_target();
            curr_scroll_to_raw(release_target_scroll, true, false);
            g_page07_curr.gesture.snap_target_visible_idx = -1;
            curr_start_snap_timer(CURR_SNAP_IDLE_MS);
            g_page07_curr.gesture.last_drag_tick = lv_tick_get();
            return;
        }

        curr_animate_overscroll_back();

        int vis_idx = curr_pick_nearest_visible_idx();
        g_page07_curr.model.selected_visible_idx = vis_idx;
        g_page07_curr.model.selected_abs_idx = g_page07_curr.model.visible_indices[vis_idx];
        curr_apply_selected_style();
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx, true, true);
    }
}

static void curr_card_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) return;

    if (lv_tick_elaps(g_page07_curr.gesture.last_drag_tick) < 220) {
        return;
    }

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;
    curr_select_and_exit_abs(g_page07_curr.model.visible_indices[vis_idx]);
}

static void curr_fav_icon_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    page07_curr_model_toggle_favorite(abs_idx);

    if (g_page07_curr.model.favorite_only && !page07_curr_model_is_favorite(abs_idx)) {
        curr_refresh_right_views();
        return;
    }

    curr_apply_selected_style();
}

static void curr_grid_fav_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_GRID) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    page07_curr_model_toggle_favorite(abs_idx);

    if (g_page07_curr.model.favorite_only && !page07_curr_model_is_favorite(abs_idx)) {
        curr_refresh_right_views();
        return;
    }

    curr_update_grid_fav_ui(vis_idx);
}

static void curr_grid_item_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_GRID) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    g_page07_curr.model.selected_abs_idx = abs_idx;
    g_page07_curr.model.selected_visible_idx = vis_idx;
    curr_set_left_info_by_abs(abs_idx);
    curr_select_and_exit_abs(abs_idx);
}

static void curr_build_card_layer(void)
{
    g_page07_curr.objects.card_layer = lv_obj_create(g_page07_curr.objects.right_area);
    /* Build the complete layer while hidden. Child creation and style setup
     * then cannot enqueue hundreds of intermediate invalid areas. Showing the
     * finished layer produces one coherent first frame. */
    lv_obj_add_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(g_page07_curr.objects.card_layer);
    lv_obj_set_size(g_page07_curr.objects.card_layer, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.card_layer, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.card_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.card_layer, LV_SCROLLBAR_MODE_OFF);

    g_page07_curr.objects.list = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.list);
    lv_obj_set_size(g_page07_curr.objects.list,
                    CURR_VIEW_W, CURR_CARD_SCROLL_H);
    lv_obj_set_pos(g_page07_curr.objects.list, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.list, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_page07_curr.objects.list, LV_OBJ_FLAG_SCROLLABLE);
    perf_profile_watch_invalidation(g_page07_curr.objects.list,
                                    "CURRENCY_CARD_SCROLL");

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.model.visible_indices[i];
        int x = CURR_CARD_FIRST_X + i * CURR_CARD_STRIDE;
        char curr_code[4];

        if (!currency_state_get_code((uint8_t)abs_idx, curr_code)) continue;

        g_page07_curr.cards[i].abs_idx = abs_idx;
        g_page07_curr.cards[i].base_x = x;
        g_page07_curr.cards[i].base_y = CURR_CARD_Y;

        g_page07_curr.cards[i].render_root =
            lv_obj_create(g_page07_curr.objects.list);
        lv_obj_remove_style_all(g_page07_curr.cards[i].render_root);
        lv_obj_set_style_bg_opa(g_page07_curr.cards[i].render_root,
                                LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g_page07_curr.cards[i].render_root,
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].render_root,
                                  LV_SCROLLBAR_MODE_OFF);

        g_page07_curr.cards[i].background =
            lv_img_create(g_page07_curr.cards[i].render_root);
        lv_img_set_src(g_page07_curr.cards[i].background,
                       CURR_CARD_UNSELECTED_BG_PATH);
        curr_set_path_target_width(g_page07_curr.cards[i].background,
                                   CURR_CARD_UNSELECTED_BG_PATH,
                                   CURR_CARD_W);
        lv_obj_set_pos(g_page07_curr.cards[i].background, 0, 0);
        lv_obj_clear_flag(g_page07_curr.cards[i].background,
                          LV_OBJ_FLAG_CLICKABLE);

        g_page07_curr.cards[i].visual_card =
            lv_obj_create(g_page07_curr.cards[i].render_root);
        lv_obj_remove_style_all(g_page07_curr.cards[i].visual_card);
        lv_obj_set_style_bg_opa(g_page07_curr.cards[i].visual_card,
                                LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g_page07_curr.cards[i].visual_card,
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].visual_card,
                                  LV_SCROLLBAR_MODE_OFF);

        g_page07_curr.cards[i].img =
            lv_img_create(g_page07_curr.cards[i].visual_card);
        lv_img_set_src(g_page07_curr.cards[i].img, get_currency_img(curr_code));
        curr_set_img_target_width(g_page07_curr.cards[i].img, curr_code, CURR_FLAG_TARGET_W);
        lv_obj_set_pos(g_page07_curr.cards[i].img, -35, CURR_FLAG_Y_IN_CARD);

        g_page07_curr.cards[i].name =
            lv_label_create(g_page07_curr.cards[i].visual_card);
        lv_label_set_text(g_page07_curr.cards[i].name,
                          currency_state_display_code(curr_code));
        lv_obj_set_pos(g_page07_curr.cards[i].name, 21, 174);
        lv_obj_set_style_text_font(g_page07_curr.cards[i].name, &lv_font_instrument_sans_medium_30, 0);

        g_page07_curr.cards[i].no =
            lv_label_create(g_page07_curr.cards[i].visual_card);
        lv_label_set_text_fmt(g_page07_curr.cards[i].no, "NO.%02d", abs_idx + 1);
        lv_obj_set_pos(g_page07_curr.cards[i].no, 21, 224);
        lv_obj_set_style_text_font(g_page07_curr.cards[i].no, &lv_font_instrument_sans_medium_14, 0);

        /* Page construction must stay cheap. Reuse an existing DMA image if
         * available; missing images are captured later by the idle prewarmer
         * while the Currency page is hidden. */
        curr_attach_cached_card_snapshots(i);

        g_page07_curr.cards[i].composite =
            lv_img_create(g_page07_curr.objects.list);
        lv_obj_clear_flag(g_page07_curr.cards[i].composite,
                          LV_OBJ_FLAG_CLICKABLE);

        g_page07_curr.cards[i].card = lv_obj_create(g_page07_curr.objects.list);
        lv_obj_set_size(g_page07_curr.cards[i].card, CURR_CARD_W,
                        CURR_CARD_H);
        lv_obj_set_pos(g_page07_curr.cards[i].card, x, CURR_CARD_Y);
        lv_obj_set_style_radius(g_page07_curr.cards[i].card, 0, 0);
        lv_obj_set_style_bg_opa(g_page07_curr.cards[i].card,
                                LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_page07_curr.cards[i].card, 0, 0);
        lv_obj_set_style_shadow_width(g_page07_curr.cards[i].card, 0, 0);
        lv_obj_set_style_shadow_opa(g_page07_curr.cards[i].card, LV_OPA_0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].card,
                                  LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.cards[i].card,
                          LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(g_page07_curr.cards[i].card,
                        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_port_indev_set_drag_obj(g_page07_curr.cards[i].card, true);
        lv_obj_add_event_cb(g_page07_curr.cards[i].card, curr_card_click_cb,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(g_page07_curr.cards[i].card, curr_right_drag_cb,
                            LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].card, curr_right_drag_cb,
                            LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].card, curr_right_drag_cb,
                            LV_EVENT_RELEASED, NULL);

        g_page07_curr.cards[i].fav_btn =
            lv_obj_create(g_page07_curr.cards[i].card);
        lv_obj_set_size(g_page07_curr.cards[i].fav_btn, CURR_FAV_BTN_IN_CARD_W, CURR_FAV_BTN_IN_CARD_H);
        lv_obj_set_pos(g_page07_curr.cards[i].fav_btn, CURR_FAV_BTN_IN_CARD_X, CURR_FAV_BTN_IN_CARD_Y);
        lv_obj_set_style_radius(g_page07_curr.cards[i].fav_btn, 14, 0);
        lv_obj_set_style_border_width(g_page07_curr.cards[i].fav_btn, 0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].fav_btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.cards[i].fav_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(g_page07_curr.cards[i].fav_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_icon_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

        g_page07_curr.cards[i].fav_icon = lv_img_create(g_page07_curr.cards[i].fav_btn);
        lv_img_set_src(g_page07_curr.cards[i].fav_icon, "L:/usr/local/share/lvgl_data/unfav.png");
        lv_obj_center(g_page07_curr.cards[i].fav_icon);

        curr_apply_card_visual(i, false, x, CURR_CARD_Y);
    }

    lv_obj_t* tail = lv_obj_create(g_page07_curr.objects.list);
    lv_obj_remove_style_all(tail);
    lv_obj_set_size(tail, 1, 1);
    lv_obj_set_pos(tail, CURR_CARD_FIRST_X + g_page07_curr.model.visible_count * CURR_CARD_STRIDE + CURR_CARD_PAD_RIGHT, 1);

    g_page07_curr.objects.track = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.track);
    lv_obj_set_size(g_page07_curr.objects.track, CURR_VIEW_W, CURR_TRACK_H);
    lv_obj_set_pos(g_page07_curr.objects.track, 0, CURR_TRACK_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.track, lv_color_hex(CURR_TRACK_BG), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.track, LV_OPA_TRANSP, 0);

    g_page07_curr.objects.thumb = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.thumb);
    lv_obj_set_size(g_page07_curr.objects.thumb, 1, CURR_TRACK_H);
    lv_obj_set_pos(g_page07_curr.objects.thumb, 0, CURR_TRACK_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.thumb, lv_color_hex(CURR_TRACK_FG), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.thumb, LV_OPA_90, 0);
    lv_obj_set_style_radius(g_page07_curr.objects.thumb, 3, 0);
    g_curr_track_x = -1;
    g_curr_track_w = -1;
    g_curr_card_styled_visible_idx = -1;

}

static void curr_build_grid_layer(void)
{
    g_page07_curr.objects.grid_layer = lv_obj_create(g_page07_curr.objects.right_area);
    /* Keep the incomplete grid invisible for the same reason as the card
     * layer: only publish it after every child is ready. */
    lv_obj_add_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(g_page07_curr.objects.grid_layer);
    lv_obj_set_size(g_page07_curr.objects.grid_layer, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.grid_layer, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.grid_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.grid_layer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_SCROLLABLE);

    g_page07_curr.objects.grid_scroll = lv_obj_create(g_page07_curr.objects.grid_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.grid_scroll);
    lv_obj_set_size(g_page07_curr.objects.grid_scroll, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.grid_scroll, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.grid_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(g_page07_curr.objects.grid_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.grid_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(g_page07_curr.objects.grid_scroll, LV_OBJ_FLAG_SCROLLABLE);
    perf_profile_watch_invalidation(g_page07_curr.objects.grid_scroll,
                                    "CURRENCY_GRID_SCROLL");

    int rows = (g_page07_curr.model.visible_count + CURR_GRID_COLS - 1) / CURR_GRID_COLS;
    int content_h = CURR_GRID_START_Y + rows * CURR_GRID_ROW_STEP + 10;
    if (content_h < CURR_VIEW_H) content_h = CURR_VIEW_H;

    lv_obj_t* content = lv_obj_create(g_page07_curr.objects.grid_scroll);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, CURR_VIEW_W, content_h);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.model.visible_indices[i];
        int row = i / CURR_GRID_COLS;
        int col = i % CURR_GRID_COLS;
        int x = CURR_GRID_START_X + col * CURR_GRID_CELL_W;
        int y = CURR_GRID_START_Y + row * CURR_GRID_ROW_STEP;
        char curr_code[4];

        if (!currency_state_get_code((uint8_t)abs_idx, curr_code)) continue;

        g_page07_curr.grid_items[i].abs_idx = abs_idx;

        g_page07_curr.grid_items[i].item = lv_obj_create(content);
        lv_obj_remove_style_all(g_page07_curr.grid_items[i].item);
        lv_obj_set_size(g_page07_curr.grid_items[i].item, CURR_GRID_ITEM_W, CURR_GRID_CELL_H);
        lv_obj_set_pos(g_page07_curr.grid_items[i].item, x, y);
        lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].item, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g_page07_curr.grid_items[i].item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(g_page07_curr.grid_items[i].item, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(g_page07_curr.grid_items[i].item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].item, curr_grid_item_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        g_page07_curr.grid_items[i].img = lv_img_create(g_page07_curr.grid_items[i].item);
        lv_img_set_src(g_page07_curr.grid_items[i].img, get_currency_img(curr_code));
        curr_set_img_target_width(g_page07_curr.grid_items[i].img, curr_code, CURR_FLAG_TARGET_W);
        lv_obj_align(g_page07_curr.grid_items[i].img, LV_ALIGN_TOP_MID, CURR_GRID_GROUP_OFS_X, CURR_GRID_FLAG_Y);

        g_page07_curr.grid_items[i].selected_mark = lv_img_create(g_page07_curr.grid_items[i].item);
        lv_img_set_src(g_page07_curr.grid_items[i].selected_mark, CURR_GRID_SELECTED_MARK_PATH);
        lv_obj_set_size(g_page07_curr.grid_items[i].selected_mark, 24, 24);
        lv_obj_align_to(g_page07_curr.grid_items[i].selected_mark,
                        g_page07_curr.grid_items[i].img,
                        LV_ALIGN_CENTER, 0, 0);
        if (abs_idx != g_page07_curr.model.selected_abs_idx) {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark, LV_OBJ_FLAG_HIDDEN);
        }

        g_page07_curr.grid_items[i].fav_btn = lv_obj_create(g_page07_curr.grid_items[i].item);
        lv_obj_set_size(g_page07_curr.grid_items[i].fav_btn, CURR_FAV_BTN_IN_CARD_W - 2, CURR_FAV_BTN_IN_CARD_H - 2);
        lv_obj_set_pos(g_page07_curr.grid_items[i].fav_btn, CURR_GRID_FAV_X, CURR_GRID_FAV_Y);
        lv_obj_set_style_radius(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_shadow_width(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_shadow_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.grid_items[i].fav_btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.grid_items[i].fav_btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(g_page07_curr.grid_items[i].fav_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_grid_fav_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

        g_page07_curr.grid_items[i].fav_icon = lv_img_create(g_page07_curr.grid_items[i].fav_btn);
        lv_img_set_src(g_page07_curr.grid_items[i].fav_icon, "L:/usr/local/share/lvgl_data/unfav.png");
        lv_obj_center(g_page07_curr.grid_items[i].fav_icon);

        g_page07_curr.grid_items[i].name = lv_label_create(g_page07_curr.grid_items[i].item);
        lv_label_set_text(g_page07_curr.grid_items[i].name,
                          currency_state_display_code(curr_code));
        lv_obj_set_width(g_page07_curr.grid_items[i].name, CURR_GRID_ITEM_W);
        lv_obj_set_style_text_align(g_page07_curr.grid_items[i].name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(g_page07_curr.grid_items[i].name, &lv_font_instrument_sans_medium_20, 0);
        lv_obj_set_style_text_color(g_page07_curr.grid_items[i].name,
                                    (abs_idx == g_page07_curr.model.selected_abs_idx) ? lv_color_hex(CURR_TEXT_SEL) : lv_color_hex(0x7E7E7E), 0);
        lv_obj_align(g_page07_curr.grid_items[i].name, LV_ALIGN_BOTTOM_MID, CURR_GRID_GROUP_OFS_X, -CURR_GRID_TEXT_BOTTOM);

        if (abs_idx == g_page07_curr.model.selected_abs_idx) {
            curr_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            curr_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        curr_update_grid_fav_ui(i);
    }

    g_page07_curr.objects.empty_label = NULL;
    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

static void curr_apply_grid_selected_style(void)
{
    if (g_page07_curr.objects.grid_layer == NULL ||
        g_curr_grid_styled_abs_idx ==
            g_page07_curr.model.selected_abs_idx) {
        return;
    }

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.grid_items[i].abs_idx;
        bool selected = abs_idx == g_page07_curr.model.selected_abs_idx;

        if (selected) {
            lv_obj_clear_flag(g_page07_curr.grid_items[i].selected_mark,
                              LV_OBJ_FLAG_HIDDEN);
            curr_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark,
                            LV_OBJ_FLAG_HIDDEN);
            curr_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        lv_obj_set_style_text_color(
            g_page07_curr.grid_items[i].name,
            selected ? lv_color_hex(CURR_TEXT_SEL)
                     : lv_color_hex(0x7E7E7E), 0);
    }

    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

static void curr_set_mode_visible(void)
{
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        if (g_page07_curr.objects.grid_layer) {
            lv_obj_add_flag(g_page07_curr.objects.grid_layer,
                            LV_OBJ_FLAG_HIDDEN);
        }
        if (g_page07_curr.objects.card_layer) {
            lv_obj_clear_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_HIDDEN);
            curr_apply_selected_style();
            curr_update_track_by_scroll(curr_scroll_x_abs());
        }
    } else {
        if (g_page07_curr.objects.card_layer) {
            lv_obj_add_flag(g_page07_curr.objects.card_layer,
                            LV_OBJ_FLAG_HIDDEN);
        }
        if (g_page07_curr.objects.grid_layer) {
            lv_obj_clear_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_HIDDEN);
            curr_apply_grid_selected_style();
            for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
                curr_update_grid_fav_ui(i);
            }
        }
    }
}

static void curr_switch_cached_view(void)
{
    if (g_page07_curr.model.visible_count <= 0) {
        curr_refresh_right_views();
        return;
    }

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        if (g_page07_curr.objects.card_layer == NULL ||
            !lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
            memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
            curr_build_card_layer();
        }
    } else if (g_page07_curr.objects.grid_layer == NULL ||
               !lv_obj_is_valid(g_page07_curr.objects.grid_layer)) {
        memset(g_page07_curr.grid_items, 0,
               sizeof(g_page07_curr.grid_items));
        curr_build_grid_layer();
    }

    curr_set_mode_visible();
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_scroll_to_visible_idx(
            g_page07_curr.model.selected_visible_idx, false, true);
        curr_update_track_by_scroll(curr_scroll_x_abs());
    }
}

static void curr_refresh_right_views(void)
{
    char curr_code[4];

    if (g_page07_curr.objects.right_area == NULL) return;

    if (g_page07_curr.objects.empty_label && lv_obj_is_valid(g_page07_curr.objects.empty_label)) {
        lv_obj_del(g_page07_curr.objects.empty_label);
        g_page07_curr.objects.empty_label = NULL;
    }

    page07_curr_model_refresh_visible();

    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);

    if (g_page07_curr.objects.card_layer && lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
        perf_profile_unwatch_invalidation(g_page07_curr.objects.list);
        lv_obj_del(g_page07_curr.objects.card_layer);
        curr_release_card_snapshots();
        g_page07_curr.objects.card_layer = NULL;
        g_page07_curr.objects.list = NULL;
        g_page07_curr.objects.track = NULL;
        g_page07_curr.objects.thumb = NULL;
    }

    if (g_page07_curr.objects.grid_layer && lv_obj_is_valid(g_page07_curr.objects.grid_layer)) {
        perf_profile_unwatch_invalidation(g_page07_curr.objects.grid_scroll);
        lv_obj_del(g_page07_curr.objects.grid_layer);
        g_page07_curr.objects.grid_layer = NULL;
        g_page07_curr.objects.grid_scroll = NULL;
        g_page07_curr.objects.empty_label = NULL;
    }

    memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
    memset(g_page07_curr.grid_items, 0, sizeof(g_page07_curr.grid_items));

    if (g_page07_curr.model.visible_count <= 0) {
        g_page07_curr.objects.empty_label = lv_label_create(g_page07_curr.objects.right_area);
        lv_label_set_text(g_page07_curr.objects.empty_label, g_page07_curr.model.favorite_only ? "NO FAVORITE CURRENCY" : "NO CURRENCY");
        lv_obj_set_style_text_color(g_page07_curr.objects.empty_label, lv_color_hex(0xB3B3B3), 0);
        lv_obj_set_style_text_font(g_page07_curr.objects.empty_label, &lv_font_instrument_sans_medium_20, 0);
        lv_obj_center(g_page07_curr.objects.empty_label);
        return;
    }

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_build_card_layer();
    } else {
        curr_build_grid_layer();
    }
    curr_set_mode_visible();

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx, false, true);
        curr_apply_selected_style();
        curr_update_track_by_scroll(curr_scroll_x_abs());
    }
}

static void curr_view_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_page07_curr.model.view_mode = (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) ? PAGE07_CURR_VIEW_GRID : PAGE07_CURR_VIEW_CARD;
    curr_switch_cached_view();
    curr_refresh_left_buttons();
    page07_curr_model_save();

}

static void curr_fav_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_page07_curr.model.favorite_only = !g_page07_curr.model.favorite_only;
    page07_curr_model_save();
    curr_refresh_left_buttons();
    curr_refresh_right_views();
}

void page_07_curr_img_reset(void)
{
    if (g_page07_curr.gesture.snap_timer) {
        lv_timer_del(g_page07_curr.gesture.snap_timer);
        g_page07_curr.gesture.snap_timer = NULL;
    }

    perf_profile_unwatch_invalidation(g_page07_curr.objects.list);
    perf_profile_unwatch_invalidation(g_page07_curr.objects.grid_scroll);

    if (g_page07_curr.objects.root && lv_obj_is_valid(g_page07_curr.objects.root)) {
        lv_obj_del(g_page07_curr.objects.root);
    }
    curr_release_card_snapshots();

    g_page07_curr.objects.root = NULL;
    g_page07_curr.objects.left_panel = NULL;
    g_page07_curr.objects.left_img = NULL;
    g_page07_curr.objects.left_code = NULL;
    g_page07_curr.objects.left_code_decor = NULL;
    g_page07_curr.objects.left_no = NULL;

    g_page07_curr.objects.btn_view = NULL;
    g_page07_curr.objects.btn_view_label = NULL;
    g_page07_curr.objects.btn_favorite = NULL;
    g_page07_curr.objects.btn_favorite_label = NULL;
    g_page07_curr.objects.btn_back = NULL;
    g_page07_curr.objects.btn_back_label = NULL;

    g_page07_curr.objects.right_area = NULL;
    g_page07_curr.objects.card_layer = NULL;
    g_page07_curr.objects.grid_layer = NULL;

    g_page07_curr.objects.list = NULL;
    g_page07_curr.objects.track = NULL;
    g_page07_curr.objects.thumb = NULL;

    g_page07_curr.objects.grid_scroll = NULL;
    g_page07_curr.objects.empty_label = NULL;

    g_page07_curr.gesture.active = false;
    g_page07_curr.gesture.dragging = false;
    g_page07_curr.gesture.start_scroll = 0;
    g_page07_curr.gesture.last_dx = 0;
    g_page07_curr.gesture.last_drag_tick = 0;

    memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
    memset(g_page07_curr.grid_items, 0, sizeof(g_page07_curr.grid_items));
    memset(g_page07_curr.model.visible_indices, 0, sizeof(g_page07_curr.model.visible_indices));
    g_page07_curr.model.visible_count = 0;
    g_curr_track_x = -1;
    g_curr_track_w = -1;
    g_curr_card_styled_visible_idx = -1;
    g_curr_grid_styled_abs_idx = -1;
}

void page_07_curr_img_refre(void)
{
    if (curr_page == NULL || currency_state_count() <= 0) return;

    page07_curr_model_load();

    page_07_curr_img_reset();

    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);

    g_page07_curr.objects.root = lv_obj_create(curr_page);
    lv_obj_remove_style_all(g_page07_curr.objects.root);
    lv_obj_set_size(g_page07_curr.objects.root, 1280, 400);
    lv_obj_set_pos(g_page07_curr.objects.root, 0, 0);
    lv_obj_clear_flag(g_page07_curr.objects.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.root, LV_SCROLLBAR_MODE_OFF);

    g_page07_curr.objects.left_panel = lv_obj_create(g_page07_curr.objects.root);
    lv_obj_remove_style_all(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.left_panel, CURR_SEL_W, CURR_SEL_H);
    lv_obj_set_pos(g_page07_curr.objects.left_panel, 0, 0);
    lv_obj_set_style_bg_color(g_page07_curr.objects.left_panel, lv_color_hex(CURR_LEFT_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.left_panel, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.left_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.left_panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* left_title = lv_label_create(g_page07_curr.objects.left_panel);
    lv_label_set_text(left_title, "CURRENCY");
    lv_obj_set_pos(left_title, 105, 14);
    lv_obj_set_style_text_font(left_title, &lv_font_instrument_sans_semibold_24, 0);
    lv_obj_set_style_text_color(left_title, lv_color_hex(0x707070), 0);

    g_page07_curr.objects.left_img = lv_img_create(g_page07_curr.objects.left_panel);
    lv_img_set_zoom(g_page07_curr.objects.left_img, 170);
    lv_obj_align(g_page07_curr.objects.left_img, LV_ALIGN_TOP_MID, CURR_LEFT_IMG_ALIGN_X, CURR_LEFT_IMG_ALIGN_Y);

    g_page07_curr.objects.left_code_decor = lv_label_create(g_page07_curr.objects.left_panel);
    lv_obj_set_pos(g_page07_curr.objects.left_code_decor, CURR_LEFT_CODE_DECOR_X, CURR_LEFT_CODE_DECOR_Y);
    lv_obj_set_style_text_font(g_page07_curr.objects.left_code_decor, &lv_font_instrument_sans_medium_48, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.left_code_decor, lv_color_hex(0xEBEBEB), 0);

    g_page07_curr.objects.left_code = lv_label_create(g_page07_curr.objects.left_panel);
    lv_obj_set_pos(g_page07_curr.objects.left_code, CURR_LEFT_CODE_X, CURR_LEFT_CODE_Y);
    lv_obj_set_style_text_font(g_page07_curr.objects.left_code, &lv_font_instrument_sans_medium_30, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.left_code, lv_color_hex(0x202020), 0);

    g_page07_curr.objects.left_no = lv_label_create(g_page07_curr.objects.left_panel);
    lv_obj_set_pos(g_page07_curr.objects.left_no, CURR_LEFT_NO_X, CURR_LEFT_NO_Y);
    lv_obj_set_style_text_font(g_page07_curr.objects.left_no, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.left_no, lv_color_hex(0x202020), 0);

    g_page07_curr.objects.btn_view = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_view, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_view, CURR_VIEW_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_view, curr_view_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_view_label = lv_label_create(g_page07_curr.objects.btn_view);
    lv_label_set_text(g_page07_curr.objects.btn_view_label, "CARD");
    lv_obj_center(g_page07_curr.objects.btn_view_label);

    g_page07_curr.objects.btn_favorite = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_favorite, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_favorite, CURR_FAV_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_favorite, curr_fav_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_favorite_label = lv_label_create(g_page07_curr.objects.btn_favorite);
    lv_label_set_text(g_page07_curr.objects.btn_favorite_label, "FAV");
    lv_obj_center(g_page07_curr.objects.btn_favorite_label);

    g_page07_curr.objects.btn_back = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_back, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_back, CURR_BACK_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_back, curr_back_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_back_label = lv_label_create(g_page07_curr.objects.btn_back);
    lv_label_set_text(g_page07_curr.objects.btn_back_label, "BACK");
    lv_obj_center(g_page07_curr.objects.btn_back_label);

    g_page07_curr.objects.right_area = lv_obj_create(g_page07_curr.objects.root);
    lv_obj_remove_style_all(g_page07_curr.objects.right_area);
    lv_obj_set_size(g_page07_curr.objects.right_area, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.right_area, CURR_VIEW_X, CURR_VIEW_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.right_area, lv_color_hex(CURR_RIGHT_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.right_area, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.right_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.right_area, LV_SCROLLBAR_MODE_OFF);

    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    curr_refresh_left_buttons();
    curr_refresh_right_views();
    currency_state_get_snapshot(&g_curr_page_snapshot);
    currency_state_get_selected_code(g_curr_page_selected_code);
    g_curr_page_snapshot_valid = true;
}

static bool curr_cached_list_matches(const currency_state_snapshot_t* snapshot)
{
    if (!g_curr_page_snapshot_valid || snapshot == NULL ||
        snapshot->count != g_curr_page_snapshot.count) {
        return false;
    }
    return memcmp(snapshot->codes, g_curr_page_snapshot.codes,
                  sizeof(snapshot->codes)) == 0;
}

static void curr_snapshot_prewarm_timer_cb(lv_timer_t *timer)
{
    currency_state_snapshot_t snapshot;

    if (timer == NULL || curr_page == NULL ||
        !lv_obj_is_valid(curr_page) ||
        !lv_obj_has_flag(curr_page, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    /* Snapshot capture is intentionally confined to a quiet UI window.  Do
     * not gate it on lv_anim_count_running(): cached/hidden pages and shared
     * loaders own infinite animations, so that global count may never reach
     * zero even when the operator-visible page is idle.  Currency is hidden
     * here and its gesture timer/flags are stopped by suspend; input idle is
     * therefore the relevant foreground-safety condition. */
    if (lv_disp_get_inactive_time(NULL) < 1200U ||
        g_page07_curr.gesture.active ||
        g_page07_curr.gesture.dragging ||
        g_page07_curr.gesture.snap_timer != NULL) {
        return;
    }
    if (currency_state_count() <= 0) return;

    currency_state_get_snapshot(&snapshot);
    if (!curr_cached_list_matches(&snapshot)) {
        page_07_curr_img_refre();
        lv_timer_set_period(timer, 120);
        return;
    }

    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD ||
        g_page07_curr.objects.card_layer == NULL ||
        !lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
        lv_timer_set_period(timer, 500);
        return;
    }

    if (curr_build_one_missing_card_snapshot()) {
        lv_timer_set_period(timer, 120);
    } else {
        lv_timer_set_period(timer, 500);
    }
}

static bool curr_cached_selection_matches(
    const currency_state_snapshot_t* snapshot,
    const char selected_code[4])
{
    if (!g_curr_page_snapshot_valid || snapshot == NULL ||
        selected_code == NULL) {
        return false;
    }

    return strncmp(snapshot->active_code,
                   g_curr_page_snapshot.active_code, 3) == 0 &&
           snapshot->active_currency == g_curr_page_snapshot.active_currency &&
           snapshot->active_index == g_curr_page_snapshot.active_index &&
           strncmp(selected_code, g_curr_page_selected_code, 3) == 0;
}

static void curr_refresh_cached_selection(void)
{
    char curr_code[4];

    page07_curr_model_load();
    page07_curr_model_refresh_visible();
    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx =
        page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    curr_refresh_left_buttons();

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_apply_selected_style();
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx,
                                   false, true);
        return;
    }

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.grid_items[i].abs_idx;
        bool selected = abs_idx == g_page07_curr.model.selected_abs_idx;

        if (selected) {
            lv_obj_clear_flag(g_page07_curr.grid_items[i].selected_mark,
                              LV_OBJ_FLAG_HIDDEN);
            curr_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark,
                            LV_OBJ_FLAG_HIDDEN);
            curr_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        lv_obj_set_style_text_color(g_page07_curr.grid_items[i].name,
                                    selected ? lv_color_hex(CURR_TEXT_SEL)
                                             : lv_color_hex(0x7E7E7E), 0);
        curr_update_grid_fav_ui(i);
    }
    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

bool ui_page_07_curr_resume(void)
{
    currency_state_snapshot_t snapshot;
    char selected_code[4];
    bool profile_enabled;
    uint64_t profile_started_us = 0;
    const char* profile_event = "RESUME_CLEAN";

    if (curr_page == NULL || !lv_obj_is_valid(curr_page)) {
        return false;
    }

    currency_state_get_snapshot(&snapshot);
    currency_state_get_selected_code(selected_code);
    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
    }
    if (!curr_cached_list_matches(&snapshot)) {
        profile_event = "RESUME_LIST";
        page_07_curr_img_refre();
    } else if (!curr_cached_selection_matches(&snapshot, selected_code)) {
        profile_event = "RESUME_SELECTION";
        curr_refresh_cached_selection();
        g_curr_page_snapshot = snapshot;
        memcpy(g_curr_page_selected_code, selected_code,
               sizeof(g_curr_page_selected_code));
    }
    lv_obj_clear_flag(curr_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(curr_page);
    if (profile_enabled) {
        perf_profile_report_event_us(
            "CURRENCY", profile_event,
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_07_curr_suspend(void)
{
    if (curr_page == NULL || !lv_obj_is_valid(curr_page)) {
        return;
    }

    if (g_page07_curr.gesture.snap_timer) {
        lv_timer_del(g_page07_curr.gesture.snap_timer);
        g_page07_curr.gesture.snap_timer = NULL;
    }
    g_page07_curr.gesture.active = false;
    g_page07_curr.gesture.dragging = false;
    /*
     * g_curr_page_snapshot describes what has actually been rendered, not
     * merely the latest model state.  A successful currency command can
     * switch to MAIN before this cached page has moved its card viewport.
     * Capturing the model here would make resume incorrectly treat the stale
     * viewport as current and skip RESUME_SELECTION.
    */
    lv_obj_add_flag(curr_page, LV_OBJ_FLAG_HIDDEN);
    if (g_curr_snapshot_prewarm_timer != NULL) {
        lv_timer_set_period(g_curr_snapshot_prewarm_timer, 120);
        lv_timer_ready(g_curr_snapshot_prewarm_timer);
    }
}
