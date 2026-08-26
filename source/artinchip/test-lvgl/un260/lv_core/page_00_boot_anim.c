#include "un260/lv_core/page_00_boot_anim.h"

#include "un260/lv_core/lv_page_manager.h"

#include <stdint.h>
#include <string.h>

#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_B

#define BOOT_INTRO_WIDTH              1280
#define BOOT_INTRO_HEIGHT             400
#define BOOT_INTRO_HALF_HEIGHT        (BOOT_INTRO_HEIGHT / 2)
#define BOOT_INTRO_SIGNATURE_START_MS 450U
#define BOOT_INTRO_BREATH_START_MS    1800U
#define BOOT_INTRO_REVEAL_START_MS    6500U
#define BOOT_INTRO_TERMINAL_STOP_MS   7875U
#define BOOT_INTRO_FADE_DURATION_MS   320U
#define BOOT_INTRO_TIMER_MS           20U
#define BOOT_INTRO_BREATH_PERIOD_MS   2200U
#define BOOT_INTRO_MESSAGE_COUNT      4U
#define BOOT_INTRO_LOG_COUNT          22U
#define BOOT_INTRO_LOG_BUFFER_SIZE    768U
#define BOOT_INTRO_LOG_VISIBLE_LINES  8U
#define BOOT_INTRO_LOG_CHARS_PER_TICK 12U
#define BOOT_INTRO_CURSOR_BLINK_MS    600U
#define BOOT_INTRO_CORNER_COUNT       4U

typedef struct {
    lv_obj_t *root;
    lv_obj_t *top_panel;
    lv_obj_t *bottom_panel;
    lv_obj_t *brand_intro;
    lv_obj_t *brand_corner_h[BOOT_INTRO_CORNER_COUNT];
    lv_obj_t *brand_corner_v[BOOT_INTRO_CORNER_COUNT];
    lv_obj_t *message[BOOT_INTRO_MESSAGE_COUNT];
    lv_obj_t *meta_left;
    lv_obj_t *meta_right;
    lv_obj_t *terminal_view;
    lv_obj_t *terminal_log;
    lv_obj_t *terminal_cursor;
    lv_obj_t *rail;
    lv_obj_t *rail_fill;
    lv_timer_t *timer;
    uint32_t start_tick;
    uint32_t terminal_next_char_ms;
    uint32_t terminal_random_state;
    uint32_t terminal_line_index;
    uint32_t terminal_char_index;
    uint32_t terminal_completed_lines;
    uint32_t terminal_fade_start_ms;
    size_t terminal_text_length;
    bool terminal_complete;
    char terminal_text[BOOT_INTRO_LOG_BUFFER_SIZE];
} boot_intro_context_t;

static boot_intro_context_t g_boot_intro;

static const char *const g_boot_intro_message_text[BOOT_INTRO_MESSAGE_COUNT] = {
    "INTELLIGENT CASH MACHINE",
    "PRECISION IN EVERY COUNT",
    "ENGINEERED FOR TRUST",
    "UN260  /  SYSTEM READY",
};

static const char *const g_boot_intro_log_text[BOOT_INTRO_LOG_COUNT] = {
    "root@d213ecv:~# systemctl start cash-engine",
    "Starting Cash Counting Engine... [  OK  ]",
    "Initializing hardware interfaces... [  OK  ]",
    "Loading RISC-V kernel modules... [  OK  ]",
    "Initializing DDR controller... [  OK  ]",
    "Mounting NAND filesystem... [  OK  ]",
    "Starting network interface... [  OK  ]",
    "inet addr:127.0.0.1  Mask:255.0.0.0",
    "UP LOOPBACK RUNNING  MTU:65536  Metric:1",
    "RX packets:0 errors:0 dropped:0 overruns:0 frame:0",
    "TX packets:0 errors:0 dropped:0 overruns:0 carrier:0",
    "collisions:0 txqueuelen:1000",
    "RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)",
    "Probing banknote sensor array... [  OK  ]",
    "Calibrating feed and stacker motors... [  OK  ]",
    "Touch controller ONLINE [  OK  ]",
    "LVGL display pipeline READY [  OK  ]",
    "UART communication channels ONLINE [  OK  ]",
    "UN260 protocol frame FD DF synchronized [  OK  ]",
    "Loading currency recognition profiles... [  OK  ]",
    "Counter services READY [  OK  ]",
    "root@d213ecv:~# cash-engine --verify",
};

