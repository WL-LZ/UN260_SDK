#!/usr/bin/env python3
"""UN260 product-image filtering + hash-pinned USB cleanup companion.

Only explicitly reviewed SDK GE demos and retired icons; never scans for large
files or removes user images. Source originals stay available for rollback.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import stat
import subprocess
import tempfile

SDK = Path(__file__).resolve().parents[2]
APP = SDK / 'source/artinchip/test-lvgl'
GE_FILES = ('alpha_back_ground.bmp', 'alpha_dst.bmp', 'alpha_src.bmp', 'clock.bmp',
            'scale.bmp', 'scan_order.bmp', 'second.bmp', 'singer_alpha.bmp', 'tux.bmp')
GE_BINS = ('ge_alpha_blending', 'ge_bitblt', 'ge_bitblt_alpha', 'ge_dither',
           'ge_fillrect', 'ge_format', 'ge_rotate', 'ge_scale', 'ge_scan_order')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_target(root, rel):
    path = root / rel
    if Path(rel).is_absolute() or '..' in Path(rel).parts:
        raise RuntimeError('Unsafe relative path: ' + rel)
    if path.is_symlink() or any(p.is_symlink() for p in path.parents):
        raise RuntimeError('Refusing symlink path: ' + str(path))
    if root.resolve() not in path.resolve().parents:
        raise RuntimeError('Path escaped target: ' + str(path))
    return path


def unique_json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError('Duplicate locale font metadata key: ' + key)
        result[key] = value
    return result


def compact_locale_manifest(target):
    # Only remove JSON formatting from the verified build copy. Font files,
    # glyph coverage, license metadata and readable source remain unchanged.
    rel = 'usr/local/share/lvgl_data/locale_fonts/manifest.json'
    path = safe_target(target, rel)
    source = safe_target(APP, 'aic_ui/lvgl_data/locale_fonts/manifest.json')
    if not path.is_file() or not source.is_file() or stat.S_IMODE(path.stat().st_mode) != 0o644 or stat.S_IMODE(source.stat().st_mode) not in (0o644, 0o664) or path.stat().st_size > 2 * 1024 * 1024 or source.stat().st_size > 2 * 1024 * 1024:
        raise RuntimeError('Invalid locale font metadata path or permissions')
    original = path.read_bytes()
    expected = json.loads(source.read_bytes(), object_pairs_hook=unique_json_object)
    installed = json.loads(original, object_pairs_hook=unique_json_object)
    if installed != expected:
        raise RuntimeError('Unknown locale font metadata retained; source differs from target')
    if not isinstance(expected, dict) or set(expected) != {'status', 'sizes', 'locales'} or expected['status'] != 'candidate-not-released':
        raise RuntimeError('Unknown locale font metadata schema retained')
    sizes = expected['sizes']
    if not isinstance(sizes, list) or not sizes or any(type(n) is not int or not 0 < n <= 128 for n in sizes) or len(sizes) != len(set(sizes)) or not isinstance(expected['locales'], dict) or not expected['locales']:
        raise RuntimeError('Invalid locale font metadata sizes/locales')
    locale_keys = {'family', 'source_sha256', 'face_index', 'candidate_entries', 'missing_translation_count',
                   'codepoints', 'shaped_codepoints', 'files', 'license_metadata', 'bytes'}
    file_keys = {'size', 'file', 'bytes', 'sha256', 'codepoints', 'text_keys'}
    count = 0
    for tag, locale in expected['locales'].items():
        if not re.fullmatch(r'[A-Za-z]{2,8}(?:-[A-Za-z0-9]{1,8})*', tag) or not isinstance(locale, dict) or set(locale) != locale_keys:
            raise RuntimeError('Unknown locale font metadata entry retained')
        if not isinstance(locale['family'], str) or not isinstance(locale['source_sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', locale['source_sha256']) or not isinstance(locale['license_metadata'], dict):
            raise RuntimeError('Invalid locale font source/license metadata')
        for key in ('face_index', 'candidate_entries', 'missing_translation_count', 'bytes'):
            if type(locale[key]) is not int or locale[key] < 0:
                raise RuntimeError('Invalid locale font metadata count')
        if locale['missing_translation_count'] != 0 or not isinstance(locale['files'], list) or len(locale['files']) != len(sizes):
            raise RuntimeError('Incomplete locale font metadata')
        total = 0
        for size, entry in zip(sizes, locale['files']):
            if not isinstance(entry, dict) or set(entry) != file_keys or type(entry['size']) is not int or entry['size'] != size or entry['file'] != 'font-%d.bin' % size or type(entry['bytes']) is not int or entry['bytes'] <= 0 or not isinstance(entry['sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', entry['sha256']):
                raise RuntimeError('Invalid locale font file metadata')
            for points in (locale['codepoints'], locale['shaped_codepoints'], entry['codepoints']):
                if not isinstance(points, list) or any(type(point) is not int or not 0 <= point <= 0x10ffff for point in points):
                    raise RuntimeError('Invalid locale font codepoints')
            keys = entry['text_keys']
            if keys is not None and (not isinstance(keys, list) or any(not isinstance(key, str) for key in keys)):
                raise RuntimeError('Invalid locale font text contract')
            font = safe_target(target, rel.rsplit('/', 1)[0] + '/' + tag + '/' + entry['file'])
            if not font.is_file() or stat.S_IMODE(font.stat().st_mode) != 0o644 or font.stat().st_size != entry['bytes'] or digest(font) != entry['sha256']:
                raise RuntimeError('Locale font bytes/hash/permissions differ: ' + str(font))
            total += entry['bytes']
            count += 1
        if total != locale['bytes']:
            raise RuntimeError('Locale font aggregate size differs')
    compact = (json.dumps(expected, ensure_ascii=False, separators=(',', ':')) + '\n').encode('utf-8')
    if json.loads(compact) != expected:
        raise RuntimeError('Locale font metadata compaction changed meaning')
    if original != compact:
        temporary = safe_target(target, rel + '.un260-build-new')
        if temporary.exists():
            raise RuntimeError('Unknown locale font metadata temporary retained')
        created = False
        try:
            with temporary.open('xb') as output:
                created = True
                output.write(compact)
            temporary.chmod(0o644)
            temporary.replace(path)
        finally:
            if created and temporary.exists():
                temporary.unlink()
    return dict(before=len(original), after=len(compact), saved=len(original)-len(compact), fonts=count)


def inventory(target):
    spec = importlib.util.spec_from_file_location('retired_assets', APP / 'tools/retired_assets.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    retired = module.load_retired(APP)
    entries = []
    for name in GE_FILES:
        source = SDK / 'source/artinchip/aic-mpp/mpp_test/ge_data' / name
        entries.append(dict(path='usr/local/share/ge_data/' + name, sha256=digest(source),
                            bytes=source.stat().st_size, mode='0644', reason='SDK GE test image'))
    for name, entry in retired.items():
        source = APP / 'aic_ui/lvgl_data' / name
        entries.append(dict(path='usr/local/share/lvgl_data/' + name, sha256=entry['sha256'],
                            bytes=source.stat().st_size, mode='0644', reason='Reviewed unused recent icon'))
    # Installed/stripped demo hashes may vary across toolchains. Persist the
    # captured known build hashes in the generated report for incremental make.
    report = target.parent / 'images/un260-resource-cleanup.json'
    old = {e['path']: e for e in json.loads(report.read_text())['entries']} if report.exists() else {}
    for name in GE_BINS:
        rel = 'usr/local/bin/' + name
        path = safe_target(target, rel)
        if path.exists():
            if not path.is_file():
                raise RuntimeError('Not a regular demo executable: ' + rel)
            entries.append(dict(path=rel, sha256=digest(path), bytes=path.stat().st_size,
                                mode='0755', reason='SDK GE demo executable'))
        elif rel in old:
            entries.append(old[rel])
    return entries


def prune(target, entries):
    planned = []
    for entry in entries:
        path = safe_target(target, entry['path'])
        if not path.exists():
            continue
        if not path.is_file() or digest(path) != entry['sha256']:
            raise RuntimeError('Modified asset kept; review before release: ' + str(path))
        planned.append(path)
    # Prevalidate the whole plan; only then unlink exact files, never directories.
    for path in planned:
        path.unlink()
    return len(planned)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--cleanup-script', type=Path, required=True)
    args = parser.parse_args()
    target = args.target.absolute()
    if (target.name != 'target' or target.parent.parent != SDK / 'output'
            or not target.is_dir() or target.is_symlink()):
        raise RuntimeError('Not an SDK output/<board>/target directory')
    entries = inventory(target)
    # An ordinary SDK make can package an older installed app without rebuilding
    # its package. Do not authorize icon removal for such a stale executable.
    build = target.parent / 'build/test-lvgl'
    cache = (build / 'CMakeCache.txt').read_text()
    nm = re.search(r'^CMAKE_NM:FILEPATH=(.+)$', cache, re.M)[1]
    objcopy = re.search(r'^CMAKE_OBJCOPY:FILEPATH=(.+)$', cache, re.M)[1]
    symbols = subprocess.check_output([nm, str(build / 'test_lvgl')]).decode()
    for entry in entries:
        if entry['reason'] == 'Reviewed unused recent icon':
            name = entry['path'].split('/lvgl_data/', 1)[1]
            symbol = 'un260_asset_' + hashlib.sha256(name.encode()).hexdigest()[:16]
            if symbol in symbols:
                raise RuntimeError('Application still embeds retired icons; run make test-lvgl-rebuild first')
    with tempfile.TemporaryDirectory(prefix='un260-resource-elf-') as directory:
        for section in ('.text', '.rodata', '.data'):
            blobs = []
            for index, binary in enumerate((build / 'test_lvgl', target / 'usr/local/bin/test_lvgl')):
                part = Path(directory) / str(index)
                subprocess.run([objcopy, '--dump-section', section + '=' + str(part), str(binary)], check=True)
                blobs.append(part.read_bytes())
            if blobs[0] != blobs[1]:
                raise RuntimeError('Installed executable does not match the build; rebuild before packaging')
    app_sha = digest(target / 'usr/local/bin/test_lvgl')
    # Validate before publishing any deletion plan.
    for entry in entries:
        path = safe_target(target, entry['path'])
        if path.exists() and (not path.is_file() or digest(path) != entry['sha256']):
            raise RuntimeError('Unknown build file: ' + str(path))
    font_metadata = compact_locale_manifest(target)
    print('UN260 locale font metadata: %(before)d -> %(after)d bytes; saved=%(saved)d; %(fonts)d font files unchanged' % font_metadata)
    script = (Path(__file__).with_name('resource_cleanup.sh.in')).read_text()
    manifest = '\n'.join('%s|%d|%s|%s' % (e['sha256'], e['bytes'], e['mode'], e['path']) for e in entries)
    args.cleanup_script.parent.mkdir(parents=True, exist_ok=True)
    args.cleanup_script.write_text(script.replace('@MANIFEST@', manifest).replace('@APP_SHA256@', app_sha))
    # The installed helper may run while an older UI still has its executable
    # mapped. Never remove UI icons here, even after the on-disk app changes.
    sdk_manifest = '\n'.join('%s|%d|%s|%s' % (e['sha256'], e['bytes'], e['mode'], e['path'])
                             for e in entries if e['reason'].startswith('SDK GE '))
    helper = safe_target(target, 'usr/local/bin/un260_resource_cleanup')
    helper.parent.mkdir(parents=True, exist_ok=True)
    helper.write_text(script.replace('@MANIFEST@', sdk_manifest).replace('@APP_SHA256@', '-'))
    helper.chmod(0o755)
    report = target.parent / 'images/un260-resource-cleanup.json'
    report.write_text(json.dumps(dict(schema=1, app_sha256=app_sha, entries=entries,
                                     logical_bytes=sum(e['bytes'] for e in entries)), indent=2) + '\n')
    count = prune(target, entries)
    print('UN260 resource filter: removed %d exact build files; originals retained; %d cleanup entries' % (count, len(entries)))


if __name__ == '__main__':
    main()
