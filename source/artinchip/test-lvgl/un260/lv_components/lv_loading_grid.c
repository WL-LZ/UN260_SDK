#include "lv_loading_grid.h"

/* Private 3x3 loading-grid construction and animation implementation. */
#define LOADING_GRID_DOT_COUNT 9
#define LOADING_GRID_COL_COUNT 3
#define LOADING_GRID_ROW_COUNT 3

#define LOADING_GRID_DEFAULT_DOT_SIZE 12
#define LOADING_GRID_DEFAULT_DOT_GAP 6
#define LOADING_GRID_DEFAULT_DOT_COLOR 0x333333
#define LOADING_GRID_DEFAULT_OPA_MIN LV_OPA_10
#define LOADING_GRID_DEFAULT_OPA_MAX LV_OPA_COVER
#define LOADING_GRID_DEFAULT_ANIM_TIME 750
#define LOADING_GRID_DEFAULT_ANIM_PLAYBACK 750
#define LOADING_GRID_DEFAULT_DELAY_STEP 200

typedef struct {
    lv_coord_t dot_size;
    lv_coord_t dot_gap;
    lv_color_t dot_color;
    lv_opa_t opa_min;
    lv_opa_t opa_max;
    uint16_t anim_time;
    uint16_t anim_playback_time;
    uint16_t delay_step;
} lv_loading_grid_config_t;

static const uint8_t g_loading_grid_wave_group[LOADING_GRID_DOT_COUNT] = {
    0, 1, 2,
    1, 2, 3,
    2, 3, 4
};

static lv_coord_t loading_grid_clamp_coord(lv_coord_t value, lv_coord_t min_value);
static void lv_loading_grid_config_init(lv_loading_grid_config_t *cfg);
static void loading_grid_anim_opa_cb(void *var, int32_t v);
static void loading_grid_start_dot_anim(lv_obj_t *dot, const lv_loading_grid_config_t *cfg,
                                        uint16_t delay_ms);
static void loading_grid_apply_host_style(lv_obj_t *host, lv_coord_t host_size,
                                          lv_coord_t pad, lv_coord_t dot_gap);
static lv_obj_t *lv_loading_grid_create_sized_with_config(
    lv_obj_t *parent, lv_coord_t size, const lv_loading_grid_config_t *cfg_in);
static lv_obj_t *lv_loading_grid_create_with_config(lv_obj_t *parent,
                                                    const lv_loading_grid_config_t *cfg);

lv_obj_t *lv_loading_grid_create(lv_obj_t *parent)
{
    return lv_loading_grid_create_with_config(parent, NULL);
}

lv_obj_t *lv_loading_grid_create_sized(lv_obj_t *parent, lv_coord_t size)
{
    lv_loading_grid_config_t cfg;

    lv_loading_grid_config_init(&cfg);
    return lv_loading_grid_create_sized_with_config(parent, size, &cfg);
}

void lv_loading_grid_set_opa(lv_obj_t *grid, lv_opa_t opa)
{
    if (!grid || !lv_obj_is_valid(grid)) {
        return;
    }

    lv_obj_set_style_opa(grid, opa, 0);
}

static lv_coord_t loading_grid_clamp_coord(lv_coord_t value, lv_coord_t min_value)
{
    return value < min_value ? min_value : value;
}

static void lv_loading_grid_config_init(lv_loading_grid_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->dot_size = LOADING_GRID_DEFAULT_DOT_SIZE;
    cfg->dot_gap = LOADING_GRID_DEFAULT_DOT_GAP;
    cfg->dot_color = lv_color_hex(LOADING_GRID_DEFAULT_DOT_COLOR);
    cfg->opa_min = LOADING_GRID_DEFAULT_OPA_MIN;
    cfg->opa_max = LOADING_GRID_DEFAULT_OPA_MAX;
    cfg->anim_time = LOADING_GRID_DEFAULT_ANIM_TIME;
    cfg->anim_playback_time = LOADING_GRID_DEFAULT_ANIM_PLAYBACK;
    cfg->delay_step = LOADING_GRID_DEFAULT_DELAY_STEP;
}

