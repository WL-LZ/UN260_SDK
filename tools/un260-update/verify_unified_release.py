#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path
import tarfile
import tempfile
from build_delta import load
from build_unified import BRIDGE, NESTED, REQUEST, APPROVED

parser = argparse.ArgumentParser()
parser.add_argument('--unified', required=True)
parser.add_argument('--full', required=True)
parser.add_argument('--target', required=True)
args = parser.parse_args()
meta, outer = load(args.unified)
full_meta, full = load(args.full)
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
    assert migration_meta['storage_migration'] == APPROVED
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
