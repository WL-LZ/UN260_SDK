#!/usr/bin/env python3
"""One release identity for the factory filesystem and USB packages."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess

INPUT_ROOTS = ('source/artinchip/test-lvgl', 'source/third-party/lvgl-8.3.2',
               'package/artinchip/test-lvgl', 'package/third-party/lvgl',
               'target/d211/d213_devkitf', 'tools/un260-update', 'tools/un260-startup')
IGNORE_DIRS = {'.git', '__pycache__', '.cache', 'build', 'output', 'work'}


def build_config(sdk):
    """Select the current SDK profile, including out-of-tree build outputs."""
    candidates = []
    if os.environ.get('BR2_CONFIG'):
        candidates.append(Path(os.environ['BR2_CONFIG']))
    if os.environ.get('TARGET_DIR'):
        candidates.append(Path(os.environ['TARGET_DIR']).parent / '.config')
    if os.environ.get('OUTPUT_DIR'):
        candidates.append(Path(os.environ['OUTPUT_DIR']) / '.config')
    if os.environ.get('BINARIES_DIR'):
        candidates.append(Path(os.environ['BINARIES_DIR']).parent / '.config')
    candidates.append(Path(sdk) / 'output/d211_d213_devkitf/.config')
    return next((path for path in candidates if path.is_file()), None)


def input_identity(sdk):
    """Hash source inputs, independent of file timestamps and build outputs."""
    sdk = Path(sdk)
    digest = hashlib.sha256()
    for name in INPUT_ROOTS:
        root = sdk / name
        if not root.exists():
            continue
        for directory, dirs, names in os.walk(root, followlinks=False):
            dirs[:] = sorted(d for d in dirs if d not in IGNORE_DIRS)
            for filename in sorted(names):
                if filename.startswith('.') or filename.endswith(('.pyc', '.pyo', '~', '.tmp')):
                    continue
                path = Path(directory) / filename
                rel = path.relative_to(sdk).as_posix()
                if path.is_symlink():
                    data = ('symlink:' + os.readlink(path)).encode()
                elif path.is_file():
                    data = path.read_bytes()
                else:
                    raise ValueError('Unsupported source input: ' + rel)
                digest.update(rel.encode() + b'\0' + hashlib.sha256(data).digest())
    config = build_config(sdk)
    if config is not None:
        # Content, not the absolute output location, identifies this profile.
        digest.update(b'build-config\0' + hashlib.sha256(config.read_bytes()).digest())
    return digest.hexdigest()

def image_version(sdk):
    config = (sdk / 'target/d211/d213_devkitf/image_cfg.json').read_text()
    match = re.search(r'"version"\s*:\s*"([^"]+)"', config)
    if not match or not re.fullmatch(r'[A-Za-z0-9._-]{1,48}', match.group(1)):
        raise ValueError('Invalid image base version')
    return match.group(1)

def version(sdk, explicit=None):
    if explicit is not None and explicit != '':
        result = explicit
    else:
        base = image_version(sdk)
        try:
            rev = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', '--short=12', 'HEAD'],
                                          text=True, stderr=subprocess.DEVNULL).strip()
            dirty = subprocess.check_output(['git', '-C', str(sdk), 'status', '--porcelain', '--untracked-files=normal'],
                                            text=True, stderr=subprocess.DEVNULL)
            suffix = '-' + rev + ('-dirty' if dirty else '') + '-' + input_identity(sdk)[:12]
            result = base[:48-len(suffix)] + suffix
        except (OSError, subprocess.CalledProcessError):
            suffix = '-local-' + input_identity(sdk)[:12]
            result = base[:48-len(suffix)] + suffix
    if not re.fullmatch(r'[A-Za-z0-9._-]{1,48}', result):
        raise ValueError('Invalid UN260 release version')
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', action='store_true')
    args = parser.parse_args()
    sdk = Path(__file__).resolve().parents[2]
    print(image_version(sdk) if args.base else version(sdk, os.environ.get('UN260_UPDATE_VERSION')))
