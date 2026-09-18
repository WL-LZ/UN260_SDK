#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/lv_system/ui_lang.h"
/* Inspect actual private projection, without firmware test accessors. */
#include "un260/lv_core/page_01_main_detail.c"

static unsigned navigations;
static bool auto_selected;
bool currency_state_auto_selected(void) { return auto_selected; }
static bool destroy_on_navigation, fail_owner, fail_timer;
void ui_manager_push_page(ui_page_t page)
{
    assert(page == UI_PAGE_LIST);
    ++navigations;
    if (destroy_on_navigation) page_01_main_detail_destroy();
}

void *__real_lv_mem_alloc(size_t size);
void *__wrap_lv_mem_alloc(size_t size)
{
    if (fail_owner && size == sizeof(main_detail_view_t)) { fail_owner = false; return NULL; }
    return __real_lv_mem_alloc(size);
}
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t callback, uint32_t period, void *data);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t callback, uint32_t period, void *data)
{
    if (fail_timer && period == 16) { fail_timer = false; return NULL; }
    return __real_lv_timer_create(callback, period, data);
}

static lv_color_t framebuffer[1280 * 400];
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    int width = lv_area_get_width(area);
    assert(area->x1 >= 0 && area->y1 >= 0 && area->x2 < 1280 && area->y2 < 400);
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(framebuffer + y * 1280 + area->x1,
               pixels + (y - area->y1) * width, width * sizeof(*pixels));
    lv_disp_flush_ready(driver);
}

static void tick(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 20) { lv_tick_inc(20); lv_timer_handler(); } }

static void render(void)
{ lv_obj_update_layout(lv_scr_act()); lv_obj_invalidate(lv_scr_act()); lv_refr_now(NULL); }

static void write_bmp(const char *name)
{
    const char *directory = getenv("MAIN_DETAIL_RASTER_OUTPUT");
    if (!directory) return;
    render();
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
    FILE *file = fopen(path, "wb"); assert(file);
    uint8_t header[54] = {0};
    uint32_t size = 54 + 1280 * 400 * 4, offset = 54, dib = 40, width = 1280;
    int32_t height = -400; uint16_t planes = 1, bits = 32;
    header[0] = 'B'; header[1] = 'M'; memcpy(header + 2, &size, 4);
    memcpy(header + 10, &offset, 4); memcpy(header + 14, &dib, 4);
    memcpy(header + 18, &width, 4); memcpy(header + 22, &height, 4);
    memcpy(header + 26, &planes, 2); memcpy(header + 28, &bits, 2);
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(framebuffer, 1, sizeof(framebuffer), file) == sizeof(framebuffer));
    fclose(file);
}

static unsigned timer_count(void)
{ unsigned count = 0; for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++count; return count; }

static lv_timer_t *motion(main_detail_section_t *section)
{
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t))
        if (t->user_data == section->list) return t;
    return NULL;
}

static main_detail_section_t *active(void)
{ return &detail_view->section[detail_view->active]; }

static const ui_list_window_t *window(void)
{ return lv_recycled_list_window(active()->list); }

static lv_obj_t *row_at(unsigned index)
{
    assert(index >= window()->first && index <= window()->first + window()->rows);
    return lv_obj_get_child(lv_recycled_list_object(active()->list), index % (window()->rows + 1U));
}

static const char *cell(unsigned index, unsigned column)
{ return lv_label_get_text(lv_obj_get_child(row_at(index), column)); }

static void assert_geometry(void)
{
    render();
    assert(window()->rows == 5 && window()->row_height == 34);
    assert(lv_obj_get_child_cnt(page_01_main_detail_scroll_obj()) == 7); /* Six rows and scrollbar; empty state is outside the viewport. */
    for (unsigned k = 0; k < window()->rows + 1U; ++k) {
        unsigned index = window()->first + k;
        if (index >= window()->count) break;
        lv_obj_t *row = row_at(index);
        assert(!lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN));
        int y = lv_obj_get_y(row);
        assert(y >= -34 && y <= 170);
        assert(lv_obj_get_height(row) == 34);
        for (unsigned col = 0; col < 3; ++col) {
            lv_obj_t *label = lv_obj_get_child(row, col);
            assert(abs(2 * lv_obj_get_y(label) + lv_obj_get_height(label) - 34) <= 1);
        }
    }
}

