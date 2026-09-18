#include "lv_damped_button.h"

#include <string.h>

#define LV_DAMPED_BUTTON_PRESS_MS       90U
#define LV_DAMPED_BUTTON_RELEASE_MS     190U
#define LV_DAMPED_BUTTON_RELEASE_DELAY  24U
/* 20 / 255 = 7.84%, i.e. the requested perceptual 8% darkening. */
#define LV_DAMPED_BUTTON_DARKEN_OPA     ((lv_opa_t)20U)

lv_color_t lv_damped_button_pressed_color(lv_color_t normal_color)
{
    return lv_color_darken(normal_color, LV_DAMPED_BUTTON_DARKEN_OPA);
}

typedef struct lv_damped_button_ctx {
    lv_obj_t *button;
    lv_color_t normal_color;
    lv_color_t pressed_color;
    lv_color_t current_color;
    lv_color_t anim_from;
    lv_color_t anim_to;
    struct lv_damped_button_ctx *next;
} lv_damped_button_ctx_t;

static lv_damped_button_ctx_t *s_damped_button_ctx_list;

static lv_damped_button_ctx_t *lv_damped_button_ctx_find(lv_obj_t *button)
{
    lv_damped_button_ctx_t *ctx = s_damped_button_ctx_list;

    while (ctx != NULL) {
        if (ctx->button == button) return ctx;
        ctx = ctx->next;
    }
    return NULL;
}

static void lv_damped_button_color_apply(lv_damped_button_ctx_t *ctx,
                                         lv_color_t color)
{
    if (ctx == NULL || ctx->button == NULL ||
        !lv_obj_is_valid(ctx->button)) {
        return;
    }

    ctx->current_color = color;
    /* Write both selectors.  This intentionally outranks legacy APPLE and
       ANDROID pressed styles, so a state change can never expose their white
       fallback frame. */
    lv_obj_set_style_bg_color(ctx->button, color,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ctx->button, color,
                              LV_PART_MAIN | LV_STATE_PRESSED);
}

static void lv_damped_button_color_anim_cb(void *var, int32_t value)
{
    lv_damped_button_ctx_t *ctx = (lv_damped_button_ctx_t *)var;
    lv_color_t color;

    if (ctx == NULL) return;
    color = lv_color_mix(ctx->anim_to, ctx->anim_from, (lv_opa_t)value);
    lv_damped_button_color_apply(ctx, color);
}

static void lv_damped_button_color_anim_start(lv_damped_button_ctx_t *ctx,
                                               lv_color_t target,
                                               uint32_t duration,
                                               uint32_t delay)
{
    lv_anim_t anim;

    if (ctx == NULL || ctx->button == NULL ||
        !lv_obj_is_valid(ctx->button)) {
        return;
    }
    lv_anim_del(ctx, lv_damped_button_color_anim_cb);
    if (ctx->current_color.full == target.full) return;

    ctx->anim_from = ctx->current_color;
    ctx->anim_to = target;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx);
    lv_anim_set_exec_cb(&anim, lv_damped_button_color_anim_cb);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_delay(&anim, delay);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static void lv_damped_button_feedback_event_cb(lv_event_t *event)
{
    lv_damped_button_ctx_t *ctx = lv_event_get_user_data(event);

    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        lv_damped_button_color_anim_start(ctx, ctx->pressed_color,
                                          LV_DAMPED_BUTTON_PRESS_MS, 0);
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        lv_damped_button_color_anim_start(ctx, ctx->normal_color,
                                          LV_DAMPED_BUTTON_RELEASE_MS,
                                          LV_DAMPED_BUTTON_RELEASE_DELAY);
        break;
    case LV_EVENT_DELETE:
        {
            lv_damped_button_ctx_t **link = &s_damped_button_ctx_list;

            lv_anim_del(ctx, lv_damped_button_color_anim_cb);
            while (*link != NULL && *link != ctx) link = &(*link)->next;
            if (*link == ctx) *link = ctx->next;
            lv_mem_free(ctx);
        }
        break;
    default:
        break;
    }
}

void lv_damped_button_set_palette(lv_obj_t *button,
                                  lv_color_t normal_color,
                                  lv_color_t pressed_color)
{
    lv_damped_button_ctx_t *ctx;

    if (button == NULL || !lv_obj_is_valid(button)) return;

    /* pressed_color is retained in the public signature for source
       compatibility.  Product-wide feedback is now deterministic: the
       pressed shade is always approximately 8% darker than the current base. */
    LV_UNUSED(pressed_color);
    pressed_color = lv_damped_button_pressed_color(normal_color);

    ctx = lv_damped_button_ctx_find(button);
    if (ctx != NULL) {
        lv_anim_del(ctx, lv_damped_button_color_anim_cb);
        ctx->normal_color = normal_color;
        ctx->pressed_color = pressed_color;
        lv_damped_button_color_apply(
            ctx, lv_obj_has_state(button, LV_STATE_PRESSED) ?
                 pressed_color : normal_color);
        return;
    }

    lv_obj_set_style_bg_color(button, normal_color,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, pressed_color,
                              LV_PART_MAIN | LV_STATE_PRESSED);
}

