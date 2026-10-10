#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path
import tarfile
import tempfile
from build_delta import load
from boot_bundle import verify_metadata
from release_profile import check_profile
from build_unified import BRIDGE, NESTED, REQUEST, APPROVED, POLICY_V2, POLICY_V3
from build_storage_bootstrap import POLICY_COMPONENTS

parser = argparse.ArgumentParser()
parser.add_argument('--unified', required=True)
parser.add_argument('--full', required=True)
parser.add_argument('--target', required=True)
parser.add_argument('--migration-policy', choices=(APPROVED, POLICY_V2, POLICY_V3))
parser.add_argument('--legacy-test', action='store_true', help='Explicitly qualify an old-base application test')
args = parser.parse_args()
meta, outer = load(args.unified)
full_meta, full = load(args.full)
check_profile(full_meta, full, legacy_test=args.legacy_test)
assert meta.get('release_profile', '') == full_meta.get('release_profile', '')
verify_metadata(full_meta, full, Path(args.target))
assert meta['storage_guard'] == full_meta['storage_guard']
assert meta['package_type'] == 'ui-unified'
assert meta['version'] == full_meta['version']
assert outer[NESTED + 'application.upk'][0] == Path(args.full).read_bytes()
with tarfile.open(args.unified) as archive:
    plan = archive.extractfile('install.tsv').read().decode().splitlines()
    assert plan == ['file|%04o|%s' % (outer[p][1], p) for p in BRIDGE] + ['tree|0755|usr/local/share/ge_data']
request = outer[REQUEST][0].decode().splitlines()
for i, name in enumerate(('migration', 'application')):
    assert hashlib.sha256(outer[NESTED + name + '.upk'][0]).hexdigest() == request[i]
with tempfile.TemporaryDirectory(prefix='un260-release-check-') as directory:
    nested = Path(directory) / 'migration.upk'
    nested.write_bytes(outer[NESTED + 'migration.upk'][0])
    migration_meta, migration = load(nested)
    assert migration_meta['storage_guard'] == full_meta['storage_guard']
    selected_policy = migration_meta['storage_migration']
    assert selected_policy in (APPROVED, POLICY_V2, POLICY_V3)
    if args.migration_policy is not None:
        assert selected_policy == args.migration_policy
    if selected_policy in (POLICY_V2, POLICY_V3):
        marker = b'UN260_STORAGE_POLICY_V3' if selected_policy == POLICY_V3 else b'UN260_STORAGE_POLICY_V2'
        for name in POLICY_COMPONENTS:
            assert marker in migration[name][0], name
    for name, value in migration.items():
        assert value == full[name]
for name in BRIDGE:
    if name != REQUEST:
        assert outer[name] == full[name]
for name, (data, mode) in full.items():
    if name == 'etc/un260/package-version':
        assert data.decode().strip() == full_meta['version']
        continue  # Generated release metadata, not a runtime target-tree file.
    target = Path(args.target) / name
    assert target.read_bytes() == data, name
    assert target.stat().st_mode & 0o777 == mode, name
print('PASS one outer UPK, legacy tiny plan, nested digests, approved migration, payload bytes/modes match target; generated version matches release')
