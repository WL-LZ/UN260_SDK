#include "lv_selftest_list.h"

#include <stdio.h>
#include <string.h>

#define SELFTEST_LIST_MAX_CTX              8
#define SELFTEST_LIST_NAME_LEN             48
#define SELFTEST_LIST_DEFAULT_ITEM_W       548
#define SELFTEST_LIST_DEFAULT_ITEM_H        26
#define SELFTEST_LIST_DEFAULT_ITEM_GAP       7
#define SELFTEST_LIST_DEFAULT_ITEM_PAD_X     7
#define SELFTEST_LIST_DEFAULT_ICON_SIZE     15
#define SELFTEST_LIST_DEFAULT_SPINNER_SIZE   15
#define SELFTEST_LIST_DEFAULT_NAME_GAP       7
#define SELFTEST_LIST_DEFAULT_STATE_W       74
#define SELFTEST_LIST_DEFAULT_SPINNER_TIME  900

#define SELFTEST_LIST_SUCCESS_ICON_PATH      "L:/usr/local/share/lvgl_data/success_icon.png"
#define SELFTEST_LIST_SUCCESS_ICON_FALLBACK  "L:/usr/local/share/lvgl_data/page_03_ok_icon.png"
#define SELFTEST_LIST_ERROR_ICON_PATH        "L:/usr/local/share/lvgl_data/err_icon.png"
#define SELFTEST_LIST_ERROR_ICON_FALLBACK    "L:/usr/local/share/lvgl_data/0a00_err.png"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *left_box;
    lv_obj_t *icon_box;
    lv_obj_t *spinner_arc;
    lv_obj_t *pending_ring;
    lv_obj_t *icon_img;
    lv_obj_t *name_label;
    lv_obj_t *state_label;
    lv_selftest_list_state_t state;
    char name_buf[SELFTEST_LIST_NAME_LEN];
} lv_selftest_list_item_t;

typedef struct {
    lv_obj_t *root;
    lv_selftest_list_config_t cfg;
    lv_selftest_list_item_t *items;
    uint8_t item_count;
} lv_selftest_list_ctx_t;

static lv_selftest_list_ctx_t *g_selftest_list_ctx[SELFTEST_LIST_MAX_CTX];

static void selftest_list_spinner_rotate_cb(void *var, int32_t v); // 旋转加载图标
static void selftest_list_root_delete_cb(lv_event_t *e); // 列表销毁时释放资源
static lv_selftest_list_ctx_t *selftest_list_ctx_find(lv_obj_t *list); // 按根对象查找上下文
static bool selftest_list_ctx_register(lv_selftest_list_ctx_t *ctx); // 注册上下文
static void selftest_list_ctx_unregister(lv_selftest_list_ctx_t *ctx); // 注销上下文
static void selftest_list_item_release(lv_selftest_list_item_t *item); // 释放单项对象
static void selftest_list_rebuild(lv_selftest_list_ctx_t *ctx); // 重建列表
static void selftest_list_apply_item_style(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item); // 刷新单项样式
static void selftest_list_apply_state(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item,
                                      lv_selftest_list_state_t state); // 刷新单项状态显示
static void selftest_list_item_layout_update(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item); // 刷新单项位置
static void selftest_list_set_icon_src(lv_obj_t *img_obj, const char *primary_path,
                                       const char *fallback_path); // 设置图片并按大小自适应
static void selftest_list_spinner_start(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item); // 启动加载动画
static void selftest_list_spinner_stop(lv_selftest_list_item_t *item); // 停止加载动画
static lv_selftest_list_item_t *selftest_list_item_get(lv_selftest_list_ctx_t *ctx, uint8_t index); // 获取指定项
static lv_obj_t *selftest_list_create_item_card(lv_selftest_list_ctx_t *ctx, lv_obj_t *parent,
                                                uint8_t index); // 创建单项卡片

