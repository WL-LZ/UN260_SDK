#!/usr/bin/env python3
"""Host regressions for MULTI result capability, with no UART or filesystem I/O."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, name
    opening = source.index("{", signature.start())
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[signature.start():token.end()]
    raise AssertionError(name)


def main():
    parts = []
    for path, names in (
        ("un260/lv_system/ui_history_data_fs.c", ["ui_history_record_build_from_session"]),
        ("un260/lv_core/lv_page_event.c", ["page_01_print_btn_event_cb"]),
        ("un260/lv_system/ui_export_data.c", ["ui_export_data_request"]),
        ("un260/lv_core/page_18_pure.c", ["pure_format_amount", "pure_format_pcs", "pure_refresh_values"]),
        ("un260/lv_core/page_02_list.c", ["text_set", "list_monetary_result_supported", "row_bind", "commit"]),
        ("un260/lv_components/smart_island/smart_island_view.c",
         ["smart_island_get_currency_code", "smart_island_rebuild_scene_texts"]),
        ("un260/lv_components/smart_island/smart_island_action.c", ["smart_island_show_qr_popup"]),
    ):
        source = (ROOT / path).read_text(encoding="utf-8")
        parts.extend(function(source, name) for name in names)
    with tempfile.TemporaryDirectory(prefix="un260-multi-safety-") as directory:
        work = Path(directory)
        (work / "multi_safety_under_test.inc").write_text("\n".join(parts), encoding="utf-8")
        binary = work / ("multi-safety.exe" if os.name == "nt" else "multi-safety")
        sources = ["tools/test_multi_result_safety.c", "un260/counting/counting_data_store.c",
                   "un260/counting/counting_history_service.c", "un260/currency/currency_state.c",
                   "un260/lv_system/ui_qr_data.c", "un260/counting/counting_reject_reason.c"]
        sources.append("un260/lv_core/page_02_list_data.c")
        for optimization in ("-O0", "-O2"):
            command = [os.environ.get("CC", "cc"), "-std=c11", optimization,
                       "-Wall", "-Wextra", "-Werror", f"-I{ROOT}", f"-I{work}"]
            if os.name != "nt":
                command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
            subprocess.run(command + [str(ROOT / path) for path in sources]
                           + ["-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
