#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_core/ui_frame_commit.h"
/* Inspect the real page's private projection without firmware-only accessors. */
#include "un260/lv_core/page_03_menu.c"

/* Host boundaries. A successful request means queued, NEVER confirmed.
 * Tests explicitly deliver an ACK/result later. Transport retries/timeouts,
 * controller protocol semantics, DMA/GE, evdev and navigation remain board QA. */
static bool accept_request = true;
static unsigned requests[7], home_requests, main_refreshes;
static unsigned request_value[7];
static setting_batch_result_t pending_batch;
static bool request(unsigned kind, unsigned value)
{ ++requests[kind]; request_value[kind] = value; return accept_request; }
bool setting_service_request_beep(bool value) { return request(0, value); }
bool setting_service_request_speed(uint8_t value) { return request(1, value); }
bool setting_service_request_add(bool value) { return request(2, value); }
bool setting_service_request_fo_mode(uint8_t value) { return request(3, value); }
bool setting_service_request_work_mode(uint8_t value) { return request(4, value); }
bool setting_service_request_batch_number(uint8_t value, bool previous_enable, uint8_t previous_num)
{
    pending_batch = (setting_batch_result_t){SETTING_BATCH_REQUEST_NUMBER,
        {previous_enable, value}, {previous_enable, previous_num}};
    return request(5, value);
}
bool setting_service_request_batch_switch(bool enabled, uint8_t value, bool previous_enable, uint8_t previous_num)
{
    pending_batch = (setting_batch_result_t){SETTING_BATCH_REQUEST_SWITCH,
        {enabled, value}, {previous_enable, previous_num}};
    return request(6, value);
}
void ui_manager_switch(ui_page_t page) { assert(page == UI_PAGE_MAIN); ++home_requests; }
void page_01_batch_refre(void) { ++main_refreshes; }
bool perf_profile_is_enabled(void) { return false; }
void perf_profile_report_event_us(const char *page, const char *event, uint32_t elapsed)
{ (void)page; (void)event; (void)elapsed; }
uint64_t app_clock_monotonic_us(void) { return (uint64_t)lv_tick_get() * 1000; }
uint32_t app_clock_elapsed_us32(uint64_t begin, uint64_t end) { return (uint32_t)(end - begin); }
bool lv_dma_static_surface_attach(lv_dma_static_surface_t *s, lv_obj_t *o, const char *key)
{ (void)s; (void)o; (void)key; return false; }
bool lv_dma_static_skin_attach(lv_dma_static_skin_t *s, lv_obj_t *o, const char *key)
{ (void)s; (void)o; (void)key; return false; }
void lv_dma_static_surface_release(lv_dma_static_surface_t *s) { memset(s, 0, sizeof(*s)); }
void lv_dma_static_skin_release(lv_dma_static_skin_t *s) { memset(s, 0, sizeof(*s)); }

