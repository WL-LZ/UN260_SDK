#!/usr/bin/env python3
"""One legacy-compatible USB bundle. Only tiny bridge files go to rootfs.

The nested migration is armed for ONE explicitly reviewed NAND fingerprint.
No automatic widening to other layouts/content is allowed.
"""
import argparse
import hashlib
import io
from pathlib import Path
import tarfile
import tempfile
from build_delta import load
from build_storage_bootstrap import build as build_migration, PATHS, APPROVED

REQUEST = 'etc/un260/unified-request'
DISPLAY = 'usr/local/bin/un260_upgrade_display'
NESTED = 'usr/local/share/un260-unified/'
BRIDGE = PATHS + (DISPLAY, REQUEST)


def build(full, output, approved):
    if approved != APPROVED:
        raise ValueError('An exact reviewed fingerprint is required')
    metadata, files = load(full)
    for name in PATHS + (DISPLAY,):
        if name not in files or files[name][1] != 0o755:
            raise ValueError('Missing executable bridge component: ' + name)
    if metadata.get('storage_guard') != 'syncfs-v1' or metadata.get('storage_layout') != 'app-volume-v1':
        raise ValueError('Full application lacks storage capabilities')
    for name in ('usr/bin/ui_update.sh', 'etc/init.d/S00lvgl'):
        if b'UN260_UNIFIED_1' not in files[name][0]:
            raise ValueError('Missing boot continuation: ' + name)
    with tempfile.TemporaryDirectory(prefix='un260-unified-build-') as temp:
        migration = Path(temp) / 'migration.upk'
        build_migration(full, migration, True)
        nested = {'migration': migration.read_bytes(), 'application': Path(full).read_bytes()}
    payload = {name: files[name] for name in PATHS + (DISPLAY,)}
    payload[REQUEST] = (''.join(hashlib.sha256(nested[n]).hexdigest() + '\n'
                              for n in ('migration', 'application')).encode(), 0o644)
    root_kb = 2048 + sum((len(d) + 1023) // 1024 + 4 for d, _ in payload.values())
    if root_kb > 2560:
        raise ValueError('Bridge exceeds 2.5 MiB root preflight budget')
    payload.update({NESTED + n + '.upk': (data, 0o644) for n, data in nested.items()})
    if sum(len(d) for d, _ in payload.values()) > 24 * 1024 * 1024:
        raise ValueError('Outer payload exceeds legacy unpacker budget')
    checks = ''.join('%s  payload/%s\n' % (hashlib.sha256(d).hexdigest(), n)
                     for n, (d, _) in sorted(payload.items())).encode()
    manifest = ('format=UN260_UPGRADE\nschema=1\nproduct=UN260\npackage_type=ui-unified\n'
                'version=%s\npackage_id=%s\nrequires_reboot=1\n'
                'storage_layout=app-volume-v1\nstorage_guard=syncfs-v1\n' %
                (metadata['version'], hashlib.sha256(checks).hexdigest())).encode()
    # The old upgrader understands this subset manifest without new opcodes.
    plan = ''.join('file|%04o|%s\n' % (payload[n][1], n) for n in BRIDGE)
    plan += 'tree|0755|usr/local/share/ge_data\n'
    entries = {'manifest.ini': (manifest, 0o644), 'checksums.sha256': (checks, 0o644),
               'install.tsv': (plan.encode(), 0o644)}
    entries.update({'payload/' + n: v for n, v in payload.items()})
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + '.tmp')
    with tarfile.open(temporary, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
        for name in ('payload', 'payload/usr/local/share/ge_data'):
            item = tarfile.TarInfo(name); item.type = tarfile.DIRTYPE; item.mode = 0o755
            archive.addfile(item)
        for name, (data, mode) in entries.items():
            item = tarfile.TarInfo(name); item.mode = mode; item.size = len(data)
            archive.addfile(item, io.BytesIO(data))
    _, readback = load(temporary)
    if readback != payload:
        raise ValueError('Unified readback mismatch')
    temporary.replace(output)
    output.with_suffix('.upk.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
    print('UNIFIED bridge_required_kb=%d nested_bytes=%d output=%s' %
          (root_kb, sum(map(len, nested.values())), output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--full', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--approved-fingerprint', required=True)
    args = parser.parse_args()
    build(args.full, args.output, args.approved_fingerprint)
