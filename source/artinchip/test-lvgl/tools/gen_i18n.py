#!/usr/bin/env python3
"""Offline LVGL i18n maintenance: JSON (YAML subset) -> pinned upstream C.

--extract adds marked English msgids without inventing translations.
--generate needs Node, but uses the vendored, hash-checked 0.2.1 generator.
--check needs Python only and verifies catalogues, format arguments and outputs.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
I18N = ROOT / 'i18n'
LITERAL = r'"(?:\\.|[^"\\])*"'
STRING = rf'{LITERAL}(?:\s*{LITERAL})*'
FORMAT = re.compile(r'%(?:(\d+)\$)?[-+ #0\']*(\d+|\*(?:\d+\$)?)?(?:\.(\d+|\*(?:\d+\$)?))?(hh|ll|[hljztL])?([diuoxXfFeEgGaAcspn%])')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'{path}: duplicate key {key!r}')
            result[key] = value
        return result
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode('utf-8'))


def json_text(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + '\n'


def c_string(value):
    # C forbids universal-character names for low controls such as context EOT.
    encoded = json.dumps(value, ensure_ascii=False)
    return re.sub(r'\\u00([0-1][0-9a-f])', lambda m: '\\%03o' % int(m[1], 16), encoded)


def decode(value):
    return ''.join(ast.literal_eval(part) for part in re.findall(LITERAL, value))


def marked_keys():
    result = set()
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|' + LITERAL + r"|'(?:\\.|[^'\\])*'")
    def no_comment(match):
        value = match[0]
        return ' ' * len(value) if value.startswith(('//', '/*')) else value
    paths = [ROOT / 'main.c'] + sorted((ROOT / 'un260').rglob('*'))
    for path in paths:
        if (path.suffix not in ('.c', '.h', '.inc') or path.name.startswith('lv_font_')
                or 'tests' in path.relative_to(ROOT).parts):
            continue
        source = token.sub(no_comment, path.read_text(encoding='utf-8'))
        for match in re.finditer(r'\b(?:UI_N_|ui_tr|ui_trn)\s*\(\s*(' + STRING + ')', source):
            result.add(decode(match[1]))
        for match in re.finditer(r'\bui_trc\s*\(\s*(' + STRING + r')\s*,\s*(' + STRING + ')', source):
            result.add(decode(match[1]) + '\004' + decode(match[2]))
        for match in re.finditer(r'\bui_tr_for\s*\(\s*[^,;]+,\s*(' + STRING + ')', source):
            result.add(decode(match[1]))
        # This catalogue's compact macros hold source keys in fixed positions.
        if path.name == 'fault_guide_catalog.c':
            for match in re.finditer(r'\b(?:T|E)\s*\(\s*(' + STRING + ')', source):
                result.add(decode(match[1]))
            for match in re.finditer(r'\bE\s*\(\s*' + STRING + r'\s*,\s*(' + STRING + ')', source):
                result.add(decode(match[1]))
    return result


def format_args(text, strict=False):
    args = {}
    next_index = 1
    parameter_modes = set()
    consumed = 0
    for match in FORMAT.finditer(text):
        if strict and "%" in text[consumed:match.start()]:
            raise ValueError("unknown or incomplete printf conversion")
        consumed = match.end()
        position, width, precision, length, spec = match.groups()
        if spec == '%':
            if strict and match[0] != '%%':
                raise ValueError('invalid escaped printf percent')
            continue
        if spec == 'n':
            raise ValueError('%n is forbidden in UI translations')
        for value in (width, precision):
            if value and value.startswith('*'):
                explicit = value[1:-1] if value.endswith('$') else ''
                parameter_modes.add('positional' if explicit else 'sequential')
                index = int(explicit) if explicit else next_index
                if not explicit:
                    next_index += 1
                args[index] = 'int'
        parameter_modes.add('positional' if position else 'sequential')
        index = int(position) if position else next_index
        if not position:
            next_index += 1
        kind = ('signed' if spec in 'di' else 'unsigned' if spec in 'uoxX' else
                'float' if spec in 'fFeEgGaA' else spec)
        value = (length or '') + kind
        if index in args and args[index] != value:
            raise ValueError('inconsistent positional argument types')
        args[index] = value
    if strict and '%' in text[consumed:]:
        raise ValueError('unknown or incomplete printf conversion')
    if strict and len(parameter_modes) > 1:
        raise ValueError('mixed positional and sequential printf parameters')
    return args


def catalogues(extract=False):
    registry = read_json(I18N / 'locales.json')
    if not registry or registry[0]['tag'] != 'en' or registry[0]['id'] != 0:
        raise ValueError('English must be the first/default registry entry with stable ID 0')
    if not all(registry[0][key] for key in ('enabled', 'translations_ready', 'font_ready', 'shaping_ready')):
        raise ValueError('The fallback English locale must stay enabled and resource-ready')
    ids, tags = set(), set()
    catalog = {}
    for item in registry:
        tag = item['tag']
        if not re.fullmatch(r'[a-zA-Z]{2,8}(?:-[a-zA-Z0-9]{1,8})*', tag):
            raise ValueError(f'Invalid locale tag {tag}')
        if tag in tags or item['id'] in ids or not 0 <= item['id'] < 65535:
            raise ValueError('Duplicate/invalid locale tag or stable ID')
        ids.add(item['id']); tags.add(tag)
        if item['direction'] not in ('ltr', 'rtl'):
            raise ValueError(f'{tag}: invalid direction')
        if item['enabled'] and not all(item[k] for k in ('translations_ready', 'font_ready', 'shaping_ready')):
            raise ValueError(f'{tag}: cannot enable an unverified language')
        path = I18N / 'locales' / f'{tag}.json'
        content = read_json(path)
        if set(content) != {tag}:
            raise ValueError(f'{path}: expected only {tag}')
        catalog[tag] = content[tag].copy()
    for path in sorted((I18N / 'fragments').glob('*.json')):
        for tag, entries in read_json(path).items():
            if tag not in catalog:
                raise ValueError(f'{path}: locale absent from registry: {tag}')
            for key, value in entries.items():
                old = catalog[tag].get(key)
                if old is not None and old != value:
                    if not (tag == 'en' and isinstance(value, dict) and value.get('other') == old):
                        raise ValueError(f'{path}: conflicting translation for {key!r}')
                catalog[tag][key] = value
    keys = marked_keys()
    for entries in catalog.values():
        keys.update(entries)
    english_path = I18N / 'locales/en.json'
    english = read_json(english_path)['en']
    missing = keys - set(catalog['en'])
    if missing and not extract:
        raise ValueError('Unextracted English msgids; run --extract: ' + repr(sorted(missing)[:8]))
    if extract:
        for key in missing:
            english[key] = key.split('\004', 1)[-1]
        write(english_path, json_text({'en': dict(sorted(english.items()))}))
        for key, value in english.items():
            catalog['en'].setdefault(key, value)
    # CN/KR drafts predate plural resources. Their existing no-plural-form text
    # remains the `other` translation rather than silently reverting to English.
    for key, base in catalog['en'].items():
        if isinstance(base, dict):
            for tag, entries in catalog.items():
                if tag != 'en' and isinstance(entries.get(key), str):
                    entries[key] = {'other': entries[key]}
    for tag, entries in catalog.items():
        if len(entries) >= 65535:
            raise ValueError(f'{tag}: catalogue exceeds the upstream 16-bit phrase limit')
        for key, value in entries.items():
            if value is None:
                continue
            base = catalog['en'][key]
            source = base.get('other', '') if isinstance(base, dict) else base
            # Static copy may contain a literal percent (e.g. 100%). Only a
            # source with printf arguments opts into strict conversion parsing.
            expected = format_args(source)
            strict = bool(expected)
            if strict:
                expected = format_args(source, strict=True)
            variants = value.values() if isinstance(value, dict) else (value,)
            for translated in variants:
                if translated is not None and format_args(translated, strict=strict) != expected:
                    raise ValueError(f'{tag}: printf arguments differ: {key!r}')
    for item in registry:
        if item['enabled'] and item['tag'] != 'en':
            entries = catalog[item['tag']]
            missing = [key for key in catalog['en'] if not entries.get(key)]
            if missing:
                raise ValueError(f"{item['tag']}: enabled locale has {len(missing)} untranslated keys")
    return registry, {tag: dict(sorted(entries.items())) for tag, entries in catalog.items()}


def patch_runtime(folder):
    """Small audited adaptation of the official runtime, without semantic changes."""
    h = (folder / 'lv_i18n.h').read_text(encoding='utf-8')
    h = h.replace('lv_i18n_phrase_t * singulars;', 'const lv_i18n_phrase_t * singulars;\n    uint16_t singular_count;')
    h = h.replace('lv_i18n_phrase_t * plurals[_LV_I18N_PLURAL_TYPE_NUM];',
                  'const lv_i18n_phrase_t * plurals[_LV_I18N_PLURAL_TYPE_NUM];\n    uint16_t plural_count[_LV_I18N_PLURAL_TYPE_NUM];')
    h = h.replace('const char * lv_i18n_get_text(const char * msg_id);',
                  'const char * lv_i18n_get_text(const char * msg_id);\n'
                  '/* Explicit immutable pack lookup for background export snapshots. */\n'
                  'const char *lv_i18n_get_text_from_pack(const lv_i18n_language_pack_t *pack,\n'
                  '                                      const char *locale, const char *msg_id);')
    source = (folder / 'lv_i18n.c').read_text(encoding='utf-8')
    source = source.replace('static lv_i18n_phrase_t ', 'static const lv_i18n_phrase_t ')
    source = source.replace('(uint32_t)(val < 0 ? -val : val)', '(val < 0 ? 0U - (uint32_t)val : (uint32_t)val)')
    source = re.sub(r'(static uint8_t \w+_plural_fn\(int32_t num\)\n\{)', r'\1\n    UNUSED(num);', source)
    source = re.sub(r'(    \.singulars = (\w+),)', r'\1\n    .singular_count = sizeof(\2) / sizeof(\2[0]) - 1,', source)
    source = re.sub(r'(    \.plurals\[([^]]+)\] = (\w+),)', r'\1\n    .plural_count[\2] = sizeof(\3) / sizeof(\3[0]) - 1,', source)
    start = source.index('static const char * __lv_i18n_get_text_core(')
    end = source.index('\n\n/**', start)
    source = source[:start] + '''/* UN260: generated tables are sorted by UTF-8 msgid; no heap or linear scan. */
static const char * __lv_i18n_get_text_core(const lv_i18n_phrase_t *trans,
                                          uint16_t count, const char *msg_id)
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = strcmp(trans[mid].msg_id, msg_id);
        if (order < 0) lo = mid + 1;
        else if (order > 0) hi = mid;
        else return trans[mid].translation;
    }
    return NULL;
}''' + source[end:]
    source = source.replace('__lv_i18n_get_text_core(lang->singulars, msg_id)',
                            '__lv_i18n_get_text_core(lang->singulars, lang->singular_count, msg_id)')
    source = source.replace('__lv_i18n_get_text_core(lang->plurals[ptype], msg_id)',
                            '__lv_i18n_get_text_core(lang->plurals[ptype], lang->plural_count[ptype], msg_id)')
    source += '''
/* UN260: frozen locale lookups do not access or mutate the UI-thread state. */
const char *lv_i18n_get_text_from_pack(const lv_i18n_language_pack_t *pack,
                                      const char *locale, const char *msg_id)
{
    if (!msg_id) return "";
    if (!pack || !pack[0]) return msg_id;
    const lv_i18n_lang_t *language = pack[0];
    if (locale) for (size_t i = 0; pack[i]; ++i)
        if (strcmp(locale, pack[i]->locale_name) == 0) { language = pack[i]; break; }
    const char *text = language->singulars ?
        __lv_i18n_get_text_core(language->singulars, language->singular_count, msg_id) : NULL;
    if (!text && language != pack[0] && pack[0]->singulars)
        text = __lv_i18n_get_text_core(pack[0]->singulars, pack[0]->singular_count, msg_id);
    return text ? text : msg_id;
}
'''
    source = re.sub(r'\\u00([0-1][0-9a-f])', lambda m: '\\%03o' % int(m[1], 16), source)
    # JS orders integer-like keys before strings. Re-sort generated arrays by decoded key.
    def sort_array(match):
        rows = re.findall(r'^    \{(' + LITERAL + r'), (' + LITERAL + r')\},$', match[2], re.M)
        rows.sort(key=lambda row: decode(row[0]).encode('utf-8'))
        return match[1] + '\n'.join('    {' + k + ', ' + v + '},' for k, v in rows) + '\n    {NULL, NULL} // End mark\n};'
    source = re.sub(r'(static const lv_i18n_phrase_t \w+\[\] = \{\n)(.*?\n\};)', sort_array, source, flags=re.S)
    banner = '/* Generated by pinned lv_i18n 0.2.1 + tools/gen_i18n.py; see i18n/upstream.json and MIT license. */\n'
    write(folder / 'lv_i18n.h', banner + h)
    write(folder / 'lv_i18n.c', banner + source)


def generated_auxiliary(registry):
    rows = []
    for item in registry:
        fields = [str(item['id'])] + [c_string(item[k]) for k in ('tag', 'code', 'name')]
        fields += ['UI_LANG_' + item['direction'].upper()]
        fields += [str(item[k]).lower() for k in ('enabled', 'translations_ready', 'font_ready', 'shaping_ready')]
        fields += [c_string(item['font_profile'])]
        rows.append('    {' + ', '.join(fields) + '},')
    write(I18N / 'generated/ui_locale_registry.inc', '/* Generated from i18n/locales.json. */\nstatic const ui_locale_t ui_locales[] = {\n' + '\n'.join(rows) + '\n};\n')
    keys = read_json(I18N / 'legacy_ids.json')
    write(I18N / 'generated/ui_text_keys.inc', '/* Stable enum compatibility; translations live in locale catalogues. */\n' + ''.join(f'    [{enum}] = {c_string(key)},\n' for enum, key in keys.items()))


def input_hashes(registry, catalog):
    data = {'registry': registry, 'catalog': catalog, 'legacy_ids': read_json(I18N / 'legacy_ids.json')}
    return {'catalog_sha256': digest(json_text(data).encode()),
            'generator_sha256': digest(Path(__file__).read_bytes()),
            'upstream_sha256': digest((ROOT / 'tools/vendor/lv_i18n-0.2.1.tgz').read_bytes())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extract', action='store_true')
    parser.add_argument('--generate', action='store_true')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--node', default='node')
    args = parser.parse_args()
    registry, catalog = catalogues(args.extract)
    inputs = input_hashes(registry, catalog)
    upstream = read_json(I18N / 'upstream.json')
    if inputs['upstream_sha256'] != upstream['sha256']:
        raise ValueError('Pinned upstream generator hash mismatch')
    manifest_path = I18N / 'generated/manifest.json'
    if args.generate:
        with tempfile.TemporaryDirectory(prefix='un260-i18n-') as temp:
            temp = Path(temp)
            with tarfile.open(ROOT / 'tools/vendor/lv_i18n-0.2.1.tgz') as archive:
                for member in archive.getmembers():
                    if Path(member.name).is_absolute() or '..' in Path(member.name).parts or member.issym() or member.islnk():
                        raise ValueError('Unsafe vendored archive member')
                    # Explicit path/link validation above also supports older build-host Python.
                    if hasattr(tarfile, 'data_filter'):
                        archive.extract(member, temp, filter='data')
                    else:
                        archive.extract(member, temp)
            translation_dir = temp / 'translations'; translation_dir.mkdir()
            for tag, entries in catalog.items():
                write(translation_dir / (tag + '.json'), json_text({tag: entries}))
            output = I18N / 'generated'; output.mkdir(exist_ok=True)
            subprocess.run([args.node, str(temp / 'package/lv_i18n.js'), 'compile',
                            '-t', str(translation_dir / '*.json'), '-o', str(output), '-l', 'en'], check=True)
        patch_runtime(output)
        generated_auxiliary(registry)
        outputs = {p.name: digest(p.read_bytes()) for p in sorted(output.iterdir()) if p.name != 'manifest.json'}
        write(manifest_path, json_text({'inputs': inputs, 'outputs': outputs}))
    if args.check:
        manifest = read_json(manifest_path)
        if manifest['inputs'] != inputs:
            raise ValueError('I18n generated catalogue is stale; run --extract --generate')
        for name, expected in manifest['outputs'].items():
            if digest((I18N / 'generated' / name).read_bytes()) != expected:
                raise ValueError(f'Generated i18n file changed: {name}')
    print('I18n PASS: ' + ', '.join(f'{tag}={len(entries)}' for tag, entries in catalog.items()))


if __name__ == '__main__':
    main()
