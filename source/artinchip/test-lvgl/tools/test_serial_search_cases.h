#ifndef TEST_SERIAL_SEARCH_CASES_H
#define TEST_SERIAL_SEARCH_CASES_H

/* Include after the real search/page translation units and tick/write_bmp/timers.
 * Uses real LVGL objects/events and public keyboard/navigation APIs; no UI mocks. */
static void serial_search_cases_flush(void)
{
    ui_frame_commit_flush();
    if (view) lv_obj_update_layout(view->page);
    tick(40);
}

static lv_obj_t *serial_search_cases_button(lv_obj_t *parent, const char *text)
{
    if (!parent || !lv_obj_is_visible(parent)) return NULL;
    for (uint32_t i = lv_obj_get_child_cnt(parent); i > 0; --i) {
        lv_obj_t *found = serial_search_cases_button(lv_obj_get_child(parent, i - 1), text);
        if (found) return found;
    }
    if (lv_obj_check_type(parent, &lv_btn_class)) {
        lv_obj_t *label = lv_damped_button_get_label(parent);
        if (label && !strcmp(lv_label_get_text(label), text)) return parent;
    }
    return NULL;
}

static void serial_search_cases_click(lv_obj_t *button)
{
    assert(button && lv_obj_is_visible(button));
    assert(!lv_obj_has_state(button, LV_STATE_DISABLED));
    /* A close callback may delete the event target, producing LV_RES_INV. */
    lv_event_send(button, LV_EVENT_CLICKED, NULL);
    serial_search_cases_flush();
}

static page_02_list_search_t *serial_search_cases_open(void)
{
    assert(view && !view->search);
    lv_obj_t *button = view->actions[LIST_ACTION_SEARCH];
    assert(lv_obj_get_parent(button) == view->page);
    assert(serial_search_cases_button(view->page, ui_text_get(UI_TEXT_SERIAL_SEARCH)) == button);
    serial_search_cases_click(button);
    assert(view->search && lv_obj_is_visible(view->search->root));
    return view->search;
}

static void serial_search_cases_input(page_02_list_search_t *s, const char *text)
{
    serial_search_cases_click(s->input);
    assert(s->keyboard && lv_alnum_keyboard_is_visible(s->keyboard));
    assert(lv_alnum_keyboard_set_text(s->keyboard, text));
    serial_search_cases_click(serial_search_cases_button(s->root, ui_text_get(UI_TEXT_SERIAL_SEARCH)));
    assert(!lv_alnum_keyboard_is_visible(s->keyboard));
    assert(!strcmp(s->query.text, text));
}

static void serial_search_cases_result(page_02_list_search_t *s, const uint16_t *slots,
                                       uint32_t count, uint32_t valid)
{
    assert(s == view->search);
    assert(s->result.valid_count == valid && s->result.matched_count == count);
    assert(s->result.written_count == count);
    assert(lv_recycled_list_window(s->list)->count == count);
    assert(lv_obj_has_flag(s->empty, LV_OBJ_FLAG_HIDDEN) == (count > 0));
    if (count && slots) assert(!memcmp(s->slots, slots, count * sizeof(*slots)));
}

static lv_obj_t *serial_search_cases_denom(page_02_list_search_t *s, int value)
{
    for (size_t i = 0; i < s->denomination_count; ++i)
        if (s->denominations[i] == value) return s->denom_buttons[i];
    assert(!"expected source denomination option missing");
    return NULL;
}

static lv_obj_t *serial_search_cases_row(lv_recycled_list_t *list, const char *number,
                                        unsigned columns)
{
    lv_obj_t *viewport = lv_recycled_list_object(list);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(viewport); ++i) {
        lv_obj_t *row = lv_obj_get_child(viewport, i);
        if (lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_child_cnt(row) != columns)
            continue;
        if (!strcmp(lv_label_get_text(lv_obj_get_child(row, 0)), number)) return row;
    }
    return NULL;
}

static uint64_t serial_search_cases_hash(const counting_sim_t *data)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < sizeof(*data); ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    for (int i = 0; i < counting_data_serial_scan_limit(data); ++i) {
        const unsigned char *serial = (const unsigned char *)data->sn_str[i];
        if (serial) do { hash = (hash ^ *serial) * UINT64_C(1099511628211); } while (*serial++);
    }
    return hash;
}