static void fixture(void)
{
    counting_sim_t *data = counting_data_mutable();
    counting_data_clear_serials(data); counting_data_clear_errors(data);
    memset(data, 0, sizeof(*data));
    data->denom_number = 15;
    for (unsigned i = 0; i < 15; ++i) {
        data->denom[i].value = (i + 1U) * 5;
        data->denom[i].pcs = i;
        data->denom[i].amount = data->denom[i].value * i;
    }
    data->denom[4].value = 0;
    assert(counting_data_ensure_serial_capacity(data, 10000));
    assert(counting_data_ensure_error_capacity(data, 10000));
    for (unsigned i = 0; i < 10000; ++i) {
        char text[24]; snprintf(text, sizeof(text), "AB%010u", i + 1U);
        data->sn_str[i] = malloc(strlen(text) + 1); assert(data->sn_str[i]);
        strcpy(data->sn_str[i], text);
        data->denom_mix[i] = i % 9 ? 100 : 0;
        data->err_pcs[i] = i % 251 + 1;
        data->err_code[i] = i % 50;
    }
    data->err_num = 10000;
}

/* Real LVGL input driver: hit testing, PRESS_LOCK, press/click dispatch and
 * released state are exercised instead of directly invoking the tap callback. */
static lv_point_t pointer_position;
static lv_indev_state_t pointer_state;
static void pointer_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{ (void)driver; data->point = pointer_position; data->state = pointer_state; }
static void pointer(int x, int y, bool pressed)
{
    pointer_position.x = x; pointer_position.y = y;
    pointer_state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    tick(40);
}
static void tap(int x, int y)
{ pointer(x, y, true); pointer(x, y, false); }

static void test_projection(void)
{
    assert(page_01_main_detail_create(lv_scr_act(), 620, 120, 518, 200));
    assert(!page_01_main_detail_create(lv_scr_act(), 620, 120, 518, 200));
    assert(window()->count == 14);
    assert(!strcmp(cell(0, 0), "5") && !strcmp(cell(0, 1), "0"));
    assert(!strcmp(cell(4, 0), "30"));
    assert_geometry(); write_bmp("main-detail-report");
    lv_recycled_list_scroll_to_index(active()->list, 11);
    float report_offset = window()->offset;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    assert(window()->count == 8888 && !strcmp(cell(0, 0), "2"));
    assert(!strcmp(cell(0, 1), "AB0000000002"));
    assert(!strcmp(cell(0, 2), "100"));
    assert_geometry(); write_bmp("main-detail-serial");
    assert(lv_recycled_list_scroll_to_index(active()->list, 8887));
    float serial_offset = window()->offset;
    assert(serial_offset > 300000 && !strcmp(cell(8887, 0), "9999"));
    assert_geometry(); write_bmp("main-detail-serial-last");
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_C);
    assert(window()->count == 10000);
    assert(lv_recycled_list_scroll_to_index(active()->list, 9999));
    assert(!strcmp(cell(9999, 0), "10000"));
    assert(!strcmp(cell(9999, 2), ui_text_counting_reject_reason(counting_data_current()->err_code[9999])));
    assert_geometry(); write_bmp("main-detail-reject-last");
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    assert(window()->offset == report_offset);
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    assert(window()->offset == serial_offset);
    /* Data refresh, not scrolling, rebuilds the valid-slot map. */
    counting_data_mutable()->denom_mix[9999] = 100;
    lv_recycled_list_scroll_to_index(active()->list, 0);
    assert(window()->count == 8888 && detail_view->serial_slots[8887] == 9998);
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    assert(window()->count == 8889 && detail_view->serial_slots[8888] == 9999);
    page_01_main_detail_set_visible(false);
    for (unsigned i = 0; i < 3; ++i) assert(motion(&detail_view->section[i])->paused);
    counting_data_mutable()->denom_mix[0] = 100;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    assert(detail_view->serial_count == 8889); /* Hidden refresh is dirty-only. */
    page_01_main_detail_set_visible(true);
    assert(detail_view->serial_count == 8890);
    page_01_main_detail_reset();
    for (unsigned i = 0; i < 3; ++i) assert(lv_recycled_list_window(detail_view->section[i].list)->offset == 0);
}

