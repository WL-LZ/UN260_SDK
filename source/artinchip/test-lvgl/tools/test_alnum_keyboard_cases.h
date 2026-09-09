#ifndef TEST_ALNUM_KEYBOARD_CASES_H
#define TEST_ALNUM_KEYBOARD_CASES_H

/* Include in the actual LVGL host test after its display has been registered.
 * Call test_alnum_keyboard_cases() while other UI animations are idle. */
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include <assert.h>
#include <string.h>

typedef struct {
    unsigned submits, cancels, backs;
    char submitted[LV_ALNUM_KEYBOARD_MAX_TEXT + 1];
    lv_obj_t *owner;
    bool delete_on_callback;
} alnum_test_context_t;

static void alnum_test_submit(const char *text, void *context)
{
    alnum_test_context_t *state = context;
    ++state->submits;
    assert(strlen(text) <= LV_ALNUM_KEYBOARD_MAX_TEXT);
    strcpy(state->submitted, text);
    if (state->delete_on_callback) {
        lv_obj_del(state->owner);
        state->owner = NULL;
    }
}

static void alnum_test_cancel(void *context)
{
    alnum_test_context_t *state = context;
    ++state->cancels;
    if (state->delete_on_callback) {
        lv_obj_del(state->owner);
        state->owner = NULL;
    }
}

static void alnum_test_back(lv_event_t *event)
{ ++((alnum_test_context_t *)lv_event_get_user_data(event))->backs; }

static lv_alnum_keyboard_config_t alnum_test_config(alnum_test_context_t *context)
{
    const lv_alnum_keyboard_config_t config = {
        .title = "Serial number", .placeholder = "Type letters or numbers",
        .apply_text = "Apply", .clear_text = "Clear", .cancel_text = "Cancel",
        .max_length = 32, .submit = alnum_test_submit, .cancel = alnum_test_cancel,
        .context = context
    };
    return config;
}

static lv_obj_t *alnum_test_owner(void)
{
    lv_obj_t *owner = lv_obj_create(lv_scr_act());
    assert(owner);
    lv_obj_remove_style_all(owner);
    lv_obj_set_pos(owner, 0, 0);
    lv_obj_set_size(owner, 1280, 400);
    lv_obj_clear_flag(owner, LV_OBJ_FLAG_SCROLLABLE);
    return owner;
}

static lv_obj_t *alnum_test_find_key(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_btn_class)) {
        lv_obj_t *label = lv_damped_button_get_label(root);
        if (label && !strcmp(lv_label_get_text(label), text)) return root;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(root); ++i) {
        lv_obj_t *found = alnum_test_find_key(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}

static void alnum_test_click(lv_obj_t *root, const char *text)
{
    lv_obj_t *button = alnum_test_find_key(root, text);
    assert(button);
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
}

static unsigned alnum_test_timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer; timer = lv_timer_get_next(timer)) ++count;
    return count;
}

static void alnum_test_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    data->state = LV_INDEV_STATE_REL;
    data->point.x = data->point.y = 0;
}