static void serial_search_cases_store(counting_sim_t *data, unsigned slot,
                                      const char *serial, int denomination)
{
    assert(slot < COUNTING_DATA_MAX_ITEMS);
    assert(counting_data_ensure_serial_capacity(data, (int)slot + 1));
    char *copy = malloc(strlen(serial) + 1);
    assert(copy);
    strcpy(copy, serial);
    free(data->sn_str[slot]);
    data->sn_str[slot] = copy;
    data->denom_mix[slot] = denomination;
}

static void serial_search_cases_fixture(counting_sim_t *data)
{
    static const struct { unsigned slot; const char *serial; int denomination; } rows[] = {
        {2, "AA100", 100}, {8, "xxAA100yy", 50}, {10, "AA100", 20},
        {15, "BB200", 100}, {21, "", 10}, {27, "CC300", 50},
        {32, "AA200", 20}, {47, "DD400", 5}, {63, "EE500", 100},
        {80, "FF600", 20}, {96, "GG700", 10}, {111, "HH800", 50}
    };
    counting_data_clear_serials(data);
    memset(data->denom, 0, sizeof(data->denom));
    data->denom_number = 3;
    data->denom[0].value = 200; /* Directory-only option remains selectable. */
    data->denom[1].value = 100;
    data->denom[2].value = 20;
    data->total_pcs = 12;
    data->total_amount = 1234;
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        serial_search_cases_store(data, rows[i].slot, rows[i].serial, rows[i].denomination);
    page_02_list_report_reset();
    serial_search_cases_flush();
}

static void serial_search_cases_filters(counting_sim_t *data)
{
    uint64_t source_before = serial_search_cases_hash(data);
    const uint16_t all[] = {2, 8, 10, 15, 21, 27, 32, 47, 63, 80, 96, 111};
    page_02_list_search_t *s = serial_search_cases_open();
    serial_search_cases_result(s, all, 12, 12);
    assert(s->denomination_count == 6 && s->denominations[0] == 200);
    lv_obj_t *row = serial_search_cases_row(s->list, "3", 4);
    assert(row && !strcmp(lv_label_get_text(lv_obj_get_child(row, 1)), "AA100"));
    assert(!serial_search_cases_row(s->list, "1", 4));
    serial_search_cases_input(s, "aa100");
    serial_search_cases_result(s, (uint16_t[]){2, 8, 10}, 3, 12);
    write_bmp("search-results");
    serial_search_cases_click(s->starts);
    assert(s->query.match == COUNTING_SERIAL_MATCH_PREFIX);
    assert(lv_obj_has_state(s->starts, LV_STATE_CHECKED));
    assert(!lv_obj_has_state(s->contains, LV_STATE_CHECKED));
    serial_search_cases_result(s, (uint16_t[]){2, 10}, 2, 12);
    serial_search_cases_input(s, "aa");
    serial_search_cases_result(s, (uint16_t[]){2, 10, 32}, 3, 12);
    write_bmp("search-prefix");
    serial_search_cases_click(s->ends);
    assert(s->query.match == COUNTING_SERIAL_MATCH_SUFFIX);
    assert(lv_obj_has_state(s->ends, LV_STATE_CHECKED));
    assert(!lv_obj_has_state(s->starts, LV_STATE_CHECKED));
    serial_search_cases_result(s, NULL, 0, 12);
    serial_search_cases_input(s, "100");
    serial_search_cases_result(s, (uint16_t[]){2, 10}, 2, 12);
    write_bmp("search-suffix");
    serial_search_cases_click(s->exclude);
    serial_search_cases_result(s, (uint16_t[]){8, 15, 21, 27, 32, 47, 63, 80, 96, 111}, 10, 12);
    serial_search_cases_click(s->exclude);
    serial_search_cases_input(s, "aa100");
    serial_search_cases_click(s->contains);
    serial_search_cases_result(s, (uint16_t[]){2, 8, 10}, 3, 12);
    serial_search_cases_click(s->exact);
    serial_search_cases_result(s, (uint16_t[]){2, 10}, 2, 12);
    assert(serial_search_cases_row(s->list, "3", 4));
    assert(serial_search_cases_row(s->list, "11", 4));
    serial_search_cases_click(s->exclude);
    serial_search_cases_result(s, (uint16_t[]){8, 15, 21, 27, 32, 47, 63, 80, 96, 111}, 10, 12);
    serial_search_cases_click(s->exclude);
    serial_search_cases_click(s->contains);
    serial_search_cases_click(serial_search_cases_denom(s, 100));
    serial_search_cases_result(s, (uint16_t[]){2}, 1, 12);
    serial_search_cases_click(serial_search_cases_denom(s, 20));
    serial_search_cases_result(s, (uint16_t[]){2, 10}, 2, 12);
    serial_search_cases_click(s->except);
    serial_search_cases_result(s, (uint16_t[]){8}, 1, 12);
    serial_search_cases_click(s->exclude);
    serial_search_cases_result(s, (uint16_t[]){21, 27, 47, 96, 111}, 5, 12);
    serial_search_cases_click(s->sort);
    serial_search_cases_result(s, (uint16_t[]){111, 96, 47, 27, 21}, 5, 12);
    serial_search_cases_click(s->all);
    assert(s->query.denomination_count == 0 && s->result.matched_count == 9);
    serial_search_cases_click(serial_search_cases_button(s->root, ui_text_get(UI_TEXT_SERIAL_RESET)));
    serial_search_cases_result(s, all, 12, 12);
    assert(!s->query.text[0] && !s->query.exclude_text && !s->query.exclude_denominations);
    assert(!s->query.descending && s->query.match == COUNTING_SERIAL_MATCH_CONTAINS);
    serial_search_cases_input(s, "NOTFOUND999");
    serial_search_cases_result(s, NULL, 0, 12);
    assert(!strcmp(lv_label_get_text(s->empty), ui_text_get(UI_TEXT_SERIAL_NO_MATCH)));
    write_bmp("search-no-match");
    serial_search_cases_input(s, "aa100");
    serial_search_cases_click(s->input);
    assert(lv_alnum_keyboard_set_text(s->keyboard, "DRAFT999"));
    write_bmp("search-keyboard");
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(view->search == s && !lv_alnum_keyboard_is_visible(s->keyboard));
    assert(!strcmp(s->query.text, "aa100"));
    serial_search_cases_result(s, (uint16_t[]){2, 8, 10}, 3, 12);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(!view->search && source_before == serial_search_cases_hash(data));
}

