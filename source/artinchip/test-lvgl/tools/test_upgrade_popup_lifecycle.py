#!/usr/bin/env python3
"""Exercise production popup callbacks at timer, storage and protocol boundaries."""
from pathlib import Path
import subprocess
import tempfile
import re

def function(source, name):
    match = re.search(r'^static [\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[start:], re.S):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[match.start():start + token.end()]
    raise AssertionError(name)

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/lv_components/lv_upgrade_popup.c').read_text()
names = ('upgrade_popup_reboot_timer_cb', 'upgrade_popup_on_reboot_later_click',
         'upgrade_popup_on_reboot_click', 'upgrade_popup_on_result_confirm_click',
         'upgrade_popup_result_timer_cb')
with tempfile.TemporaryDirectory(prefix='un260-upgrade-popup-') as directory:
    temp = Path(directory)
    (temp / 'actual_upgrade_popup_functions.h').write_text(
        '\n\n'.join(function(source, name) for name in names))
    exe = temp / 'popup'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
                    f'-I{root}', f'-I{temp}', str(root / 'tools/test_upgrade_popup_lifecycle.c'),
                    str(root / 'un260/app_service/upgrade_session.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
