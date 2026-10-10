#!/usr/bin/env python3
"""Offline shaped recovery labels; normal builds only check committed pixels."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zlib

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'un260/lv_drivers/upgrade_display.h'
OUTPUT = ROOT / 'un260/font/upgrade_native_art.h'
MANIFEST = ROOT / 'un260/font/upgrade_native_manifest.json'


def inputs():
    keys = sorted({json.loads(value) for value in re.findall(r'UI_N_\(\s*("(?:\\.|[^"\\])*\")', HEADER.read_text())})
    registry = [item for item in json.loads((ROOT / 'i18n/locales.json').read_text()) if item['enabled']]
    profiles = json.loads((ROOT / 'i18n/font_profiles.json').read_text())
    catalogs = {}
    for path in sorted((ROOT / 'i18n/locales').glob('*.json')) + sorted((ROOT / 'i18n/fragments').glob('*.json')):
        for tag, entries in json.loads(path.read_text()).items():
            catalogs.setdefault(tag, {}).update(entries)
    return {'keys': keys, 'locales': registry, 'profiles': profiles,
            'texts': {locale['tag']: {key: catalogs.get(locale['tag'], {}).get(key) for key in keys} for locale in registry},
            'fonts': {name: hashlib.sha256(Path(profile['source']).read_bytes()).hexdigest() for name, profile in profiles.items()}}


def write(path, text):
    data = text.encode('utf-8')
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def generate():
    from PIL import Image, ImageDraw, ImageFont, features
    from fontTools.ttLib import TTFont
    if not features.check('raqm'):
        raise ValueError('RAQM required for shaped Arabic; use the existing handwriting Python runtime')
    locales = [item for item in json.loads((ROOT / 'i18n/locales.json').read_text()) if item['enabled']]
    profiles = json.loads((ROOT / 'i18n/font_profiles.json').read_text())
    catalogs = {}
    for path in sorted((ROOT / 'i18n/locales').glob('*.json')) + sorted((ROOT / 'i18n/fragments').glob('*.json')):
        for tag, entries in json.loads(path.read_text()).items():
            catalogs.setdefault(tag, {}).update(entries)
    keys = sorted({json.loads(value) for value in re.findall(r'UI_N_\(\s*("(?:\\.|[^"\\])*")', HEADER.read_text())})
    # Native labels fit the existing 1280x400 maintenance layout, with complete
    # text at a known role size. Dynamic technical diagnostics stay in the log.
    sizes = {key: 20 for key in keys}
    for key in ['UPDATE PAUSED', 'UPDATE COMPLETE', 'UPDATING YOUR DEVICE']:
        sizes[key] = 35
    for key in ['UN260 PROTECTION MODE', 'STOP FEEDING BANKNOTES']:
        sizes[key] = 28
    for key in ['KEEP NOTES SEPARATE AND RECOUNT', 'COUNT AND CONTROLLER STATE REQUIRE CHECKING']:
        sizes[key] = 23
    for key in ['RAM LOG UI UPDATE LOG  USB COPY MAY BE UNAVAILABLE', 'BACKUP   STORAGE   APPLICATION   VERIFY',
                'CONTACT SERVICE  DO NOT RESUME COUNTING']:
        sizes[key] = 17
    blob = bytearray()
    rows = []
    report = []
    for locale in locales:
        tag = locale['tag']
        profile = profiles[locale['font_profile']]
        source = Path(profile['source'])
        index = profile.get('face_indices', {}).get(tag, profile.get('face_index', 0))
        cmap = TTFont(str(source), fontNumber=index).getBestCmap()
        for key in keys:
            text = catalogs.get(tag, {}).get(key)
            if not isinstance(text, str) or not text:
                raise ValueError('Missing recovery translation: ' + tag + '/' + key)
            missing = [c for c in text if not c.isspace() and ord(c) not in cmap]
            if missing:
                raise ValueError('Missing native glyph: ' + tag + '/' + repr(missing))
            font = ImageFont.truetype(str(source), sizes[key], index=index, layout_engine=ImageFont.Layout.RAQM)
            style = {'direction': locale['direction'], 'language': tag.split('-')[0]}
            box = font.getbbox(text, **style)
            width, height = box[2] - box[0] + 2, box[3] - box[1] + 2
            if not 0 < width <= 1152 or not 0 < height <= 56:
                raise ValueError('Recovery label exceeds reserved row: ' + tag + '/' + key + '/' + str((width, height)))
            image = Image.new('L', (width, height))
            ImageDraw.Draw(image).text((1-box[0], 1-box[1]), text, font=font, fill=255, **style)
            levels = [(value + 8) // 17 for value in image.tobytes()]
            data = bytes((levels[n] << 4) | (levels[n+1] if n+1 < len(levels) else 0) for n in range(0, len(levels), 2))
            packed = zlib.compress(data, 9)
            rows.append('{%d,%d,%d,%d}' % (len(blob), len(packed), width, height))
            blob.extend(packed)
            report.append({'tag': tag, 'key': key, 'text': text, 'width': width, 'height': height,
                           'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                           'pixels_sha256': hashlib.sha256(data).hexdigest()})
    header = '/* Generated offline with regional fonts and RAQM; no runtime font dependency. */\n'
    header += '#define UPGRADE_ART_KEY_COUNT %dU\n#define UPGRADE_ART_LOCALE_COUNT %dU\n' % (len(keys), len(locales))
    header += 'static const char *const upgrade_art_tags[]={' + ','.join(json.dumps(item['tag']) for item in locales) + '};\n'
    header += 'static const char *const upgrade_art_keys[]={' + ','.join(json.dumps(key) for key in keys) + '};\n'
    header += 'static const unsigned char upgrade_art_pixels[]={\n'
    header += '\n'.join(','.join(str(value) for value in blob[n:n+24])+',' for n in range(0, len(blob), 24)) + '\n};\n'
    header += 'static const struct {unsigned offset,bytes;unsigned short w,h;} upgrade_art_rows[]={' + ','.join(rows) + '};\n'
    write(OUTPUT, header)
    manifest = {'inputs': inputs(), 'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'output_sha256': hashlib.sha256(OUTPUT.read_bytes()).hexdigest(), 'compressed_bytes': len(blob), 'labels': report}
    write(MANIFEST, json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print('Native recovery art: %d locales, %d labels, %d compressed bytes' % (len(locales), len(rows), len(blob)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not args.check:
        generate()
        return
    manifest = json.loads(MANIFEST.read_text())
    if manifest['inputs'] != inputs() or manifest['generator_sha256'] != hashlib.sha256(Path(__file__).read_bytes()).hexdigest() or manifest['output_sha256'] != hashlib.sha256(OUTPUT.read_bytes()).hexdigest():
        raise ValueError('Recovery text/art changed; regenerate offline shaped labels')
    print('Native recovery art inputs and committed pixels match')


if __name__ == '__main__':
    main()