static void serial_search_cases_late_data(counting_sim_t *data)
{
    page_02_list_search_t *s = serial_search_cases_open();
    serial_search_cases_input(s, "aa100");
    serial_search_cases_click(serial_search_cases_denom(s, 100));
    serial_search_cases_click(serial_search_cases_denom(s, 20));
    serial_search_cases_click(s->sort);
    counting_serial_query_t saved_query = s->query;
    uint32_t base_count = view->data.serial_count;
    float base_offset = lv_recycled_list_window(view->section[1].list)->offset;
    uint32_t base_window_count = lv_recycled_list_window(view->section[1].list)->count;
    char base_pcs[32];
    snprintf(base_pcs, sizeof(base_pcs), "%s", lv_label_get_text(view->pcs));
    serial_search_cases_store(data, 120, "AA100", 100);
    ++data->total_pcs;
    ui_frame_commit_begin_batch();
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    assert(ui_frame_commit_pending());
    ui_frame_commit_end_batch();
    serial_search_cases_flush();
    serial_search_cases_result(s, (uint16_t[]){120, 10, 2}, 3, 13);
    assert(!memcmp(&s->query, &saved_query, sizeof(saved_query)));
    assert(view->data.serial_count == base_count);
    assert(lv_recycled_list_window(view->section[1].list)->count == base_window_count);
    assert(lv_recycled_list_window(view->section[1].list)->offset == base_offset);
    assert(!strcmp(lv_label_get_text(view->pcs), base_pcs));
    serial_search_cases_click(s->input);
    assert(lv_alnum_keyboard_set_text(s->keyboard, "UNSUBMITTED"));
    serial_search_cases_store(data, 121, "AA100", 100);
    ++data->total_pcs;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    serial_search_cases_flush();
    assert(s->data_dirty && s->result.matched_count == 3);
    assert(!strcmp(s->query.text, "aa100") && view->data.serial_count == base_count);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    serial_search_cases_result(s, (uint16_t[]){121, 120, 10, 2}, 4, 14);
    assert(!memcmp(&s->query, &saved_query, sizeof(saved_query)));
    uint64_t source_before = serial_search_cases_hash(data);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(!view->search && view->data.serial_count == 14);
    assert(lv_recycled_list_window(view->section[1].list)->count == 14);
    assert(!strcmp(lv_label_get_text(view->pcs), "14"));
    assert(source_before == serial_search_cases_hash(data));
}

