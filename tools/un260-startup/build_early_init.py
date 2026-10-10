#!/usr/bin/env python3
"""Build the stable, artwork-free PID-1 launcher for the UN260 NAND image."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--cc', required=True)
p.add_argument('--output', required=True)
p.add_argument('--kernel', required=True)
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
app = repo / 'source/artinchip/test-lvgl'
out = Path(a.output).resolve()
out.mkdir(parents=True, exist_ok=True)
binary = out / 'un260-early-init'
temp = out / 'un260-early-init.new'
subprocess.run([a.cc, '-O2', '-g0', '-static', '-s', '-Wall', '-Wextra', '-Werror',
               '-I' + str(app), str(repo / 'tools/un260-startup/early_init.c'),
               str(app / 'un260/boot/boot_bundle.c'), '-o', str(temp)], check=True)
data = temp.read_bytes()
assert len(data) < 1024 * 1024, 'stable initramfs launcher exceeds 1 MiB budget'
if not binary.exists() or binary.read_bytes() != data:
    temp.replace(binary)
else:
    temp.unlink()
listing = out / 'un260-initramfs.list'
content = ('dir /dev 0755 0 0\nnod /dev/console 0600 0 0 c 5 1\n'
           'file /init ' + str(binary) + ' 0755 0 0\n')
if not listing.exists() or listing.read_text() != content:
    listing.write_text(content)
config = repo / 'source/linux-5.10/scripts/config'
subprocess.run([str(config), '--file', str(Path(a.kernel) / '.config'),
               '--enable', 'BLK_DEV_INITRD', '--set-str', 'INITRAMFS_SOURCE', str(listing),
               '--set-val', 'INITRAMFS_ROOT_UID', '0', '--set-val', 'INITRAMFS_ROOT_GID', '0'], check=True)
report = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'static': True,
          'launcher_only': True, 'embedded_artwork': False,
          'root_mount': 'kernel command line; no hardcoded partition number'}
(out / 'un260-early-build.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report))
