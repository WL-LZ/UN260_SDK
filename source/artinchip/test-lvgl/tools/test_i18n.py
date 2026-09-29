#!/usr/bin/env python3
"""Host tests; no LVGL or device I/O. Real storage uses a temporary POSIX path."""
from pathlib import Path
import argparse
import importlib.util
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='un260-i18n-test-') as folder:
        folder = Path(folder)
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT)]
        if args.sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        exe = folder / ('core.exe' if os.name == 'nt' else 'core')
        files = ['tools/test_i18n_core.c', 'un260/lv_system/ui_i18n.c',
                 'un260/lv_system/ui_lang.c', 'un260/lv_system/ui_text_page.c',
                 'un260/lv_system/ui_text_widget.c', 'i18n/generated/lv_i18n.c']
        subprocess.run([args.cc, *flags, *(str(ROOT / file) for file in files), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        if os.name != 'nt':
            exe = folder / 'store'
            path = folder / 'state'
            subprocess.run([args.cc, *flags, '-D_GNU_SOURCE', '-DUI_STATE_DIR="' + str(path) + '"',
                            str(ROOT / 'tools/test_locale_store.c'), str(ROOT / 'un260/storage/ui_locale_store.c'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
    spec = importlib.util.spec_from_file_location('generator', ROOT / 'tools/gen_i18n.py')
    generator = importlib.util.module_from_spec(spec); spec.loader.exec_module(generator)
    assert generator.format_args('%u %s') == generator.format_args('%2$s %1$u')
    assert generator.format_args('%lu') != generator.format_args('%u')
    assert generator.format_args('%.*s') != generator.format_args('%s')
    try:
        generator.format_args('%n')
    except ValueError:
        pass
    else:
        raise AssertionError('%n accepted')
    assert generator.format_args('100%') == {}
    assert generator.format_args('Ready 100%', strict=False) == {}
    assert generator.format_args('%u%%', strict=True) == {1: 'unsigned'}
    assert generator.format_args('%s %u', strict=True) == generator.format_args('%2$u %1$s', strict=True)
    assert generator.format_args('%.*s', strict=True) == generator.format_args('%2$.*1$s', strict=True)
    for invalid in ('%u %q', '%u %', '%u %1', '%2$s %u', '%2$*s', '%*1$s', '%u %02%'):
        try:
            generator.format_args(invalid, strict=True)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid printf accepted: ' + invalid)
    # Test fixtures are not production text, even when they use the same macros.
    original_root = generator.ROOT
    with tempfile.TemporaryDirectory(prefix='un260-i18n-extract-') as directory:
        fixture = Path(directory)
        (fixture / 'un260/module/tests').mkdir(parents=True)
        (fixture / 'main.c').write_text('UI_N_("Visible copy")')
        (fixture / 'un260/module/tests/fake.c').write_text('ui_tr("Test-only key")')
        generator.ROOT = fixture
        try:
            assert generator.marked_keys() == {'Visible copy'}
        finally:
            generator.ROOT = original_root
    print('I18n format validator and production extraction PASS')


if __name__ == '__main__':
    main()