static void serial_search_cases_locate(void)
{
    lv_recycled_list_t *list = view->section[PAGE_02_SECTION_B].list;
    for (unsigned paged = 0; paged < 2; ++paged) {
        lv_recycled_list_set_paged(list, paged != 0);
        assert(lv_recycled_list_scroll_to_index(list, 0));
        page_02_list_search_t *s = serial_search_cases_open();
        serial_search_cases_input(s, "FF600");
        serial_search_cases_result(s, (uint16_t[]){80}, 1, 14);
        search_closed(s->slots[0], NULL);
        serial_search_cases_flush();
        const ui_list_window_t *window = lv_recycled_list_window(list);
        assert(!view->search && view->located_slot == 80);
        assert(window->paged == (paged != 0) && window->first == 7);
        assert(window->offset == 7 * ROW_HEIGHT);
        lv_obj_t *row = serial_search_cases_row(list, "81", 3);
        assert(row && !strcmp(lv_label_get_text(lv_obj_get_child(row, 1)), "FF600"));
        assert(lv_obj_get_style_bg_color(row, 0).full == lv_color_hex(0xDCE6EC).full);
    }
}

static void serial_search_cases_pointer(lv_obj_t *viewport, lv_indev_t *indev,
                                        lv_event_code_t code, lv_point_t point,
                                        uint32_t elapsed)
{
    lv_tick_inc(elapsed);
    indev->proc.types.pointer.act_point = point;
    indev->proc.state = code == LV_EVENT_RELEASED
        ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
    /* RELEASED may synchronously delete viewport, list and search. */
    lv_event_send(viewport, code, indev);
}

static lv_point_t serial_search_cases_row_point(page_02_list_search_t *s,
                                                const char *number)
{
    lv_obj_update_layout(s->root);
    lv_obj_t *row = serial_search_cases_row(s->list, number, 4);
    assert(row);
    lv_area_t area;
    lv_obj_get_coords(row, &area);
    return (lv_point_t){area.x1 + 150, (area.y1 + area.y2) / 2};
}

static void serial_search_cases_pointer_cancelled(page_02_list_search_t *s)
{
    assert(view->search == s && view->located_slot == UINT16_MAX);
    assert(!s->tapping);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(!view->search && view->located_slot == UINT16_MAX);
}

