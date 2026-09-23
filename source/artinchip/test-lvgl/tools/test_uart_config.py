#!/usr/bin/env python3
"""Exercise real UART configuration and runtime without operating hardware."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cases = {
    'termios': ['tools/test_uart_config.c', 'un260/lv_drivers/lv_drivers.c'],
    'runtime': ['tools/test_app_serial_runtime.c', 'un260/app_service/app_serial_runtime.c',
                'un260/protocol/protocol_frame.c'],
}
with tempfile.TemporaryDirectory(prefix='un260-uart-test-') as tmp:
    for opt in ('-O0', '-O2'):
        for name, sources in cases.items():
            exe = str(Path(tmp) / name)
            subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', opt, '-Wall', '-Wextra',
                            '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            '-I' + str(root), *[str(root / p) for p in sources],
                            *(['-Wl,--wrap=ioctl'] if name == 'termios' else []),
                            '-o', exe], check=True)
            subprocess.run([exe], check=True)