static float boot_intro_clamp(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float boot_intro_progress(uint32_t elapsed_ms, uint32_t start_ms,
                                 uint32_t duration_ms)
{
    if (duration_ms == 0U || elapsed_ms <= start_ms) return 0.0f;
    return boot_intro_clamp((float)(elapsed_ms - start_ms) /
                            (float)duration_ms, 0.0f, 1.0f);
}

static float boot_intro_ease_out_cubic(float value)
{
    float inverse = 1.0f - value;
    return 1.0f - inverse * inverse * inverse;
}

static float boot_intro_ease_in_out_cubic(float value)
{
    if (value < 0.5f) return 4.0f * value * value * value;
    value = -2.0f * value + 2.0f;
    return 1.0f - value * value * value / 2.0f;
}

static float boot_intro_ease_out_back(float value)
{
    const float overshoot = 1.15f;
    float shifted = value - 1.0f;

    return 1.0f + (overshoot + 1.0f) * shifted * shifted * shifted +
           overshoot * shifted * shifted;
}

static lv_opa_t boot_intro_opa(float value)
{
    return (lv_opa_t)(255.0f * boot_intro_clamp(value, 0.0f, 1.0f));
}

static float boot_intro_window_opacity(uint32_t elapsed_ms,
                                       uint32_t fade_in_start_ms,
                                       uint32_t fade_in_duration_ms,
                                       uint32_t fade_out_start_ms,
                                       uint32_t fade_out_duration_ms)
{
    float fade_in = boot_intro_ease_out_cubic(
        boot_intro_progress(elapsed_ms, fade_in_start_ms,
                            fade_in_duration_ms));
    float fade_out = boot_intro_progress(elapsed_ms, fade_out_start_ms,
                                         fade_out_duration_ms);
    return boot_intro_clamp(fade_in * (1.0f - fade_out), 0.0f, 1.0f);
}

static void boot_intro_set_text(lv_obj_t *label, lv_color_t color,
                                lv_opa_t opacity)
{
    if (label == NULL) return;
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, opacity, 0);
}

static void boot_intro_set_bar(lv_obj_t *bar, int x, int y, int width,
                               int height, lv_opa_t opacity)
{
    if (bar == NULL) return;
    if (width <= 0 || height <= 0 || opacity == LV_OPA_TRANSP) {
        if (!lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_obj_get_x(bar) != x || lv_obj_get_y(bar) != y) {
        lv_obj_set_pos(bar, x, y);
    }
    if (lv_obj_get_width(bar) != width ||
        lv_obj_get_height(bar) != height) {
        lv_obj_set_size(bar, width, height);
    }
    if (lv_obj_get_style_bg_opa(bar, 0) != opacity) {
        lv_obj_set_style_bg_opa(bar, opacity, 0);
    }
}

static lv_obj_t *boot_intro_create_panel(lv_obj_t *parent, int y)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 0, y);
    lv_obj_set_size(panel, BOOT_INTRO_WIDTH, BOOT_INTRO_HALF_HEIGHT);
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return panel;
}

static lv_obj_t *boot_intro_create_bar(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_style_bg_color(bar, color, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    return bar;
}

static lv_obj_t *boot_intro_create_label(lv_obj_t *parent, const char *text,
                                         const lv_font_t *font,
                                         int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, 0);
    return label;
}

static void boot_intro_set_backdrop_opacity(lv_opa_t opacity)
{
    lv_obj_set_style_bg_opa(g_boot_intro.top_panel, opacity, 0);
    lv_obj_set_style_bg_opa(g_boot_intro.bottom_panel, opacity, 0);
}

