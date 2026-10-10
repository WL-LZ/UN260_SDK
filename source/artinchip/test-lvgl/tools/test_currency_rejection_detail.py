#!/usr/bin/env python3
"""Exercise the currency-rejection exception through production RX dispatch.

Compile the real currency/request/reply/fault stores and unmodified extracted
runtime/dispatch functions. Only UI, UART and unrelated command sinks are spies.
No controller, configuration or history files are changed by this host test.
"""
from pathlib import Path
import os
import subprocess
import tempfile

from test_currency_modes import function
from test_i18n_support import with_i18n

ROOT = Path(__file__).resolve().parents[1]


def main():
    runtime = (ROOT / "un260/app_service/app_currency_runtime.c").read_text(encoding="utf-8")
    command = (ROOT / "un260/app_service/app_command_runtime.c").read_text(encoding="utf-8")
    recovery = (ROOT / "un260/app_service/app_fault_recovery.c").read_text(encoding="utf-8")
    parts = ["static struct { uint8_t command; uint64_t received_ms; } setting_rejection; static uint8_t deferred_setting_rejection;"]
    parts += [function(recovery, name) for name in ('app_fault_recovery_cancel_setting_detail', 'setting_command', 'app_fault_recovery_cancel_setting_presentation', 'app_fault_recovery_resolve_setting_rejection', 'setting_request_started', 'app_fault_recovery_report_setting_rejection', 'app_fault_recovery_observe_setting_detail', 'present_deferred_setting_rejection')]
    parts += [function(runtime, "app_currency_runtime_handle_reply")]
    parts.append(function(command, "app_command_runtime_dispatch"))
    with tempfile.TemporaryDirectory(prefix="un260-currency-rejection-") as directory:
        work = Path(directory)
        (work / "currency_rejection_under_test.inc").write_text("\n\n".join(parts), encoding="utf-8")
        driver = work / "un260/lv_drivers/lv_drivers.h"
        driver.parent.mkdir(parents=True)
        driver.write_text("void uart_debug_printf(const char *format, ...);\n", encoding="utf-8")
        sources = ["tools/test_currency_rejection_detail.c", "un260/currency/currency_state.c",
                   "un260/currency/currency_service.c", "un260/currency/currency_reply.c",
                   "un260/protocol/protocol_request.c", "un260/protocol/protocol_frame.c",
                   "un260/machine_state/machine_fault.c"]
        binary = work / ("currency-rejection.exe" if os.name == "nt" else "currency-rejection")
        for optimization in ("-O0", "-O2"):
            command = [os.environ.get("CC", "cc"), "-std=c11", optimization,
                       "-Wall", "-Wextra", "-Werror", f"-I{work}", f"-I{ROOT}"]
            if os.name != "nt":
                command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie"]
            subprocess.run(command + [str(ROOT / path) for path in sources] +
                           list(map(str, with_i18n([], ROOT))) + ["-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
