#!/usr/bin/env python3
"""Remove only reviewed retired guide/UI pictures from a build install directory."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data-dir', required=True)
args = parser.parse_args()
root = Path(args.data_dir).resolve()
if root.name != 'lvgl_data' or not root.is_dir():
    raise SystemExit('Expected an existing lvgl_data install directory')
policy = json.loads(Path(__file__).with_name('retired_fault_assets.json').read_text())
removed = 0
for item in policy['retired']:
    path = root / item['name']
    if path.parent != root or path.is_symlink():
        raise SystemExit('Invalid retired picture path: ' + str(path))
    if not path.exists():
        continue
    if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
        raise SystemExit('Changed old picture retained for review: ' + str(path))
    path.unlink()
    removed += 1
print('Removed %d archived guide pictures from the build install tree' % removed)
