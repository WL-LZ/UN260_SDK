#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_core/lv_page_manager.h"
#include <math.h>

#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_A

static lv_obj_t* boot_anim_page = NULL;
static lv_timer_t* boot_anim_timer = NULL;
static lv_obj_t* line_base = NULL;
static lv_obj_t* line_mid = NULL;
static lv_obj_t* line_core = NULL;
static lv_obj_t* line_glow = NULL;
static lv_obj_t* line_trail = NULL;
static uint32_t boot_anim_start_tick = 0;

static float clampf(float v, float min, float max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static float ease_out_cubic(float t)
{
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static float ease_in_cubic(float t)
{
    return t * t * t;
}
static float ease_out_quint(float t)
{
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv * inv;
}

static float open_damped(float t)
{
    if (t < 0.38f) {
        float p = t / 0.38f;
        return 0.04f * p * p;
    } else {
        float p = (t - 0.38f) / 0.62f;
        return 0.04f + 0.96f * ease_out_quint(p);
    }
}
static void boot_anim_set_bar(lv_obj_t* obj, int x, int y, int w, int h, lv_opa_t opa)
{
    if (obj == NULL) return;

    if (w <= 0 || h <= 0 || opa == 0) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, opa, 0);
}

static void boot_anim_finish(void)
{
    bool registered_page =
        ui_manager_get_current_page() == UI_PAGE_BOOT_ANIM;

    if (boot_anim_timer) {
        lv_timer_del(boot_anim_timer);
        boot_anim_timer = NULL;
    }

    if (registered_page) {
        ui_manager_switch(UI_PAGE_BOOT);
    } else {
        ui_page_00_boot_anim_destroy();
    }
}

static void boot_anim_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    if (boot_anim_page == NULL) return;

    const int scr_w = 1280;
    const int scr_h = 400;
    const int cx = scr_w / 2;
    const int cy = scr_h / 2;

    uint32_t now = lv_tick_get();
    float t = (float)(now - boot_anim_start_tick) / 1000.0f;

    if (t < 0.9f) {
        boot_anim_set_bar(line_base, 0, 0, 0, 0, 0);
        boot_anim_set_bar(line_mid, 0, 0, 0, 0, 0);
        boot_anim_set_bar(line_core, 0, 0, 0, 0, 0);
        boot_anim_set_bar(line_glow, 0, 0, 0, 0, 0);
        boot_anim_set_bar(line_trail, 0, 0, 0, 0, 0);
        return;
    }

    float t1 = t - 0.9f;
    float grow_t = clampf(t1 / 0.98f, 0.0f, 1.0f);
    float half_len = (scr_w * 0.34f) * open_damped(grow_t);
    float appear_alpha = ease_out_cubic(clampf(t1 / 0.9f, 0.0f, 1.0f));

    float breathe_start = 1.0f;
    float breath_t = t1 - breathe_start;
    if (breath_t < 0.0f) breath_t = 0.0f;
    float breathe = 0.5f + 0.5f * sinf(breath_t * 3.1415926f * 0.75f);
    float breathe_alpha = 0.82f + breathe * 0.10f;

    float end_fade = 1.0f;
    if (t > 4.0f) {
        end_fade = 1.0f - ease_in_cubic(clampf((t - 4.0f) / 0.8f, 0.0f, 1.0f));
    }

    float line_alpha = appear_alpha * breathe_alpha * end_fade;

    int x1 = (int)(cx - half_len);
    int x2 = (int)(cx + half_len);
    int line_w = x2 - x1;

    boot_anim_set_bar(line_base, x1, cy - 1, line_w, 2, (lv_opa_t)(255.0f * 0.18f * line_alpha));
    boot_anim_set_bar(line_mid,  x1, cy - 1, line_w, 2, (lv_opa_t)(255.0f * 0.52f * line_alpha));
    boot_anim_set_bar(line_core, 0, 0, 0, 0, 0);
    {
        float travel_start = 1.55f;
        float travel_dur = 1.95f;
        float tt = clampf((t1 - travel_start) / travel_dur, 0.0f, 1.0f);
        if (tt > 0.0f && tt < 1.0f && line_w > 0) {
            float move_p = 0.0f;

            if (tt < 0.42f) {
                move_p = ease_out_cubic(tt / 0.42f) * 0.48f;
            } else if (tt < 0.58f) {
                float mid = (tt - 0.42f) / 0.16f;
                move_p = 0.48f + mid * 0.04f;
            } else {
                move_p = 0.52f + ease_in_cubic((tt - 0.58f) / 0.42f) * 0.48f;
            }

            int px = x1 + (int)((x2 - x1) * move_p);

            int max_w = 52;
            int tail_len = 72;
            float grow_ratio = 0.14f;
            float shrink_ratio = 0.16f;

            int glow_w = max_w;

            if (move_p < grow_ratio) {
                float k = move_p / grow_ratio;
                glow_w = (int)(max_w * ease_out_cubic(k));
            } else if (move_p > (1.0f - shrink_ratio)) {
                float k = (move_p - (1.0f - shrink_ratio)) / shrink_ratio;
                glow_w = (int)(max_w * (1.0f - ease_in_cubic(k)));
            }

            if (glow_w < 1) glow_w = 1;

            /* 从左端开始生长，到右端逐渐收束 */
            int gx = px;

            /* 尾巴更长、更淡 */
            int tx1 = gx - tail_len;
            if (tx1 < x1) tx1 = x1;

            /* 高光透明度也跟长度一起变化，避免像死块 */
            float glow_alpha_k = (float)glow_w / (float)max_w;
            lv_opa_t glow_opa = (lv_opa_t)(255.0f * 0.92f * glow_alpha_k * end_fade);
            lv_opa_t tail_opa = (lv_opa_t)(255.0f * 0.08f * glow_alpha_k * end_fade);

            boot_anim_set_bar(line_glow,
                gx,
                cy - 1,
                glow_w,
                2,
                glow_opa);

            boot_anim_set_bar(line_trail,
                tx1,
                cy,
                gx - tx1,
                1,
                tail_opa);
        } else {
            boot_anim_set_bar(line_glow, 0, 0, 0, 0, 0);
            boot_anim_set_bar(line_trail, 0, 0, 0, 0, 0);
        }
    }

    if (t >= 4.8f) {
        boot_anim_finish();
    }
}

