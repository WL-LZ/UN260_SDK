#!/usr/bin/env python3
"""Host tests; no LVGL or device I/O. Real storage uses a temporary POSIX path."""
from pathlib import Path
import argparse
import importlib.util
import json
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
                 'un260/lv_system/ui_text_widget.c', 'un260/counting/counting_reject_reason.c',
                 'i18n/generated/lv_i18n.c']
        subprocess.run([args.cc, *flags, *(str(ROOT / file) for file in files), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        # An enabled fixture exercises production resource gates without
        # marking the actual unfinished rendering rollout as released.
        (folder / 'i18n/generated').mkdir(parents=True)
        registry = (ROOT / 'i18n/generated/ui_locale_registry.inc').read_text()
        (folder / 'i18n/generated/ui_locale_registry.inc').write_text(registry.replace('false', 'true'))
        guarded = folder / 'ui_lang.c'
        guarded.write_text((ROOT / 'un260/lv_system/ui_lang.c').read_text())
        guard_exe = folder / 'resource_guard'
        subprocess.run([args.cc, *flags, '-I'+str(ROOT / 'un260/lv_system'), str(ROOT / 'tools/test_locale_resource_guard.c'),
                        str(guarded), str(ROOT / 'i18n/generated/lv_i18n.c'),
                        '-o', str(guard_exe)], check=True)
        subprocess.run([str(guard_exe)], check=True)
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
    original_i18n = generator.I18N
    with tempfile.TemporaryDirectory(prefix='un260-plural-gate-') as directory:
        fixture = Path(directory)
        (fixture / 'i18n/locales').mkdir(parents=True)
        (fixture / 'i18n/fragments').mkdir()
        (fixture / 'main.c').write_text('')
        def save(name, data):
            (fixture / 'i18n' / name).write_text(json.dumps(data), encoding='utf-8')
        en = dict(id=0, tag='en', code='EN', name='English', direction='ltr',
                  enabled=True, translations_ready=True, font_ready=True,
                  shaping_ready=True, font_profile='test')
        save('locales/en.json', {'en': {'%u notes': {'one': '%u note', 'other': '%u notes'}}})
        generator.ROOT, generator.I18N = fixture, fixture / 'i18n'
        try:
            for tag in ('zh-Hans', 'ko', 'ja', 'ar'):
                locale = dict(en, id=1, tag=tag, direction='rtl' if tag == 'ar' else 'ltr')
                save('locales.json', [en, locale])
                save('locales/' + tag + '.json', {tag: {'%u notes': {'other': 'N: %u'}}})
                if tag == 'ar':
                    try:
                        generator.catalogues()
                    except ValueError as error:
                        assert 'incomplete plural forms' in str(error)
                    else:
                        raise AssertionError('Arabic release accepted without six plural categories')
                    save('locales/ar.json', {'ar': {'%u notes': {form: 'N: %u' for form in
                         ('zero', 'one', 'two', 'few', 'many', 'other')}}})
                generator.catalogues()
        finally:
            generator.ROOT, generator.I18N = original_root, original_i18n
    print('Four-language plural release gates PASS')


if __name__ == '__main__':
    main()