static void boot_intro_apply_messages(uint32_t elapsed_ms, float fade_scale)
{
    static const uint32_t fade_in_start[BOOT_INTRO_MESSAGE_COUNT] = {
        760U, 2350U, 3550U, 4800U,
    };
    static const uint32_t fade_out_start[BOOT_INTRO_MESSAGE_COUNT] = {
        2200U, 3400U, 4600U, 6200U,
    };
    static const uint32_t fade_in_duration[BOOT_INTRO_MESSAGE_COUNT] = {
        720U, 420U, 420U, 460U,
    };
    static const uint32_t fade_out_duration[BOOT_INTRO_MESSAGE_COUNT] = {
        420U, 420U, 420U, 300U,
    };
    lv_color_t silver = lv_color_hex(0xB8BDC5);
    uint32_t index;

    for (index = 0U; index < BOOT_INTRO_MESSAGE_COUNT; ++index) {
        float opacity;

        if (index == BOOT_INTRO_MESSAGE_COUNT - 1U) {
            opacity = boot_intro_ease_out_cubic(
                boot_intro_progress(elapsed_ms, fade_in_start[index],
                                    fade_in_duration[index]));
        } else {
            opacity = boot_intro_window_opacity(
                elapsed_ms, fade_in_start[index], fade_in_duration[index],
                fade_out_start[index], fade_out_duration[index]);
        }
        boot_intro_set_text(g_boot_intro.message[index], silver,
                            boot_intro_opa(opacity * fade_scale));
    }
}

static void boot_intro_apply_breath(uint32_t elapsed_ms, float opacity_scale)
{
    uint32_t breath_elapsed = elapsed_ms - BOOT_INTRO_BREATH_START_MS;
    uint32_t half_period = BOOT_INTRO_BREATH_PERIOD_MS / 2U;
    uint32_t phase = breath_elapsed % BOOT_INTRO_BREATH_PERIOD_MS;
    float pulse;
    float brightness;

    if (phase < half_period) {
        pulse = 1.0f - (float)phase / (float)half_period;
    } else {
        pulse = (float)(phase - half_period) / (float)half_period;
    }
    pulse = boot_intro_ease_in_out_cubic(pulse);
    brightness = 0.78f + 0.22f * pulse;
    boot_intro_set_text(g_boot_intro.brand_intro, lv_color_white(),
                        boot_intro_opa(brightness * opacity_scale));
}

static void boot_intro_apply_progress(uint32_t elapsed_ms, float fade_scale)
{
    float appear = boot_intro_ease_out_cubic(
        boot_intro_progress(elapsed_ms, 1500U, 500U));
    float fill = g_boot_intro.terminal_complete ? 1.0f :
        boot_intro_progress(elapsed_ms, 1500U,
                            BOOT_INTRO_TERMINAL_STOP_MS - 1500U) * 0.995f;
    int fill_width = (int)(480.0f * fill);

    boot_intro_set_bar(g_boot_intro.rail, 400, 310, 480, 1,
                       boot_intro_opa(appear * fade_scale * 0.22f));
    boot_intro_set_bar(g_boot_intro.rail_fill, 400, 310, fill_width, 2,
                       boot_intro_opa(appear * fade_scale));
}

static void boot_intro_apply_brand_corners(uint32_t elapsed_ms,
                                           float opacity_scale)
{
    static const int target_x[BOOT_INTRO_CORNER_COUNT] = {
        390, 890, 390, 890,
    };
    static const int target_y[BOOT_INTRO_CORNER_COUNT] = {
        90, 90, 193, 193,
    };
    const int center_x = 640;
    const int center_y = 142;
    float progress = boot_intro_ease_out_cubic(
        boot_intro_progress(elapsed_ms, BOOT_INTRO_SIGNATURE_START_MS, 1100U));
    float travel = boot_intro_ease_out_back(progress);
    float arm_progress = boot_intro_ease_out_cubic(progress);
    int arm_width = (int)(20.0f * arm_progress);
    int arm_height = (int)(20.0f * arm_progress);
    lv_opa_t opacity = boot_intro_opa(progress * opacity_scale * 0.70f);
    uint32_t index;

    for (index = 0U; index < BOOT_INTRO_CORNER_COUNT; ++index) {
        int anchor_x = center_x +
            (int)((float)(target_x[index] - center_x) * travel);
        int anchor_y = center_y +
            (int)((float)(target_y[index] - center_y) * travel);
        bool right = index == 1U || index == 3U;
        bool bottom = index >= 2U;
        int horizontal_x = right ? anchor_x - arm_width : anchor_x;
        int horizontal_y = bottom ? anchor_y - 5 : anchor_y;
        int vertical_x = right ? anchor_x - 5 : anchor_x;
        int vertical_y = bottom ? anchor_y - arm_height : anchor_y;

        boot_intro_set_bar(g_boot_intro.brand_corner_h[index],
                           horizontal_x, horizontal_y,
                           arm_width, 5, opacity);
        boot_intro_set_bar(g_boot_intro.brand_corner_v[index],
                           vertical_x, vertical_y,
                           5, arm_height, opacity);
    }
}

