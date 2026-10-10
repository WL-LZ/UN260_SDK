#!/usr/bin/env python3
"""Explicit host-only qualification for an application test on old firmware."""
import argparse
from pathlib import Path

PROFILE = 'legacy-test'


def boot_target(name):
    return (name in ('init', 'sbin/init', 'usr/local/bin/early_init',
                     'usr/local/bin/un260_early_init', 'etc/un260/boot-mode') or
            name.startswith(('boot/', 'lib/modules/', 'usr/local/bin/un260_boot_',
                             'usr/local/lib/un260-boot/', 'dev/.un260-boot-')) or
            name == 'usr/local/lib/un260-boot' or
            name == 'etc/un260/boot-active' or name.startswith('etc/un260/boot-active.'))


def check_payload(files):
    for name in files:
        if boot_target(name):
            raise ValueError('Legacy test cannot install native boot files: ' + name)


def check_profile(metadata, files, legacy_test=False):
    profile = metadata.get('release_profile', '')
    if not legacy_test:
        if profile:
            raise ValueError('Explicit --legacy-test qualification required for release profile: ' + profile)
        return
    if profile != PROFILE or metadata.get('storage_guard') != 'syncfs-v1':
        raise ValueError('Legacy test profile requires the ordinary syncfs-v1 guard')
    if metadata.get('schema') != '1' or metadata.get('package_type') != 'ui':
        raise ValueError('Legacy test must be a full ordinary UI package')
    if metadata.get('storage_layout') != 'app-volume-v1' or metadata.get('storage_migration'):
        raise ValueError('Legacy test application cannot change storage enrollment')
    if 'boot_bundle_id' in metadata or 'boot_manifest_sha256' in metadata:
        raise ValueError('Legacy test cannot publish a boot bundle')
    check_payload(files)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    args = parser.parse_args()
    paths = []
    for path in args.payload.rglob('*'):
        if path.is_symlink():
            raise ValueError('Symlink in legacy test payload: ' + str(path))
        paths.append(path.relative_to(args.payload).as_posix())
    check_payload(paths)
