#!/usr/bin/env python3
"""Build inspectable locale font candidates without enabling a UI language.

Uses the installed regional Noto CJK faces and DejaVu Arabic. Output is a
separate candidate directory, never an implicit firmware/install-tree update.
Incomplete translation coverage is recorded in the manifest, not concealed.
"""
import argparse
import hashlib
import json
import subprocess
import unicodedata
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import gen_i18n

SIZES = (10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 32, 36, 37, 50, 55)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def strings(entries):
    for value in entries.values():
        for text in value.values() if isinstance(value, dict) else (value,):
            if text:
                yield text


def glyphs(tag, entries, cmap, name, presentation_forms, include_ascii=False):
    required = {ord(c) for text in [*strings(entries), name] for c in text
                if ord(c) >= (32 if include_ascii else 128) and unicodedata.category(c) != 'Cf'}
    missing = sorted(required - set(cmap))
    if missing:
        raise ValueError('%s source lacks %s' % (tag, ', '.join('U+%04X' % c for c in missing)))
    # LVGL's Arabic shaping produces presentation forms before font lookup.
    # Include the font's complete presentation-form inventory, including lam-alef.
    shaped = {c for c in cmap if 0xfb50 <= c <= 0xfdff or 0xfe70 <= c <= 0xfeff} if presentation_forms else set()
    if not required and 0xa0 in cmap:
        required.add(0xa0)  # An ASCII-only locale still needs a valid fallback asset.
    return sorted(required | shaped), sorted(shaped)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--converter', type=Path, required=True)
    parser.add_argument('--profiles', type=Path, default=gen_i18n.I18N / 'font_profiles.json')
    parser.add_argument('--sizes', type=int, nargs='+', default=SIZES)
    args = parser.parse_args()
    from fontTools.ttLib import TTFont
    from fontTools import subset
    if any(size < 8 or size > 64 for size in args.sizes):
        parser.error('Font sizes must be 8..64 pixels')
    args.output.mkdir(parents=True, exist_ok=True)
    registry, catalogs = gen_i18n.catalogues()
    profiles = gen_i18n.read_json(args.profiles)
    contracts = gen_i18n.read_json(gen_i18n.I18N / 'font_contracts.json')['large_text']
    manifest = {'status': 'candidate-not-released', 'sizes': args.sizes, 'locales': {}}
    for locale in registry:
        if locale['id'] == 0:
            continue
        tag = locale['tag']
        entries = catalogs[tag]
        missing = [key for key in catalogs['en'] if not entries.get(key)]
        if missing:
            raise ValueError('%s has %d untranslated keys' % (tag, len(missing)))
        profile = profiles[locale['font_profile']]
        source = Path(profile['source'])
        index = profile.get('face_indices', {}).get(tag, profile.get('face_index', 0))
        font = TTFont(str(source), fontNumber=index)
        family = font['name'].getDebugName(1)
        legal = {str(i): font['name'].getDebugName(i) for i in (0, 13, 14)}
        points, shaped = glyphs(tag, entries, font.getBestCmap(), locale['name'], profile.get('presentation_forms', False))
        per_size = {}
        for size, keys in contracts.items():
            selected = {key: entries[key] for key in keys}
            # Large base faces only contain digits and English OFF, not the
            # full ASCII alphabet used by translated states.
            per_size[int(size)], _ = glyphs(tag, selected, font.getBestCmap(), '', profile.get('presentation_forms', False), include_ascii=True)
        options = subset.Options()
        options.layout_features = []  # LVGL performs Arabic shaping itself.
        options.name_IDs = [0,1,2,3,4,5,6,13,14]
        subsetter = subset.Subsetter(options=options)
        # Size contracts can add an ASCII-only fallback sentinel absent from
        # the full translated catalogue; retain every conversion's input.
        subset_points = set(points)
        for size_points in per_size.values():
            subset_points.update(size_points)
        subsetter.populate(unicodes=subset_points)
        subsetter.subset(font)
        for record in font['name'].names:
            if record.nameID in (1,4,6):
                record.string = ('UN260 Locale Candidate '+tag).replace(' ','').encode(record.getEncoding())
        folder = args.output / tag
        folder.mkdir(exist_ok=True)
        # An extracted regional face avoids lv_font_conv silently taking TTC face 0.
        extracted = folder / ('source.ttf' if tag == 'ar' else 'source.otf')
        font.save(str(extracted))
        def convert(size):
            path = folder / ('font-%d.bin' % size)
            size_points = per_size.get(size, points)
            subprocess.run(['node', str(args.converter), '--no-compress', '--no-prefilter',
                            '--bpp', '4', '--size', str(size), '--font', str(extracted),
                            '-r', ','.join('0x%X' % c for c in size_points), '--format', 'bin',
                            '-o', str(path)], check=True)
            return {'size': size, 'file': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path),
                    'codepoints': size_points, 'text_keys': contracts.get(str(size))}
        with ThreadPoolExecutor(max_workers=4) as pool:
            outputs = list(pool.map(convert, args.sizes))
        manifest['locales'][tag] = {'family': family,
            'source_sha256': sha(source), 'face_index': index,
            'candidate_entries': len(entries),
            'missing_translation_count': len(set(catalogs['en']) - set(entries)),
            'codepoints': points, 'shaped_codepoints': shaped, 'files': outputs,
            'license_metadata': legal,
            'bytes': sum(item['bytes'] for item in outputs)}
        print('%s: %s, %d glyphs, %d bytes' % (tag, family, len(points), manifest['locales'][tag]['bytes']), flush=True)
        font.close()
    (args.output / 'manifest.json').write_bytes((json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))


if __name__ == '__main__':
    main()
