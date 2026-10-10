#!/usr/bin/env python3
"""Canonical full-package content identity and factory updater contract."""
import argparse
import hashlib
from pathlib import Path
import re
import stat
from factory_layout import APP, validate_app_size

ENVELOPE = 'UN260_UPDATE_ENVELOPE_V1'
PLATFORM = 'UN260_UPDATER_PLATFORM_V1'
ENGINE_ABI = '1'
ENGINE_MARKER = b'UN260_PACKAGE_ENGINE_V1'
CORE = ('usr/bin/ui_update.sh', 'usr/local/bin/un260_storage_sync',
        'usr/local/bin/un260_unpack', 'usr/local/bin/un260_app_storage',
        'usr/local/bin/un260_resource_cleanup')
RUNTIME = 'usr/local/lib/un260-startup/runtime.sh'
RESCUE = 'usr/local/lib/un260-updater/rescue'
RESCUE_NAMES = ('engine.sh', 'sync', 'unpack', 'app_storage', 'resource_cleanup')
CORE_KEYS = ('engine_sha256=', 'sync_sha256=', 'unpack_sha256=', 'app_storage_sha256=', 'resource_cleanup_sha256=')
CORE_DESCRIPTOR = 'etc/un260/update-core.manifest'
RUNTIME_DESCRIPTOR = 'etc/un260/startup-runtime.sha256'
RESERVED = ('etc/un260/updater-platform', 'usr/local/bin/un260_update_dispatch',
            'usr/local/lib/un260-updater/rescue')
HEX = re.compile(r'[0-9a-f]{64}')
MAX_TABLE_ROWS = 8192
MAX_TABLE_BYTES = 3 * 1024 * 1024
MAX_PATH_BYTES = 255
MAX_MEMBER_BYTES = 24 * 1024 * 1024
MAX_ARCHIVE_BYTES = 48 * 1024 * 1024
LEGACY_PAYLOAD_BYTES = 24 * 1024 * 1024
PLATFORM_PAYLOAD_BYTES = 32 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def reserved(path):
    return any(path == name or path.startswith(name + '/') for name in RESERVED)


def payload_budget(metadata):
    if (metadata.get('updater_envelope') == ENVELOPE and
            metadata.get('updater_platform') == PLATFORM and
            metadata.get('updater_engine_abi') == ENGINE_ABI and
            metadata.get('schema') == '1' and metadata.get('package_type') == 'ui' and
            metadata.get('storage_layout') == 'app-volume-v1' and
            metadata.get('release_profile') != 'legacy-test'):
        return PLATFORM_PAYLOAD_BYTES
    return LEGACY_PAYLOAD_BYTES


def validate_payload_budget(metadata, files):
    if any(len(data) > MAX_MEMBER_BYTES for data, _ in files.values()):
        raise ValueError('Oversized payload member')
    if sum(len(data) for data, _ in files.values()) > payload_budget(metadata):
        raise ValueError('Payload budget exceeded')
    if APP in files:
        validate_app_size(len(files[APP][0]), legacy_test=payload_budget(metadata) != PLATFORM_PAYLOAD_BYTES)


def validate_table_limits(data):
    if not 0 < data.count(b'\n') <= MAX_TABLE_ROWS:
        raise ValueError('Release target table exceeds row limit')
    if len(data) > MAX_TABLE_BYTES:
        raise ValueError('Release target table exceeds byte limit')


def table(files):
    if len(files) > MAX_TABLE_ROWS:
        raise ValueError('Release target table exceeds row limit')
    rows = []
    for path, (data, mode) in sorted(files.items()):
        if (not re.fullmatch(r'[A-Za-z0-9_./-]+', path) or path.startswith('/') or
                '..' in path or '//' in path or len(path) > MAX_PATH_BYTES or reserved(path)):
            raise ValueError('Unsafe or factory-owned release path: ' + path)
        if mode not in (0o644, 0o755):
            raise ValueError('Unsupported release mode: ' + path)
        rows.append('%s|%04o|%d|%s\n' % (sha(data), mode, len(data), path))
    if not rows:
        raise ValueError('Empty release inventory')
    result = ''.join(rows).encode('ascii')
    validate_table_limits(result)
    return result


def payload_files(root):
    root = Path(root)
    files = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('Symlink in release payload: ' + str(path))
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError('Unsupported release payload member: ' + str(path))
        files[path.relative_to(root).as_posix()] = (path.read_bytes(), stat.S_IMODE(path.stat().st_mode))
    return files


def verify(metadata, files, target_table, require_envelope=True):
    validate_payload_budget(metadata, files)
    expected = table(files)
    if target_table != expected or metadata.get('target_id') != sha(expected):
        raise ValueError('Full target table or content identity differs')
    if not require_envelope:
        return
    if (metadata.get('updater_envelope') != ENVELOPE or
            metadata.get('updater_platform') != PLATFORM or
            metadata.get('updater_engine_abi') != ENGINE_ABI):
        raise ValueError('Unsupported factory updater envelope')
    for name in CORE + (RUNTIME,):
        if name not in files or files[name][1] != 0o755:
            raise ValueError('Missing executable updater core: ' + name)
        if name in CORE and len(files[name][0]) > 262144:
            raise ValueError('Updater core exceeds snapshot limit: ' + name)
    if ENGINE_MARKER not in files[CORE[0]][0]:
        raise ValueError('Package lacks the candidate engine entry')
    verify_bindings(files)