void lv_selftest_list_config_init(lv_selftest_list_config_t *cfg) // 初始化自检列表配置
{
    if (cfg == NULL) {
        return;
    }

    cfg->item_w = SELFTEST_LIST_DEFAULT_ITEM_W;
    cfg->item_h = SELFTEST_LIST_DEFAULT_ITEM_H;
    cfg->item_gap = SELFTEST_LIST_DEFAULT_ITEM_GAP;
    cfg->item_pad_x = SELFTEST_LIST_DEFAULT_ITEM_PAD_X;
    cfg->icon_size = SELFTEST_LIST_DEFAULT_ICON_SIZE;
    cfg->spinner_size = SELFTEST_LIST_DEFAULT_SPINNER_SIZE;
    cfg->name_gap = SELFTEST_LIST_DEFAULT_NAME_GAP;
    cfg->state_w = SELFTEST_LIST_DEFAULT_STATE_W;
    cfg->success_border_color = lv_color_hex(0xE5E7EB);
    cfg->loading_border_color = lv_color_hex(0x93C5FD);
    cfg->pending_border_color = lv_color_hex(0xF3F4F6);
    cfg->error_border_color = lv_color_hex(0xFECACA);
    cfg->success_bg_color = lv_color_hex(0xFFFFFF);
    cfg->loading_bg_color = lv_color_hex(0xEFF6FF);
    cfg->pending_bg_color = lv_color_hex(0xF9FAFB);
    cfg->error_bg_color = lv_color_hex(0xFFF7F7);
    cfg->success_text_color = lv_color_hex(0x374151);
    cfg->loading_text_color = lv_color_hex(0x1E3A8A);
    cfg->pending_text_color = lv_color_hex(0x9CA3AF);
    cfg->error_text_color = lv_color_hex(0x374151);
    cfg->success_state_color = lv_color_hex(0x10B981);
    cfg->loading_state_color = lv_color_hex(0x3B82F6);
    cfg->pending_state_color = lv_color_hex(0x9CA3AF);
    cfg->error_state_color = lv_color_hex(0xEF4444);
    cfg->success_icon_path = SELFTEST_LIST_SUCCESS_ICON_PATH;
    cfg->error_icon_path = SELFTEST_LIST_ERROR_ICON_PATH;
    cfg->spinner_time = SELFTEST_LIST_DEFAULT_SPINNER_TIME;
}

static void selftest_list_spinner_rotate_cb(void *var, int32_t v) // 旋转加载图标
{
    lv_obj_t *arc = (lv_obj_t *)var;

    if (arc == NULL || !lv_obj_is_valid(arc)) {
        return;
    }

    lv_arc_set_rotation(arc, (int16_t)v);
}

static void selftest_list_set_icon_src(lv_obj_t *img_obj, const char *primary_path,
                                       const char *fallback_path) // 设置图片并按大小自适应
{
    lv_img_header_t header;
    lv_coord_t obj_w;
    lv_coord_t obj_h;
    lv_coord_t zoom_w;
    lv_coord_t zoom_h;
    uint16_t zoom;

    if (img_obj == NULL || !lv_obj_is_valid(img_obj)) {
        return;
    }

    if (primary_path != NULL && lv_img_decoder_get_info(primary_path, &header) == LV_RES_OK) {
        lv_img_set_src(img_obj, primary_path);
    } else if (fallback_path != NULL && lv_img_decoder_get_info(fallback_path, &header) == LV_RES_OK) {
        lv_img_set_src(img_obj, fallback_path);
    } else {
        return;
    }

    obj_w = lv_obj_get_width(lv_obj_get_parent(img_obj));
    obj_h = lv_obj_get_height(lv_obj_get_parent(img_obj));
    if (header.w <= 0 || header.h <= 0 || obj_w <= 0 || obj_h <= 0) {
        return;
    }

    zoom_w = (lv_coord_t)((obj_w * 256) / header.w);
    zoom_h = (lv_coord_t)((obj_h * 256) / header.h);
    zoom = (uint16_t)(zoom_w < zoom_h ? zoom_w : zoom_h);
    if (zoom < 1) {
        zoom = 1;
    }
    lv_img_set_zoom(img_obj, zoom);
}

