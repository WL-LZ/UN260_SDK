#!/usr/bin/env python3
"""Mirror updater full-package staging costs; never discount UBIFS compression.

Release qualification assumes at least 16 MiB root free. The device must still
check its real free space. Missing files in a full package are NOT deletions.
"""
import argparse
import json
from pathlib import Path
from build_delta import load

ROOT_FREE_FLOOR_KB = 16384
ROOT_RESERVE_KB = 2048

def budget(old, new):
    changed = {p: v for p, v in new.items() if old.get(p) != v}
    staged = sum((len(v[0]) + 1023) // 1024 + 4 for v in changed.values())
    backup = sum((len(old[p][0]) + 1023) // 1024 + 4 for p in changed if p in old)
    return dict(changed=len(changed), unchanged=len(new)-len(changed),
                root_staged_kb=staged,
                root_required_kb=staged + ROOT_RESERVE_KB if changed else 0,
                usb_backup_required_kb=backup + 4096 if changed else 0,
                qualified_root_free_kb=ROOT_FREE_FLOOR_KB)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', required=True)
    ap.add_argument('--payload', required=True)
    args = ap.parse_args()
    _, old = load(args.base)
    root = Path(args.payload)
    new = {}
    for p in root.rglob('*'):
        if p.is_symlink():
            raise ValueError('Symlink in payload: '+str(p))
        if p.is_file():
            new[p.relative_to(root).as_posix()] = (p.read_bytes(), p.stat().st_mode & 0o777)
    report = budget(old, new)
    report['baseline'] = str(Path(args.base).resolve())
    print('STORAGE_BUDGET ' + json.dumps(report, sort_keys=True))
    if report['root_required_kb'] > ROOT_FREE_FLOOR_KB:
        raise SystemExit('Release staging exceeds 16 MiB root-free qualification; '
                         'reduce payload or qualify a separate full-firmware migration. '
                         'Do not bypass device preflight.')

if __name__ == '__main__':
    main()
