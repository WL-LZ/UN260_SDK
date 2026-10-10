#!/usr/bin/env python3
"""Exercise extended image framing with real uart_recv, RX thread and queue.

The production runtime stream-routing prefix and handler are extracted so this
fixture checks its uint16_t boundaries without linking unrelated UI services.
The separate diagnostics fixture renders the actual page with LVGL.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def function(text, signature):
    start = text.index(signature)
    begin = text.index("{", start)
    end, depth = begin + 1, 1
    while depth:
        if text[end] == "{": depth += 1
        elif text[end] == "}": depth -= 1
        end += 1
    return text[start:end]

protocol = (ROOT / "un260/app_service/app_protocol_runtime.c").read_text()
command = (ROOT / "un260/app_service/app_command_runtime.c").read_text()
handler = function(protocol, "static void handle_stream_data_reply(")
protocol_prefix = function(protocol, "bool app_protocol_runtime_handle_reply(").split(
    "    if (frame_len > PROTOCOL_FRAME_MAX_SIZE)")[0] + "    return false;\n}\n"
command_prefix = function(command, "static bool app_command_runtime_dispatch(").split(
    "    /* Every other controller reply")[0] + "    return false;\n}\n"
sources = ["tools/test_image_frame_receive.c", "un260/protocol/protocol_frame.c",
           "un260/protocol/protocol_frame_parser.c", "un260/protocol/stream_data_reply.c",
           "un260/protocol/protocol_frame_queue.c", "un260/protocol/protocol_request.c",
           "un260/protocol/protocol_rx_service.c", "un260/lv_drivers/uart_io.c",
           "un260/lv_system/app_clock.c", "un260/app_service/app_runtime_wakeup.c"]
with tempfile.TemporaryDirectory(prefix="un260-image-receive-") as directory:
    work = Path(directory)
    (work / "image_runtime_route.inc").write_text(handler + "\n" + protocol_prefix + "\n" + command_prefix)
    binary = work / "test"
    for optimization in ("-O0", "-O2"):
        flags = [os.environ.get("CC", "cc"), "-std=c11", "-D_GNU_SOURCE", optimization,
                 "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                 "-fno-sanitize-recover=all", "-pthread", "-I" + str(ROOT), "-I" + str(work)]
        subprocess.run(flags + [str(ROOT / item) for item in sources] +
                       ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)
