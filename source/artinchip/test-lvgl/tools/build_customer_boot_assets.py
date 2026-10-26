#!/usr/bin/env python3
"""Export approved customer-boot pixels without choosing a pack schema.

The early player and LVGL can consume the same byte slices.  This module does
not mutate bundle selection, player configuration, CMake, or application state.
Pillow is an offline dependency; device-side PNG/SVG decoding is unnecessary.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def export_source_images(root=None):
    """Return (index, payload); offsets address payload, with no implicit padding.

    Every index entry has id, format, width, height, stride, offset and bytes.
    BGRA8888 stores bytes B/G/R/A, with straight alpha unless alpha_mode=opaque.
    A8 stores coverage only; color_rgb is the approved default tint.  Remaining
    manifest metadata contains the reference optical box and fixed hand anchor.
    No runtime or disk-format field numbering is imposed by this helper.
    """
    root = Path(root) if root is not None else ROOT
    source = root / 'tools/assets/customer_boot'
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    configured = re.search(r'#define\s+UI_VERSION\s+"([^"]+)"', (root / 'un260/lv_system/user_cfg.h').read_text()).group(1)
    if manifest.get('version_string') != configured:
        raise ValueError('customer version artwork differs from UI_VERSION; regenerate titles offline')
    index, pieces, offset, ids = [], [], 0, set()
    for asset in manifest['assets'] + manifest.get('locale_titles', []):
        if asset['id'] in ids:
            raise ValueError('duplicate customer boot asset: ' + asset['id'])
        ids.add(asset['id'])
        path = source / asset['png']
        if path.parent != source or path.suffix != '.png':
            raise ValueError('asset is not a direct PNG source: ' + str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest() != asset['sha256']:
            raise ValueError('customer source changed without manifest review: ' + asset['id'])
        with Image.open(path) as original:
            image = original.convert('RGBA')
        if asset['role'] == 'background' and image.getchannel('A').getextrema() != (255, 255):
            raise ValueError('customer background must be fully opaque')
        if asset['asset_id'] == 12 and asset.get('firmware_version') != configured:
            raise ValueError('localized version artwork differs from UI_VERSION')
        width, height = image.size
        if asset['format'] == 'BGRA8888':
            pixels = image.tobytes('raw', 'BGRA')
            channels = 4
        elif asset['format'] == 'A8':
            pixels = image.getchannel('A').tobytes()
            channels = 1
        else:
            raise ValueError('unknown source pixel format: ' + asset['format'])
        if asset['role'] == 'gesture':
            if image.size != (200, 220) or asset['anchor_xy'] != [100, 210]:
                raise ValueError('customer hand geometry changed: ' + asset['id'])
            if image.getchannel('A').crop((0, 210, 200, 220)).getbbox():
                raise ValueError('customer wrist extends below shared clip')
        if asset['role'] == 'background' and image.size != tuple(manifest['canvas']):
            raise ValueError('customer background dimensions differ from canvas')
        entry = dict(asset)
        entry.update(width=width, height=height, stride=width * channels,
                     offset=offset, bytes=len(pixels),
                     pixel_sha256=hashlib.sha256(pixels).hexdigest())
        index.append(entry)
        pieces.append(pixels)
        offset += len(pixels)
    return index, b''.join(pieces)


def resource_budget(index, payload):
    """Measured static payload sizes; peak excludes external framebuffer owners."""
    packed = zlib.compress(payload, 9)
    largest = max(item['bytes'] for item in index if item['role'] != 'background')
    return {'sprites': len(index), 'raw_bytes': len(payload),
            'zlib9_bytes': len(packed), 'pixel_sha256': hashlib.sha256(payload).hexdigest(),
            'raw_plus_packed_bytes': len(payload) + len(packed),
            'optional_hand_bgra_scratch_bytes': 200 * 220 * 4,
            'optional_largest_sprite_scratch_bytes': largest,
            'framebuffer_bytes_each': 1280 * 400 * 4,
            'per_asset': [{key: item[key] for key in ('id', 'format', 'width', 'height', 'bytes')}
                          for item in index]}



def generate_localized_titles(root=None):
    """Offline whole-word title art. Normal builds only consume committed PNGs."""
    root = Path(root) if root is not None else ROOT
    sys.path.insert(0, str(root / 'tools'))
    import gen_i18n
    import cairo
    import gi
    gi.require_version('Pango', '1.0')
    gi.require_version('PangoCairo', '1.0')
    from gi.repository import Pango, PangoCairo
    from fontTools.ttLib import TTFont
    registry, catalogues = gen_i18n.catalogues()
    profiles = json.loads((root / 'i18n/font_profiles.json').read_text())
    source = root / 'tools/assets/customer_boot'
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    templates = [a for a in manifest['assets'] if a['role'] == 'title' or a['asset_id'] == 12]
    version_source = (root / 'un260/lv_system/user_cfg.h').read_text()
    version = re.search(r'#define\s+UI_VERSION\s+"([^"]+)"', version_source).group(1)
    rows = []
    for locale in registry:
        tag = locale['tag']
        profile = profiles[locale['font_profile']]
        font_path = Path(profile['source'])
        face = profile.get('face_indices', {}).get(tag, profile.get('face_index', 0))
        font_source = TTFont(str(font_path), fontNumber=face)
        family = font_source['name'].getDebugName(1)
        coverage = font_source.getBestCmap()
        for template in templates:
            if tag == 'en' and template['role'] == 'title':
                continue
            is_version = template['asset_id'] == 12
            text = catalogues[tag][template['msgid']] + (' ' + version if is_version else '')
            if not text or any(ord(char) not in coverage for char in text if not char.isspace()):
                raise ValueError('missing title glyph: ' + tag + ' ' + template['msgid'])
            scratch = cairo.ImageSurface(cairo.FORMAT_A8, 1, 1)
            context = cairo.Context(scratch)
            layout = PangoCairo.create_layout(context)
            language = Pango.Language.from_string(tag)
            layout.get_context().set_language(language)
            layout.get_context().set_base_dir(Pango.Direction.RTL if locale['direction'] == 'rtl' else Pango.Direction.LTR)
            description = Pango.FontDescription()
            description.set_family(family)
            description.set_absolute_size(160 * Pango.SCALE)
            layout.set_font_description(description)
            layout.set_text(text, -1)
            ink, logical = layout.get_pixel_extents()
            surface = cairo.ImageSurface(cairo.FORMAT_A8, ink.width + 8, ink.height + 8)
            context = cairo.Context(surface)
            context.set_source_rgba(1, 1, 1, 1)
            context.move_to(4 - ink.x, 4 - ink.y)
            PangoCairo.show_layout(context, layout)
            surface.flush()
            mask = Image.frombytes('L', (surface.get_width(), surface.get_height()),
                                   bytes(surface.get_data()), 'raw', 'L', surface.get_stride())
            mask = mask.crop(mask.getbbox())
            sx, sy, sw, sh = template['source_bbox_xywh']
            resampling = getattr(Image, 'Resampling', Image).LANCZOS
            mask.thumbnail((396, sh - 4), resampling)
            image = Image.new('RGBA', (mask.width + 4, mask.height + 4), (255, 255, 255, 0))
            alpha = Image.new('L', image.size)
            alpha.paste(mask, (2, 2))
            image.putalpha(alpha)
            filename = ('version-' + tag if is_version else 'title-' + tag + '-' + template['id']) + '.png'
            path = source / filename
            image.save(path)
            width, height = image.size
            row = {'id': ('version' if tag == 'en' else 'version.' + tag) if is_version else template['id'] + '.' + tag, 'asset_id': template['asset_id'],
                   'png': filename, 'format': 'A8', 'role': template['role'],
                   'language': tag, 'tag': tag, 'language_id': locale['id'],
                   'msgid': template['msgid'], 'text': text,
                   'color_rgb': template['color_rgb'],
                   'source_bbox_xywh': [sx + sw - width if is_version else round(sx + (sw - width) / 2), round(sy + (sh - height) / 2), width, height],
                   'ink_bbox_xywh': [2, 2, width - 4, height - 4],
                   'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                   'font_family': family, 'font_sha256': hashlib.sha256(font_path.read_bytes()).hexdigest(),
                   'font_face': face, 'shaper': 'PangoCairo ' + Pango.version_string()}
            if 'aliases' in template:
                row['aliases'] = template['aliases']
            if is_version:
                row['firmware_version'] = version
            if tag == 'en':
                position = next(i for i, item in enumerate(manifest['assets']) if item['asset_id'] == 12)
                manifest['assets'][position] = row
            else:
                rows.append(row)
        font_source.close()
    manifest['locale_titles'] = rows
    manifest['version_string'] = version
    (source / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--generate-titles', action='store_true', help='offline: update committed locale title PNGs using Pango/Cairo')
    parser.add_argument('--output', type=Path,
                        help='optional diagnostic output directory, never a theme pack')
    args = parser.parse_args()
    if args.generate_titles:
        generate_localized_titles(args.root)
    index, pixels = export_source_images(args.root)
    budget = resource_budget(index, pixels)
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / 'customer-source-pixels.bin').write_bytes(pixels)
        (args.output / 'customer-source-index.json').write_text(
            json.dumps(index, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        (args.output / 'customer-source-budget.json').write_text(
            json.dumps(budget, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(budget, indent=2))


if __name__ == '__main__':
    main()
