#!/usr/bin/env python3
"""Qualify the ordinary full UPK: install now, reboot only to run the new app."""
import argparse
from pathlib import Path
import tarfile
from build_delta import load


def verify(package, target):
    meta, files = load(package)
    assert meta['schema'] == '1' and meta['package_type'] == 'ui', 'Routine release must be a normal full UI package'
    assert meta['storage_layout'] == 'app-volume-v1' and meta['storage_guard'] == 'syncfs-v1'
    assert not meta.get('storage_migration'), 'Routine release must not migrate storage'
    assert 'etc/un260/unified-request' not in files, 'Routine release must not arm boot continuation'
    assert not any(name.startswith('usr/local/share/un260-unified/') for name in files)
    assert 'usr/local/bin/test_lvgl' in files and 'usr/local/lib/liblvgl.so' in files
    with tarfile.open(package) as archive:
        plan = archive.extractfile('install.tsv').read().decode().splitlines()
        assert 'file|0755|usr/local/bin/test_lvgl' in plan, 'Application must be installed in this transaction'
    for name, (data, mode) in files.items():
        if name == 'etc/un260/package-version':
            assert data.decode().strip() == meta['version']
            continue
        installed = target / name
        assert installed.read_bytes() == data, name
        assert installed.stat().st_mode & 0o777 == mode, name
    return len(files)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--target', type=Path, required=True)
    args = parser.parse_args()
    count = verify(args.package, args.target)
    print('PASS direct full UPK: %d files match target, application installs now, no migration or boot continuation request' % count)
