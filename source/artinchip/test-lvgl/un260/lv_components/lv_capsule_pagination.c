#include "lv_capsule_pagination.h"

#include <string.h>

#define CAPSULE_PAGINATION_MAX_CTX         8
#define CAPSULE_PAGINATION_DOT_H           6
#define CAPSULE_PAGINATION_DOT_W           6
#define CAPSULE_PAGINATION_ACTIVE_DOT_W   13
#define CAPSULE_PAGINATION_DOT_GAP         6
#define CAPSULE_PAGINATION_ANIM_TIME     300
#define CAPSULE_PAGINATION_DOT_GRAY  0x7C7C80
#define CAPSULE_PAGINATION_DOT_BLUE  0xFFFFFF

typedef struct {
    lv_obj_t *root;
    lv_obj_t **dots;
    uint8_t dot_count;
    uint8_t active_index;
} lv_capsule_pagination_ctx_t;

static lv_capsule_pagination_ctx_t *g_capsule_pagination_ctx[CAPSULE_PAGINATION_MAX_CTX];

static bool capsule_pagination_ctx_register(lv_capsule_pagination_ctx_t *ctx); // 注册上下文
static void capsule_pagination_ctx_unregister(lv_capsule_pagination_ctx_t *ctx); // 注销上下文
static lv_capsule_pagination_ctx_t *capsule_pagination_ctx_find(lv_obj_t *pagination); // 查找上下文
static void capsule_pagination_release_dots(lv_capsule_pagination_ctx_t *ctx, bool delete_obj); // 释放圆点数组
static bool capsule_pagination_rebuild(lv_capsule_pagination_ctx_t *ctx); // 重建圆点
static void capsule_pagination_dot_apply_state(lv_obj_t *dot, bool active); // 刷新圆点状态
static void capsule_pagination_anim_width_cb(void *var, int32_t v); // 圆点宽度动画
static void capsule_pagination_delete_cb(lv_event_t *e); // 销毁回调
static bool capsule_pagination_set_active_core(lv_obj_t *pagination, uint8_t index, bool anim_en); // 设置激活页

static bool capsule_pagination_ctx_register(lv_capsule_pagination_ctx_t *ctx) // 注册上下文
{
    for (uint32_t i = 0; i < CAPSULE_PAGINATION_MAX_CTX; i++) {
        if (g_capsule_pagination_ctx[i] == NULL) {
            g_capsule_pagination_ctx[i] = ctx;
            return true;
        }
    }

    return false;
}

static void capsule_pagination_ctx_unregister(lv_capsule_pagination_ctx_t *ctx) // 注销上下文
{
    for (uint32_t i = 0; i < CAPSULE_PAGINATION_MAX_CTX; i++) {
        if (g_capsule_pagination_ctx[i] == ctx) {
            g_capsule_pagination_ctx[i] = NULL;
            return;
        }
    }
}

static lv_capsule_pagination_ctx_t *capsule_pagination_ctx_find(lv_obj_t *pagination) // 查找上下文
{
    for (uint32_t i = 0; i < CAPSULE_PAGINATION_MAX_CTX; i++) {
        if (g_capsule_pagination_ctx[i] != NULL && g_capsule_pagination_ctx[i]->root == pagination) {
            return g_capsule_pagination_ctx[i];
        }
    }

    return NULL;
}

static void capsule_pagination_anim_width_cb(void *var, int32_t v) // 圆点宽度动画
{
    lv_obj_t *dot = (lv_obj_t *)var;

    if (dot == NULL || !lv_obj_is_valid(dot)) {
        return;
    }

    lv_obj_set_width(dot, (lv_coord_t)v);
}

static void capsule_pagination_dot_apply_state(lv_obj_t *dot, bool active) // 刷新圆点状态
{
    if (dot == NULL || !lv_obj_is_valid(dot)) {
        return;
    }

    lv_anim_del(dot, capsule_pagination_anim_width_cb);
    lv_obj_set_height(dot, CAPSULE_PAGINATION_DOT_H);
    lv_obj_set_width(dot, active ? CAPSULE_PAGINATION_ACTIVE_DOT_W : CAPSULE_PAGINATION_DOT_W);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot,
        active ? lv_color_hex(CAPSULE_PAGINATION_DOT_BLUE) : lv_color_hex(CAPSULE_PAGINATION_DOT_GRAY), 0);
}

