#!/usr/bin/env python3
"""UN260 language maintenance. Run --help; changes never enable unverified locales."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import gen_i18n as g

FORMS = {'zero', 'one', 'two', 'few', 'many', 'other'}


def fingerprint(english):
    return hashlib.sha256(json.dumps(english, ensure_ascii=False, sort_keys=True).encode()).hexdigest()


def replace_files(changes):
    """Validate first, stage all files, rollback on an ordinary write failure."""
    originals = {p: p.read_bytes() if p.exists() else None for p in changes}
    staged = {}
    try:
        for path, value in changes.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            fd, name = tempfile.mkstemp(prefix='.' + path.name, dir=path.parent)
            staged[path] = Path(name)
            with os.fdopen(fd, 'wb') as stream:
                stream.write(g.json_text(value).encode('utf-8'))
                stream.flush()
                os.fsync(stream.fileno())
        for path, stage in staged.items():
            os.replace(stage, path)
    except BaseException:
        for path, data in originals.items():
            if data is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(data)
        raise
    finally:
        for stage in staged.values():
            stage.unlink(missing_ok=True)


def selected(tag):
    registry, catalogs = g.catalogues()
    item = next((x for x in registry if x['tag'] == tag), None)
    if item is None or tag == 'en':
        raise ValueError('Select a registered non-English locale')
    return item, catalogs


def add(args):
    registry, _ = g.catalogues()
    if not re.fullmatch(r'[a-zA-Z]{2,8}(?:-[a-zA-Z0-9]{1,8})*', args.locale):
        raise ValueError('Invalid locale tag')
    if any(x['tag'].lower() == args.locale.lower() for x in registry):
        raise ValueError('Locale already registered')
    profiles = g.read_json(g.I18N / 'font_profiles.json')
    if args.font_profile not in profiles:
        raise ValueError('Configure the font profile before adding a locale')
    forms = args.plural_forms.split(',')
    if not set(forms) <= FORMS or 'other' not in forms or len(set(forms)) != len(forms):
        raise ValueError('Invalid plural forms')
    number = max(x['id'] for x in registry) + 1
    if number >= 65535:
        raise ValueError('Locale ID space exhausted')
    registry.append(dict(id=number, tag=args.locale, code=args.code, name=args.name,
                         direction=args.direction, enabled=False, translations_ready=False,
                         font_ready=False, shaping_ready=False,
                         font_profile=args.font_profile, plural_forms=forms))
    replace_files({g.I18N / 'locales.json': registry,
                   g.I18N / 'locales' / (args.locale + '.json'): {args.locale: {}}})
    print('Registered disabled locale', args.locale, 'stable ID', number)


def export(args):
    item, catalogs = selected(args.locale)
    forms = item.get('plural_forms', ['zero','one','two','few','many','other'] if args.locale == 'ar' else ['other'])
    entries = {}
    for key, source in catalogs['en'].items():
        value = catalogs[args.locale].get(key)
        if isinstance(source, dict):
            value = {form: value.get(form, '') if isinstance(value, dict) else '' for form in forms}
        entries[key] = {'source': source, 'translation': value or ''}
    replace_files({args.file: dict(schema=1, locale=args.locale,
                                  source_sha256=fingerprint(catalogs['en']), entries=entries)})
    print('Exported', len(entries), 'translation keys:', args.file)


def import_translation(args):
    item, catalogs = selected(args.locale)
    data = g.read_json(args.file)
    english = catalogs['en']
    if data.get('schema') != 1 or data.get('locale') != args.locale:
        raise ValueError('Template schema/locale mismatch')
    if data.get('source_sha256') != fingerprint(english):
        raise ValueError('Source changed; export a fresh template and merge translations')
    if set(data['entries']) != set(english):
        raise ValueError('Missing or unknown translation keys')
    values = {}
    forms = set(item.get('plural_forms', ['zero','one','two','few','many','other'] if args.locale == 'ar' else ['other']))
    for key, source in english.items():
        row = data['entries'][key]
        if row['source'] != source:
            raise ValueError('Do not edit source: ' + repr(key))
        value = row['translation']
        if isinstance(source, dict):
            if not isinstance(value, dict) or set(value) != forms:
                raise ValueError('Required plural forms differ: ' + repr(key))
            variants = value.values()
            source = source['other']
        else:
            variants = [value]
        expected = g.format_args(source)
        for text in variants:
            if not isinstance(text, str) or not text.strip() or '\x00' in text:
                raise ValueError('Empty/invalid translation: ' + repr(key))
            if g.format_args(text, strict=bool(expected)) != expected:
                raise ValueError('Printf arguments differ: ' + repr(key))
        values[key] = value
    base = g.I18N / 'locales' / (args.locale + '.json')
    documents = {p: g.read_json(p) for p in [base, *sorted((g.I18N / 'fragments').glob('*.json'))]}
    changes = {}
    for key, value in values.items():
        owners = [p for p, content in documents.items() if key in content.get(args.locale, {})]
        for path in owners or [base]:
            documents[path].setdefault(args.locale, {})[key] = value
            changes[path] = documents[path]
    # Any edited translation invalidates release acceptance, not other locales.
    registry = g.read_json(g.I18N / 'locales.json')
    for entry in registry:
        if entry['tag'] == args.locale:
            entry.update(enabled=False, translations_ready=False, font_ready=False, shaping_ready=False)
    changes[g.I18N / 'locales.json'] = registry
    replace_files(changes)
    print('Imported', len(values), 'keys; locale remains disabled pending acceptance')


def run(script, *args):
    subprocess.run([sys.executable, str(g.ROOT / 'tools' / script), *map(str, args)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('add')
    p.add_argument('locale'); p.add_argument('--name', required=True); p.add_argument('--code', required=True)
    p.add_argument('--direction', choices=['ltr','rtl'], required=True)
    p.add_argument('--font-profile', required=True); p.add_argument('--plural-forms', required=True)
    for command in ('export', 'import'):
        p = commands.add_parser(command); p.add_argument('locale'); p.add_argument('file', type=Path)
    p = commands.add_parser('fonts'); p.add_argument('--converter', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('check'); p.add_argument('--output', type=Path, required=True)
    p.add_argument('--converter', type=Path)
    p = commands.add_parser('build'); p.add_argument('--output', type=Path, required=True)
    p.add_argument('--converter', type=Path)
    args = parser.parse_args()
    if args.command == 'add': add(args)
    elif args.command == 'export': export(args)
    elif args.command == 'import': import_translation(args)
    elif args.command == 'fonts':
        run('gen_locale_fonts.py', '--converter', args.converter, '--output', args.output)
        run('prepare_locale_font_assets.py', args.output)
    else:
        if args.converter:
            candidate=(args.output / 'font-candidate').resolve()
            run('gen_locale_fonts.py', '--converter', args.converter, '--output', candidate)
            run('prepare_locale_font_assets.py', candidate)
        run('gen_i18n.py', '--generate')
        run('gen_i18n.py', '--check')
        run('prepare_locale_font_assets.py', '--check')
        run('test_locale_fonts.py', g.ROOT / 'aic_ui/lvgl_data/locale_fonts', args.output / 'fonts')
        run('test_i18n.py', '--sanitize')
        run('test_settings_view.py', args.output / 'settings')
        cash_output = (args.output / 'cash-menu').resolve()
        cash_output.mkdir(parents=True, exist_ok=True)
        subprocess.run([sys.executable, str(g.ROOT / 'tools/test_cash_view.py')],
                       env=dict(os.environ, MENU_OUTPUT=str(cash_output)), check=True)
        run('test_main_view.py', '--lvgl-dir', g.ROOT.parents[1] / 'third-party/lvgl-8.3.2',
            '--output-dir', args.output / 'main')
        subprocess.run([sys.executable, str(g.ROOT / 'tools/test_notice_view.py')],
                       env=dict(os.environ, NOTICE_OUTPUT=str((args.output / 'notices').resolve())), check=True)
        if args.command == 'build':
            repo = g.ROOT.parents[2]
            if not (repo / 'Makefile').exists():
                raise ValueError('Build requires the authoritative Linux SDK checkout')
            subprocess.run(['make', '-j2', 'BR2_JLEVEL=2', 'lvgl-rebuild'], cwd=repo, check=True)
            subprocess.run(['make', '-j2', 'BR2_JLEVEL=2', 'test-lvgl-rebuild'], cwd=repo, check=True)
            subprocess.run(['make', '-j2', 'BR2_JLEVEL=2'], cwd=repo, check=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, subprocess.CalledProcessError) as exc:
        sys.exit(str(exc))
