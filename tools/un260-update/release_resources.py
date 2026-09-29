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