static void loading_grid_anim_opa_cb(void *var, int32_t v)
{
    lv_obj_t *dot = (lv_obj_t *)var;

    if (!dot || !lv_obj_is_valid(dot)) {
        return;
    }

    lv_obj_set_style_bg_opa(dot, (lv_opa_t)v, 0);
}

static void loading_grid_start_dot_anim(lv_obj_t *dot, const lv_loading_grid_config_t *cfg,
                                        uint16_t delay_ms)
{
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, dot);
    lv_anim_set_exec_cb(&a, loading_grid_anim_opa_cb);
    lv_anim_set_values(&a, cfg->opa_min, cfg->opa_max);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_time(&a, cfg->anim_time);
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_set_playback_time(&a, cfg->anim_playback_time);
    lv_anim_set_repeat_delay(&a, 0);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void loading_grid_apply_host_style(lv_obj_t *host, lv_coord_t host_size,
                                          lv_coord_t pad, lv_coord_t dot_gap)
{
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, host_size, host_size);
    lv_obj_set_layout(host, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(host, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(host, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(host, 0, 0);
    lv_obj_set_style_outline_width(host, 0, 0);
    lv_obj_set_style_shadow_width(host, 0, 0);
    lv_obj_set_style_pad_left(host, pad, 0);
    lv_obj_set_style_pad_right(host, pad, 0);
    lv_obj_set_style_pad_top(host, pad, 0);
    lv_obj_set_style_pad_bottom(host, pad, 0);
    lv_obj_set_style_pad_row(host, dot_gap, 0);
    lv_obj_set_style_pad_column(host, dot_gap, 0);
    lv_obj_set_scrollbar_mode(host, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(host, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *lv_loading_grid_create_sized_with_config(
    lv_obj_t *parent, lv_coord_t size, const lv_loading_grid_config_t *cfg_in)
{
    lv_loading_grid_config_t cfg_default;
    const lv_loading_grid_config_t *cfg = cfg_in;
    lv_coord_t grid_size;
    lv_coord_t host_size;
    lv_coord_t pad;
    lv_coord_t dot_size;
    lv_coord_t dot_gap;
    lv_obj_t *host;

    if (cfg == NULL) {
        lv_loading_grid_config_init(&cfg_default);
        cfg = &cfg_default;
    }

    dot_size = loading_grid_clamp_coord(cfg->dot_size, 2);
    dot_gap = loading_grid_clamp_coord(cfg->dot_gap, 0);
    grid_size = dot_size * LOADING_GRID_COL_COUNT + dot_gap * (LOADING_GRID_COL_COUNT - 1);
    host_size = loading_grid_clamp_coord(size, grid_size);
    pad = (host_size - grid_size) / 2;

    host = lv_obj_create(parent);
    loading_grid_apply_host_style(host, host_size, pad, dot_gap);

    for (uint32_t i = 0; i < LOADING_GRID_DOT_COUNT; i++) {
        uint16_t delay_ms = (uint16_t)(g_loading_grid_wave_group[i] * cfg->delay_step);
        lv_obj_t *dot = lv_obj_create(host);

        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, dot_size, dot_size);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, cfg->dot_color, 0);
        lv_obj_set_style_bg_opa(dot, cfg->opa_min, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_outline_width(dot, 0, 0);
        lv_obj_set_style_shadow_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_set_scrollbar_mode(dot, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        loading_grid_start_dot_anim(dot, cfg, delay_ms);
    }

    return host;
}

static lv_obj_t *lv_loading_grid_create_with_config(lv_obj_t *parent,
                                                    const lv_loading_grid_config_t *cfg)
{
    lv_loading_grid_config_t cfg_default;

    if (cfg == NULL) {
        lv_loading_grid_config_init(&cfg_default);
        cfg = &cfg_default;
    }

    return lv_loading_grid_create_sized_with_config(
        parent,
        (lv_coord_t)(cfg->dot_size * LOADING_GRID_COL_COUNT +
                     cfg->dot_gap * (LOADING_GRID_COL_COUNT - 1)),
        cfg);
}