static uint32_t boot_intro_terminal_next_delay(void)
{
    g_boot_intro.terminal_random_state =
        g_boot_intro.terminal_random_state * 1664525U + 1013904223U;
    return 1U + ((g_boot_intro.terminal_random_state >> 24U) % 3U);
}

static void boot_intro_terminal_remove_oldest_line(void)
{
    char *next_line = strchr(g_boot_intro.terminal_text, '\n');
    size_t remove_length;

    if (next_line == NULL) return;
    remove_length = (size_t)(next_line - g_boot_intro.terminal_text) + 1U;
    memmove(g_boot_intro.terminal_text,
            g_boot_intro.terminal_text + remove_length,
            g_boot_intro.terminal_text_length - remove_length + 1U);
    g_boot_intro.terminal_text_length -= remove_length;
    if (g_boot_intro.terminal_completed_lines > 0U) {
        --g_boot_intro.terminal_completed_lines;
    }
}

static bool boot_intro_terminal_append(char character)
{
    if (g_boot_intro.terminal_text_length + 1U >=
        sizeof(g_boot_intro.terminal_text)) {
        return false;
    }
    g_boot_intro.terminal_text[g_boot_intro.terminal_text_length++] = character;
    g_boot_intro.terminal_text[g_boot_intro.terminal_text_length] = '\0';
    return true;
}

static void boot_intro_apply_terminal(uint32_t elapsed_ms)
{
    const lv_font_t *font = &lv_font_instrument_sans_medium_10;
    uint32_t emitted = 0U;
    bool text_changed = false;
    bool cursor_visible;
    const char *current_line;
    lv_coord_t cursor_x;
    lv_coord_t line_height;
    lv_opa_t cursor_opacity;
    float fade_scale;

    while (!g_boot_intro.terminal_complete &&
           elapsed_ms >= g_boot_intro.terminal_next_char_ms &&
           emitted < BOOT_INTRO_LOG_CHARS_PER_TICK) {
        const char *line = g_boot_intro_log_text[
            g_boot_intro.terminal_line_index % BOOT_INTRO_LOG_COUNT];
        char character = line[g_boot_intro.terminal_char_index];

        if (character != '\0') {
            if (boot_intro_terminal_append(character)) {
                ++g_boot_intro.terminal_char_index;
                text_changed = true;
            }
            g_boot_intro.terminal_next_char_ms +=
                boot_intro_terminal_next_delay();
        } else if (elapsed_ms >= BOOT_INTRO_TERMINAL_STOP_MS) {
            g_boot_intro.terminal_complete = true;
            g_boot_intro.terminal_fade_start_ms = elapsed_ms;
            break;
        } else {
            if (boot_intro_terminal_append('\n')) {
                ++g_boot_intro.terminal_completed_lines;
                text_changed = true;
            }
            while (g_boot_intro.terminal_completed_lines >=
                   BOOT_INTRO_LOG_VISIBLE_LINES) {
                boot_intro_terminal_remove_oldest_line();
            }
            g_boot_intro.terminal_line_index =
                (g_boot_intro.terminal_line_index + 1U) %
                BOOT_INTRO_LOG_COUNT;
            g_boot_intro.terminal_char_index = 0U;
            g_boot_intro.terminal_next_char_ms +=
                5U + boot_intro_terminal_next_delay();
        }
        ++emitted;
    }

    fade_scale = g_boot_intro.terminal_complete ?
        1.0f - boot_intro_ease_in_out_cubic(
            boot_intro_progress(elapsed_ms,
                                g_boot_intro.terminal_fade_start_ms,
                                BOOT_INTRO_FADE_DURATION_MS)) :
        1.0f;

    cursor_visible = ((elapsed_ms / BOOT_INTRO_CURSOR_BLINK_MS) % 2U) == 0U;
    if (text_changed) {
        lv_label_set_text(g_boot_intro.terminal_log,
                          g_boot_intro.terminal_text);
    }

    current_line = strrchr(g_boot_intro.terminal_text, '\n');
    current_line = current_line != NULL ? current_line + 1 :
                                          g_boot_intro.terminal_text;
    cursor_x = (lv_coord_t)lv_txt_get_width(
        current_line, (uint32_t)strlen(current_line), font, 1,
        LV_TEXT_FLAG_NONE);
    line_height = lv_font_get_line_height(font) + 2;
    lv_obj_set_pos(g_boot_intro.terminal_cursor, cursor_x + 1,
                   (lv_coord_t)g_boot_intro.terminal_completed_lines *
                   line_height);
    lv_obj_set_size(g_boot_intro.terminal_cursor, 5, line_height - 2);
    cursor_opacity = cursor_visible ?
        boot_intro_opa(fade_scale * 0.30f) : LV_OPA_TRANSP;
    if (lv_obj_get_style_bg_opa(g_boot_intro.terminal_cursor, 0) !=
        cursor_opacity) {
        lv_obj_set_style_bg_opa(g_boot_intro.terminal_cursor,
                                cursor_opacity, 0);
    }
    boot_intro_set_text(g_boot_intro.terminal_log,
                        lv_color_white(),
                        boot_intro_opa(fade_scale * 0.50f));
}