static void test_alnum_keyboard_cases(void)
{
    unsigned base_timers = alnum_test_timer_count();
    uint16_t base_anims = lv_anim_count_running();
    alnum_test_context_t state = { 0 };
    state.owner = alnum_test_owner();
    lv_alnum_keyboard_config_t config = alnum_test_config(&state);
    assert(!lv_alnum_keyboard_create(NULL, &config));
    config.max_length = 0;
    assert(!lv_alnum_keyboard_create(state.owner, &config));
    config.max_length = 33;
    assert(!lv_alnum_keyboard_create(state.owner, &config));
    config.max_length = 32;
    assert(lv_nav_button_create(state.owner, 0, 0, 60, 36, alnum_test_back, &state));
    lv_alnum_keyboard_t *keyboard = lv_alnum_keyboard_create(state.owner, &config);
    assert(keyboard && !lv_alnum_keyboard_is_visible(keyboard));
    assert(!alnum_test_find_key(state.owner, "#+="));
    assert(alnum_test_timer_count() == base_timers);
    assert(lv_alnum_keyboard_set_text(keyboard, "ab09"));
    lv_alnum_keyboard_show(keyboard);
    lv_obj_update_layout(state.owner);
    assert(lv_alnum_keyboard_is_visible(keyboard));
    alnum_test_click(state.owner, "K");
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "ab09K"));
    alnum_test_click(state.owner, LV_SYMBOL_BACKSPACE);
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "ab09"));
    assert(!lv_alnum_keyboard_set_text(keyboard, "bad\ntext"));
    assert(!lv_alnum_keyboard_set_text(keyboard, "\xC3\xA9"));
    assert(!lv_alnum_keyboard_set_text(keyboard, NULL));
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "ab09"));
    assert(lv_alnum_keyboard_set_text(keyboard, "12345678901234567890123456789012"));
    alnum_test_click(state.owner, "A");
    assert(strlen(lv_alnum_keyboard_get_text(keyboard)) == 32);
    assert(!lv_alnum_keyboard_set_text(keyboard, "123456789012345678901234567890123"));
    assert(strlen(lv_alnum_keyboard_get_text(keyboard)) == 32);
    alnum_test_click(state.owner, "Clear");
    alnum_test_click(state.owner, LV_SYMBOL_BACKSPACE);
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), ""));
    assert(lv_alnum_keyboard_set_text(keyboard, "UNCHANGED"));
    alnum_test_click(state.owner, "Apply");
    assert(state.submits == 1 && !strcmp(state.submitted, "UNCHANGED"));
    assert(!lv_alnum_keyboard_is_visible(keyboard));
    lv_alnum_keyboard_show(keyboard);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(state.cancels == 1 && state.submits == 1 && state.backs == 0);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED && state.backs == 1);
    lv_alnum_keyboard_show(keyboard);
    lv_alnum_keyboard_hide(keyboard);
    assert(state.cancels == 1 && state.submits == 1);

    /* Active hold cancellation and late release cannot restart hidden work. */
    lv_indev_drv_t driver;
    lv_indev_drv_init(&driver);
    driver.type = LV_INDEV_TYPE_POINTER;
    driver.read_cb = alnum_test_read;
    lv_indev_t *indev = lv_indev_drv_register(&driver);
    assert(indev);
    lv_alnum_keyboard_show(keyboard);
    lv_obj_t *key = alnum_test_find_key(state.owner, "A");
    assert(key);
    lv_event_send(key, LV_EVENT_PRESSED, indev);
    lv_tick_inc(40); lv_timer_handler();
    lv_alnum_keyboard_hide(keyboard);
    assert(indev->proc.wait_until_release);
    assert(lv_anim_count_running() == base_anims);
    lv_event_send(key, LV_EVENT_PRESS_LOST, indev);
    lv_event_send(key, LV_EVENT_RELEASED, indev);
    assert(lv_anim_count_running() == base_anims);
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "UNCHANGED"));
    lv_indev_delete(indev);
    lv_alnum_keyboard_destroy(keyboard);
    lv_obj_del(state.owner);
    assert(alnum_test_timer_count() == base_timers && lv_anim_count_running() == base_anims);

    /* Opt-in symbols retain input while switching pages and expose every
     * delimiter/comparator needed by numeric and calendar fields. */
    memset(&state, 0, sizeof(state)); state.owner = alnum_test_owner();
    config = alnum_test_config(&state); config.symbols = true;
    keyboard = lv_alnum_keyboard_create(state.owner, &config);
    assert(keyboard);
    lv_alnum_keyboard_show(keyboard); lv_obj_update_layout(state.owner);
    alnum_test_click(state.owner, "A");
    alnum_test_click(state.owner, "#+=");
    static const char symbols[] = ".:-<>=";
    for (unsigned i = 0; i < sizeof(symbols) - 1; ++i) {
        char text[2] = { symbols[i], '\0' };
        alnum_test_click(state.owner, text);
    }
    alnum_test_click(state.owner, "ABC");
    alnum_test_click(state.owner, "B");
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "A.:-<>=B"));
    alnum_test_click(state.owner, "Clear");
    alnum_test_click(state.owner, "#+=");
    alnum_test_click(state.owner, ">"); alnum_test_click(state.owner, "=");
    alnum_test_click(state.owner, "1"); alnum_test_click(state.owner, "0");
    alnum_test_click(state.owner, "0");
    assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), ">=100"));
    alnum_test_click(state.owner, "Apply");
    assert(state.submits == 1 && !strcmp(state.submitted, ">=100"));
    alnum_test_click(state.owner, "ABC"); /* Hidden late events cannot switch pages. */
    assert(alnum_test_find_key(state.owner, "ABC"));
    lv_obj_del(state.owner);
    assert(alnum_test_timer_count() == base_timers && lv_anim_count_running() == base_anims);

    /* Both callback kinds may synchronously destroy the complete owner. */
    for (unsigned cancel = 0; cancel < 2; ++cancel) {
        memset(&state, 0, sizeof(state));
        state.owner = alnum_test_owner(); state.delete_on_callback = true;
        config = alnum_test_config(&state);
        keyboard = lv_alnum_keyboard_create(state.owner, &config);
        assert(keyboard);
        assert(lv_alnum_keyboard_set_text(keyboard, "SAFE"));
        lv_alnum_keyboard_show(keyboard); lv_obj_update_layout(state.owner);
        alnum_test_click(state.owner, cancel ? "Cancel" : "Apply");
        assert(!state.owner);
        assert(cancel ? state.cancels == 1 && state.submits == 0 :
                        state.submits == 1 && !strcmp(state.submitted, "SAFE"));
        assert(alnum_test_timer_count() == base_timers && lv_anim_count_running() == base_anims);
    }
    for (unsigned repeat = 0; repeat < 5; ++repeat) {
        memset(&state, 0, sizeof(state)); state.owner = alnum_test_owner();
        config = alnum_test_config(&state); config.max_length = 3;
        keyboard = lv_alnum_keyboard_create(state.owner, &config);
        assert(keyboard);
        assert(lv_alnum_keyboard_set_text(keyboard, "AB"));
        lv_alnum_keyboard_show(keyboard); lv_obj_update_layout(state.owner);
        alnum_test_click(state.owner, "C"); alnum_test_click(state.owner, "D");
        assert(!strcmp(lv_alnum_keyboard_get_text(keyboard), "ABC"));
        key = alnum_test_find_key(state.owner, "A");
        lv_event_send(key, LV_EVENT_PRESSED, NULL);
        lv_obj_del(state.owner);
        assert(!state.submits && !state.cancels);
        assert(alnum_test_timer_count() == base_timers && lv_anim_count_running() == base_anims);
    }
}

#endif