static void serial_search_cases_pointer_flow(counting_sim_t *data)
{
    uint64_t source_before = serial_search_cases_hash(data);
    lv_indev_drv_t driver;
    lv_indev_drv_init(&driver);
    driver.type = LV_INDEV_TYPE_POINTER;
    lv_indev_t indev = {0};
    indev.driver = &driver;
    page_02_list_report_reset();
    serial_search_cases_flush();
    page_02_list_search_t *s = serial_search_cases_open();
    lv_obj_t *viewport = lv_recycled_list_object(s->list);
    lv_point_t point = serial_search_cases_row_point(s, "11");
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 20);
    assert(s->tapping && s->pressed_slot == 10);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, point, 20);
    assert(!view->search && view->located_slot == 10);
    serial_search_cases_flush();

    const struct { int dx, dy; bool pressing; } drags[] = {
        {6, 0, false}, {-6, 0, true}, {0, 6, true}, {0, -6, true}
    };
    for (unsigned i = 0; i < sizeof(drags) / sizeof(drags[0]); ++i) {
        page_02_list_report_reset();
        serial_search_cases_flush();
        s = serial_search_cases_open();
        viewport = lv_recycled_list_object(s->list);
        point = serial_search_cases_row_point(s, "3");
        lv_point_t moved = {point.x + drags[i].dx, point.y + drags[i].dy};
        serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 20);
        assert(s->tapping && s->pressed_slot == 2);
        if (drags[i].pressing) {
            serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSING, moved, 30);
            assert(!s->tapping);
        }
        serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, moved, 30);
        serial_search_cases_pointer_cancelled(s);
    }

    s = serial_search_cases_open();
    viewport = lv_recycled_list_object(s->list);
    point = serial_search_cases_row_point(s, "3");
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 20);
    assert(s->tapping);
    uint32_t revision = s->revision;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    assert(s->revision != revision && !s->tapping);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, point, 20);
    serial_search_cases_pointer_cancelled(s);

    s = serial_search_cases_open();
    viewport = lv_recycled_list_object(s->list);
    point = serial_search_cases_row_point(s, "3");
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 20);
    assert(s->tapping);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESS_LOST, point, 20);
    assert(!s->tapping);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, point, 20);
    serial_search_cases_pointer_cancelled(s);

    s = serial_search_cases_open();
    viewport = lv_recycled_list_object(s->list);
    point = serial_search_cases_row_point(s, "3");
    lv_point_t pulled = {point.x, point.y + 144};
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 20);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSING, pulled, 80);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, pulled, 0);
    assert(view->search == s && !s->tapping);
    lv_timer_t *motion = NULL;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer; timer = lv_timer_get_next(timer))
        if (timer->user_data == s->list) motion = timer;
    assert(motion && !motion->paused);
    tick(40);
    lv_obj_update_layout(s->root);
    lv_obj_t *row = serial_search_cases_row(s->list, "3", 4);
    assert(row && lv_obj_get_y(row) > 0 && lv_obj_get_y(row) < SEARCH_ROW_HEIGHT);
    assert(lv_recycled_list_window(s->list)->offset == 0 && !motion->paused);

    /* Empty rubber-band space is not the logical first row. */
    lv_area_t area;
    lv_obj_get_coords(viewport, &area);
    lv_point_t gap = {area.x1 + 150, area.y1 + 1};
    uint32_t hit = UINT32_MAX;
    assert(!lv_recycled_list_index_at_point(s->list, &gap, &hit));
    assert(hit == UINT32_MAX);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, gap, 0);
    assert(!s->tapping);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, gap, 0);
    assert(view->search == s && view->located_slot == UINT16_MAX);
    tick(20);

    /* The lower part of the shifted first row lies in the unshifted second-row
     * band. The old offset-only hit test would select source slot 8, not 2. */
    lv_obj_update_layout(s->root);
    int pulled_y = lv_obj_get_y(row);
    assert(pulled_y > 3 && !motion->paused);
    lv_area_t row_area;
    lv_obj_get_coords(row, &row_area);
    point = (lv_point_t){row_area.x1 + 150, row_area.y2 - 2};
    assert((point.y - area.y1) / SEARCH_ROW_HEIGHT == 1);
    assert(lv_recycled_list_index_at_point(s->list, &point, &hit) && hit == 0);
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_PRESSED, point, 0);
    assert(s->tapping && s->pressed_slot == 2 && motion->paused);
    assert(lv_obj_get_y(row) == pulled_y); /* Regrabbing never snaps the rows. */
    serial_search_cases_pointer(viewport, &indev, LV_EVENT_RELEASED, point, 0);
    assert(!view->search && view->located_slot == 2);
    serial_search_cases_flush();
    assert(source_before == serial_search_cases_hash(data));
    page_02_list_report_reset();
    serial_search_cases_flush();
    puts("PASS real search pointer tap, 6px cancellation, revision/PRESS_LOST and elastic-row hit test");
}