static void boot_intro_apply_opening(uint32_t elapsed_ms)
{
    float progress = boot_intro_ease_out_cubic(
        boot_intro_progress(elapsed_ms, 50U, 400U));
    lv_color_t dim = lv_color_hex(0x686E77);

    boot_intro_set_backdrop_opacity(LV_OPA_COVER);
    boot_intro_set_text(g_boot_intro.meta_left, dim,
                        boot_intro_opa(progress * 0.62f));
    boot_intro_set_text(g_boot_intro.meta_right, dim,
                        boot_intro_opa(progress * 0.62f));
    boot_intro_set_text(g_boot_intro.brand_intro, lv_color_white(),
                        boot_intro_opa(progress * 0.04f));
    boot_intro_apply_brand_corners(elapsed_ms, 1.0f);
    boot_intro_apply_messages(elapsed_ms, 1.0f);
}

static void boot_intro_apply_signature(uint32_t elapsed_ms)
{
    float progress = boot_intro_ease_out_cubic(
        boot_intro_progress(elapsed_ms, BOOT_INTRO_SIGNATURE_START_MS, 1150U));
    lv_color_t dim = lv_color_hex(0x686E77);

    boot_intro_set_backdrop_opacity(LV_OPA_COVER);
    lv_obj_set_pos(g_boot_intro.brand_intro, 290, 116);
    boot_intro_set_text(g_boot_intro.brand_intro, lv_color_white(),
                        boot_intro_opa(progress));
    boot_intro_set_text(g_boot_intro.meta_left, dim, boot_intro_opa(0.62f));
    boot_intro_set_text(g_boot_intro.meta_right, dim, boot_intro_opa(0.62f));
    boot_intro_apply_brand_corners(elapsed_ms, 1.0f);
    boot_intro_apply_messages(elapsed_ms, 1.0f);
    boot_intro_apply_progress(elapsed_ms, 1.0f);
}

static void boot_intro_apply_showcase(uint32_t elapsed_ms)
{
    lv_color_t dim = lv_color_hex(0x686E77);

    boot_intro_set_backdrop_opacity(LV_OPA_COVER);
    lv_obj_set_pos(g_boot_intro.message[0], 340, 236);
    lv_obj_set_pos(g_boot_intro.brand_intro, 290, 116);
    boot_intro_apply_breath(elapsed_ms, 1.0f);
    boot_intro_set_text(g_boot_intro.meta_left, dim, boot_intro_opa(0.62f));
    boot_intro_set_text(g_boot_intro.meta_right, dim, boot_intro_opa(0.62f));
    boot_intro_apply_brand_corners(elapsed_ms, 1.0f);
    boot_intro_apply_messages(elapsed_ms, 1.0f);
    boot_intro_apply_progress(elapsed_ms, 1.0f);
}

