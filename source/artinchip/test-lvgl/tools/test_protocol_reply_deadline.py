#!/usr/bin/env python3
"""Compile production parser/queue/request with deterministic monotonic time."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sources = ["tools/test_protocol_reply_deadline.c", "un260/protocol/protocol_request.c",
           "un260/protocol/protocol_frame_queue.c", "un260/protocol/protocol_frame.c",
           "un260/protocol/protocol_frame_parser.c", "un260/protocol/stream_data_reply.c"]
with tempfile.TemporaryDirectory(prefix="un260-reply-deadline-") as directory:
    binary = Path(directory) / "test"
    for optimization in ("-O0", "-O2"):
        command = [os.environ.get("CC", "cc"), "-std=c11", "-D_GNU_SOURCE", optimization,
                   "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                   "-fno-sanitize-recover=all", "-pthread", "-I" + str(root)]
        subprocess.run(command + [str(root / item) for item in sources] + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
