#!/usr/bin/env python3
"""Mirror updater full-package staging costs; never discount UBIFS compression.

Release qualification assumes at least 16 MiB root free. The device must still
check its real free space. Only omissions in the fully owned static tree retire files.
"""
import argparse
import json
from pathlib import Path
from build_delta import load
from release_manifest import table

ROOT_FREE_FLOOR_KB = 16384
ROOT_RESERVE_KB = 2048
APP_FREE_FLOOR_KB = 16384
APP_RESERVE_KB = 2048
APP_PATH = 'usr/local/bin/test_lvgl'
BOOT_PREFIX = 'usr/local/lib/un260-boot/versions/'
BOOT_PUBLICATION_KB = 8
STATIC_PREFIX = 'usr/local/share/lvgl_data/'
TRIAL_METADATA_OVERHEAD_KB = 32

def budget(old, new, route='legacy-root'):
    if route not in ('legacy-root', 'unified-app-volume', 'factory-app-volume'):
        raise ValueError('Unknown qualification route: ' + route)
    changed = {p: v for p, v in new.items() if old.get(p) != v}
    # A normal full package owns the complete static tree. Its omitted files
    # are journalled deletes, with old inodes retained until trial confirmation.
    retired = {p: v for p, v in old.items() if p.startswith(STATIC_PREFIX) and p not in new}
    transaction = bool(changed or retired)
    costs = {p: (len(v[0]) + 1023) // 1024 + 4 for p, v in changed.items()}
    app_volume = route != 'legacy-root'
    app_staged = costs.get(APP_PATH, 0) if app_volume else 0
    boot_publication = BOOT_PUBLICATION_KB if transaction and any(p.startswith(BOOT_PREFIX) for p in new) else 0
    file_staged = sum(costs.values()) - app_staged
    # Preserve the existing root_staged field: it includes boot publication.
    staged = file_staged + boot_publication
    trial_enabled = route == 'factory-app-volume'
    target_table_bytes = len(table(new)) if trial_enabled and new else 0
    trial_metadata = ((target_table_bytes + 1023) // 1024 + TRIAL_METADATA_OVERHEAD_KB
                      if trial_enabled and transaction else 0)
    # Already allocated boot versions and retired static files cannot be
    # credited as free space: their rollback copies survive installation.
    boot_retained = sum((len(v[0]) + 1023) // 1024 for p, v in old.items() if p.startswith(BOOT_PREFIX))
    backup_paths = (changed.keys() & old.keys()) | retired.keys()
    backup = sum((len(old[p][0]) + 1023) // 1024 + 4 for p in backup_paths)
    return dict(changed=len(changed) + len(retired), replaced=len(changed), deleted=len(retired),
                unchanged=len(new)-len(changed), route=route,
                root_file_staged_kb=file_staged, root_staged_kb=staged, root_peak_kb=staged,
                boot_publication_kb=boot_publication, boot_retained_kb=boot_retained,
                trial_enabled=trial_enabled, target_table_bytes=target_table_bytes,
                trial_metadata_kb=trial_metadata,
                root_required_kb=staged + trial_metadata + ROOT_RESERVE_KB if transaction else 0,
                app_staged_kb=app_staged,
                app_required_kb=app_staged + APP_RESERVE_KB if transaction and app_volume else 0,
                qualified_app_free_kb=APP_FREE_FLOOR_KB if app_volume else 0,
                usb_backup_required_kb=backup + 4096 if transaction else 0,
                qualified_root_free_kb=ROOT_FREE_FLOOR_KB,
                baseline_scope='Package files; assumes matching baseline boot descriptor and no extra installed files')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', required=True)
    ap.add_argument('--payload', required=True)
    ap.add_argument('--route', choices=('legacy-root','unified-app-volume','factory-app-volume'), default='legacy-root',
                    help='Factory route uses the pre-enrolled application volume; unified route requires migration')
    args = ap.parse_args()
    _, old = load(args.base)
    root = Path(args.payload)
    new = {}
    for p in root.rglob('*'):
        if p.is_symlink():
            raise ValueError('Symlink in payload: '+str(p))
        if p.is_file():
            new[p.relative_to(root).as_posix()] = (p.read_bytes(), p.stat().st_mode & 0o777)
    report = budget(old, new, args.route)
    report['baseline'] = str(Path(args.base).resolve())
    print('STORAGE_BUDGET ' + json.dumps(report, sort_keys=True))
    if report['root_required_kb'] > ROOT_FREE_FLOOR_KB:
        raise SystemExit('Release staging exceeds 16 MiB root-free qualification; '
                         'reduce payload or qualify a separate full-firmware migration. '
                         'Do not bypass device preflight.')
    if report['app_required_kb'] > APP_FREE_FLOOR_KB:
        raise SystemExit('Release staging exceeds 16 MiB application-volume-free qualification; '
                         'reduce the application or qualify a separate migration. Do not bypass device preflight.')

if __name__ == '__main__':
    main()
