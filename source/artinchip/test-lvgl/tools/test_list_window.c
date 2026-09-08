#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "un260/lv_components/ui_list_window.h"

static void assert_offset(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.01f);
}

static void assert_view(const ui_list_window_t *w, uint32_t first,
                        uint32_t last, uint32_t page, uint32_t pages)
{
    assert(w->first == first);
    assert(ui_list_window_last(w) == last);
    assert(ui_list_window_page_number(w) == page);
    assert(ui_list_window_pages(w) == pages);
}

static void test_empty_and_zero_dimensions(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 0, 0);
    assert(w.rows == 1 && w.row_height == 1);
    assert_view(&w, 0, 0, 1, 1);
    assert_offset(ui_list_window_limit(&w), 0);
    ui_list_window_move(&w, 1000.25f);
    assert_offset(w.offset, 0);
    ui_list_window_mode(&w, true);
    ui_list_window_page(&w, INT_MAX);
    assert_view(&w, 0, 0, 1, 1);
    ui_list_window_page(&w, INT_MIN);
    assert_view(&w, 0, 0, 1, 1);
    ui_list_window_update(&w, 0, true);
    assert_offset(w.offset, 0);
}

static void test_exact_viewport(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 7, false);
    assert_view(&w, 0, 7, 1, 1);
    assert_offset(ui_list_window_limit(&w), 0);
    ui_list_window_move(&w, FLT_MAX);
    assert_view(&w, 0, 7, 1, 1);
    ui_list_window_mode(&w, true);
    ui_list_window_page(&w, 1);
    assert_view(&w, 0, 7, 1, 1);
}

static void test_eight_rows_and_last_partial_page(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 8, false);
    assert_view(&w, 0, 7, 1, 2);
    assert_offset(ui_list_window_limit(&w), 32);
    ui_list_window_move(&w, 32);
    assert_view(&w, 1, 8, 1, 2);
    ui_list_window_mode(&w, true);
    assert_view(&w, 0, 7, 1, 2);
    ui_list_window_page(&w, 1);
    assert_view(&w, 7, 8, 2, 2);
    assert_offset(w.offset, 224);
    ui_list_window_update(&w, 15, false);
    assert_view(&w, 7, 14, 2, 3);
    ui_list_window_page(&w, 1);
    assert_view(&w, 14, 15, 3, 3);
}

static void test_ten_thousand_rows_and_wide_offsets(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 10000, false);
    assert_view(&w, 0, 7, 1, 1429);
    assert(ui_list_window_pages(&w) > UINT8_MAX);
    assert_offset(ui_list_window_limit(&w), 319776);
    ui_list_window_move(&w, FLT_MAX);
    assert_view(&w, 9993, 10000, 1428, 1429);
    assert(w.offset > UINT16_MAX);
    ui_list_window_mode(&w, true);
    ui_list_window_page(&w, INT_MAX);
    assert_view(&w, 9996, 10000, 1429, 1429);
    assert_offset(w.offset, 319872);
    ui_list_window_page(&w, -1);
    assert_view(&w, 9989, 9996, 1428, 1429);
}

static void test_page_step_clamps(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 10000, false);
    ui_list_window_page(&w, INT_MAX);
    assert_view(&w, 0, 7, 1, 1429); /* Paging is inert in scroll mode. */
    ui_list_window_mode(&w, true);
    ui_list_window_page(&w, 300);
    assert_view(&w, 2100, 2107, 301, 1429);
    ui_list_window_page(&w, 0);
    assert_view(&w, 2100, 2107, 301, 1429);
    ui_list_window_page(&w, INT_MAX);
    assert_view(&w, 9996, 10000, 1429, 1429);
    ui_list_window_page(&w, INT_MAX);
    assert_view(&w, 9996, 10000, 1429, 1429);
    ui_list_window_page(&w, INT_MIN);
    assert_view(&w, 0, 7, 1, 1429);
    ui_list_window_page(&w, INT_MIN);
    assert_view(&w, 0, 7, 1, 1429);
}

static void test_mode_toggles_preserve_visible_anchor(void)
{
    ui_list_window_t w;
    uint32_t anchor;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 100, false);
    ui_list_window_move(&w, 23.0f * 32 + 11.25f);
    assert_view(&w, 23, 31, 4, 15);
    anchor = w.first;
    ui_list_window_mode(&w, true);
    assert_view(&w, 21, 28, 4, 15);
    assert(w.first <= anchor && anchor < ui_list_window_last(&w));
    ui_list_window_mode(&w, false);
    assert_view(&w, 21, 28, 4, 15);
    assert_offset(w.offset, 672);
    ui_list_window_mode(&w, true);
    ui_list_window_page(&w, INT_MAX);
    anchor = w.first;
    assert_view(&w, 98, 100, 15, 15);
    ui_list_window_mode(&w, false);
    assert_view(&w, 93, 100, 14, 15);
    assert(w.first <= anchor && anchor < ui_list_window_last(&w));
}

