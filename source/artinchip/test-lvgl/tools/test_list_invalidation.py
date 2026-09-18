#!/usr/bin/env python3
"""Source contracts for List invalidation integration; not an LVGL/device test.

The page owns coalescing, anchor policy and hidden-page work. These checks keep
the application callbacks publishing model changes without adding UART writes
or replacing the existing Main-page notifications.
"""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = (ROOT / "un260/app_service/app_counting_runtime.c").read_text(encoding="utf-8")
PLATFORM = (ROOT / "un260/lv_system/platform_app.c").read_text(encoding="utf-8")
DETAIL = (ROOT / "un260/counting/counting_reject_sn_reply.c").read_text(encoding="utf-8")
DENOM = (ROOT / "un260/counting/counting_denom_reply.c").read_text(encoding="utf-8")
MAIN = (ROOT / "un260/lv_core/page_01_main.c").read_text(encoding="utf-8")
MAIN_DETAIL = (ROOT / "un260/lv_core/page_01_main_detail.c").read_text(encoding="utf-8")


def function(source, name):
    match = re.search(r"^[\w *]+\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", source, re.M)
    if match is None:
        raise AssertionError(f"Function not found: {name}")
    start = source.index("{", match.start())
    depth = 1
    end = start + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


def mark(section):
    return f"page_02_list_section_mark_dirty(PAGE_02_SECTION_{section});"