static void test_taps(lv_indev_t *indev)
{
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    render();
    unsigned start = navigations;
    tap(800, 130); assert(navigations == start); /* Header is not a List entry. */
    tap(800, 165); assert(navigations == ++start); /* Table row. */
    pointer(800, 240, true); pointer(800, 233, true); pointer(800, 233, false);
    assert(navigations == start); /* 7px genuine scroll is not a sub-10px tap. */
    lv_recycled_list_stop(active()->list);
    lv_recycled_list_scroll_to_index(active()->list, 0);
    pointer(800, 180, true); pointer(800, 187, true); pointer(800, 187, false);
    assert(navigations == start); /* Rubber stretch at the top also cancels. */
    lv_recycled_list_stop(active()->list);
    pointer(800, 180, true); pointer(812, 180, true); pointer(800, 180, true); pointer(800, 180, false);
    assert(navigations == start); /* Return to start does not undo movement. */
    pointer(800, 180, true);
    assert(lv_event_send(page_01_main_detail_scroll_obj(), LV_EVENT_PRESS_LOST, indev) == LV_RES_OK);
    pointer(800, 180, false); assert(navigations == start);
    pointer(800, 180, true);
    for (unsigned i = 0; i < 4; ++i) {
        page_01_main_detail_set_visible(true);
        page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
        tick(40);
    }
    pointer(800, 180, false); assert(navigations == ++start); /* Packets do not steal a tap into List. */
    pointer(800, 180, true);
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    pointer(800, 180, false); assert(navigations == start); /* Changing the actual tab still cancels. */
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    pointer(800, 180, true); page_01_main_detail_reset();
    pointer(800, 180, false); assert(navigations == start);
    pointer(800, 180, true); page_01_main_detail_set_visible(false);
    pointer(800, 180, false); page_01_main_detail_set_visible(true);
    assert(navigations == start);
    lv_obj_t *title = surface(lv_scr_act(), 620, 24, 518, 40, 0xFFFFFF);
    page_01_main_detail_bind_tap(title); page_01_main_detail_bind_tap(title);
    render(); tap(800, 40); assert(navigations == ++start); /* Rebinding is idempotent. */
    page_01_main_detail_set_visible(false);
    tap(800, 40); assert(navigations == ++start); /* Main's visible MULTI card can still open List. */
    page_01_main_detail_set_visible(true);
    pointer(800, 40, true); pointer(810, 40, true); pointer(810, 40, false);
    assert(navigations == start); /* Exactly10px fails strict tap threshold. */
    counting_data_clear_serials(counting_data_mutable());
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_B);
    assert(window()->count == 0 && lv_obj_is_visible(active()->empty));
    write_bmp("main-detail-serial-empty");
    tap(800, 250); assert(navigations == ++start); /* Empty state remains an entry. */
    destroy_on_navigation = true;
    tap(800, 40); assert(navigations == ++start && detail_view == NULL);
    destroy_on_navigation = false;
    tap(800, 40); assert(navigations == start); /* Surviving external callback is inert. */
    lv_obj_del(title);
}

