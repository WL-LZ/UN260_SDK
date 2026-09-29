#!/usr/bin/env python3
"""Build-time only: enroll a fresh IMG copy, never access board MTD/USB.

The base target stays an ELF for UPK generation. Only the isolated UBIFS
fakeroot copy receives the launcher/active marker; the app is a required
second UBIFS image. Boot never initializes storage or needs a USB package.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from release_version import version
import os

OWNER = b'UN260_APP_VOLUME_V1\n'
LAUNCHER = b'#!/bin/sh\nexec /usr/local/bin/un260_app_storage --launch "$@"\n'
APP = 'usr/local/bin/test_lvgl'
STATE = 'etc/un260/app-storage'
APP_IMAGE = 'un260_app_page_2k_block_128k.ubifs'

def regular(path):
    if path.is_symlink() or any(p.is_symlink() for p in path.parents) or not path.is_file():
        raise ValueError('Expected regular non-symlink file: ' + str(path))
    return path.read_bytes()

def write(path, data, mode):
    if path.is_symlink() or any(p.is_symlink() for p in path.parents):
        raise ValueError('Unsafe factory path: ' + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    path.chmod(mode)

def enroll(root, base, appdir, release):
    app = regular(base / APP)
    if not app.startswith(b'\x7fELF') or regular(root / APP) != app:
        raise ValueError('Factory app is not the matching built ELF')
    if len(app) * 2 + 4 * 1024 * 1024 > 31 * 1024 * 1024:
        raise ValueError('App leaves insufficient uncompressed replacement headroom')
    for forbidden in (STATE, 'etc/un260/unified-request', 'var/lib/un260-updater', 'etc/ui_state'):
        if (root / forbidden).exists() or (root / forbidden).is_symlink():
            raise ValueError('Factory copy contains runtime/customer state: ' + forbidden)
    if appdir.exists() and (appdir.is_symlink() or any(appdir.iterdir())):
        raise ValueError('Factory application staging must be empty')
    write(appdir / 'test_lvgl', app, 0o755)
    write(appdir / '.un260-owner', OWNER, 0o644)
    write(root / APP, LAUNCHER, 0o755)
    write(root / STATE / 'active', OWNER, 0o644)
    write(root / 'etc/un260/package-version', (release + '\n').encode(), 0o644)
    return dict(schema=1, release=release, layout='app-volume-v1',
                geometry=dict(nand_bytes=134217728, page_bytes=2048, erase_bytes=131072,
                              root_bytes=67108864, app_partition_bytes=39845888),
                app_bytes=len(app), app_sha256=hashlib.sha256(app).hexdigest(),
                launcher_sha256=hashlib.sha256(LAUNCHER).hexdigest(),
                requires_usb=False, first_boot_formats=False)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    args = parser.parse_args()
    sdk = Path(__file__).resolve().parents[2]
    output = sdk / 'output/d211_d213_devkitf'
    root = args.root.absolute()
    expected = output / 'build/luban-fs/ubifs/target'
    if root == output / 'build/luban-fs/ubi/target':
        return  # UBI wraps the already built UBIFS; never alter the base tree.
    if root != expected or root.resolve() != expected or not root.is_dir():
        raise ValueError('Refusing a non-fakeroot-UBIFS build target')
    images = output / 'images'
    app_image = images / APP_IMAGE
    with tempfile.TemporaryDirectory(prefix='un260-app-', dir=root.parent) as temp:
        appdir = Path(temp) / 'files'
        report = enroll(root, output / 'target', appdir,
                        version(sdk, os.environ.get('UN260_UPDATE_VERSION')))
        generated = Path(temp) / APP_IMAGE
        subprocess.run([str(output / 'host/sbin/mkfs.ubifs'), '-d', str(appdir),
                        '-e', '126976', '-m', '2048', '-c', '304', '-x', 'lzo', '-F',
                        '-o', str(generated)], check=True)
        # Copy through an atomic same-directory replacement; never ship a
        # truncated previous app image if mkfs fails.
        staged = images / (APP_IMAGE + '.new')
        shutil.copyfile(generated, staged)
        staged.replace(app_image)
        report['app_ubifs_sha256'] = hashlib.sha256(app_image.read_bytes()).hexdigest()
        write(images / 'un260-factory-layout.json',
              (json.dumps(report, indent=2) + '\n').encode(), 0o644)
    print('UN260 factory layout: root launcher + owned app volume; no USB/first-boot format')

if __name__ == '__main__':
    main()