static void capsule_pagination_release_dots(lv_capsule_pagination_ctx_t *ctx, bool delete_obj) // 释放圆点数组
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->dots != NULL) {
        for (uint8_t i = 0; i < ctx->dot_count; i++) {
            if (ctx->dots[i] != NULL && lv_obj_is_valid(ctx->dots[i])) {
                lv_anim_del(ctx->dots[i], capsule_pagination_anim_width_cb);
                if (delete_obj) {
                    lv_obj_del(ctx->dots[i]);
                }
            }
        }
        lv_mem_free(ctx->dots);
        ctx->dots = NULL;
    }

    ctx->dot_count = 0;
}

static bool capsule_pagination_rebuild(lv_capsule_pagination_ctx_t *ctx) // 重建圆点
{
    lv_obj_t *dot;

    if (ctx == NULL || ctx->root == NULL || !lv_obj_is_valid(ctx->root)) {
        return false;
    }

    if (ctx->dot_count == 0U) {
        capsule_pagination_release_dots(ctx, true);
        return true;
    }

    ctx->dots = (lv_obj_t **)lv_mem_alloc(sizeof(lv_obj_t *) * ctx->dot_count);
    if (ctx->dots == NULL) {
        ctx->dot_count = 0;
        ctx->active_index = 0;
        return false;
    }

    memset(ctx->dots, 0, sizeof(lv_obj_t *) * ctx->dot_count);

    if (ctx->active_index >= ctx->dot_count) {
        ctx->active_index = ctx->dot_count - 1U;
    }

    for (uint8_t i = 0; i < ctx->dot_count; i++) {
        dot = lv_obj_create(ctx->root);
        lv_obj_remove_style_all(dot);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        capsule_pagination_dot_apply_state(dot, i == ctx->active_index);
        ctx->dots[i] = dot;
    }

    lv_obj_update_layout(ctx->root);
    return true;
}

static void capsule_pagination_delete_cb(lv_event_t *e) // 销毁回调
{
    lv_obj_t *pagination;
    lv_capsule_pagination_ctx_t *ctx;

    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }

    pagination = lv_event_get_target(e);
    ctx = capsule_pagination_ctx_find(pagination);
    if (ctx == NULL) {
        return;
    }

    capsule_pagination_release_dots(ctx, false);
    capsule_pagination_ctx_unregister(ctx);
    lv_mem_free(ctx);
}

