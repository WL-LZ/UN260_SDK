#!/usr/bin/env python3
"""Real List index/store regression and source contracts; not a board UI test."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PAGE = (ROOT / "un260/lv_core/page_02_list.c").read_text(encoding="utf-8")

HARNESS = r'''
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_data_store.h"
#include "un260/lv_core/page_02_list_data.h"

static void matches_store(const page_02_list_data_t *view, const counting_sim_t *data)
{
    assert(view->serial_count == counting_data_serial_valid_count(data));
    for (unsigned int i = 0; i < view->serial_count; ++i) {
        assert(view->serial[i] == counting_data_serial_nth_valid_index(data, (int)i));
        if (i) assert(view->serial[i - 1] < view->serial[i]);
    }
    assert(counting_data_serial_nth_valid_index(data, view->serial_count) == -1);
}

static void test_empty_and_null(void)
{
    counting_sim_t data = {0};
    page_02_list_data_t view;
    memset(&view, 0xff, sizeof(view));
    page_02_list_data_denoms(&view, NULL);
    page_02_list_data_serials(&view, NULL);
    assert(view.denom_count == 0 && view.serial_count == 0);
    page_02_list_data_denoms(&view, &data);
    page_02_list_data_serials(&view, &data);
    assert(view.denom_count == 0 && view.serial_count == 0);
    matches_store(&view, &data);
}

static void test_denom_holes_and_zero_pcs(void)
{
    counting_sim_t data = {0}, before;
    page_02_list_data_t view = {0};
    const int values[] = {100, 0, -1, 50, INT_MIN, 20, 0, 5};
    const uint8_t expected[] = {0, 3, 5, 7};
    data.denom_number = 8;
    for (unsigned int i = 0; i < 8; ++i) {
        data.denom[i].value = values[i];
        data.denom[i].pcs = 0; /* A valid denomination with zero pieces is retained. */
        data.denom[i].amount = 0;
    }
    data.denom[10].value = 1000; /* Outside denom_number must not leak into A. */
    memcpy(&before, &data, sizeof(data));
    page_02_list_data_denoms(&view, &data);
    assert(view.denom_count == sizeof(expected) / sizeof(expected[0]));
    assert(memcmp(view.denom, expected, sizeof(expected)) == 0);
    assert(memcmp(&data, &before, sizeof(data)) == 0);
    data.denom[0].value = -100;
    page_02_list_data_denoms(&view, &data);
    assert(view.denom_count == 3 && view.denom[0] == 3);
    data.denom_number = 0;
    page_02_list_data_denoms(&view, &data);
    assert(view.denom_count == 0);
}

static void test_denom_maximum_and_invalid_declared_count(void)
{
    counting_sim_t data = {0};
    struct { uint64_t before; page_02_list_data_t view; uint64_t after; } guarded = {0};
    guarded.before = guarded.after = UINT64_C(0x123456789abcdef0);
    for (unsigned int i = 0; i < COUNTING_DENOM_MAX_ITEMS; ++i) data.denom[i].value = (int)i + 1;
    data.denom_number = COUNTING_DENOM_MAX_ITEMS;
    page_02_list_data_denoms(&guarded.view, &data);
    assert(guarded.view.denom_count == COUNTING_DENOM_MAX_ITEMS);
    for (unsigned int i = 0; i < COUNTING_DENOM_MAX_ITEMS; ++i) assert(guarded.view.denom[i] == i);
    data.denom_number = UINT8_MAX;
    page_02_list_data_denoms(&guarded.view, &data);
    assert(guarded.view.denom_count == COUNTING_DENOM_MAX_ITEMS);
    assert(guarded.before == UINT64_C(0x123456789abcdef0));
    assert(guarded.after == UINT64_C(0x123456789abcdef0));
}

static void test_sparse_out_of_order_and_empty_serials(void)
{
    counting_sim_t data = {0}, before;
    page_02_list_data_t view = {0};
    char *slots[32] = {0};
    const uint16_t expected[] = {2, 6, 9, 31};
    data.sn_str = slots;
    data.sn_capacity = 32;
    data.total_pcs = 1; /* Scan capacity, not a prematurely reported live total. */
    slots[31] = "LAST"; data.denom_mix[31] = 10;
    slots[9] = "NINE"; data.denom_mix[9] = 50;
    slots[2] = ""; data.denom_mix[2] = 100; /* Non-NULL empty strings match store semantics. */
    slots[6] = "SIX"; data.denom_mix[6] = 20;
    slots[0] = "ZERO-DENOM"; data.denom_mix[0] = 0;
    slots[3] = "NEGATIVE-DENOM"; data.denom_mix[3] = -1;
    slots[5] = "MIN-DENOM"; data.denom_mix[5] = INT_MIN;
    data.denom_mix[7] = 100; /* Positive denomination with NULL serial is excluded. */
    memcpy(&before, &data, sizeof(data));
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == sizeof(expected) / sizeof(expected[0]));
    assert(memcmp(view.serial, expected, sizeof(expected)) == 0);
    assert(data.sn_str[view.serial[0]][0] == '\0');
    assert(memcmp(&data, &before, sizeof(data)) == 0);
    matches_store(&view, &data);
}

static void test_duplicate_strings_and_slot_overwrite(void)
{
    counting_sim_t data = {0};
    page_02_list_data_t view = {0};
    char *slots[16] = {0};
    data.sn_str = slots; data.sn_capacity = 16;
    slots[3] = "DUPLICATE"; slots[8] = "DUPLICATE";
    data.denom_mix[3] = data.denom_mix[8] = 100;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 2 && view.serial[0] == 3 && view.serial[1] == 8);
    slots[3] = "CORRECTED";
    data.denom_mix[3] = 50;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 2 && view.serial[0] == 3);
    assert(strcmp(data.sn_str[view.serial[0]], "CORRECTED") == 0);
    assert(data.denom_mix[view.serial[0]] == 50);
    matches_store(&view, &data);
    slots[8] = NULL;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 1 && view.serial[0] == 3);
    data.denom_mix[3] = 0;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 0);
    matches_store(&view, &data);
}

static void test_invalid_serial_capacities(void)
{
    counting_sim_t data = {0};
    page_02_list_data_t view = {0};
    char *slots[1] = {"ONE"};
    const int invalid[] = {INT_MIN, -1, COUNTING_DATA_MAX_ITEMS + 1, INT_MAX};
    data.sn_str = slots; data.denom_mix[0] = 100;
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        data.sn_capacity = invalid[i];
        page_02_list_data_serials(&view, &data);
        assert(view.serial_count == 0);
        matches_store(&view, &data);
    }
    data.sn_capacity = 0;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 0);
    data.sn_capacity = 1;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 1 && view.serial[0] == 0);
    data.sn_str = NULL;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 0);
}

static void test_ten_thousand_serials(void)
{
    counting_sim_t data = {0};
    page_02_list_data_t view = {0};
    assert(counting_data_ensure_serial_capacity(&data, COUNTING_DATA_MAX_ITEMS));
    for (int i = COUNTING_DATA_MAX_ITEMS - 1; i >= 0; --i) {
        data.sn_str[i] = malloc(2);
        assert(data.sn_str[i] != NULL);
        memcpy(data.sn_str[i], "X", 2);
        data.denom_mix[i] = 100;
    }
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == COUNTING_DATA_MAX_ITEMS);
    assert(view.serial_count == counting_data_serial_valid_count(&data));
    for (unsigned int i = 0; i < COUNTING_DATA_MAX_ITEMS; ++i) assert(view.serial[i] == i);
    const int checkpoints[] = {0, 7, 255, 256, 9998, 9999};
    for (unsigned int i = 0; i < sizeof(checkpoints) / sizeof(checkpoints[0]); ++i)
        assert(view.serial[checkpoints[i]] == counting_data_serial_nth_valid_index(&data, checkpoints[i]));
    assert(counting_data_serial_nth_valid_index(&data, COUNTING_DATA_MAX_ITEMS) == -1);
    data.denom_mix[0] = data.denom_mix[5000] = data.denom_mix[9999] = 0;
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 9997);
    assert(view.serial[0] == 1 && view.serial[4998] == 4999);
    assert(view.serial[4999] == 5001 && view.serial[9996] == 9998);
    assert(view.serial_count == counting_data_serial_valid_count(&data));
    counting_data_clear_serials(&data);
    page_02_list_data_serials(&view, &data);
    assert(view.serial_count == 0);
}

int main(void)
{
    test_empty_and_null();
    test_denom_holes_and_zero_pcs();
    test_denom_maximum_and_invalid_declared_count();
    test_sparse_out_of_order_and_empty_serials();
    test_duplicate_strings_and_slot_overwrite();
    test_invalid_serial_capacities();
    test_ten_thousand_serials();
    puts("list data: 7 real-store projection regressions passed, including 10000 serial slots");
    return 0;
}
'''


class ListDataContracts(unittest.TestCase):
    def test_real_projection_matches_real_counting_store(self):
        compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
        self.assertIsNotNone(compiler, "Host C compiler required")
        with tempfile.TemporaryDirectory(prefix="un260-list-data-") as directory:
            source = Path(directory) / "test.c"
            source.write_text(HARNESS, encoding="utf-8")
            executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
            command = shlex.split(compiler) if os.environ.get("CC") else [compiler]
            command += ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(ROOT), str(source),
                        str(ROOT / "un260/lv_core/page_02_list_data.c"),
                        str(ROOT / "un260/counting/counting_data_store.c"),
                        "-o", str(executable)]
            if os.name != "nt":
                command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)

    def test_visible_text_uses_registered_i18n_ids(self):
        for suffix in ("DENOMINATIONS", "SERIAL_NUMBERS", "REJECT_ANALYSIS", "COL_SERIAL_NUMBER",
                       "COL_REASON", "TOTAL", "HISTORY", "PRINT", "MAIN", "REJECTED_NOTES",
                       "SCROLL", "PAGES", "RANGE_FMT", "PAGE_FMT", "NO_SERIAL_NUMBERS", "NO_REJECT_DETAILS"):
            self.assertIn("UI_TEXT_LIST_" + suffix, PAGE)
        for suffix in ("DENOM", "PCS", "AMOUNT", "NO"):
            self.assertIn("UI_TEXT_PAGE01_DETAIL_COL_" + suffix, PAGE)
        self.assertIn("ui_text_counting_reject_reason(", PAGE)
        self.assertNotIn("counting_reject_reason_get(", PAGE)
        literals = set(re.findall(r'"([^"\n]*)"', PAGE))
        self.assertTrue(literals.isdisjoint({"Denominations", "Serial numbers", "Reject analysis",
                                            "SERIAL NUMBER", "REASON", "TOTAL", "HISTORY", "PRINT",
                                            "MAIN", "REJECTED NOTES", "SCROLL", "PAGES",
                                            "%u-%u of %u", "%u / %u", "No serial numbers", "No reject details"}))

    def test_no_old_background_or_invisible_page_hotzones(self):
        self.assertNotRegex(PAGE, r'page_0[23][^"\n]*bg[^"\n]*\.png')
        self.assertNotIn("UI_BTN_STYLE_NO_FEEDBACK", PAGE)
        self.assertNotRegex(PAGE, r'page_03_[abc]_(?:up|down)_event_cb')
        self.assertIn("lv_recycled_list_set_paged(", PAGE)
        self.assertIn("lv_recycled_list_page_step(", PAGE)
        self.assertRegex(PAGE, r'lv_damped_button_set_enabled\s*\(\s*s->previous\s*,')
        self.assertRegex(PAGE, r'lv_damped_button_set_enabled\s*\(\s*s->next\s*,')

    def test_page_is_a_read_only_projection_with_owned_lifecycle(self):
        self.assertNotIn("counting_data_mutable(", PAGE)
        self.assertNotIn("protocol_send(", PAGE)
        self.assertNotIn("counting_data_serial_nth_valid_index(", PAGE)
        self.assertIn("page_02_list_data_denoms(", PAGE)
        self.assertIn("page_02_list_data_serials(", PAGE)
        self.assertRegex(PAGE, r'ui_frame_commit_cancel\s*\(\s*commit\s*,\s*NULL\s*\)')
        self.assertIn("lv_recycled_list_stop(", PAGE)
        self.assertIn("perf_profile_unwatch_invalidation(", PAGE)
        self.assertIn("lv_mem_free(view)", PAGE)


if __name__ == "__main__":
    unittest.main(verbosity=2)