void lv_damped_button_register(lv_obj_t *button,
                               lv_color_t normal_color,
                               lv_color_t pressed_color)
{
    uint32_t i;
    uint32_t child_count;
    lv_damped_button_ctx_t *ctx;
    lv_coord_t shadow_width;
    lv_coord_t shadow_ofs_y;
    lv_opa_t shadow_opa;

    if (button == NULL || !lv_obj_is_valid(button)) return;

    ctx = lv_damped_button_ctx_find(button);
    if (ctx != NULL) {
        lv_damped_button_set_palette(button, normal_color, pressed_color);
        return;
    }

    ctx = lv_mem_alloc(sizeof(*ctx));
    if (ctx == NULL) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->button = button;
    ctx->normal_color = normal_color;
    ctx->pressed_color = lv_damped_button_pressed_color(normal_color);
    ctx->current_color = normal_color;
    ctx->next = s_damped_button_ctx_list;
    s_damped_button_ctx_list = ctx;

    shadow_width = lv_obj_get_style_shadow_width(button, LV_PART_MAIN);
    shadow_ofs_y = lv_obj_get_style_shadow_ofs_y(button, LV_PART_MAIN);
    shadow_opa = lv_obj_get_style_shadow_opa(button, LV_PART_MAIN);

    /* Remove LVGL state transitions installed by an older registration.
       The shared component owns color interpolation explicitly so a palette
       update (for example SPEED or SORT selection) cannot finish toward a
       stale color after the protocol reply arrives. */
    lv_obj_remove_local_style_prop(button, LV_STYLE_TRANSITION,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_local_style_prop(button, LV_STYLE_TRANSITION,
                                   LV_PART_MAIN | LV_STATE_PRESSED);
    lv_damped_button_color_apply(ctx, normal_color);
    /* The button and all of its contents stay fully opaque.  On this target,
       parent zoom/opacity states can route children through a transformed GE
       layer and make labels or the complete button disappear. */
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_COVER,
                         LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(button, LV_OPA_COVER,
                         LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_translate_y(button, 0,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_translate_y(button, 0,
                                 LV_PART_MAIN | LV_STATE_PRESSED);
    /* Explicitly neutralize legacy APPLE/PRESS_FEEL/ANDROID transforms. */
    lv_obj_set_style_transform_zoom(button, 256,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_zoom(button, 256,
                                    LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(button, 0,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_width(button, 0,
                                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(button, 0,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_height(button, 0,
                                      LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button, shadow_width,
                                  LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_ofs_y(button, shadow_ofs_y,
                                  LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(button, shadow_opa,
                                LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, lv_damped_button_feedback_event_cb,
                        LV_EVENT_ALL, ctx);

    child_count = lv_obj_get_child_cnt(button);
    for (i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(button, i);
        if (child == NULL || !lv_obj_is_valid(child)) continue;
        lv_obj_set_style_opa(child, LV_OPA_COVER,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(child, LV_OPA_COVER,
                             LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_opa(child, LV_OPA_COVER,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(child, LV_OPA_COVER,
                                  LV_PART_MAIN | LV_STATE_PRESSED);
    }
}

lv_obj_t *lv_damped_button_create(lv_obj_t *parent,
                                  const lv_damped_button_style_t *style,
                                  const char *text,
                                  const lv_font_t *font)
{
    lv_obj_t *button;
    lv_obj_t *label;

    if (parent == NULL || style == NULL) return NULL;
    button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->normal_color), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->pressed_color), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->disabled_color), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, style->radius, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(style->normal_color), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_30, LV_STATE_PRESSED);
    label = lv_label_create(button);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(style->text_color), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(style->disabled_text_color), LV_STATE_DISABLED);
    lv_obj_center(label);
    lv_damped_button_register(button,
                              lv_color_hex(style->normal_color),
                              lv_color_hex(style->pressed_color));
    return button;
}

lv_obj_t *lv_damped_button_get_label(lv_obj_t *button)
{
    return (button != NULL && lv_obj_is_valid(button) && lv_obj_get_child_cnt(button) > 0)
        ? lv_obj_get_child(button, 0) : NULL;
}

void lv_damped_button_set_text(lv_obj_t *button, const char *text)
{
    lv_obj_t *label = lv_damped_button_get_label(button);
    const char *current;

    if (label == NULL) return;
    if (text == NULL) text = "";
    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

void lv_damped_button_set_enabled(lv_obj_t *button, bool enabled)
{
    lv_obj_t *label;
    if (button == NULL || !lv_obj_is_valid(button)) return;
    if (lv_damped_button_is_enabled(button) == enabled) return;
    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
    label = lv_damped_button_get_label(button);
    if (label != NULL) {
        if (enabled) lv_obj_clear_state(label, LV_STATE_DISABLED);
        else lv_obj_add_state(label, LV_STATE_DISABLED);
    }
}

bool lv_damped_button_is_enabled(lv_obj_t *button)
{
    return button != NULL && lv_obj_is_valid(button) &&
           !lv_obj_has_state(button, LV_STATE_DISABLED);
}
