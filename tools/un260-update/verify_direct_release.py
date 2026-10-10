#!/usr/bin/env python3
"""Qualify the ordinary full UPK: install now, reboot only to run the new app."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
from build_delta import load
from boot_bundle import verify_metadata
from release_profile import check_profile
from release_manifest import verify as verify_envelope, factory_contract


def verify(package, target, legacy_test=False):
    meta, files = load(package)
    check_profile(meta, files, legacy_test=legacy_test)
    verify_metadata(meta, files, target)
    assert meta['schema'] == '1' and meta['package_type'] == 'ui', 'Routine release must be a normal full UI package'
    assert meta['storage_layout'] == 'app-volume-v1' and meta['storage_guard'] == ('syncfs-boot-bundle-v1' if meta.get('boot_bundle_id') else 'syncfs-v1')
    assert not meta.get('storage_migration'), 'Routine release must not migrate storage'
    assert 'etc/un260/unified-request' not in files, 'Routine release must not arm boot continuation'
    assert not any(name.startswith('usr/local/share/un260-unified/') for name in files)
    if not legacy_test:
        factory_contract(target)
    assert 'usr/local/bin/test_lvgl' in files and 'usr/local/lib/liblvgl.so' in files
    with tarfile.open(package) as archive:
        verify_envelope(meta, files, archive.extractfile('target.tsv').read(), require_envelope=not legacy_test)
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
    parser.add_argument('--legacy-test', action='store_true', help='Explicitly qualify an old-base application test, not a factory release')
    parser.add_argument('--report', type=Path, help='Write the verified content inventory for release support')
    args = parser.parse_args()
    count = verify(args.package, args.target, legacy_test=args.legacy_test)
    if args.report:
        meta, files = load(args.package)
        report = dict(schema=1, release_type='direct-full', version=meta['version'],
                      target_id=meta['target_id'], package_id=meta['package_id'],
                      package_sha256=hashlib.sha256(args.package.read_bytes()).hexdigest(),
                      payload_bytes=sum(len(data) for data, _ in files.values()),
                      files=[dict(path=name, sha256=hashlib.sha256(data).hexdigest(),
                                  mode='%04o' % mode, size=len(data)) for name, (data, mode) in sorted(files.items())])
        if args.report.is_symlink() or Path(str(args.report) + '.tmp').is_symlink():
            raise ValueError('Symlinked release report')
        args.report.parent.mkdir(parents=True, exist_ok=True)
        temporary = Path(str(args.report) + '.tmp')
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(args.report)
    print('PASS direct full UPK: %d files match target, application installs now, no migration or boot continuation request' % count)