static void test_lifecycle(void)
{
    unsigned baseline = timer_count();
    lv_obj_t *warm = lv_obj_create(lv_scr_act()); lv_obj_del(warm); render();
    lv_mem_monitor_t before, after; lv_mem_monitor(&before);
    fail_owner = true;
    assert(!page_01_main_detail_create(lv_scr_act(), 620, 120, 518, 200));
    assert(!fail_owner && !detail_view && timer_count() == baseline);
    assert(!page_01_main_detail_create(lv_scr_act(), 0, 0, 200, 200));
    assert(!page_01_main_detail_create(lv_scr_act(), 0, 0, 518, 20));
    for (unsigned iteration = 0; iteration < 30; ++iteration) {
        lv_obj_t *parent = surface(lv_scr_act(), 0, 0, 1280, 400, 0xD8E2E8);
        fail_timer = iteration == 0;
        assert(page_01_main_detail_create(parent, 620, 120, 518, 200));
        lv_obj_update_layout(detail_view->root);
        for (unsigned section = 0; section < 3; ++section) {
            lv_obj_t *viewport = lv_recycled_list_object(detail_view->section[section].list);
            assert(lv_obj_get_width(viewport) == 536);
            bool found_thumb = false;
            for (unsigned child = 0; child < lv_obj_get_child_cnt(viewport); ++child) {
                lv_obj_t *object = lv_obj_get_child(viewport, child);
                if (lv_obj_get_width(object) == 8) {
                    assert(lv_obj_get_x(object) == 526);
                    found_thumb = true;
                }
            }
            assert(found_thumb);
        }
        assert(!fail_timer && timer_count() == baseline + (iteration == 0 ? 2U : 3U));
        page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_C);
        lv_recycled_list_scroll_to_index(active()->list, 100);
        render(); pointer(800, 240, true); pointer(800, 170, true); pointer(800, 170, false);
        assert(motion(active()) && !motion(active())->paused);
        float offset = window()->offset;
        page_01_main_detail_set_visible(false);
        tick(120); assert(window()->offset == offset);
        page_01_main_detail_set_visible(true);
        if (iteration % 2) page_01_main_detail_destroy();
        lv_obj_del(parent); /* Parent-only deletion must release all three timers. */
        assert(!detail_view && page_01_main_detail_scroll_obj() == NULL);
        tick(40); assert(timer_count() == baseline);
        page_01_main_detail_reset(); page_01_main_detail_set_visible(true); page_01_main_detail_destroy();
    }
    render(); lv_mem_monitor(&after);
    assert(after.free_size == before.free_size);
}

int main(void)
{
    assert(sizeof(lv_coord_t) == 2);
    lv_init();
    static lv_color_t draw_pixels[1280 * 40];
    static lv_disp_draw_buf_t draw_buffer;
    lv_disp_draw_buf_init(&draw_buffer, draw_pixels, NULL, 1280 * 40);
    static lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res = 1280; display.ver_res = 400; display.draw_buf = &draw_buffer; display.flush_cb = flush;
    assert(lv_disp_drv_register(&display));
    static lv_indev_drv_t pointer_driver; lv_indev_drv_init(&pointer_driver);
    pointer_driver.type = LV_INDEV_TYPE_POINTER; pointer_driver.read_cb = pointer_read;
    lv_indev_t *indev = lv_indev_drv_register(&pointer_driver); assert(indev);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xD8E2E8), 0);
    fixture(); test_projection(); test_taps(indev); test_lifecycle();
    lv_obj_t *parent=lv_obj_create(lv_scr_act());
    page_01_main_detail_create(parent,0,0,518,244);
    page_01_main_detail_set_visible(true);
    counting_sim_t *data=counting_data_mutable();
    auto_selected=true; data->total_pcs=0;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    assert(window()->count==0);
    assert(!lv_obj_has_flag(detail_view->section[0].empty,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_child_cnt(detail_view->section[0].empty)==2);
    data->total_pcs=1;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    assert(window()->count>0);
    data->total_pcs=0; auto_selected=false;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    assert(window()->count>0);
    lv_obj_del(parent);
    counting_data_clear_serials(counting_data_mutable());
    counting_data_clear_errors(counting_data_mutable());
    lv_indev_delete(indev); lv_deinit();
    puts("PASS real LVGL Main detail: full zero-PCS catalog, sparse original serial slots, 10000 rejects/coord16, bounded pools, tab positions, empty/header/row taps, drag cancellation, hidden dirty refresh, allocation/timer failures and lifecycle");
    return 0;
}