static void serial_search_cases_lifecycle(void)
{
    serial_search_cases_flush();
    unsigned baseline = timers();
    for (unsigned action = 0; action < 3; ++action) {
        page_02_list_search_t *s = serial_search_cases_open();
        serial_search_cases_click(s->input);
        lv_obj_t *old_root = s->root;
        assert(lv_alnum_keyboard_is_visible(s->keyboard));
        ui_frame_commit_begin_batch();
        page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
        assert(ui_frame_commit_pending());
        if (action == 0) page_02_list_report_reset();
        else if (action == 1) ui_page_02_list_suspend();
        else ui_page_02_list_destroy();
        assert(!lv_obj_is_valid(old_root));
        assert(!view || !view->search);
        ui_frame_commit_end_batch();
        serial_search_cases_flush();
        assert(!ui_frame_commit_pending());
        if (action == 1) assert(ui_page_02_list_resume());
        if (action == 2) ui_page_02_list_create(lv_scr_act());
        serial_search_cases_flush();
        assert(view && timers() == baseline);
    }
    lv_mem_monitor_t memory_before, memory_after;
    lv_mem_monitor(&memory_before);
    for (unsigned i = 0; i < 21; ++i) {
        page_02_list_search_t *s = serial_search_cases_open();
        serial_search_cases_click(s->input);
        assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
        serial_search_cases_flush();
        assert(view->search == s && !lv_alnum_keyboard_is_visible(s->keyboard));
        assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
        serial_search_cases_flush();
        assert(!view->search && timers() == baseline && !ui_frame_commit_pending());
        lv_mem_monitor(&memory_after);
        assert(memory_after.used_cnt == memory_before.used_cnt);
        if (i == 0) {
            /* Establish steady-state after the first complete render/restore:
             * LVGL's shared scratch/allocation sizing need not equal a fresh
             * page's. All subsequent cycles must retain exactly this baseline. */
            fprintf(stderr,"SEARCH_MEMORY first_cycle_delta=%ld live_blocks=%u\n",
                (long)memory_before.free_size-(long)memory_after.free_size,
                (unsigned)memory_after.used_cnt);
            memory_before=memory_after;
        } else assert(memory_after.free_size == memory_before.free_size);
    }
    puts("PASS search steady-state heap and live blocks unchanged across 20 reopen cycles");
}

static void serial_search_cases_large_and_empty(counting_sim_t *data)
{
    counting_data_clear_serials(data);
    assert(counting_data_ensure_serial_capacity(data, COUNTING_DATA_MAX_ITEMS));
    for (unsigned i = 0; i < COUNTING_DATA_MAX_ITEMS; ++i)
        serial_search_cases_store(data, i, i == 9999 ? "TAIL9999" : "BASE0001", 100);
    page_02_list_report_reset();
    serial_search_cases_flush();
    uint64_t source_before = serial_search_cases_hash(data);
    page_02_list_search_t *s = serial_search_cases_open();
    serial_search_cases_result(s, NULL, 10000, 10000);
    assert(lv_obj_get_child_cnt(lv_recycled_list_object(s->list)) <= SEARCH_ROWS + 2);
    serial_search_cases_click(s->sort);
    assert(s->slots[0] == 9999 && s->slots[9999] == 0);
    serial_search_cases_input(s, "TAIL9999");
    serial_search_cases_result(s, (uint16_t[]){9999}, 1, 10000);
    assert(serial_search_cases_row(s->list, "10000", 4));
    assert(source_before == serial_search_cases_hash(data));
    counting_data_clear_serials(data);
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    serial_search_cases_flush();
    serial_search_cases_result(s, NULL, 0, 0);
    assert(!strcmp(lv_label_get_text(s->empty), ui_text_get(UI_TEXT_SERIAL_NO_RECORDS)));
    assert(!strcmp(s->query.text, "TAIL9999") && s->query.descending);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    serial_search_cases_flush();
    assert(!view->search && view->data.serial_count == 0);
}

static void test_serial_search_cases(counting_sim_t *data)
{
    assert(view && data == counting_data_current());
    denom_t saved_denoms[COUNTING_DENOM_MAX_ITEMS];
    memcpy(saved_denoms, data->denom, sizeof(saved_denoms));
    uint8_t saved_denom_count = data->denom_number;
    int saved_total = data->total_pcs;
    float saved_amount = data->total_amount;
    serial_search_cases_fixture(data);
    unsigned baseline = timers();
    serial_search_cases_filters(data);
    serial_search_cases_pointer_flow(data);
    serial_search_cases_late_data(data);
    serial_search_cases_locate();
    serial_search_cases_lifecycle();
    serial_search_cases_large_and_empty(data);
    counting_data_clear_serials(data);
    memcpy(data->denom, saved_denoms, sizeof(saved_denoms));
    data->denom_number = saved_denom_count;
    data->total_pcs = saved_total;
    data->total_amount = saved_amount;
    page_02_list_report_reset();
    serial_search_cases_flush();
    assert(view && !view->search && !ui_frame_commit_pending() && timers() == baseline);
    puts("PASS real serial search filters, source NO, keyboard ESC layers, late data, locate, 10000 rows and lifecycle");
}

#endif