static void test_scroll_update_preserves_and_clamps(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 100, false);
    ui_list_window_move(&w, 10.0f * 32 + 9.25f);
    ui_list_window_update(&w, 10000, false);
    assert_view(&w, 10, 18, 2, 1429);
    assert_offset(w.offset, 329.25f);
    ui_list_window_update(&w, 8, false);
    assert_view(&w, 1, 8, 1, 2);
    assert_offset(w.offset, 32);
    ui_list_window_update(&w, 3, false);
    assert_view(&w, 0, 3, 1, 1);
    assert_offset(w.offset, 0);
    ui_list_window_update(&w, 100, false);
    ui_list_window_move(&w, 1000.25f);
    ui_list_window_update(&w, 10000, true);
    assert_view(&w, 0, 7, 1, 1429);
    assert_offset(w.offset, 0);
    ui_list_window_update(&w, 0, false);
    assert_view(&w, 0, 0, 1, 1);
}

static void test_paged_update_preserves_and_clamps(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_mode(&w, true);
    ui_list_window_update(&w, 50, false);
    ui_list_window_page(&w, 6);
    assert_view(&w, 42, 49, 7, 8);
    ui_list_window_update(&w, 10000, false);
    assert_view(&w, 42, 49, 7, 1429);
    assert_offset(w.offset, 1344);
    ui_list_window_update(&w, 10, false);
    assert_view(&w, 7, 10, 2, 2);
    ui_list_window_update(&w, 100, true);
    assert_view(&w, 0, 7, 1, 15);
    ui_list_window_page(&w, INT_MAX);
    ui_list_window_update(&w, 0, false);
    assert_view(&w, 0, 0, 1, 1);
    assert_offset(w.offset, 0);
    assert(w.paged);
}

static void test_fractional_visible_range(void)
{
    ui_list_window_t w;
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 100, false);
    ui_list_window_move(&w, 64);
    assert_view(&w, 2, 9, 1, 15);
    ui_list_window_move(&w, 64.75f);
    assert_view(&w, 2, 9, 1, 15);
    ui_list_window_move(&w, 65.25f);
    assert_view(&w, 2, 10, 1, 15);
    assert_offset(w.offset, 65.25f);
    ui_list_window_move(&w, ui_list_window_limit(&w) - 1.25f);
    assert_view(&w, 92, 100, 14, 15);
    ui_list_window_move(&w, ui_list_window_limit(&w));
    assert_view(&w, 93, 100, 14, 15);
}

static void test_nonfinite_offsets_are_rejected(void)
{
    ui_list_window_t w;
    const float invalid[] = {NAN, INFINITY, -INFINITY};
    ui_list_window_init(&w, 7, 32);
    ui_list_window_update(&w, 10000, false);
    ui_list_window_move(&w, 130001.25f);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        const uint32_t first = w.first;
        const float offset = w.offset;
        ui_list_window_move(&w, invalid[i]);
        assert(w.first == first);
        assert_offset(w.offset, offset);
    }
    ui_list_window_move(&w, -FLT_MAX);
    assert_offset(w.offset, 0);
    assert_view(&w, 0, 7, 1, 1429);
    ui_list_window_move(&w, FLT_MAX);
    assert_view(&w, 9993, 10000, 1428, 1429);
}

static void test_deterministic_mixed_lifecycle(void)
{
    ui_list_window_t w;
    uint32_t random = 0x26012345U;
    ui_list_window_init(&w, 7, 32);
    for (unsigned int i = 0; i < 10000; ++i) {
        random = random * 1664525U + 1013904223U;
        switch (random % 6) {
        case 0: ui_list_window_update(&w, (random >> 8) % 10001, false); break;
        case 1: ui_list_window_update(&w, (random >> 8) % 10001, true); break;
        case 2: ui_list_window_mode(&w, !w.paged); break;
        case 3:
            if (!w.paged) ui_list_window_move(&w, (float)((random >> 8) % 400000) + 0.75f);
            break;
        case 4: ui_list_window_page(&w, INT_MAX); break;
        case 5: ui_list_window_page(&w, INT_MIN); break;
        }
        assert(w.first <= ui_list_window_last(&w));
        assert(ui_list_window_last(&w) <= w.count);
        assert(ui_list_window_page_number(&w) <= ui_list_window_pages(&w));
        assert(isfinite(w.offset) && w.offset >= 0);
        if (w.paged) {
            assert(w.first % w.rows == 0);
            assert_offset(w.offset, (float)w.first * w.row_height);
        } else {
            assert(w.offset <= ui_list_window_limit(&w));
            assert(w.first == (uint32_t)(w.offset / w.row_height));
        }
    }
}

int main(void)
{
    test_empty_and_zero_dimensions();
    test_exact_viewport();
    test_eight_rows_and_last_partial_page();
    test_ten_thousand_rows_and_wide_offsets();
    test_page_step_clamps();
    test_mode_toggles_preserve_visible_anchor();
    test_scroll_update_preserves_and_clamps();
    test_paged_update_preserves_and_clamps();
    test_fractional_visible_range();
    test_nonfinite_offsets_are_rejected();
    test_deterministic_mixed_lifecycle();
    puts("list window: 11 deterministic scenarios and 10000 mixed lifecycle steps passed");
    return 0;
}