static void selftest_list_spinner_start(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item) // 启动加载动画
{
    lv_anim_t a;

    if (ctx == NULL || item == NULL || item->spinner_arc == NULL || !lv_obj_is_valid(item->spinner_arc)) {
        return;
    }

    lv_anim_del(item->spinner_arc, selftest_list_spinner_rotate_cb);
    lv_anim_init(&a);
    lv_anim_set_var(&a, item->spinner_arc);
    lv_anim_set_exec_cb(&a, selftest_list_spinner_rotate_cb);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_time(&a, ctx->cfg.spinner_time);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

static void selftest_list_spinner_stop(lv_selftest_list_item_t *item) // 停止加载动画
{
    if (item == NULL || item->spinner_arc == NULL || !lv_obj_is_valid(item->spinner_arc)) {
        return;
    }

    lv_anim_del(item->spinner_arc, selftest_list_spinner_rotate_cb);
}

static bool selftest_list_ctx_register(lv_selftest_list_ctx_t *ctx) // 注册上下文
{
    for (uint32_t i = 0; i < SELFTEST_LIST_MAX_CTX; i++) {
        if (g_selftest_list_ctx[i] == NULL) {
            g_selftest_list_ctx[i] = ctx;
            return true;
        }
    }

    return false;
}

static void selftest_list_ctx_unregister(lv_selftest_list_ctx_t *ctx) // 注销上下文
{
    for (uint32_t i = 0; i < SELFTEST_LIST_MAX_CTX; i++) {
        if (g_selftest_list_ctx[i] == ctx) {
            g_selftest_list_ctx[i] = NULL;
            return;
        }
    }
}

static lv_selftest_list_ctx_t *selftest_list_ctx_find(lv_obj_t *list) // 按根对象查找上下文
{
    for (uint32_t i = 0; i < SELFTEST_LIST_MAX_CTX; i++) {
        if (g_selftest_list_ctx[i] != NULL && g_selftest_list_ctx[i]->root == list) {
            return g_selftest_list_ctx[i];
        }
    }

    return NULL;
}

static void selftest_list_item_release(lv_selftest_list_item_t *item) // 释放单项对象
{
    if (item == NULL) {
        return;
    }

    selftest_list_spinner_stop(item);
    memset(item, 0, sizeof(*item));
}

static lv_selftest_list_item_t *selftest_list_item_get(lv_selftest_list_ctx_t *ctx, uint8_t index) // 获取指定项
{
    if (ctx == NULL || ctx->items == NULL || index >= ctx->item_count) {
        return NULL;
    }

    return &ctx->items[index];
}

static void selftest_list_apply_item_style(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item) // 刷新单项样式
{
    if (ctx == NULL || item == NULL || item->card == NULL || !lv_obj_is_valid(item->card)) {
        return;
    }

    lv_obj_set_size(item->card, ctx->cfg.item_w, ctx->cfg.item_h);
    lv_obj_set_style_radius(item->card, 7, 0);
    lv_obj_set_style_border_width(item->card, 1, 0);
    lv_obj_set_style_border_opa(item->card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(item->card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(item->card, ctx->cfg.item_pad_x, 0);
    lv_obj_set_style_pad_right(item->card, ctx->cfg.item_pad_x, 0);
    lv_obj_set_style_pad_top(item->card, 0, 0);
    lv_obj_set_style_pad_bottom(item->card, 0, 0);
    lv_obj_set_style_pad_row(item->card, 0, 0);
    lv_obj_set_style_text_font(item->card, &lv_font_instrument_sans_medium_14, 0);

    lv_obj_set_size(item->icon_box, ctx->cfg.icon_size, ctx->cfg.icon_size);
    lv_obj_set_size(item->spinner_arc, ctx->cfg.spinner_size, ctx->cfg.spinner_size);
    lv_obj_set_size(item->pending_ring, ctx->cfg.icon_size, ctx->cfg.icon_size);
    lv_obj_set_size(item->icon_img, ctx->cfg.icon_size, ctx->cfg.icon_size);
    lv_obj_set_size(item->name_label,
                    ctx->cfg.item_w - ctx->cfg.item_pad_x * 2 - ctx->cfg.icon_size - ctx->cfg.name_gap - ctx->cfg.state_w,
                    ctx->cfg.item_h);
    lv_obj_set_size(item->state_label, ctx->cfg.state_w, ctx->cfg.item_h);

    lv_obj_set_style_pad_top(item->name_label, 0, 0);
    lv_obj_set_style_pad_bottom(item->name_label, 0, 0);
    lv_obj_set_style_pad_left(item->name_label, 0, 0);
    lv_obj_set_style_pad_right(item->name_label, 0, 0);
    lv_obj_set_style_text_align(item->name_label, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_set_style_pad_top(item->state_label, 0, 0);
    lv_obj_set_style_pad_bottom(item->state_label, 0, 0);
    lv_obj_set_style_pad_left(item->state_label, 0, 0);
    lv_obj_set_style_pad_right(item->state_label, 0, 0);
    lv_obj_set_style_text_align(item->state_label, LV_TEXT_ALIGN_CENTER, 0);

    selftest_list_item_layout_update(ctx, item);
}

static void selftest_list_item_layout_update(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item) // 刷新单项位置
{
    lv_coord_t icon_x;
    lv_coord_t icon_y;
    lv_coord_t state_x;
    lv_coord_t name_w;
    lv_coord_t text_y;
    lv_coord_t line_h;

    if (ctx == NULL || item == NULL || item->card == NULL || item->left_box == NULL ||
        item->icon_box == NULL || item->name_label == NULL || item->state_label == NULL) {
        return;
    }

    icon_x = ctx->cfg.item_pad_x;
    icon_y = 0;
    if (ctx->cfg.item_h > ctx->cfg.icon_size) {
        icon_y = (ctx->cfg.item_h - ctx->cfg.icon_size) / 2;
    }
    lv_obj_set_pos(item->icon_box, icon_x, icon_y);
    lv_obj_center(item->spinner_arc);
    lv_obj_center(item->pending_ring);
    lv_obj_center(item->icon_img);

    name_w = ctx->cfg.item_w - ctx->cfg.item_pad_x * 2 - ctx->cfg.icon_size - ctx->cfg.name_gap - ctx->cfg.state_w;
    if (name_w < 0) {
        name_w = 0;
    }
    line_h = lv_font_get_line_height(&lv_font_instrument_sans_medium_14);
    text_y = (ctx->cfg.item_h > line_h) ? (lv_coord_t)((ctx->cfg.item_h - line_h) / 2) : 0;
    lv_obj_set_pos(item->name_label,
                   ctx->cfg.item_pad_x + ctx->cfg.icon_size + ctx->cfg.name_gap,
                   text_y);
    lv_obj_set_width(item->name_label, name_w);

    state_x = ctx->cfg.item_w - ctx->cfg.item_pad_x - ctx->cfg.state_w;
    lv_obj_set_pos(item->state_label, state_x, text_y);
}

static void selftest_list_apply_state(lv_selftest_list_ctx_t *ctx, lv_selftest_list_item_t *item,
                                      lv_selftest_list_state_t state) // 刷新单项状态显示
{
    if (ctx == NULL || item == NULL || item->card == NULL || !lv_obj_is_valid(item->card)) {
        return;
    }

    item->state = state;

    lv_obj_clear_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(item->pending_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(item->icon_img, LV_OBJ_FLAG_HIDDEN);

    switch (state) {
    case LV_SELFTEST_LIST_STATE_SUCCESS:
        lv_obj_set_style_border_color(item->card, ctx->cfg.success_border_color, 0);
        lv_obj_set_style_bg_color(item->card, ctx->cfg.success_bg_color, 0);
        lv_obj_set_style_text_color(item->name_label, ctx->cfg.success_text_color, 0);
        lv_obj_set_style_text_color(item->state_label, ctx->cfg.success_state_color, 0);
        lv_label_set_text(item->state_label, "PASS");
        lv_obj_add_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(item->pending_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(item->icon_img, LV_OBJ_FLAG_HIDDEN);
        selftest_list_set_icon_src(item->icon_img, ctx->cfg.success_icon_path, SELFTEST_LIST_SUCCESS_ICON_FALLBACK);
        break;
    case LV_SELFTEST_LIST_STATE_LOADING:
        lv_obj_set_style_border_color(item->card, ctx->cfg.loading_border_color, 0);
        lv_obj_set_style_bg_color(item->card, ctx->cfg.loading_bg_color, 0);
        lv_obj_set_style_text_color(item->name_label, ctx->cfg.loading_text_color, 0);
        lv_obj_set_style_text_color(item->state_label, ctx->cfg.loading_state_color, 0);
        lv_label_set_text(item->state_label, "Loading...");
        lv_obj_add_flag(item->pending_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(item->icon_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);
        selftest_list_spinner_start(ctx, item);
        break;
    case LV_SELFTEST_LIST_STATE_PENDING:
        lv_obj_set_style_border_color(item->card, ctx->cfg.pending_border_color, 0);
        lv_obj_set_style_bg_color(item->card, ctx->cfg.pending_bg_color, 0);
        lv_obj_set_style_text_color(item->name_label, ctx->cfg.pending_text_color, 0);
        lv_obj_set_style_text_color(item->state_label, ctx->cfg.pending_state_color, 0);
        lv_label_set_text(item->state_label, "---");
        lv_obj_add_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(item->icon_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(item->pending_ring, LV_OBJ_FLAG_HIDDEN);
        break;
    case LV_SELFTEST_LIST_STATE_ERROR:
    default:
        lv_obj_set_style_border_color(item->card, ctx->cfg.error_border_color, 0);
        lv_obj_set_style_bg_color(item->card, ctx->cfg.error_bg_color, 0);
        lv_obj_set_style_text_color(item->name_label, ctx->cfg.error_text_color, 0);
        lv_obj_set_style_text_color(item->state_label, ctx->cfg.error_state_color, 0);
        lv_label_set_text(item->state_label, "FAIL");
        lv_obj_add_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(item->pending_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(item->icon_img, LV_OBJ_FLAG_HIDDEN);
        selftest_list_set_icon_src(item->icon_img, ctx->cfg.error_icon_path, SELFTEST_LIST_ERROR_ICON_FALLBACK);
        break;
    }
}

static lv_obj_t *selftest_list_create_item_card(lv_selftest_list_ctx_t *ctx, lv_obj_t *parent,
                                                uint8_t index) // 创建单项卡片
{
    lv_selftest_list_item_t *item;
    lv_obj_t *card;

    if (ctx == NULL || parent == NULL || ctx->items == NULL || index >= ctx->item_count) {
        return NULL;
    }

    item = &ctx->items[index];
    card = lv_obj_create(parent);
    item->card = card;
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 7, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, ctx->cfg.pending_border_color, 0);
    lv_obj_set_style_bg_color(card, ctx->cfg.pending_bg_color, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(card, ctx->cfg.item_pad_x, 0);
    lv_obj_set_style_pad_right(card, ctx->cfg.item_pad_x, 0);
    lv_obj_set_style_pad_top(card, 0, 0);
    lv_obj_set_style_pad_bottom(card, 0, 0);
    lv_obj_set_style_text_font(card, &lv_font_instrument_sans_medium_14, 0);

    item->left_box = lv_obj_create(card);
    lv_obj_remove_style_all(item->left_box);
    lv_obj_clear_flag(item->left_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item->left_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(item->left_box, 1, 1);

    item->icon_box = lv_obj_create(card);
    lv_obj_remove_style_all(item->icon_box);
    lv_obj_clear_flag(item->icon_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(item->icon_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(item->icon_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(item->icon_box, 0, 0);
    lv_obj_set_style_radius(item->icon_box, LV_RADIUS_CIRCLE, 0);

    item->spinner_arc = lv_arc_create(item->icon_box);
    lv_obj_remove_style(item->spinner_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(item->spinner_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_range(item->spinner_arc, 0, 100);
    lv_arc_set_bg_angles(item->spinner_arc, 0, 360);
    lv_arc_set_rotation(item->spinner_arc, 270);
    lv_arc_set_value(item->spinner_arc, 22);
    lv_obj_set_style_arc_width(item->spinner_arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(item->spinner_arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(item->spinner_arc, lv_color_hex(0xDBEAFE), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(item->spinner_arc, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_arc_color(item->spinner_arc, lv_color_hex(0x3B82F6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(item->spinner_arc, true, LV_PART_INDICATOR);
    lv_obj_center(item->spinner_arc);
    lv_obj_add_flag(item->spinner_arc, LV_OBJ_FLAG_HIDDEN);

    item->pending_ring = lv_obj_create(item->icon_box);
    lv_obj_remove_style_all(item->pending_ring);
    lv_obj_set_size(item->pending_ring, ctx->cfg.icon_size, ctx->cfg.icon_size);
    lv_obj_set_style_radius(item->pending_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(item->pending_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item->pending_ring, 1, 0);
    lv_obj_set_style_border_color(item->pending_ring, lv_color_hex(0xD1D5DB), 0);
    lv_obj_center(item->pending_ring);

    item->icon_img = lv_img_create(item->icon_box);
    lv_obj_clear_flag(item->icon_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(item->icon_img);

    item->name_label = lv_label_create(card);
    lv_label_set_long_mode(item->name_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(item->name_label, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(item->name_label, ctx->cfg.pending_text_color, 0);

    item->state_label = lv_label_create(card);
    lv_label_set_long_mode(item->state_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(item->state_label, ctx->cfg.state_w);
    lv_obj_set_style_text_font(item->state_label, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_align(item->state_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(item->state_label, ctx->cfg.pending_state_color, 0);

    selftest_list_apply_item_style(ctx, item);

    lv_label_set_text(item->name_label, "");
    lv_label_set_text(item->state_label, "---");
    item->state = LV_SELFTEST_LIST_STATE_PENDING;

    return card;
}

static void selftest_list_rebuild(lv_selftest_list_ctx_t *ctx) // 重建列表
{
    uint16_t list_h;

    if (ctx == NULL || ctx->root == NULL || !lv_obj_is_valid(ctx->root)) {
        return;
    }

    lv_obj_clean(ctx->root);

    if (ctx->items != NULL) {
        for (uint8_t i = 0; i < ctx->item_count; i++) {
            selftest_list_item_release(&ctx->items[i]);
        }
        lv_mem_free(ctx->items);
        ctx->items = NULL;
    }

    if (ctx->item_count == 0) {
        list_h = 1;
        lv_obj_set_size(ctx->root, ctx->cfg.item_w, list_h);
        return;
    }

    ctx->items = lv_mem_alloc(sizeof(lv_selftest_list_item_t) * ctx->item_count);
    if (ctx->items == NULL) {
        ctx->item_count = 0;
        lv_obj_set_size(ctx->root, ctx->cfg.item_w, 1);
        return;
    }
    memset(ctx->items, 0, sizeof(lv_selftest_list_item_t) * ctx->item_count);

    lv_obj_set_layout(ctx->root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctx->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctx->root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ctx->root, 0, 0);
    lv_obj_set_style_pad_row(ctx->root, ctx->cfg.item_gap, 0);
    lv_obj_set_scrollbar_mode(ctx->root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ctx->root, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < ctx->item_count; i++) {
        lv_obj_t *card = selftest_list_create_item_card(ctx, ctx->root, i);
        if (card == NULL) {
            continue;
        }
        if (i + 1 < ctx->item_count) {
            lv_obj_set_style_pad_bottom(card, 0, 0);
        }
    }

    list_h = (uint16_t)(ctx->item_count * ctx->cfg.item_h +
                        (ctx->item_count > 0 ? (ctx->item_count - 1) * ctx->cfg.item_gap : 0));
    if (list_h == 0) {
        list_h = 1;
    }
    lv_obj_set_size(ctx->root, ctx->cfg.item_w, list_h);
}

static void selftest_list_root_delete_cb(lv_event_t *e) // 列表销毁时释放资源
{
    lv_selftest_list_ctx_t *ctx;

    if (e == NULL || lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    ctx = (lv_selftest_list_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL) {
        return;
    }

    selftest_list_ctx_unregister(ctx);
    if (ctx->items != NULL) {
        for (uint8_t i = 0; i < ctx->item_count; i++) {
            selftest_list_item_release(&ctx->items[i]);
        }
        lv_mem_free(ctx->items);
        ctx->items = NULL;
    }
    lv_mem_free(ctx);
}

lv_obj_t *lv_selftest_list_create_with_config(lv_obj_t *parent, uint8_t item_count,
                                              const lv_selftest_list_config_t *cfg_in) // 按配置创建自检列表
{
    lv_selftest_list_ctx_t *ctx;
    lv_selftest_list_config_t cfg_default;

    if (parent == NULL) {
        return NULL;
    }

    ctx = lv_mem_alloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));

    if (cfg_in == NULL) {
        lv_selftest_list_config_init(&cfg_default);
        cfg_in = &cfg_default;
    }
    ctx->cfg = *cfg_in;
    ctx->item_count = item_count;

    ctx->root = lv_obj_create(parent);
    lv_obj_remove_style_all(ctx->root);
    lv_obj_clear_flag(ctx->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(ctx->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ctx->root, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(ctx->root, selftest_list_root_delete_cb, LV_EVENT_DELETE, ctx);

    if (!selftest_list_ctx_register(ctx)) {
        lv_obj_del(ctx->root);
        return NULL;
    }

    selftest_list_rebuild(ctx);
    return ctx->root;
}

lv_obj_t *lv_selftest_list_create(lv_obj_t *parent, uint8_t item_count) // 创建自检列表
{
    lv_selftest_list_config_t cfg;

    lv_selftest_list_config_init(&cfg);
    return lv_selftest_list_create_with_config(parent, item_count, &cfg);
}

void lv_selftest_list_set_count(lv_obj_t *list, uint8_t item_count) // 设置自检列表行数
{
    lv_selftest_list_ctx_t *ctx = selftest_list_ctx_find(list);

    if (ctx == NULL) {
        return;
    }

    ctx->item_count = item_count;
    selftest_list_rebuild(ctx);
}

void lv_selftest_list_set_item_name(lv_obj_t *list, uint8_t index, const char *name) // 设置单项名称
{
    lv_selftest_list_ctx_t *ctx;
    lv_selftest_list_item_t *item;

    ctx = selftest_list_ctx_find(list);
    if (ctx == NULL) {
        return;
    }

    item = selftest_list_item_get(ctx, index);
    if (item == NULL || item->name_label == NULL || !lv_obj_is_valid(item->name_label)) {
        return;
    }

    if (name == NULL) {
        name = "";
    }
    strncpy(item->name_buf, name, sizeof(item->name_buf) - 1);
    item->name_buf[sizeof(item->name_buf) - 1] = '\0';
    lv_label_set_text(item->name_label, item->name_buf);
}

void lv_selftest_list_set_item_state(lv_obj_t *list, uint8_t index,
                                     lv_selftest_list_state_t state) // 设置单项状态
{
    lv_selftest_list_ctx_t *ctx;
    lv_selftest_list_item_t *item;

    ctx = selftest_list_ctx_find(list);
    if (ctx == NULL) {
        return;
    }

    item = selftest_list_item_get(ctx, index);
    if (item == NULL) {
        return;
    }

    selftest_list_apply_state(ctx, item, state);
}

void lv_selftest_list_set_item(lv_obj_t *list, uint8_t index, const char *name,
                               lv_selftest_list_state_t state) // 设置单项名称和状态
{
    lv_selftest_list_set_item_name(list, index, name);
    lv_selftest_list_set_item_state(list, index, state);
}
