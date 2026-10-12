#!/usr/bin/env python3
"""Replay AUTO result ownership through unchanged production runtime functions.

The currency, count-info/control, data-store, MULTI, request, setting and frame
implementations are linked directly. Only peripheral UI/history/UART effects
are observed or stubbed. The small wire fixture in the C harness comes from the
2026-10-10 report; no controller replies or currency restoration are simulated.
This host test does not validate physical note recognition or board rendering.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", source, re.M)
    if signature is None:
        raise AssertionError("Missing production function: " + name)
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    opening = source.index("{", signature.start())
    depth = 0
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[signature.start():token.end()]
    raise AssertionError("Unclosed production function: " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-dir", type=Path,
                        help="Use preserved pre-fix files, without modifying the checkout")
    parser.add_argument("--output-dir", type=Path,
                        help="Retain generated source and binaries outside the repository")
    args = parser.parse_args()

    def path(relative):
        if args.baseline_dir and (args.baseline_dir / relative).is_file():
            return args.baseline_dir / relative
        return ROOT / relative

    count = path("un260/app_service/app_counting_runtime.c").read_text(encoding="utf-8")
    command = path("un260/app_service/app_command_runtime.c").read_text(encoding="utf-8")
    currency = path("un260/app_service/app_currency_runtime.c").read_text(encoding="utf-8")
    platform = path("un260/lv_system/platform_app.c").read_text(encoding="utf-8")
    setting = path("un260/app_service/app_setting_reply_basic.c").read_text(encoding="utf-8")
    # Compile the actual mode reply block and its real request ownership. Other
    # settings branches are unrelated to the result visibility transition.
    mode_begin = setting.index("    case 0x04:")
    mode_end = setting.index("    case 0x06:", mode_begin)
    mode = """static app_setting_reply_action_t mode_reply(uint8_t cmd,
        const uint8_t *buf, uint8_t len) {
        app_setting_reply_action_t actions = APP_SETTING_REPLY_ACTION_NONE;
        if (!buf) return actions;
        switch (cmd) {
    """ + setting[mode_begin:mode_end] + "default: break; } return actions; }"
    parts = [function(platform, "sim_reset_counting_data"),
             function(platform, "sim_reset_counting_result"),
             function(count, "app_counting_runtime_reset_session"),
             function(command, "app_command_runtime_clear_counting_data"),
             function(command, "app_command_runtime_handle_stacker_clear"),
             function(currency, "app_currency_runtime_handle_detected"),
             function(count, "app_counting_runtime_on_start_success"),
             function(count, "app_counting_runtime_handle_info"), mode]
    assert "if (cmd == 0x51) return app_command_runtime_handle_stacker_clear(buf, len);" in command
    assert command.index("if (cmd == 0x51)") < command.index("counting_action_handle_reply(")
    sources = ["un260/currency/currency_state.c", "un260/currency/currency_reply.c",
               "un260/counting/counting_info_reply.c", "un260/counting/counting_control_reply.c",
               "un260/counting/counting_data_store.c", "un260/counting/counting_multi.c",
               "un260/counting/counting_action_service.c", "un260/protocol/protocol_frame.c",
               "un260/protocol/protocol_request.c", "un260/protocol/mode_codec.c",
               "un260/machine_state/machine_state.c", "un260/app_service/setting_service.c"]

    def run(work):
        work.mkdir(parents=True, exist_ok=True)
        (work / "auto_currency_runtime_under_test.inc").write_text("\n\n".join(parts), encoding="utf-8")
        uart = work / "un260/lv_drivers/lv_drivers.h"
        uart.parent.mkdir(parents=True, exist_ok=True)
        uart.write_text("void uart_debug_printf(const char *format, ...);\n", encoding="utf-8")
        for optimization in ("-O0", "-O2"):
            binary = work / ("test-auto-currency-" + optimization[1:])
            flags = [os.environ.get("CC", "cc"), "-std=c11", optimization, "-g",
                     "-Wall", "-Wextra", "-Werror", "-ffunction-sections", "-fdata-sections",
                     "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                     "-I" + str(work)]
            if args.baseline_dir:
                flags += ["-I" + str(args.baseline_dir)]
            flags += ["-I" + str(ROOT), str(ROOT / "tools/test_auto_currency_result.c"),
                      *[str(path(p)) for p in sources], "-Wl,--gc-sections", "-o", str(binary)]
            subprocess.run(flags, check=True)
            subprocess.run([str(binary)], check=True, timeout=30)

    if args.output_dir:
        run(args.output_dir.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="un260-auto-currency-result-") as directory:
            run(Path(directory))


if __name__ == "__main__":
    main()