static lv_obj_t* boot_anim_create_line(lv_obj_t* parent, lv_opa_t opa)
{
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return obj;
}

void ui_page_00_boot_anim_create(lv_obj_t* parent)
{
    if (boot_anim_page) return;
    if (parent == NULL) parent = lv_layer_top();

    boot_anim_page = lv_obj_create(parent);
    lv_obj_remove_style_all(boot_anim_page);
    lv_obj_set_pos(boot_anim_page, 0, 0);
    lv_obj_set_size(boot_anim_page, 1280, 400);
    lv_obj_clear_flag(boot_anim_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(boot_anim_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(boot_anim_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(boot_anim_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(boot_anim_page, LV_OPA_COVER, 0);

    line_base = boot_anim_create_line(boot_anim_page, LV_OPA_30);
    line_mid = boot_anim_create_line(boot_anim_page, LV_OPA_60);
    line_core = boot_anim_create_line(boot_anim_page, LV_OPA_60);
    line_glow = boot_anim_create_line(boot_anim_page, LV_OPA_90);
    line_trail = boot_anim_create_line(boot_anim_page, LV_OPA_20);

    boot_anim_start_tick = lv_tick_get();

    if (boot_anim_timer) {
        lv_timer_del(boot_anim_timer);
        boot_anim_timer = NULL;
    }
    boot_anim_timer = lv_timer_create(boot_anim_timer_cb, 16, NULL);
    if (boot_anim_timer == NULL) boot_anim_finish();
}

void ui_page_00_boot_anim_destroy(void)
{
    if (boot_anim_timer) {
        lv_timer_del(boot_anim_timer);
        boot_anim_timer = NULL;
    }

    if (boot_anim_page && lv_obj_is_valid(boot_anim_page)) {
        lv_obj_del(boot_anim_page);
    }
    boot_anim_page = NULL;

    line_base = NULL;
    line_mid = NULL;
    line_core = NULL;
    line_glow = NULL;
    line_trail = NULL;
}

bool ui_page_00_boot_anim_is_active(void)
{
    return boot_anim_page != NULL && lv_obj_is_valid(boot_anim_page);
}

#endif /* UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_A */
