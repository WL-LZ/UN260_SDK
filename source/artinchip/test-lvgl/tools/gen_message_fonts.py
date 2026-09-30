#!/usr/bin/env python3
"""Maintain the message-only Noto CJK subset and LVGL 8.3 fallback bitmaps.

Normal builds consume committed C. Regeneration requires fontTools, Node and
lv_font_conv; source TTC defaults to Debian/Ubuntu fonts-noto-cjk (SIL OFL 1.1).
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SIZES = (12, 14, 16, 18, 20, 22, 24, 28)
INPUTS = ('i18n/legacy_ids.json', 'i18n/locales.json',
          'un260/lv_components/lv_fault_popup.c',
          'un260/lv_components/fault_guide/fault_guide_catalog.c')
ASSETS = ROOT / 'aic_ui/font/message'

LITERAL = r'"(?:\\.|[^"\\])*"'

def literal_codepoints(source):
    chars = set()
    for literal in re.findall(LITERAL, source):
        chars.update(ord(c) for c in literal if ord(c) >= 0x80)
    return chars

def translated_points(keys):
    """Read the maintained catalogues, not the generated C representation.

    These are the existing message fallback roles, not proof that an entire
    locale has fonts. Language activation has a separate full-UI readiness gate.
    """
    chars = {ord(c) for key in keys for c in key if ord(c) >= 0x80}
    for path in sorted((ROOT / 'i18n/locales').glob('*.json')) + sorted((ROOT / 'i18n/fragments').glob('*.json')):
        for entries in json.loads(path.read_text(encoding='utf-8')).values():
            for key in keys:
                value = entries.get(key)
                values = value.values() if isinstance(value, dict) else (value,)
                for text in values:
                    if text:
                        chars.update(ord(c) for c in text if ord(c) >= 0x80)
    return chars


def marked_keys(source):
    return {ast.literal_eval(value) for value in re.findall(
        r'\b(?:UI_N_|ui_tr|ui_trn|T)\s*\(\s*(' + LITERAL + ')', source)}


def fault_keys(source):
    keys = marked_keys(source)
    for title, location in re.findall(r'\bE\(\s*(' + LITERAL + r')\s*,\s*(' + LITERAL + ')', source):
        keys.update((ast.literal_eval(title), ast.literal_eval(location)))
    return keys


def required_codepoints():
    legacy = json.loads((ROOT / INPUTS[0]).read_text(encoding='utf-8'))
    keys = set(legacy.values())
    keys.update(marked_keys((ROOT / INPUTS[2]).read_text(encoding='utf-8')))
    keys.update(fault_keys((ROOT / INPUTS[3]).read_text(encoding='utf-8')))
    return sorted(translated_points(keys))

def required_by_size():
    """Roles match ui_notice.c and lv_fault_popup.c; keep broad message sizes.

    Fault titles have a small, explicit catalogue grammar. Locations/step titles
    deliberately accept every catalogue character so new guides need no manual
    Unicode list. The real-LVGL tests additionally walk each displayed label.
    """
    all_points = set(required_codepoints())
    popup = translated_points(marked_keys((ROOT / INPUTS[2]).read_text(encoding='utf-8')))
    catalog_source = (ROOT / INPUTS[3]).read_text(encoding='utf-8')
    catalog = translated_points(fault_keys(catalog_source))
    title_literals = re.findall(r'\bE\(\s*('+LITERAL+r')', catalog_source)
    title_literals += re.findall(r'\.title\s*=\s*T\(\s*('+LITERAL+r')', catalog_source)
    titles = translated_points({ast.literal_eval(value) for value in title_literals})
    if not titles:
        raise SystemExit('Fault title catalogue grammar changed; update role extraction')
    legacy = json.loads((ROOT / INPUTS[0]).read_text(encoding='utf-8'))
    meta = set()
    for name in ('UI_TEXT_NOTICE_NOW', 'UI_TEXT_NOTICE_ONGOING'):
        if name not in legacy:
            raise SystemExit('Missing notice metadata: '+name)
        meta.update(translated_points({legacy[name]}))
    return {12: sorted(popup | titles), 14: sorted(catalog | popup | meta),
            16: sorted(titles), 18: sorted(all_points), 20: sorted(all_points),
            22: sorted(all_points), 24: sorted(catalog), 28: sorted(titles)}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail when current message text adds unbuilt glyphs')
    parser.add_argument('--converter', help='Installed lv_font_conv.js path (no implicit downloads)')
    parser.add_argument('--source', default='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')
    parser.add_argument('--license', default='/usr/share/doc/fonts-noto-cjk/copyright',
                        help='Debian copyright file containing the complete SIL-1.1 text')
    args = parser.parse_args()
    codepoints = required_codepoints()
    by_size = required_by_size()
    manifest_path = ASSETS / 'manifest.json'
    if args.check:
        manifest = json.loads(manifest_path.read_text())
        for size in SIZES:
            missing = set(by_size[size]) - set(manifest['codepoints_by_size'][str(size)])
            if missing:
                raise SystemExit(f'Message font {size}px missing: ' + ' '.join(f'U+{c:04X}' for c in sorted(missing)))
            if not (ROOT / f'un260/font/lv_font_message_cjk_{size}.c').exists():
                raise SystemExit(f'Message font bitmap missing: {size}')
        print('Message font coverage PASS: '+', '.join(f'{size}px={len(by_size[size])}' for size in SIZES))
        return
    if not args.converter:
        parser.error('--converter is required for regeneration')
    from fontTools import subset
    from fontTools.ttLib import TTFont
    ASSETS.mkdir(parents=True, exist_ok=True)
    font = TTFont(args.source, fontNumber=2)  # Noto Sans CJK SC; includes Hangul.
    upstream_copyright = font['name'].getDebugName(0)
    legal = Path(args.license).read_text()
    license_start = legal.rfind('License: SIL-1.1\n')
    if license_start < 0:
        raise SystemExit('Full SIL-1.1 license section required')
    license_text = legal[license_start + len('License: SIL-1.1\n'):].split('\nLicense:', 1)[0]
    license_text = '\n'.join('' if line.strip() == '.' else line[1:] if line.startswith(' ') else line for line in license_text.splitlines())
    (ASSETS / 'OFL.txt').write_bytes((upstream_copyright + '\n\n' + license_text + '\n').encode('utf-8'))
    missing = set(codepoints) - set(font.getBestCmap())
    if missing:
        raise SystemExit('Source font missing: ' + ' '.join(f'U+{c:04X}' for c in sorted(missing)))
    options = subset.Options()
    options.layout_features = []
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=codepoints)
    subsetter.subset(font)
    # Modified/subset font has its own family name, retaining original OFL metadata.
    for record in font['name'].names:
        if record.nameID in (1, 4, 6):
            record.string = ('UN260 Message CJK Subset' if record.nameID != 6 else 'UN260MessageCJKSubset').encode(record.getEncoding())
    subset_file = ASSETS / 'UN260MessageCJKSubset.otf'
    font.save(subset_file)
    for size in SIZES:
        glyph_range = ','.join(f'0x{c:X}' for c in by_size[size])
        name = f'lv_font_message_cjk_{size}'
        output = ROOT / f'un260/font/{name}.c'
        subprocess.run(['node', args.converter, '--no-compress', '--no-prefilter', '--bpp', '4',
                        '--size', str(size), '--font', str(subset_file), '-r', glyph_range,
                        '--format', 'lvgl', '--lv-font-name', name, '-o', str(output)], check=True)
        generated = output.read_text()
        generated = re.sub(r'^\s*\.static_bitmap = 0,\s*$', '', generated, flags=re.M)
        # Avoid embedding host-specific paths and the large repeated range in C comments.
        generated = re.sub(r'^ \* Opts:.*$', ' * Source: aic_ui/font/message/UN260MessageCJKSubset.otf (SIL OFL 1.1)', generated, flags=re.M)
        generated = '\n'.join(line.rstrip() for line in generated.splitlines()).rstrip() + '\n'
        output.write_bytes(generated.encode('utf-8'))
    manifest_path.write_bytes((json.dumps({
        'family': 'UN260 Message CJK Subset', 'upstream': 'Noto Sans CJK SC Regular',
        'source_sha256': hashlib.sha256(Path(args.source).read_bytes()).hexdigest(),
        'subset_sha256': hashlib.sha256(subset_file.read_bytes()).hexdigest(),
        'sizes': SIZES, 'inputs': INPUTS, 'codepoints': codepoints,
        'codepoints_by_size': by_size
    }, indent=2) + '\n').encode('utf-8'))
    print(f'Generated {len(codepoints)} glyphs at {SIZES}')

if __name__ == '__main__':
    main()