static void boot_intro_apply_reveal(uint32_t elapsed_ms)
{
    float reveal_progress = g_boot_intro.terminal_complete ?
        boot_intro_ease_in_out_cubic(
            boot_intro_progress(elapsed_ms,
                                g_boot_intro.terminal_fade_start_ms,
                                BOOT_INTRO_FADE_DURATION_MS)) :
        0.0f;
    float foreground = 1.0f - reveal_progress;
    lv_color_t dim = lv_color_hex(0x686E77);

    boot_intro_apply_breath(elapsed_ms, foreground);
    boot_intro_apply_messages(elapsed_ms, foreground);
    boot_intro_set_text(g_boot_intro.meta_left, dim,
                        boot_intro_opa(foreground * 0.62f));
    boot_intro_set_text(g_boot_intro.meta_right, dim,
                        boot_intro_opa(foreground * 0.62f));
    boot_intro_apply_brand_corners(elapsed_ms, foreground);
    boot_intro_apply_progress(elapsed_ms, foreground);
    boot_intro_set_backdrop_opacity(boot_intro_opa(1.0f - reveal_progress));
}

static void boot_intro_stop_timer(void)
{
    lv_timer_t *timer = g_boot_intro.timer;
    g_boot_intro.timer = NULL;
    if (timer != NULL) lv_timer_del(timer);
}

static void boot_intro_finish(void)
{
    bool registered_page =
        ui_manager_get_current_page() == UI_PAGE_BOOT_ANIM;

    boot_intro_stop_timer();
    if (registered_page) {
        ui_manager_switch(UI_PAGE_BOOT);
    } else {
        ui_page_00_boot_anim_destroy();
    }
}

static void boot_intro_timer_cb(lv_timer_t *timer)
{
    uint32_t elapsed_ms;

    LV_UNUSED(timer);
    if (!ui_page_00_boot_anim_is_active()) return;
    elapsed_ms = lv_tick_elaps(g_boot_intro.start_tick);
    boot_intro_apply_terminal(elapsed_ms);

    if (elapsed_ms < BOOT_INTRO_SIGNATURE_START_MS) {
        boot_intro_apply_opening(elapsed_ms);
    } else if (elapsed_ms < BOOT_INTRO_BREATH_START_MS) {
        boot_intro_apply_signature(elapsed_ms);
    } else if (elapsed_ms < BOOT_INTRO_REVEAL_START_MS) {
        boot_intro_apply_showcase(elapsed_ms);
    } else if (!g_boot_intro.terminal_complete ||
               elapsed_ms < g_boot_intro.terminal_fade_start_ms +
                            BOOT_INTRO_FADE_DURATION_MS) {
        boot_intro_apply_reveal(elapsed_ms);
    } else {
        boot_intro_finish();
    }
}