def core_digests(data):
    lines = data.splitlines()
    if (len(data) > 1024 or not data.endswith(b'\n') or b'\r' in data or
            len(lines) != 6 or lines[0] != b'UN260_UPDATE_CORE_V1'):
        raise ValueError('Invalid updater core manifest')
    result = []
    for key, line in zip(CORE_KEYS, lines[1:]):
        text = line.decode('ascii')
        digest = text[len(key):] if text.startswith(key) else ''
        if not HEX.fullmatch(digest):
            raise ValueError('Invalid updater core manifest digest')
        result.append(digest)
    return result


def verify_bindings(files):
    for name in (CORE_DESCRIPTOR, RUNTIME_DESCRIPTOR):
        if name not in files or files[name][1] != 0o644:
            raise ValueError('Missing updater/runtime descriptor: ' + name)
    for name, digest in zip(CORE, core_digests(files[CORE_DESCRIPTOR][0])):
        if name not in files or sha(files[name][0]) != digest:
            raise ValueError('Updater core descriptor differs: ' + name)
    runtime = files[RUNTIME_DESCRIPTOR][0]
    if (len(runtime) != 65 or not runtime.endswith(b'\n') or
            not HEX.fullmatch(runtime[:-1].decode('ascii')) or
            RUNTIME not in files or files[RUNTIME][1] != 0o755 or
            sha(files[RUNTIME][0]) != runtime[:-1].decode('ascii')):
        raise ValueError('Startup runtime descriptor differs')


def factory_contract(root):
    """Read the immutable IMG contract; these files are never UPK payload."""
    root = Path(root)
    expected = {'usr/local/bin/un260_update_dispatch': 0o755,
                'etc/un260/updater-platform': 0o644,
                RESCUE + '/manifest': 0o644,
                RESCUE + '/startup-entry.sha256': 0o644,
                RESCUE + '/startup.sh': 0o755, RESCUE + '/startup.sha256': 0o644,
                RESCUE + '/watch.sh': 0o755, RESCUE + '/watch.sha256': 0o644,
                RESCUE + '/display': 0o755, RESCUE + '/display.sha256': 0o644,
                CORE_DESCRIPTOR: 0o644, RUNTIME_DESCRIPTOR: 0o644, RUNTIME: 0o755}
    expected.update({RESCUE + '/' + name: (0o600 if name == 'engine.sh' else 0o755) for name in RESCUE_NAMES})
    result = {}
    for name, mode in expected.items():
        path = root / name
        for ancestor in path.parents:
            if ancestor == root: break
            if ancestor.is_symlink():
                raise ValueError('Symlink in factory contract path: ' + name)
        if path.is_symlink() or not path.is_file() or stat.S_IMODE(path.stat().st_mode) != mode:
            raise ValueError('Missing or unsafe factory updater contract: ' + name)
        result[name] = (path.read_bytes(), mode)
    for name, limit in (('startup.sh', 262144), ('watch.sh', 262144), ('display', 1048576)):
        descriptor = name.split('.')[0] + '.sha256'
        expected_digest = result[RESCUE + '/' + descriptor][0]
        data = result[RESCUE + '/' + name][0]
        if (not 0 < len(data) <= limit or len(expected_digest) != 65 or
                expected_digest[-1:] != b'\n' or not HEX.fullmatch(expected_digest[:-1].decode('ascii')) or
                sha(data) != expected_digest[:-1].decode('ascii')):
            raise ValueError('Frozen recovery component differs: ' + name)
        if name == 'display':
            from prepare_update_platform import validate_recovery_display
            try:
                validate_recovery_display(data)
            except (AssertionError, ValueError) as error:
                raise ValueError('Invalid frozen recovery display: ' + str(error)) from error
    if result['etc/un260/updater-platform'][0] != (PLATFORM + '\n').encode():
        raise ValueError('Wrong factory updater platform')
    for name, digest in zip(RESCUE_NAMES, core_digests(result[RESCUE + '/manifest'][0])):
        if len(result[RESCUE + '/' + name][0]) > 262144 or sha(result[RESCUE + '/' + name][0]) != digest:
            raise ValueError('Frozen updater rescue hash differs: ' + name)
    installed = dict(result)
    for name in CORE:
        path = root / name
        if path.is_symlink() or not path.is_file() or stat.S_IMODE(path.stat().st_mode) != 0o755:
            raise ValueError('Missing installed updater core: ' + name)
        installed[name] = (path.read_bytes(), 0o755)
    verify_bindings(installed)
    startup = result[RESCUE + '/startup-entry.sha256'][0]
    if (len(startup) != 65 or not startup.endswith(b'\n') or
            not HEX.fullmatch(startup[:-1].decode('ascii'))):
        raise ValueError('Invalid frozen startup entry digest')
    entry = root / 'etc/init.d/S00lvgl'
    if entry.is_symlink() or not entry.is_file() or stat.S_IMODE(entry.stat().st_mode) != 0o755 or sha(entry.read_bytes()) != startup[:-1].decode('ascii'):
        raise ValueError('Fixed startup entry differs from factory contract')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--table', type=Path, required=True)
    parser.add_argument('--require-updater-contract', action='store_true')
    args = parser.parse_args()
    files = payload_files(args.payload)
    data = table(files)
    if args.require_updater_contract:
        verify(dict(target_id=sha(data), schema='1', package_type='ui', storage_layout='app-volume-v1', updater_envelope=ENVELOPE,
                    updater_platform=PLATFORM, updater_engine_abi=ENGINE_ABI), files, data)
    args.table.write_bytes(data)
    print(sha(data))
