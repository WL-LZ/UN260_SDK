#!/usr/bin/env python3
"""Host regressions for the actual Main contact/transition control functions.

Resource allocation and page/driver boundaries are observable stubs. This does
not validate panel pixels, GE rendering, the physical touch driver or firmware.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

app = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
source = (app / 'un260/lv_core/page_01_main.c').read_text()
runtime = (app / 'un260/app_service/app_command_runtime.c').read_text()

def fn(text, name):
    match = re.search(r'^(?:static\s+)?(?:void|bool)\s+' + re.escape(name) + r'\([^;]*?\)\s*(?://[^\n]*)?\s*\{', text, re.M)
    assert match, name
    opening = text.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', text[opening:], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0: return text[match.start():opening + token.end()]
    raise AssertionError(name)

contact_start = source.index('typedef struct {\n    lv_obj_t *button;\n    lv_point_t start;')
contact_end = source.index('static void page_01_display_transition_y', contact_start)
production = source[contact_start:contact_end]
for name in ('page_01_display_transition_y', 'page_01_display_transition_stop',
             'page_01_display_transition_ready', 'page_01_display_change_allowed',
             'page_01_display_switch_commit',
             'page_01_main_toggle_display_mode'):
    production += '\n' + fn(source, name)
production += '\n' + fn(runtime, 'app_command_runtime_count_start_busy')
production += '\n' + fn(runtime, 'app_command_runtime_counting_confirmed')

# Existing facade routes must not dereference the old fullscreen widget tree.
for name in ('page_01_mode_switch_refre', 'page_01_err_num_refre', 'page_01_curr_img_refre',
             'page_01_bottom_a_refresh_add', 'page_01_bottom_c_refresh_speed',
             'page_01_main_refresh_totals', 'page_01_main_detail_refresh_rows_only',
             'page_01_update_language_texts'):
    assert 'panel_main_refresh(' in fn(source, name), name
assert 'panel_main_set_visible(false)' in fn(source, 'page_01_main_suspend')
assert 'page_01_display_transition_stop()' in fn(source, 'page_01_main_suspend')
assert 'page_01_display_transition_stop()' in fn(source, 'ui_main_destroy')
assert 's_display_surface.source = NULL' in production
layout = (app / 'un260/lv_core/page_01_main_layout.c').read_text()
assert 'event==LV_EVENT_PRESSED && !hold_excluded(&editor.point)' in layout

with tempfile.TemporaryDirectory(prefix='un260-display-mode-') as directory:
    work = Path(directory)
    (work / 'main_display_mode_under_test.h').write_text(production)
    command = ['cc', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=undefined', '-fno-sanitize-recover=undefined',
               '-I' + str(work), '-I' + str(app / 'un260/lv_system'),
               str(Path(__file__).with_suffix('.c')),
               str(app / 'un260/lv_system/ui_display_mode.c'), '-o', str(work / 'test')]
    subprocess.run(command, check=True, timeout=60)
    subprocess.run([str(work / 'test')], check=True, timeout=15)