class ListInvalidationContracts(unittest.TestCase):
    def assert_before(self, source, first, second):
        self.assertIn(first, source)
        self.assertIn(second, source)
        self.assertLess(source.index(first), source.index(second))

    def test_live_totals_publish_a_and_reject_summary_without_reset(self):
        body = function(RUNTIME, "app_counting_runtime_refresh_compact")
        for section in "AC":
            self.assert_before(body, mark(section), "ui_frame_commit_defer(")
        self.assertIn("smart_island_update_counting", body)
        self.assertNotIn("page_02_list_report_reset", body)

    def test_finished_totals_invalidate_even_when_main_is_not_active(self):
        body = function(RUNTIME, "app_counting_runtime_handle_info")
        finished = body[body.index("COUNTING_INFO_REPLY_FINISHED"):]
        for section in "AC":
            self.assert_before(finished, mark(section), "app_counting_runtime_main_page_active()")
        self.assertIn("smart_island_notify_count_end(NULL);", finished)
        self.assertNotIn("page_02_list_report_reset", finished)

    def test_new_result_resets_list_anchors_only_at_new_start_or_data_reset(self):
        start = function(RUNTIME, "app_counting_runtime_on_start_success")
        self.assert_before(start, "currency_state_begin_count_session();", "page_02_list_report_reset();")
        reset = function(PLATFORM, "sim_reset_counting_data")
        for expression in ("sim_data->total_pcs = 0;", "sim_data->total_amount = 0.0f;", "sim_data->err_expected = 0;"):
            self.assert_before(reset, expression, "page_02_list_report_reset();")
        self.assert_before(reset, "page_02_list_report_reset();", "ui_refresh_main_page();")
        self.assertIn("sim_reset_counting_data(sim_data, true);", function(PLATFORM, "sim_reset_for_currency"))
        self.assertIn("sim_reset_counting_data(sim_data, false);", function(PLATFORM, "sim_reset_counting_result"))
        self.assertEqual(RUNTIME.count("page_02_list_report_reset();"), 1)
        self.assertEqual(PLATFORM.count("page_02_list_report_reset();"), 1)

    def test_serial_stream_start_invalidates_cleared_b_and_preserves_main_reset(self):
        body = function(RUNTIME, "app_counting_runtime_on_serial_data_started")
        self.assert_before(body, mark("B"), "page_01_main_scroll_reset();")
        self.assertNotIn("page_02_list_report_reset", body)
        # The new public Main wrapper must still reset every detail anchor;
        # accepting a renamed call alone would hide a partial-reset regression.
        self.assertIn("page_01_main_detail_reset();", function(MAIN, "page_01_main_scroll_reset"))
        reset = function(MAIN_DETAIL, "page_01_main_detail_reset")
        self.assertIn("view->tap.pressed = false;", reset)
        self.assertRegex(reset, r"for\s*\(unsigned i\s*=\s*0;\s*i\s*<\s*DETAIL_SECTIONS;\s*\+\+i\)")
        self.assertIn("lv_recycled_list_refresh(view->section[i].list, 0, true);", reset)
        self.assertIn("view->section[i].dirty = true;", reset)
        self.assertIn("if (view->visible) section_refresh(&view->section[view->active]);", reset)

    def test_serial_completion_keeps_other_sections_and_reading_anchors(self):
        body = function(RUNTIME, "app_counting_runtime_on_serial_report_ready")
        self.assertIn("page_02_list_section_data_ready(PAGE_02_SECTION_B);", body)
        self.assertNotIn("page_02_list_report_reset", body)
        self.assertNotIn("PAGE_02_SECTION_A", body)
        self.assertNotIn("PAGE_02_SECTION_C", body)

    def test_each_serial_item_publishes_b_before_main_visibility_guard(self):
        body = function(RUNTIME, "app_counting_runtime_on_serial_item_changed")
        self.assert_before(body, mark("B"), "app_counting_runtime_main_page_active()")
        self.assertIn("page_01_detail_section_get() == PAGE_01_DETAIL_SECTION_B", body)
        self.assertIn("ui_frame_commit_defer(app_counting_visual_commit, NULL, 2U)", body)
        hooks = function(RUNTIME, "app_counting_runtime_handle_detail")
        self.assertIn("hooks.on_serial_item_changed = app_counting_runtime_on_serial_item_changed;", hooks)
        for name in ("counting_sn_reply_handle", "counting_sn_push_handle"):
            self.assertIn("counting_sn_notify_item(hooks);", function(DETAIL, name))

    def test_partial_reject_and_summary_publish_without_navigation_or_reset(self):
        reject = function(RUNTIME, "app_counting_runtime_on_reject_report_changed")
        self.assertIn("page_02_list_section_data_ready(PAGE_02_SECTION_C);", reject)
        summary = function(RUNTIME, "app_counting_runtime_on_summary_changed")
        for section in "AC":
            self.assert_before(summary, mark(section), "if (refresh_main")
        self.assertIn("smart_island_refresh_summary();", summary)
        for body in (reject, summary):
            self.assertNotIn("ui_manager_switch", body)
            self.assertNotIn("page_02_list_report_reset", body)

    def test_denom_completion_publishes_a_and_keeps_main_refresh(self):
        body = function(RUNTIME, "app_counting_runtime_on_main_data_changed")
        self.assertIn(mark("A"), body)
        self.assertIn("ui_refresh_main_page();", body)

    def test_reject_start_invalidates_only_after_parser_accepts_and_clears(self):
        body = function(RUNTIME, "app_counting_runtime_handle_detail")
        self.assertIn("result = counting_reject_sn_reply_dispatch(cmd,", body)
        guard = "if (cmd == 0x0C && result == COUNTING_DETAIL_REPLY_START)"
        self.assert_before(body, "result = counting_reject_sn_reply_dispatch(cmd,", guard)
        self.assert_before(body, guard, mark("C"))
        self.assertNotIn("page_02_list_report_reset", body)
        reject = function(DETAIL, "counting_reject_reply_handle")
        self.assert_before(reject, "counting_data_clear_errors(sim_data);", "return COUNTING_DETAIL_REPLY_START;")

    def test_detail_parser_remains_view_independent_and_keeps_wire_boundaries(self):
        self.assertNotIn("page_02", DETAIL)
        self.assertNotIn("page_02", DENOM)
        serial = function(DETAIL, "counting_sn_reply_handle")
        self.assert_before(serial, "counting_data_clear_serials(sim_data);", "hooks->on_serial_data_started")
        self.assertIn("index = (int)sequence - 1;", serial)
        self.assert_before(serial, "free(sim_data->sn_str[index]);", "sim_data->sn_str[index] = sn_copy;")
        self.assertIn("hooks->on_serial_report_ready", serial)
        reject = function(DETAIL, "counting_reject_reply_handle")
        self.assertIn("if (sim_data->err_expected == 0)", reject)
        self.assertIn("if (detail->wait_sn_after_reject_end && session &&", reject)
        self.assertIn("session->phase != COUNTING_SESSION_ACTIVE", reject)
        self.assertIn("protocol_send(0x0D, sn_req, 2);", reject)
        self.assertIn("protocol_send(0x0C, &reject_cmd, 1);", function(DENOM, "counting_denom_handle_end"))

    def test_ui_notifications_add_no_protocol_requests(self):
        for name in ("app_counting_runtime_refresh_compact", "app_counting_runtime_on_main_data_changed",
                     "app_counting_runtime_on_reject_report_changed", "app_counting_runtime_on_summary_changed",
                     "app_counting_runtime_on_serial_data_started", "app_counting_runtime_on_serial_report_ready",
                     "app_counting_runtime_on_serial_item_changed"):
            self.assertNotIn("protocol_send", function(RUNTIME, name))


if __name__ == "__main__":
    unittest.main(verbosity=2)