void ui_page_00_boot_anim_create(lv_obj_t *parent)
{
    uint32_t index;

    if (ui_page_00_boot_anim_is_active()) return;
    ui_page_00_boot_anim_destroy();
    if (parent == NULL) parent = lv_layer_top();

    g_boot_intro.root = lv_obj_create(parent);
    lv_obj_remove_style_all(g_boot_intro.root);
    lv_obj_set_pos(g_boot_intro.root, 0, 0);
    lv_obj_set_size(g_boot_intro.root, BOOT_INTRO_WIDTH, BOOT_INTRO_HEIGHT);
    lv_obj_set_style_bg_opa(g_boot_intro.root, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_boot_intro.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_boot_intro.root, LV_OBJ_FLAG_CLICKABLE);

    g_boot_intro.top_panel = boot_intro_create_panel(g_boot_intro.root, 0);
    g_boot_intro.bottom_panel = boot_intro_create_panel(
        g_boot_intro.root, BOOT_INTRO_HALF_HEIGHT);

    g_boot_intro.brand_intro = boot_intro_create_label(
        g_boot_intro.root, "UN260", &lv_font_instrument_sans_bold_48,
        290, 116, 700);
    lv_obj_set_style_text_letter_space(g_boot_intro.brand_intro, 12, 0);
    for (index = 0U; index < BOOT_INTRO_CORNER_COUNT; ++index) {
        g_boot_intro.brand_corner_h[index] = boot_intro_create_bar(
            g_boot_intro.root, lv_color_white());
        g_boot_intro.brand_corner_v[index] = boot_intro_create_bar(
            g_boot_intro.root, lv_color_white());
    }

    for (index = 0U; index < BOOT_INTRO_MESSAGE_COUNT; ++index) {
        g_boot_intro.message[index] = boot_intro_create_label(
            g_boot_intro.root, g_boot_intro_message_text[index],
            &lv_font_instrument_sans_medium_14, 340, 236, 600);
        lv_obj_set_style_text_letter_space(g_boot_intro.message[index], 4, 0);
    }

    g_boot_intro.meta_left = boot_intro_create_label(
        g_boot_intro.root, "UN / 260   SIGNATURE SERIES",
        &lv_font_instrument_sans_medium_10, 26, 20, 260);
    lv_obj_set_style_text_align(g_boot_intro.meta_left, LV_TEXT_ALIGN_LEFT, 0);
    g_boot_intro.meta_right = boot_intro_create_label(
        g_boot_intro.root, "PRECISION CASH ENGINE",
        &lv_font_instrument_sans_medium_10, 1010, 20, 240);
    lv_obj_set_style_text_align(g_boot_intro.meta_right, LV_TEXT_ALIGN_RIGHT, 0);

    g_boot_intro.terminal_view = lv_obj_create(g_boot_intro.root);
    lv_obj_remove_style_all(g_boot_intro.terminal_view);
    lv_obj_set_pos(g_boot_intro.terminal_view, 26, 270);
    lv_obj_set_size(g_boot_intro.terminal_view, 365, 112);
    lv_obj_clear_flag(g_boot_intro.terminal_view,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
                      LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    g_boot_intro.terminal_log = boot_intro_create_label(
        g_boot_intro.terminal_view, "", &lv_font_instrument_sans_medium_10,
        0, 0, 365);
    lv_obj_set_style_text_align(g_boot_intro.terminal_log,
                                LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_letter_space(g_boot_intro.terminal_log, 1, 0);
    lv_obj_set_style_text_line_space(g_boot_intro.terminal_log, 2, 0);
    g_boot_intro.terminal_cursor = boot_intro_create_bar(
        g_boot_intro.terminal_view, lv_color_hex(0xB8BDC5));
    lv_obj_clear_flag(g_boot_intro.terminal_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(g_boot_intro.terminal_cursor, 356, 98);
    lv_obj_set_size(g_boot_intro.terminal_cursor, 5, 9);
    lv_obj_set_style_bg_opa(g_boot_intro.terminal_cursor,
                            LV_OPA_TRANSP, 0);
    g_boot_intro.terminal_random_state = 0x260D213U;

    g_boot_intro.rail = boot_intro_create_bar(
        g_boot_intro.root, lv_color_hex(0x626872));
    g_boot_intro.rail_fill = boot_intro_create_bar(
        g_boot_intro.root, lv_color_white());

    lv_obj_move_foreground(g_boot_intro.root);
    g_boot_intro.start_tick = lv_tick_get();
    g_boot_intro.timer = lv_timer_create(boot_intro_timer_cb,
                                         BOOT_INTRO_TIMER_MS, NULL);
    if (g_boot_intro.timer == NULL) boot_intro_finish();
}

void ui_page_00_boot_anim_destroy(void)
{
    boot_intro_stop_timer();
    if (g_boot_intro.root != NULL &&
        lv_obj_is_valid(g_boot_intro.root)) {
        lv_obj_del(g_boot_intro.root);
    }
    memset(&g_boot_intro, 0, sizeof(g_boot_intro));
}

bool ui_page_00_boot_anim_is_active(void)
{
    return g_boot_intro.root != NULL &&
           lv_obj_is_valid(g_boot_intro.root);
}

#endif /* UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_B */
