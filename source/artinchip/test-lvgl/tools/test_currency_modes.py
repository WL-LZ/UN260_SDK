#!/usr/bin/env python3
"""Test production currency state, requests, ACK projection and selector order.

Only the LVGL, persistence and UART edges are simulated. Actual page transition
functions and the 0x04 reply branch are compiled unchanged with the real model
and request services. No device, existing config or history files are touched.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(source, opening):
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[opening:token.end()]
    raise AssertionError("Unclosed production block")


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, name
    opening = source.index("{", signature.start())
    return source[signature.start():opening] + block(source, opening)


def without_includes(source):
    return "\n".join(line for line in source.splitlines()
                     if not line.startswith("#include"))


def main():
    page = (ROOT / "un260/lv_core/page_07_curr.c").read_text(encoding="utf-8")
    model = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_model.c").read_text(encoding="utf-8")
    internal = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_internal.h").read_text(encoding="utf-8")
    reply = (ROOT / "un260/app_service/app_setting_reply_basic.c").read_text(encoding="utf-8")
    enum = re.search(r"typedef enum \{[^{}]*\} page07_curr_view_mode_t;", internal).group()
    state = re.search(r"typedef struct \{[^{}]*\} page07_curr_model_state_t;", internal).group()
    transition = re.search(r"typedef enum \{[^{}]*\} curr_mode_transition_t;", page).group()
    pending = re.search(r"static struct \{[^{}]*\} g_curr_mode_transition;", page).group()
    mode_case = reply.index("case 0x04:")
    mode_block = block(reply, reply.index("{", mode_case))
    mode_reply = """app_setting_reply_action_t app_setting_reply_handle_basic(
        uint8_t cmd, const uint8_t *buf, uint8_t len) {
        app_setting_reply_action_t actions = APP_SETTING_REPLY_ACTION_NONE;
        switch (cmd) { case 0x04: """ + mode_block + """
        break; default: break; } return actions;
    }"""
    parts = [without_includes(model)]
    for name in ("curr_select_and_exit_abs", "page_07_curr_apply_mode_result",
                 "page_07_curr_poll_selection", "page_07_curr_cancel_pending_selection",
                 "page_07_curr_reset_pending_selection", "page_07_curr_apply_switch_result",
                 "curr_grid_item_click_cb"):
        parts.append(function(page, name))
    parts.append(mode_reply)
    with tempfile.TemporaryDirectory(prefix="un260-currency-modes-") as temp:
        work = Path(temp)
        (work / "currency_mode_types.inc").write_text(
            "\n".join([enum, state, transition, pending]), encoding="utf-8")
        (work / "currency_mode_under_test.inc").write_text("\n".join(parts), encoding="utf-8")
        stub = work / "un260/lv_drivers/lv_drivers.h"
        stub.parent.mkdir(parents=True)
        stub.write_text("void uart_debug_printf(const char *format, ...);\n", encoding="utf-8")
        binary = work / ("currency-modes.exe" if os.name == "nt" else "currency-modes")
        sources = ["tools/test_currency_modes.c", "un260/currency/currency_state.c",
                   "un260/currency/currency_service.c", "un260/currency/currency_reply.c",
                   "un260/app_service/setting_service.c", "un260/protocol/protocol_request.c",
                   "un260/protocol/mode_codec.c", "un260/machine_state/machine_state.c"]
        for optimization in ("-O0", "-O2"):
            command = [os.environ.get("CC", "cc"), "-std=c11", optimization,
                       "-Wall", "-Wextra", "-Werror", f"-I{work}", f"-I{ROOT}"]
            if os.name != "nt":
                command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
            subprocess.run(command + [str(ROOT / path) for path in sources] +
                           ["-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