static bool capsule_pagination_set_active_core(lv_obj_t *pagination, uint8_t index, bool anim_en) // 设置激活页
{
    lv_capsule_pagination_ctx_t *ctx;
    lv_obj_t *old_dot;
    lv_obj_t *new_dot;
    lv_anim_t a;
    uint8_t old_index;
    uint8_t new_index;

    ctx = capsule_pagination_ctx_find(pagination);
    if (ctx == NULL || ctx->dot_count == 0U || ctx->dots == NULL) {
        return false;
    }

    new_index = (index >= ctx->dot_count) ? (ctx->dot_count - 1U) : index;
    old_index = ctx->active_index;

    if (old_index == new_index) {
        capsule_pagination_dot_apply_state(ctx->dots[new_index], true);
        return true;
    }

    old_dot = ctx->dots[old_index];
    new_dot = ctx->dots[new_index];
    ctx->active_index = new_index;

    if (old_dot == NULL || new_dot == NULL || !lv_obj_is_valid(old_dot) || !lv_obj_is_valid(new_dot)) {
        return false;
    }

    lv_anim_del(old_dot, capsule_pagination_anim_width_cb);
    lv_anim_del(new_dot, capsule_pagination_anim_width_cb);

    lv_obj_set_style_bg_color(old_dot, lv_color_hex(CAPSULE_PAGINATION_DOT_GRAY), 0);
    lv_obj_set_style_bg_color(new_dot, lv_color_hex(CAPSULE_PAGINATION_DOT_BLUE), 0);
    lv_obj_set_height(old_dot, CAPSULE_PAGINATION_DOT_H);
    lv_obj_set_height(new_dot, CAPSULE_PAGINATION_DOT_H);
    lv_obj_set_style_radius(old_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(new_dot, LV_RADIUS_CIRCLE, 0);

    if (!anim_en) {
        lv_obj_set_width(old_dot, CAPSULE_PAGINATION_DOT_W);
        lv_obj_set_width(new_dot, CAPSULE_PAGINATION_ACTIVE_DOT_W);
        lv_obj_update_layout(pagination);
        return true;
    }

    lv_anim_init(&a);
    lv_anim_set_time(&a, CAPSULE_PAGINATION_ANIM_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    lv_anim_set_var(&a, old_dot);
    lv_anim_set_exec_cb(&a, capsule_pagination_anim_width_cb);
    lv_anim_set_values(&a, lv_obj_get_width(old_dot), CAPSULE_PAGINATION_DOT_W);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_dot);
    lv_anim_set_exec_cb(&a, capsule_pagination_anim_width_cb);
    lv_anim_set_values(&a, lv_obj_get_width(new_dot), CAPSULE_PAGINATION_ACTIVE_DOT_W);
    lv_anim_set_time(&a, CAPSULE_PAGINATION_ANIM_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    return true;
}

lv_obj_t *lv_capsule_pagination_create(lv_obj_t *parent) // 创建胶囊圆点分页器
{
    lv_obj_t *pagination;
    lv_capsule_pagination_ctx_t *ctx;

    if (parent == NULL || !lv_obj_is_valid(parent)) {
        return NULL;
    }

    pagination = lv_obj_create(parent);
    if (pagination == NULL) {
        return NULL;
    }

    ctx = (lv_capsule_pagination_ctx_t *)lv_mem_alloc(sizeof(lv_capsule_pagination_ctx_t));
    if (ctx == NULL) {
        lv_obj_del(pagination);
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->root = pagination;
    ctx->dot_count = 1U;
    ctx->active_index = 0U;

    if (!capsule_pagination_ctx_register(ctx)) {
        lv_mem_free(ctx);
        lv_obj_del(pagination);
        return NULL;
    }

    lv_obj_remove_style_all(pagination);
    lv_obj_set_size(pagination, LV_SIZE_CONTENT, CAPSULE_PAGINATION_DOT_H);
    lv_obj_set_flex_flow(pagination, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pagination, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pagination, CAPSULE_PAGINATION_DOT_GAP, 0);
    lv_obj_set_style_pad_row(pagination, 0, 0);
    lv_obj_set_style_pad_all(pagination, 0, 0);
    lv_obj_set_style_bg_opa(pagination, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(pagination, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pagination, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pagination, capsule_pagination_delete_cb, LV_EVENT_DELETE, NULL);

    if (!capsule_pagination_rebuild(ctx)) {
        capsule_pagination_ctx_unregister(ctx);
        lv_mem_free(ctx);
        lv_obj_del(pagination);
        return NULL;
    }

    return pagination;
}

bool lv_capsule_pagination_set_count(lv_obj_t *pagination, uint8_t count) // 设置总页数并重建圆点
{
    lv_capsule_pagination_ctx_t *ctx;
    uint8_t new_count;

    ctx = capsule_pagination_ctx_find(pagination);
    if (ctx == NULL) {
        return false;
    }

    new_count = count;
    if (ctx->dot_count == new_count) {
        if (ctx->dot_count > 0U && ctx->active_index >= ctx->dot_count) {
            ctx->active_index = ctx->dot_count - 1U;
        }
        return true;
    }

    capsule_pagination_release_dots(ctx, true);
    ctx->dot_count = new_count;
    if (ctx->dot_count == 0U) {
        ctx->active_index = 0U;
    } else if (ctx->active_index >= ctx->dot_count) {
        ctx->active_index = ctx->dot_count - 1U;
    }

    return capsule_pagination_rebuild(ctx);
}

bool lv_capsule_pagination_set_active_page(lv_obj_t *pagination, uint8_t index) // 动画切换激活页
{
    return capsule_pagination_set_active_core(pagination, index, true);
}

bool lv_capsule_pagination_set_active_page_now(lv_obj_t *pagination, uint8_t index) // 直接切换激活页
{
    return capsule_pagination_set_active_core(pagination, index, false);
}

uint8_t lv_capsule_pagination_get_count(lv_obj_t *pagination) // 获取总页数
{
    lv_capsule_pagination_ctx_t *ctx = capsule_pagination_ctx_find(pagination);

    return ctx ? ctx->dot_count : 0U;
}

uint8_t lv_capsule_pagination_get_active_page(lv_obj_t *pagination) // 获取当前激活页
{
    lv_capsule_pagination_ctx_t *ctx = capsule_pagination_ctx_find(pagination);

    return ctx ? ctx->active_index : 0U;
}