#!/usr/bin/env python3
"""Exercise production package detection and popup eligibility with real files."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'^(?:static\s+)?[\w \n*]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
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


service = (root / 'un260/lv_core/ui_upgrade_service.c').read_text()
popup = (root / 'un260/lv_components/lv_upgrade_popup.c').read_text()
types = re.search(r'typedef struct \{.*?\} ui_upgrade_service_ctx_t;', service, re.S).group()
globals_ = service[service.index('static ui_upgrade_service_ctx_t g_ui_upgrade_service;'):
                   service.index('static bool ui_upgrade_service_file_exists')]
names = ('ui_upgrade_service_file_exists', 'ui_upgrade_service_hash_file_fnv1a64',
         'ui_upgrade_service_hash_cache_clear', 'ui_upgrade_service_hash_file_cached',
         'ui_upgrade_service_parse_hash',
         'ui_upgrade_service_read_installed_bundle_hash',
         'ui_upgrade_service_get_package_hash_status', 'ui_upgrade_service_set_status',
         'ui_upgrade_service_reset', 'ui_upgrade_service_detect',
         'ui_upgrade_service_can_reinstall', 'ui_upgrade_service_launch',
         'ui_upgrade_service_start')
with tempfile.TemporaryDirectory(prefix='un260-upgrade-detection-') as directory:
    temp = Path(directory)
    (temp / 'actual_upgrade_package_functions.h').write_text(
        types + '\n' + globals_ + '\n' +
        '\n\n'.join(function(service, name) for name in names) + '\n' +
        function(popup, 'upgrade_popup_on_retry_click') + '\n' +
        function(popup, 'lv_upgrade_popup_process_detect'))
    fixture = temp / 'fixture'
    fixture.mkdir()
    executable = temp / 'detection'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    f'-I{root}', f'-I{temp}', str(root / 'tools/test_upgrade_package_detection.c'),
                    str(root / 'un260/app_service/upgrade_session.c'),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(fixture)], check=True)
