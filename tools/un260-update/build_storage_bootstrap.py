#!/usr/bin/env python3
"""Build a small schema-1 bridge or explicitly approved migration package.

Neither package replaces the application or user data. The bridge cannot format
NAND. The migration flavor requests one reviewed partition fingerprint only.
"""
import argparse
import hashlib
import io
from pathlib import Path
import tarfile
from build_delta import load

PATHS = ('usr/local/bin/un260_storage_sync', 'usr/local/bin/un260_resource_cleanup', 'usr/local/bin/un260_app_storage',
         'usr/bin/ui_update.sh', 'etc/init.d/S00lvgl')
APPROVED = 'e19b211e20f31ce8d82c1e40b6fe3feca04e758f887c17beb976e444ab964b17'


def build(full, output, migrate=False):
    _, files = load(full)
    payload = {}
    for name in PATHS:
        if name not in files or files[name][1] != 0o755:
            raise ValueError('Missing executable bootstrap component: ' + name)
        payload[name] = files[name]
    required = sum((len(data) + 1023) // 1024 + 4 for data, _ in payload.values()) + 2048
    if required > 2304:
        raise ValueError('Bootstrap exceeds 2.25 MiB root-space qualification')
    checks = ''.join('%s  payload/%s\n' % (hashlib.sha256(data).hexdigest(), name)
                     for name, (data, _) in sorted(payload.items())).encode()
    metadata = ('format=UN260_UPGRADE\nschema=1\nproduct=UN260\n'
                'package_type=%s\nversion=storage-%s-20260928-r3\n'
                'package_id=%s\nrequires_reboot=1\nstorage_layout=app-volume-v1\nstorage_guard=syncfs-v1\n%s'
                % ('storage-migration' if migrate else 'ui-maintenance',
                   'migration' if migrate else 'bootstrap', hashlib.sha256(checks).hexdigest(),
                   'storage_migration=%s\n' % APPROVED if migrate else '')).encode()
    entries = {'manifest.ini': (metadata, 0o644), 'checksums.sha256': (checks, 0o644),
               'install.tsv': ((''.join('file|0755|%s\n' % name for name in PATHS) +
                   'tree|0755|usr/local/share/ge_data\n').encode(), 0o644)}
    entries.update({'payload/' + name: value for name, value in payload.items()})
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + '.tmp')
    with tarfile.open(temporary, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
        directory = tarfile.TarInfo('payload')
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        archive.addfile(directory)
        # Older installers skip their automatic SDK cleanup when this tree is
        # package-owned. An EMPTY additive tree changes no files: it prevents
        # the old, unguarded helper deleting SDK files during the repair bridge.
        directory = tarfile.TarInfo('payload/usr/local/share/ge_data')
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        archive.addfile(directory)
        for name, (data, mode) in entries.items():
            item = tarfile.TarInfo(name)
            item.mode = mode
            item.size = len(data)
            archive.addfile(item, io.BytesIO(data))
    # Independently parse/check before replacing a previous published package.
    _, actual = load(temporary)
    if actual != payload:
        raise ValueError('Bootstrap readback mismatch')
    temporary.replace(output)
    print('BOOTSTRAP root_required_kb=%d files=%d bytes=%d' % (required, len(payload), output.stat().st_size))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--full', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--approved-migration', action='store_true', help='ONLY the reviewed 20260928 mtd11 fingerprint; requires bridge first')
    args = parser.parse_args()
    build(args.full, args.output, args.approved_migration)