static lv_color_t framebuffer[1280 * 400], draw_buffer[1280 * 40];
static unsigned opened_assets;
static lv_point_t pointer_point;
static lv_indev_state_t pointer_state = LV_INDEV_STATE_RELEASED;
static lv_indev_t *input_device;
static void pointer_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{ (void)driver; data->point = pointer_point; data->state = pointer_state; }
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    assert(area->x1 >= 0 && area->x2 < 1280 && area->y1 >= 0 && area->y2 < 400);
    int width = lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(framebuffer + y * 1280 + area->x1, pixels + (y - area->y1) * width, width * sizeof(*pixels));
    lv_disp_flush_ready(driver);
}
static void *fs_open(lv_fs_drv_t *driver, const char *path, lv_fs_mode_t mode)
{
    (void)driver;
    assert(mode == LV_FS_MODE_RD);
    const char *prefix = "/usr/local/share/lvgl_data/";
    if (strncmp(path, prefix, strlen(prefix))) return NULL;
    const char *relative = path + strlen(prefix);
    assert(!strstr(relative, ".."));
    char file[1024]; snprintf(file, sizeof(file), "%s/%s", getenv("MENU_ASSET_ROOT"), relative);
    FILE *stream = fopen(file, "rb");
    if (!stream) fprintf(stderr, "Missing actual Menu asset: %s\n", file);
    else ++opened_assets;
    return stream;
}
static lv_fs_res_t fs_close(lv_fs_drv_t *driver, void *file)
{ (void)driver; return fclose(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK; }
static lv_fs_res_t fs_read(lv_fs_drv_t *driver, void *file, void *buffer, uint32_t count, uint32_t *read)
{ (void)driver; *read = (uint32_t)fread(buffer, 1, count, file); return ferror(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK; }
static lv_fs_res_t fs_seek(lv_fs_drv_t *driver, void *file, uint32_t pos, lv_fs_whence_t from)
{ (void)driver; return fseek(file, (long)pos, from == LV_FS_SEEK_SET ? SEEK_SET : from == LV_FS_SEEK_CUR ? SEEK_CUR : SEEK_END) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK; }
static lv_fs_res_t fs_tell(lv_fs_drv_t *driver, void *file, uint32_t *pos)
{ (void)driver; long offset = ftell(file); if (offset < 0) return LV_FS_RES_FS_ERR; *pos = (uint32_t)offset; return LV_FS_RES_OK; }
static void tick(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 20) { lv_tick_inc(20); lv_timer_handler(); } }
static void render(void)
{ lv_obj_update_layout(lv_scr_act()); lv_obj_invalidate(lv_scr_act()); lv_refr_now(NULL); }
static void write_bmp(const char *name)
{
    render();
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.bmp", getenv("MENU_RASTER_OUTPUT"), name);
    FILE *file = fopen(path, "wb"); assert(file);
    uint8_t header[54] = {0};
    uint32_t size = 54 + sizeof(framebuffer), offset = 54, dib = 40, width = 1280;
    int32_t height = -400; uint16_t planes = 1, bits = 32;
    header[0] = 'B'; header[1] = 'M'; memcpy(header + 2, &size, 4); memcpy(header + 10, &offset, 4);
    memcpy(header + 14, &dib, 4); memcpy(header + 18, &width, 4); memcpy(header + 22, &height, 4);
    memcpy(header + 26, &planes, 2); memcpy(header + 28, &bits, 2);
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(framebuffer, 1, sizeof(framebuffer), file) == sizeof(framebuffer));
    fclose(file);
    printf("RASTER %s\n", path);
}
static unsigned timer_count(void)
{ unsigned n = 0; for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++n; return n; }
static lv_obj_t *named(const char *name)
{ lv_obj_t *object = page_03_menu_find_obj(name); assert(object && lv_obj_is_valid(object)); return object; }
static void tap(lv_obj_t *object)
{
    assert(object && lv_obj_is_visible(object));
    lv_obj_update_layout(menu_page);
    lv_area_t area; lv_obj_get_coords(object, &area);
    pointer_point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    pointer_state = LV_INDEV_STATE_PRESSED; tick(80);
    pointer_state = LV_INDEV_STATE_RELEASED; tick(280);
}
static void click(const char *name) { tap(named(name)); }
static void digits(const char *text)
{ for (; *text; ++text) { char name[] = "key_0"; name[4] = *text; click(name); } }
static void edit_is(int value, bool present)
{
    int actual = -1; bool has_edit = page_03_batch_num_edit_value(&actual);
    if (has_edit != present || (present && actual != value))
        fprintf(stderr, "Edit expected %d/%d, actual %d/%d\n", present, value, has_edit, actual);
    assert(has_edit == present); if (present) assert(actual == value);
}
static bool overlap(const lv_area_t *a, const lv_area_t *b)
{ return a->x1 <= b->x2 && b->x1 <= a->x2 && a->y1 <= b->y2 && b->y1 <= a->y2; }
static void check_layout(void)
{
    static const char *const names[] = {
        "03_home_btn", "key_1", "key_2", "key_3", "key_4", "key_5", "key_6", "key_7",
        "key_8", "key_9", "key_0", "key_del", "key_enter", "03_beep_off_btn", "03_beep_on_btn",
        "03_speed_800_btn", "03_speed_1000_btn", "03_speed_1200_btn", "03_add_off_btn", "03_add_on_btn",
        "03_fo_OFF_btn", "03_fo_F_btn", "03_fo_O_btn", "03_fo_FO_btn", "03_work_auto_btn", "03_work_manaul_btn"
    };
    lv_area_t areas[27];
    lv_obj_update_layout(menu_page);
    assert(sizeof(names) / sizeof(*names) == 26);
    for (unsigned i = 0; i < 27; ++i) {
        lv_obj_t *object = i == 26 ? get_batch_switch_container() : named(names[i]);
        assert(object && lv_obj_is_visible(object) && lv_obj_has_flag(object, LV_OBJ_FLAG_CLICKABLE));
        lv_obj_get_coords(object, &areas[i]);
        if (areas[i].x1 < 0 || areas[i].x2 >= 1280 || areas[i].y1 < 0 || areas[i].y2 >= 400)
            fprintf(stderr, "Control out of bounds: %s\n", i == 26 ? "batch switch" : names[i]);
        assert(areas[i].x1 >= 0 && areas[i].x2 < 1280 && areas[i].y1 >= 0 && areas[i].y2 < 400);
        for (unsigned j = 0; j < i; ++j) {
            if (overlap(&areas[i], &areas[j])) fprintf(stderr, "Controls overlap: %u / %u\n", i, j);
            assert(!overlap(&areas[i], &areas[j]));
        }
        for (unsigned j = 0; j < lv_obj_get_child_cnt(object); ++j) {
            lv_obj_t *child = lv_obj_get_child(object, j);
            if (!lv_obj_check_type(child, &lv_label_class) || !lv_obj_is_visible(child)) continue;
            lv_point_t size;
            lv_txt_get_size(&size, lv_label_get_text(child), lv_obj_get_style_text_font(child, 0),
                lv_obj_get_style_text_letter_space(child, 0), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            assert(size.x <= lv_obj_get_width(object) && size.y <= lv_obj_get_height(object));
        }
    }
}
static void test_batch(void)
{
    int saved = machine_state_batch_num();
    edit_is(0, false); digits("00"); edit_is(0, true);
    click("key_enter"); assert(request_value[5] == 200 && machine_state_batch_num() == saved);
    edit_is(0, false); assert(g_batch_tip_label == NULL); write_bmp("menu-pending");
    page_03_batch_set_result(false, &pending_batch); assert(machine_state_batch_num() == saved && !g_batch_tip_label);
    digits("2019"); edit_is(200, true); assert(!strcmp(lv_label_get_text(g_batch_num_display), "200"));
    write_bmp("menu-edit-200"); click("key_del"); edit_is(0, false);
    digits("17"); accept_request = false; click("key_enter"); edit_is(17, true);
    assert(machine_state_batch_num() == saved && !g_batch_tip_label);
    accept_request = true; click("key_enter"); edit_is(0, false);
    assert(request_value[5] == 17 && machine_state_batch_num() == saved);
    page_03_batch_set_result(true, &pending_batch);
    assert(machine_state_batch_num() == 17 && g_batch_tip_label);
    assert(!strcmp(lv_label_get_text(named("03_batch_num_label")), "17"));
    write_bmp("menu-ack"); tick(2200); assert(!g_batch_tip_label);
    digits("1"); click("key_enter"); assert(request_value[5] == 5);
    page_03_batch_set_result(false, &pending_batch);
    digits("73"); click("key_enter");
    ui_page_03_menu_suspend(); assert(!page_03_menu_is_visible());
    page_03_batch_set_result(true, &pending_batch);
    assert(machine_state_batch_num() == 73 && !g_batch_tip_label);
    assert(!strcmp(lv_label_get_text(named("03_batch_num_label")), "17"));
    assert(ui_page_03_menu_resume());
    assert(!strcmp(lv_label_get_text(named("03_batch_num_label")), "73"));
    edit_is(0, false); write_bmp("menu-resumed-ack");
    tap(get_batch_switch_container());
    assert(requests[6] == 1 && request_value[6] == 200 && machine_state_batch_enabled());
    batch_switch_on_0x06_result(false, &pending_batch); assert(machine_state_batch_enabled());
}
static void test_functions(void)
{
    const char *targets[] = {"03_beep_off_btn", "03_speed_1000_btn", "03_add_on_btn", "03_fo_FO_btn", "03_work_manaul_btn"};
    machine_state_snapshot_t before, after;
    machine_state_get_snapshot(&before);
    lv_color_t colors[5];
    for (unsigned i = 0; i < 5; ++i) {
        colors[i] = lv_obj_get_style_bg_color(named(targets[i]), 0);
        click(targets[i]); assert(requests[i] == 1);
        assert(lv_obj_get_style_bg_color(named(targets[i]), 0).full == colors[i].full);
    }
    machine_state_get_snapshot(&after);
    assert(before.buzzer_enabled == after.buzzer_enabled && before.speed == after.speed && before.add_enabled == after.add_enabled);
    assert(before.fo_mode == after.fo_mode && before.work_mode == after.work_mode);
    ui_page_03_menu_suspend();
    /* This is the post-ACK model boundary, not a fabricated service success. */
    machine_state_confirm_buzzer(false); machine_state_confirm_speed(1); machine_state_confirm_add(true);
    machine_state_confirm_fo_mode(3); machine_state_confirm_work_mode(1);
    page_03_update_menu_button_states_refresh();
    for (unsigned i = 0; i < 5; ++i) assert(lv_obj_get_style_bg_color(named(targets[i]), 0).full == colors[i].full);
    assert(ui_page_03_menu_resume()); tick(300);
    for (unsigned i = 0; i < 5; ++i) {
        assert(lv_obj_get_style_bg_color(named(targets[i]), 0).full != colors[i].full);
        click(targets[i]); assert(requests[i] == 1); /* Already confirmed: no duplicate request. */
    }
    write_bmp("menu-functions-confirmed");
}
static void label_geometry(lv_obj_t *label)
{
    assert(label && lv_obj_is_visible(label));
    lv_area_t area, parent;
    lv_obj_get_coords(label, &area); lv_obj_get_coords(lv_obj_get_parent(label), &parent);
    assert(area.x1 >= parent.x1 && area.x2 <= parent.x2 && area.y1 >= parent.y1 && area.y2 <= parent.y2);
    lv_point_t size;
    lv_txt_get_size(&size, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0),
        lv_obj_get_style_text_letter_space(label, 0), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(size.x <= lv_obj_get_width(label) && size.y <= lv_obj_get_height(label));
    const char *text = lv_label_get_text(label);
    uint32_t offset = 0;
    while (text[offset]) {
        uint32_t codepoint = _lv_txt_encoded_next(text, &offset);
        lv_font_glyph_dsc_t glyph;
        bool found = lv_font_get_glyph_dsc(lv_obj_get_style_text_font(label, 0), &glyph, codepoint, 0);
        if (!found || glyph.is_placeholder) fprintf(stderr, "Missing visible glyph U+%04lX in '%s'\n", (unsigned long)codepoint, text);
        assert(found && !glyph.is_placeholder);
    }
}
static lv_obj_t *check_home_icon(void)
{
    lv_obj_t *home = named("03_home_btn");
    lv_obj_t *caption = lv_damped_button_get_label(home);
    assert(lv_obj_get_child_cnt(home) == 2 && lv_obj_get_child(home, 0) == caption);
    assert(lv_obj_check_type(caption, &lv_label_class));
    assert(!strcmp(lv_label_get_text(caption), ui_text_get(UI_TEXT_LIST_MAIN)));
    label_geometry(caption);
    lv_obj_t *icon = lv_obj_get_child(home, 1);
    assert(lv_obj_check_type(icon, &lv_img_class));
    assert(lv_obj_get_child_cnt(icon) == 0 && lv_obj_is_visible(icon));
    assert(lv_obj_get_width(icon) == 27 && lv_obj_get_height(icon) == 27);
    assert(!lv_obj_has_flag_any(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_get_style_bg_opa(icon, 0) == LV_OPA_TRANSP);
    assert(lv_obj_get_x(home) == MENU_SIDE_X && lv_obj_get_y(home) == MENU_HOME_Y);
    assert(lv_obj_get_width(home) == MENU_SIDE_WIDTH && lv_obj_get_height(home) == MENU_HOME_HEIGHT);
    lv_area_t button_area, content_area, icon_area, caption_area;
    lv_obj_get_coords(home, &button_area); lv_obj_get_coords(icon, &icon_area);
    lv_obj_get_content_coords(home, &content_area);
    lv_obj_get_coords(caption, &caption_area);
    assert(icon_area.x1 >= button_area.x1 && icon_area.x2 <= button_area.x2);
    assert(icon_area.y1 >= button_area.y1 && icon_area.y2 <= button_area.y2);
    assert(abs(icon_area.x1 + icon_area.x2 - button_area.x1 - button_area.x2) <= 1);
    assert(icon_area.y1 == content_area.y1 + 20);
    assert(!overlap(&icon_area, &caption_area));
    return icon;
}
static void test_home_icon(void)
{
    render();
    lv_obj_t *icon = check_home_icon(), *home = named("03_home_btn");
    lv_obj_t *caption = lv_damped_button_get_label(home);
    lv_area_t area; lv_obj_get_coords(icon, &area);
    uint32_t actual[27 * 27], signature = 0;
    for (unsigned y = 0; y < 27; ++y) for (unsigned x = 0; x < 27; ++x) {
        actual[y * 27 + x] = framebuffer[(area.y1 + y) * 1280 + area.x1 + x].full;
        signature = signature * 33 + actual[y * 27 + x];
    }
    /* Keep the existing Menu button's gradient under both glyphs. Comparing
     * against a white patch would test unrelated button-background styling. */
    lv_obj_t *reference = lv_obj_create(home); assert(reference);
    lv_obj_remove_style_all(reference);
    lv_obj_set_pos(reference, lv_obj_get_x(icon), lv_obj_get_y(icon));
    lv_obj_set_size(reference, 27, 27);
    lv_obj_clear_flag(reference, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *image=lv_img_create(reference);assert(image);
    lv_img_set_src(image,LVGL_DIR "ui_icons/home_27.png");
    lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
    render();
    for (unsigned y = 0; y < 27; ++y) for (unsigned x = 0; x < 27; ++x)
        assert(actual[y * 27 + x] == framebuffer[(area.y1 + y) * 1280 + area.x1 + x].full);
    lv_obj_del(reference); lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN); render();
    printf("HOME_GLYPH menu %08lx (standard raster 27x27 reference over Menu background)\n", (unsigned long)signature);

    lv_area_t before, pressed, icon_before, icon_pressed, caption_before, caption_pressed;
    lv_obj_get_coords(home, &before); lv_obj_get_coords(icon, &icon_before);
    lv_obj_get_coords(caption, &caption_before);
    unsigned calls = home_requests;
    pointer_point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    pointer_state = LV_INDEV_STATE_PRESSED; tick(140); render();
    assert(lv_obj_has_state(home, LV_STATE_PRESSED) && home_requests == calls);
    lv_obj_get_coords(home, &pressed); lv_obj_get_coords(icon, &icon_pressed);
    lv_obj_get_coords(caption, &caption_pressed);
    assert(!memcmp(&before, &pressed, sizeof(before)));
    assert(icon_pressed.x1 == icon_before.x1 && icon_pressed.y1 == icon_before.y1);
    assert(caption_pressed.x1 == caption_before.x1 && caption_pressed.y1 == caption_before.y1);
    assert(lv_obj_get_style_translate_y(icon, 0) == 0 && lv_obj_get_style_translate_y(caption, 0) == 0);
    lv_indev_wait_release(input_device);
    pointer_state = LV_INDEV_STATE_RELEASED; tick(300); render();
    assert(home_requests == calls && !lv_obj_has_state(home, LV_STATE_PRESSED));
    assert(lv_obj_get_style_translate_y(icon, 0) == 0 && lv_obj_get_style_translate_y(caption, 0) == 0);
    check_home_icon();
}
static void test_press_lost(void)
{
    page_03_batch_num_edit_reset();
    lv_area_t area; lv_obj_get_coords(named("key_1"), &area);
    pointer_point = (lv_point_t){(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    pointer_state = LV_INDEV_STATE_PRESSED; tick(80);
    /* Navigation/input ownership cancellation uses wait_release. Buttons retain
     * the production PRESS_LOCK semantics; leaving their box is not redefined. */
    lv_indev_wait_release(input_device);
    pointer_state = LV_INDEV_STATE_RELEASED; tick(300);
    edit_is(0, false);
    assert(!lv_obj_has_state(named("key_1"), LV_STATE_PRESSED));
    click("key_1"); edit_is(1, true); click("key_del"); edit_is(0, false);
}
#ifdef MENU_HOST_HAS_CONTEXT
void menu_host_manager_select(bool visible);
void menu_host_manager_reset_observation(void);
unsigned menu_host_manager_calls(void);
uint32_t menu_host_manager_topics(void);
uint32_t menu_host_manager_dirty(void);
void menu_host_manager_commit(void);
static void context_is(const char *currency, const char *mode)
{
    assert(!strcmp(lv_label_get_text(g_page_03_context_currency), currency));
    assert(!strcmp(lv_label_get_text(g_page_03_context_mode), mode));
    lv_obj_update_layout(menu_page);
    label_geometry(g_page_03_context_currency); label_geometry(g_page_03_context_mode);
}
static void test_context(unsigned initial_timers)
{
    assert(timer_count() == initial_timers); /* Static summary owns no polling. */
    context_is("EUR", "MDC");
    assert(currency_state_confirm_active_code("RUB")); machine_state_confirm_mode(MODE_SDC);
    ui_page_03_menu_refresh_data(UI_DATA_TOPIC_COUNTING_RESULT); context_is("EUR", "MDC");
    menu_host_manager_reset_observation(); ui_frame_commit_begin_batch();
    ui_manager_publish_data_changed(UI_DATA_TOPIC_CURRENCY_CATALOG);
    ui_manager_publish_data_changed(UI_DATA_TOPIC_CURRENCY_CATALOG | UI_DATA_TOPIC_MACHINE_SETTINGS);
    context_is("EUR", "MDC"); assert(menu_host_manager_calls() == 0);
    ui_frame_commit_end_batch(); ui_frame_commit_flush();
    assert(menu_host_manager_calls() == 1);
    assert(menu_host_manager_topics() == (UI_DATA_TOPIC_CURRENCY_CATALOG | UI_DATA_TOPIC_MACHINE_SETTINGS));
    context_is("RUB", "SDC");
    assert(currency_state_confirm_auto_selection());
    assert(currency_state_confirm_detected_code("USD"));
    machine_state_confirm_mode(MODE_CNT);
    ui_page_03_menu_refresh_data(UI_DATA_TOPIC_CURRENCY_CATALOG | UI_DATA_TOPIC_MACHINE_SETTINGS);
    context_is("AUTO", "CNT"); write_bmp("menu-context-auto-cnt");
    machine_state_confirm_mode(255); ui_page_03_menu_refresh_data(UI_DATA_TOPIC_MACHINE_SETTINGS);
    context_is("AUTO", ui_text_get(UI_TEXT_MENU_CONTEXT_UNKNOWN)); write_bmp("menu-context-unknown");
    ui_page_03_menu_suspend();
    menu_host_manager_select(false); menu_host_manager_reset_observation();
    assert(currency_state_leave_auto_selection()); assert(currency_state_confirm_active_code("EUR"));
    machine_state_confirm_mode(MODE_MDC); ui_page_03_menu_refresh_data(UI_DATA_TOPIC_MACHINE_SETTINGS);
    ui_frame_commit_begin_batch();
    ui_manager_publish_data_changed(UI_DATA_TOPIC_CURRENCY_CATALOG | UI_DATA_TOPIC_MACHINE_SETTINGS);
    ui_frame_commit_end_batch(); ui_frame_commit_flush();
    assert(menu_host_manager_calls() == 0);
    assert(menu_host_manager_dirty() == (UI_DATA_TOPIC_CURRENCY_CATALOG | UI_DATA_TOPIC_MACHINE_SETTINGS));
    assert(!strcmp(lv_label_get_text(g_page_03_context_currency), "AUTO"));
    assert(!strcmp(lv_label_get_text(g_page_03_context_mode), ui_text_get(UI_TEXT_MENU_CONTEXT_UNKNOWN)));
    assert(ui_page_03_menu_resume()); context_is("EUR", "MDC");
    menu_host_manager_select(true); menu_host_manager_commit();
    assert(menu_host_manager_calls() == 1 && menu_host_manager_dirty() == UI_DATA_TOPIC_NONE);
    check_home_icon(); /* One line-drawn house + one caption, not two labels. */
    render();
    uint32_t first = 0, second = 0;
    for (unsigned i = 0; i < 1280 * 400; ++i) first = first * 33 + framebuffer[i].full;
    tick(800); render();
    for (unsigned i = 0; i < 1280 * 400; ++i) second = second * 33 + framebuffer[i].full;
    assert(first == second && timer_count() == initial_timers);
}
#endif
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();
    lv_disp_draw_buf_t buffer; lv_disp_draw_buf_init(&buffer, draw_buffer, NULL, 1280 * 40);
    lv_disp_drv_t display; lv_disp_drv_init(&display); display.hor_res = 1280; display.ver_res = 400;
    display.draw_buf = &buffer; display.flush_cb = flush; assert(lv_disp_drv_register(&display));
    lv_fs_drv_t fs; lv_fs_drv_init(&fs); fs.letter = 'L'; fs.open_cb = fs_open; fs.close_cb = fs_close;
    fs.read_cb = fs_read; fs.seek_cb = fs_seek; fs.tell_cb = fs_tell; lv_fs_drv_register(&fs);
    lv_indev_drv_t input; lv_indev_drv_init(&input); input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = pointer_read; input_device = lv_indev_drv_register(&input); assert(input_device);
    currency_state_reset(); assert(currency_state_confirm_active_code("EUR"));
    machine_state_confirm_batch(true, 100); machine_state_confirm_mode(MODE_MDC);
    machine_state_confirm_buzzer(true); machine_state_confirm_speed(0); machine_state_confirm_add(false);
    machine_state_confirm_fo_mode(0); machine_state_confirm_work_mode(0);
    unsigned initial_timers = timer_count();
    lv_obj_t *parent = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(parent); lv_obj_set_size(parent, 1280, 400);
    ui_page_03_menu_create(parent); tick(300); assert(page_03_menu_is_created() && page_03_menu_is_visible());
    write_bmp("menu-default");
    if (strcmp(getenv("MENU_RENDER_ONLY"), "1")) {
        check_layout(); test_home_icon(); test_batch(); test_functions(); test_press_lost(); check_layout();
#ifdef MENU_HOST_HAS_CONTEXT
        test_context(initial_timers);
#endif
        lv_obj_t *original = menu_page; ui_page_03_menu_create(parent); assert(menu_page == original);
        click("03_home_btn"); assert(home_requests == 1);
        for (unsigned i = 0; i < 5; ++i) {
            ui_page_03_menu_suspend(); assert(ui_page_03_menu_resume());
            lv_obj_update_layout(menu_page); check_home_icon();
        }
        ui_page_03_menu_suspend(); ui_page_03_menu_destroy(); ui_page_03_menu_destroy();
        assert(!page_03_menu_is_created() && !ui_page_03_menu_resume());
        assert(timer_count() == initial_timers && lv_obj_get_child_cnt(parent) == 0);
        ui_page_03_menu_create(parent); tick(300); check_layout(); check_home_icon(); write_bmp("menu-rebuilt");
        ui_page_03_menu_destroy(); assert(timer_count() == initial_timers);
    } else ui_page_03_menu_destroy();
    lv_obj_del(parent);
    printf("PASS: real Menu/LVGL software raster; %u actual asset opens. Host stubs do not validate controller ACK, DMA or touch hardware.\n", opened_assets);
    return 0;
}
