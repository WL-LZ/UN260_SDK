#!/usr/bin/env python3
"""Publish a static player and bounded, self-contained theme as one version.

Root-resident only: no LVGL, app-volume or initramfs asset dependency. The
version is a content digest, and active is the sole mutable selection record.
The updater publishes active after its durable commit, not through this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib
from PIL import Image

HEADER_BYTES = 184
BG_BYTES = 1280 * 400 * 4
BASE_BYTES = BG_BYTES + 96 * 96 * 4 + 2 * 500 * 128 * 4
MAX_PACKED = 2 * 1024 * 1024
MAX_RAW = 8 * 1024 * 1024


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate boot configuration key: ' + key)
        result[key] = value
    return result


def source_theme(root):
    source = (root / 'un260/lv_core/boot_anim/boot_theme_config.h').read_text()
    matches = re.findall(r'#ifndef UI_BOOT_ANIM_THEME\s+#define UI_BOOT_ANIM_THEME\s+(UI_BOOT_ANIM_THEME_[A-D]|[1-4])\b', source)
    if len(matches) != 1:
        raise ValueError('cannot determine the default UI theme from boot_theme_config.h')
    value = matches[0]
    if value in ('UI_BOOT_ANIM_THEME_C', '3'):
        return 'c'
    if value in ('UI_BOOT_ANIM_THEME_D', '4'):
        return 'd'
    raise ValueError('the unified early boot bundle supports source theme C/D; current UI theme A/B is unsupported')


def logo_source(root, value):
    if value is None:
        return None
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_./-]+', value):
        raise ValueError('logo_surface must be a PNG path relative to the test-lvgl source tree')
    relative = Path(value)
    if relative.is_absolute() or any(part in ('', '.', '..') for part in value.split('/')) or relative.suffix.lower() != '.png':
        raise ValueError('logo_surface must be a safe relative .png path without parent traversal')
    resolved_root = root.resolve()
    candidate = (resolved_root / relative).resolve(strict=True)
    if resolved_root not in candidate.parents or not candidate.is_file():
        raise ValueError('logo_surface resolves outside the source tree or is not a regular file')
    with Image.open(candidate) as image:
        if image.format != 'PNG' or image.size != (1280, 400):
            raise ValueError('logo_surface must be a valid 1280 x 400 PNG')
        image.verify()
    return candidate


def read_config(root, path=None, theme_override=None):
    path = path or root / 'un260/boot/boot_bundle_config.json'
    if path.stat().st_size > 4096:
        raise ValueError('boot bundle configuration exceeds 4096 bytes')
    configuration = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    if not isinstance(configuration, dict) or set(configuration) != {'schema', 'mode', 'logo_surface'}:
        raise ValueError('boot bundle configuration requires exactly schema, mode and logo_surface')
    if type(configuration['schema']) is not int or configuration['schema'] != 1:
        raise ValueError('unsupported boot bundle configuration schema')
    if configuration['mode'] not in ('animation', 'logo'):
        raise ValueError('boot bundle mode must be animation or logo')
    return theme_override or source_theme(root), configuration['mode'], logo_source(root, configuration['logo_surface'])


def native_art(root, theme):
    source = (root / 'un260/font/boot_welcome_native.h').read_text()
    selector = '#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_D' if theme == 'c' else '#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D'
    selected = source.split(selector, 1)[1].split('#endif', 1)[0]
    pixel_text = selected.split('boot_welcome_pixels[]={', 1)[1].split('};', 1)[0]
    pixels = bytes(int(n, 16) for n in re.findall(r'0x([0-9a-fA-F]+)', pixel_text))
    descriptor = selected.split('boot_welcome_art[]=', 1)[1].split(';', 1)[0]
    entries = [tuple(map(int, row)) for row in re.findall(r'\{(\d+),(\d+),(\d+),(\d+)\}', descriptor)]
    registry = (root / 'un260/lv_core/boot_anim/boot_welcome_registry.h').read_text()
    tags = re.findall(r'"([A-Za-z0-9-]+)"', registry.split('boot_welcome_tags[]=', 1)[1].split(';', 1)[0])
    assert len(tags) == len(entries) and 1 <= len(tags) <= 32
    table = bytearray()
    for tag, (offset, width, height, top) in zip(tags, entries):
        encoded = tag.encode('ascii')
        assert len(encoded) < 16 and 0 < width <= 500 and 0 < height <= 128
        assert top + height <= 128 and offset + (width * height + 1) // 2 <= len(pixels)
        table.extend(struct.pack('<16sIIII', encoded, offset, width, height, top))
    return bytes(table), pixels, tags


def scene(root, theme):
    source = (root / 'un260/lv_core/boot_anim/boot_welcome_timing.h').read_text()
    branches = source.split('#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D', 1)[1].split('#else', 1)
    selected = branches[1].split('#endif', 1)[0] if theme == 'c' else branches[0]
    values = {key: int(value) for key, value in re.findall(r'#define (BOOT_[A-Z_]+) (\d+)U?\b', selected)}
    names = ['ENTRANCE_START', 'ENTRANCE_DURATION', 'BRAND_EXIT_START', 'BRAND_EXIT_DURATION',
             'WELCOME_START', 'WELCOME_DURATION', 'WELCOME_SETTLED', 'DOT_START', 'DOT_PERIOD',
             'TEXT_Y', 'DOT_Y', 'DOT_X', 'DOT_STEP', 'DOT_SIZE', 'DOT_RADIUS_SQ']
    result = [values['BOOT_' + name] for name in names]
    result.extend([78, 264] if theme == 'c' else [120, 196])
    result.extend([1, 0, 0])
    assert len(result) == 20
    return result, values['BOOT_WELCOME_SETTLED']


def theme_pack(root, theme='d', mode='animation', logo_path=None):
    raw_path = root / ('aic_ui/generated_assets/boot_theme_' + theme + '/boot-light.bin')
    raw_file = raw_path.read_bytes()
    assert raw_file[:8] == b'UNBOOT1\0' and len(raw_file) == BASE_BYTES + 16
    base = raw_file[16:]
    logo = Image.frombytes('RGBA', (1280, 400), base[:BG_BYTES], 'raw', 'BGRA')
    if logo_path:
        artwork = Image.open(logo_path).convert('RGBA')
        assert artwork.size == (1280, 400), 'custom logo surface must be 1280 x 400'
        logo.alpha_composite(artwork)
    else:
        emblem = Image.open(root / 'aic_ui/lvgl_data/boot_theme_c/emblem.png').convert('RGBA')
        assert emblem.size == (96, 96)
        logo.alpha_composite(emblem, (592, 152))
    table, native, tags = native_art(root, theme)
    logo_offset = len(base)
    table_offset = logo_offset + BG_BYTES
    native_offset = table_offset + len(table)
    payload = base + logo.tobytes('raw', 'BGRA') + table + native
    timeline, settled = scene(root, theme)
    packed = zlib.compress(payload, 9)
    assert len(payload) <= MAX_RAW and len(packed) <= MAX_PACKED, 'theme exceeds bounded boot budget'
    words = [1, HEADER_BYTES, 1280, 400, 3 if theme == 'c' else 4, 1 if mode == 'animation' else 2,
             len(payload), len(packed), zlib.crc32(payload), len(tags), len(base),
             logo_offset, table_offset, native_offset, len(native), settled if mode == 'animation' else 0]
    identity = hashlib.sha256(struct.pack('<20I', *timeline) + payload).digest()
    header = b'UNBTPK1\0' + struct.pack('<16I', *words) + identity + struct.pack('<20I', *timeline)
    assert len(header) == HEADER_BYTES
    return header + packed, {'raw_bytes': len(payload), 'packed_bytes': len(packed), 'languages': tags}


def put_version(path, data, mode):
    if path.exists():
        assert path.is_file() and not path.is_symlink() and path.read_bytes() == data, 'immutable version collision: ' + str(path)
    else:
        temporary = path.with_name(path.name + '.new')
        temporary.write_bytes(data)
        temporary.chmod(mode)
        temporary.replace(path)
    path.chmod(mode)


def publish(player, output, root, theme='d', mode='animation', logo_path=None):
    player_data = player.read_bytes()
    assert player_data[:4] == b'\x7fELF' and len(player_data) <= 2 * 1024 * 1024, 'static player exceeds boot budget'
    program_headers = subprocess.check_output(['readelf', '-l', str(player)], text=True)
    assert not re.search(r'\bINTERP\b', program_headers), 'root boot player must be static'
    pack, info = theme_pack(root, theme, mode, logo_path)
    player_sha = hashlib.sha256(player_data).hexdigest()
    theme_sha = hashlib.sha256(pack).hexdigest()
    version_id = hashlib.sha256(('player_sha256=' + player_sha + '\ntheme_sha256=' + theme_sha + '\nmode=' + mode + '\n').encode('ascii')).hexdigest()
    manifest = ('schema=un260-boot-bundle-v1\nid=' + version_id + '\nprotocol=1\n'
                'player_sha256=' + player_sha + '\nplayer_bytes=' + str(len(player_data)) + '\n'
                'theme_sha256=' + theme_sha + '\ntheme_bytes=' + str(len(pack)) + '\nmode=' + mode + '\n').encode('ascii')
    directory = output / 'versions' / version_id
    directory.mkdir(parents=True, exist_ok=True)
    assert not directory.is_symlink()
    put_version(directory / 'player', player_data, 0o755)
    put_version(directory / 'theme.pack', pack, 0o644)
    put_version(directory / 'manifest', manifest, 0o644)
    manifest_sha = hashlib.sha256(manifest).hexdigest()
    # Factory image seed only. This is never copied straight into an installed
    # device by the runtime updater, which owns durable publish and recovery.
    active = ('UN260_BOOT_ACTIVE_V1\n' + version_id + '\n' + manifest_sha + '\nnone\nnone\n').encode('ascii')
    output.mkdir(parents=True, exist_ok=True)
    temporary = output / 'boot-active.new'
    temporary.write_bytes(active)
    temporary.chmod(0o644)
    temporary.replace(output / 'boot-active')
    return version_id, manifest_sha, info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--validate-config', action='store_true')
    parser.add_argument('--theme', choices=['c', 'd'])
    parser.add_argument('--mode', choices=['animation', 'logo'])
    parser.add_argument('--logo', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    theme, mode, logo = read_config(root, args.config, args.theme)
    if args.mode:
        mode = args.mode
    if args.logo:
        try:
            relative = args.logo.resolve(strict=True).relative_to(root.resolve())
        except ValueError as error:
            raise ValueError('--logo must be inside the test-lvgl source tree') from error
        logo = logo_source(root, relative.as_posix())
    if args.validate_config:
        print('BOOT_CONFIG theme=' + theme + ' mode=' + mode + ' logo_surface=' + (str(logo) if logo else 'default'))
        return
    if not args.player or not args.output:
        parser.error('--player and --output are required to publish a boot bundle')
    version_id, manifest_sha, info = publish(args.player, args.output, root, theme, mode, logo)
    print('BOOT_BUNDLE id=' + version_id + ' manifest_sha256=' + manifest_sha)
    print('BOOT_THEME raw_bytes=' + str(info['raw_bytes']) + ' packed_bytes=' + str(info['packed_bytes']) +
          ' languages=' + ','.join(info['languages']))


if __name__ == '__main__':
    main()
