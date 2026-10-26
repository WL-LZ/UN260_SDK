#!/usr/bin/env python3
"""Build-host validation/install helpers for immutable early-boot bundles."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess

SDK = Path(__file__).resolve().parents[2]
TREE = 'usr/local/lib/un260-boot/versions'
ACTIVE = 'etc/un260/boot-active'
MAGIC = 'UN260_BOOT_ACTIVE_V1'
HEX = re.compile(r'^[0-9a-f]{64}$')
KEYS = {'schema', 'id', 'protocol', 'player_sha256', 'player_bytes',
        'theme_sha256', 'theme_bytes', 'mode'}
LIMIT = 2 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def regular(root, rel):
    root = Path(root).absolute()
    path = root / rel
    if path.is_symlink() or any(parent.is_symlink() for parent in path.parents):
        raise ValueError('Symlink in boot bundle path: ' + str(path))
    if not path.is_file() or root.resolve() not in path.resolve().parents:
        raise ValueError('Invalid boot bundle path: ' + str(path))
    return path


def descriptor(data):
    if len(data) > 400 or not data.endswith(b'\n'):
        raise ValueError('Invalid boot descriptor size or terminator')
    lines = data.decode('ascii').splitlines()
    if len(lines) != 5 or lines[0] != MAGIC or not HEX.fullmatch(lines[1]) or not HEX.fullmatch(lines[2]):
        raise ValueError('Invalid boot active descriptor')
    if lines[3] == 'none':
        if lines[4] != 'none':
            raise ValueError('Incomplete previous boot identity')
    elif not HEX.fullmatch(lines[3]) or not HEX.fullmatch(lines[4]) or lines[3] == lines[1]:
        raise ValueError('Invalid previous boot identity')
    return tuple(lines[1:])


def static_elf(data):
    if len(data) < 64 or data[:4] != b'\x7fELF' or data[5] not in (1, 2):
        raise ValueError('Boot player is not ELF')
    order = '<' if data[5] == 1 else '>'
    if data[4] == 1:
        off = struct.unpack_from(order + 'I', data, 28)[0]
        size, count = struct.unpack_from(order + 'HH', data, 42)
    elif data[4] == 2:
        off = struct.unpack_from(order + 'Q', data, 32)[0]
        size, count = struct.unpack_from(order + 'HH', data, 54)
    else:
        raise ValueError('Unsupported ELF class')
    if count == 0 or size < 4 or off + count * size > len(data):
        raise ValueError('Invalid ELF program headers')
    for index in range(count):
        kind = struct.unpack_from(order + 'I', data, off + index * size)[0]
        if kind in (2, 3):
            raise ValueError('Boot player depends on dynamic runtime')


def validate(root, identity, manifest_hash, require_elf=True):
    if not HEX.fullmatch(identity) or not HEX.fullmatch(manifest_hash):
        raise ValueError('Invalid boot identity')
    directory = Path(root) / TREE / identity
    paths = {name: regular(root, TREE + '/' + identity + '/' + name)
             for name in ('manifest', 'player', 'theme.pack')}
    if set(path.name for path in directory.iterdir()) != set(paths):
        raise ValueError('Unexpected boot bundle contents')
    raw = paths['manifest'].read_bytes()
    if len(raw) > 2048 or not raw.endswith(b'\n') or sha(raw) != manifest_hash:
        raise ValueError('Boot manifest checksum/size')
    values = {}
    for line in raw.decode('ascii').splitlines():
        if line.count('=') != 1:
            raise ValueError('Invalid boot manifest entry')
        key, value = line.split('=')
        if key in values:
            raise ValueError('Duplicate boot manifest key')
        values[key] = value
    if set(values) != KEYS or values['schema'] != 'un260-boot-bundle-v1' or values['protocol'] != '1' or values['id'] != identity:
        raise ValueError('Boot manifest schema/identity')
    if values['mode'] not in ('animation', 'logo'):
        raise ValueError('Unsupported boot mode')
    for name, mode in (('player', 0o755), ('theme.pack', 0o644), ('manifest', 0o644)):
        if stat.S_IMODE(paths[name].stat().st_mode) != mode:
            raise ValueError('Boot bundle permissions: ' + name)
    for name, key in (('player', 'player'), ('theme.pack', 'theme')):
        content = paths[name].read_bytes()
        count = values[key + '_bytes']
        if not count.isdigit() or not 0 < int(count) <= LIMIT or len(content) != int(count):
            raise ValueError('Boot bundle size: ' + name)
        if not HEX.fullmatch(values[key + '_sha256']) or sha(content) != values[key + '_sha256']:
            raise ValueError('Boot bundle checksum: ' + name)
        if name == 'player' and require_elf:
            static_elf(content)
    header = paths['theme.pack'].read_bytes()[:32]
    if len(header) != 32 or header[:8] not in (b'UNBTPK1\0', b'UNBTPK2\0') or struct.unpack_from('<I', header, 28)[0] != {'animation': 1, 'logo': 2}[values['mode']]:
        raise ValueError('Boot theme mode/header differs from manifest')
    canonical = ('player_sha256=' + values['player_sha256'] + '\ntheme_sha256=' + values['theme_sha256'] + '\nmode=' + values['mode'] + '\n').encode()
    if sha(canonical) != identity:
        raise ValueError('Boot directory is not content addressed')
    return paths, values


def active(root, required=True):
    path = Path(root) / ACTIVE
    if not path.exists() and not path.is_symlink() and not required:
        return None
    current = descriptor(regular(root, ACTIVE).read_bytes())
    validate(root, *current[:2])
    if current[2] != 'none':
        validate(root, *current[2:])
    return current


def verify_metadata(metadata, files, target=None):
    identity = metadata.get('boot_bundle_id', '')
    manifest_hash = metadata.get('boot_manifest_sha256', '')
    boot_paths = [name for name in files if name.startswith(TREE + '/')]
    if not identity and not manifest_hash:
        if boot_paths:
            raise ValueError('Boot resources without activation metadata')
        return
    if not HEX.fullmatch(identity) or not HEX.fullmatch(manifest_hash):
        raise ValueError('Invalid package boot metadata')
    if metadata.get('storage_guard') != 'syncfs-boot-bundle-v1':
        raise ValueError('Boot package lacks matching platform guard')
    expected = {TREE + '/' + identity + '/' + name for name in ('player', 'theme.pack', 'manifest')}
    if set(boot_paths) != expected or ACTIVE in files:
        raise ValueError('Package boot targets/active ownership')
    if target is not None:
        current = active(target)
        if current[:2] != (identity, manifest_hash):
            raise ValueError('Factory active differs from UPK boot metadata')
        for name in expected:
            data, mode = files[name]
            path = regular(target, name)
            if data != path.read_bytes() or mode != stat.S_IMODE(path.stat().st_mode):
                raise ValueError('Installed/package boot file differs: ' + name)


def asset_python(target):
    # Match the CMake asset generator, independently of the SDK host PATH.
    cache = Path(target).parent / 'build/test-lvgl/CMakeCache.txt'
    if cache.exists() or cache.is_symlink():
        cache = regular(Path(target).parent, 'build/test-lvgl/CMakeCache.txt')
        prefix = 'UN260_ASSET_PYTHON:FILEPATH='
        values = [line[len(prefix):] for line in cache.read_text().splitlines() if line.startswith(prefix)]
        if len(values) != 1:
            raise ValueError('CMake asset Python missing or duplicate; rebuild application package')
        selected = values[0]
    else:
        # CMake uses these directories with NO_DEFAULT_PATH in this order.
        selected = next((name for name in ('/usr/bin/python3', '/usr/local/bin/python3')
                         if Path(name).is_file() and os.access(name, os.X_OK)), '')
    path = Path(selected)
    if not selected or not path.is_absolute() or '..' in path.parts or not re.fullmatch(r'/[A-Za-z0-9_./+-]+', selected) or not path.is_file() or not os.access(path, os.X_OK):
        raise ValueError('Invalid absolute CMake asset Python: ' + selected)
    # Do not silently fall back if a configured interpreter has lost its dependency.
    subprocess.run([selected, '-c', 'from PIL import Image'], check=True)
    return selected


def stage_factory(target):
    target = Path(target).absolute()
    if target.name != 'target' or target.parent.parent != SDK / 'output' or target.is_symlink():
        raise ValueError('Not the SDK build target')
    old = active(target, required=False)
    temporary_player = (target / 'usr/local/bin/un260_boot_player').exists()
    if temporary_player:
        player = regular(target, 'usr/local/bin/un260_boot_player')
    elif old:
        player = regular(target, TREE + '/' + old[0] + '/player')
    else:
        raise ValueError('Static boot player not installed; rebuild application package')
    output = target / TREE.rsplit('/', 1)[0]
    subprocess.run([asset_python(target), str(SDK / 'source/artinchip/test-lvgl/tools/build_boot_bundle.py'),
                    '--player', str(player), '--output', str(output)], check=True)
    generated = regular(target, TREE.rsplit('/', 1)[0] + '/boot-active')
    current = descriptor(generated.read_bytes())
    validate(target, *current[:2])
    previous = old[:2] if old and old[:2] != current[:2] else (old[2:] if old else ('none', 'none'))
    data = ('\n'.join((MAGIC, *current[:2], *previous)) + '\n').encode()
    destination = target / ACTIVE
    if destination.is_symlink() or destination.with_suffix('.tmp').is_symlink():
        raise ValueError('Unsafe factory active target')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.with_suffix('.tmp').write_bytes(data)
    destination.with_suffix('.tmp').chmod(0o644)
    destination.with_suffix('.tmp').replace(destination)
    active(target)
    fallback = target / (ACTIVE + '.previous')
    if fallback.is_symlink() or Path(str(fallback) + '.tmp').is_symlink():
        raise ValueError('Unsafe factory previous descriptor')
    if previous[0] != 'none':
        fallback.write_bytes(('\n'.join((MAGIC, *previous, 'none', 'none')) + '\n').encode())
        fallback.chmod(0o644)
    elif fallback.exists():
        fallback.unlink()
    obsolete = target / 'usr/local/bin/un260_boot_light'
    if obsolete.exists() or obsolete.is_symlink():
        if obsolete.is_symlink() or not obsolete.is_file() or obsolete.read_bytes()[:4] != b'\x7fELF':
            raise ValueError('Unknown obsolete build-target boot light retained')
        obsolete.unlink()
    keep = {current[0], previous[0]}
    for directory in (target / TREE).iterdir():
        if directory.name in keep:
            continue
        if not HEX.fullmatch(directory.name) or directory.is_symlink() or not directory.is_dir():
            raise ValueError('Unknown factory boot version retained: ' + str(directory))
        old_manifest = regular(target, TREE + '/' + directory.name + '/manifest')
        paths, _ = validate(target, directory.name, sha(old_manifest.read_bytes()))
        for file in paths.values():
            file.unlink()
        directory.rmdir()
    generated.unlink()
    if player.read_bytes() != (target / TREE / current[0] / 'player').read_bytes():
        raise ValueError('Temporary boot player differs')
    if temporary_player:
        player.unlink()
    print('Factory boot bundle: ' + current[0] + '; previous=' + previous[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('factory', 'metadata'))
    parser.add_argument('--target', required=True, type=Path)
    parser.add_argument('--copy-to', type=Path)
    args = parser.parse_args()
    if args.command == 'factory':
        stage_factory(args.target)
    else:
        current = active(args.target, required=False)
        if current is None:
            if (args.target / 'usr/local/bin/un260_boot_player').exists():
                raise ValueError('Boot player not staged by post-build hook')
            return
        paths, _ = validate(args.target, *current[:2])
        if args.copy_to:
            directory = args.copy_to / TREE / current[0]
            if directory.is_symlink():
                raise ValueError('Unsafe package boot directory')
            directory.mkdir(parents=True, exist_ok=True)
            for name, path in paths.items():
                shutil.copy2(path, directory / name)
        print(current[0])
        print(current[1])


if __name__ == '__main__':
    main()
