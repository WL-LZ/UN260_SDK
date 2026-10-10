#!/usr/bin/env python3
"""Assemble the update engine and the fixed factory recovery boundary."""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path
import subprocess

ROLES = {
    'engine': 'usr/bin/ui_update.sh',
    'sync': 'usr/local/bin/un260_storage_sync',
    'unpack': 'usr/local/bin/un260_unpack',
    'app_storage': 'usr/local/bin/un260_app_storage',
    'resource_cleanup': 'usr/local/bin/un260_resource_cleanup',
}

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def write_owned(target: Path, relative: str, data: bytes, mode: int) -> Path:
    path = target / relative
    assert target.resolve() in path.resolve().parents, f'Escaping target: {relative}'
    parent = path.parent
    while parent != target:
        assert not parent.is_symlink(), f'Symlinked output parent: {parent}'
        parent = parent.parent
    assert not path.is_symlink(), f'Symlinked output: {path}'
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.un260-platform-tmp')
    assert not temporary.exists() and not temporary.is_symlink(), f'Unexpected temporary file: {temporary}'
    temporary.write_bytes(data)
    temporary.chmod(mode)
    temporary.replace(path)
    return path

def assemble_engine(repo: Path) -> bytes:
    text = (repo / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh').read_text()
    for marker, name in [('CONTEXT', 'engine_context.sh'), ('TRIAL', 'engine_trial.sh')]:
        token = '# UN260_ENGINE_' + marker + '_MODULE'
        assert text.count(token) == 1, f'Missing or repeated engine marker: {marker}'
        module = (repo / 'tools/un260-update' / name).read_text()
        text = text.replace(token, module.rstrip())
    assert '# UN260_PACKAGE_ENGINE_V1:' in text
    return text.encode()

def validate_recovery_display(data: bytes) -> None:
    """The factory fallback must be a complete static RISC-V ELF64 program."""
    assert 64 <= len(data) <= 1048576, 'Invalid recovery display size'
    assert data[:7] == b'\x7fELF\x02\x01\x01', 'Invalid recovery display ELF encoding'
    header = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = header
    assert kind == 2 and machine == 243 and version == 1 and ehsize == 64, 'Invalid recovery display architecture'
    assert phsize == 56 and 1 <= phnum <= 64 and phoff >= 64 and phoff + phsize * phnum <= len(data), 'Invalid recovery display program table'
    if shnum:
        assert shsize == 64 and shoff >= 64 and shoff + shsize * shnum <= len(data) and shstr < shnum, 'Invalid recovery display section table'
    else:
        assert shoff == 0 and shstr == 0, 'Invalid recovery display section table'
    loads = 0
    executable_entry = False
    for i in range(phnum):
        ptype, permissions, offset, address, physical, files, memory, alignment = struct.unpack_from('<IIQQQQQQ', data, phoff + i * phsize)
        assert ptype not in (2, 3), 'Recovery display must be static'
        assert offset <= len(data) and files <= len(data) - offset, 'Truncated recovery display segment'
        if ptype == 1:
            assert files <= memory and memory > 0, 'Invalid recovery display load segment'
            assert alignment in (0, 1) or (alignment & (alignment - 1) == 0 and offset % alignment == address % alignment), 'Invalid recovery display load alignment'
            loads += 1
            executable_entry |= bool(permissions & 1) and address <= entry < address + files
    assert loads and executable_entry, 'Invalid recovery display entry point'

def prepare(repo: Path, target: Path) -> None:
    assert target.is_dir() and not target.is_symlink(), 'Invalid target root'
    tools = repo / 'tools/un260-update'
    engine = assemble_engine(repo)
    engine_path = write_owned(target, ROLES['engine'], engine, 0o755)
    subprocess.run(['/bin/sh', '-n', str(engine_path)], check=True)
    runtime = (repo / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes()
    runtime_path = write_owned(target, 'usr/local/lib/un260-startup/runtime.sh', runtime, 0o755)
    subprocess.run(['/bin/sh', '-n', str(runtime_path)], check=True)
    entry = (tools / 'startup_entry.sh').read_bytes()
    entry_path = write_owned(target, 'etc/init.d/S00lvgl', entry, 0o755)
    subprocess.run(['/bin/sh', '-n', str(entry_path)], check=True)
    dispatcher = (tools / 'un260_update_dispatch.sh').read_bytes()
    dispatcher_path = write_owned(target, 'usr/local/bin/un260_update_dispatch', dispatcher, 0o755)
    subprocess.run(['/bin/sh', '-n', str(dispatcher_path)], check=True)
    rescue = 'usr/local/lib/un260-updater/rescue/'
    manifest = ['UN260_UPDATE_CORE_V1']
    for role, relative in ROLES.items():
        source = target / relative
        assert source.is_file() and not source.is_symlink(), f'Missing update core: {role}'
        data = source.read_bytes()
        assert 0 < len(data) <= 262144, f'Update core outside fixed envelope limit: {role}'
        output_name = 'engine.sh' if role == 'engine' else role
        write_owned(target, rescue + output_name, data, 0o600 if role == 'engine' else 0o755)
        manifest.append(role + '_sha256=' + digest(data))
    core = ('\n'.join(manifest) + '\n').encode()
    write_owned(target, rescue + 'manifest', core, 0o644)
    write_owned(target, 'etc/un260/update-core.manifest', core, 0o644)
    write_owned(target, 'etc/un260/updater-platform', b'UN260_UPDATER_PLATFORM_V1\n', 0o644)
    write_owned(target, 'etc/un260/startup-runtime.sha256', (digest(runtime) + '\n').encode(), 0o644)
    write_owned(target, rescue + 'startup.sh', runtime, 0o755)
    write_owned(target, rescue + 'startup.sha256', (digest(runtime) + '\n').encode(), 0o644)
    write_owned(target, rescue + 'startup-entry.sha256', (digest(entry) + '\n').encode(), 0o644)
    watch = (tools / 'startup_watch.sh').read_bytes()
    watch_path = write_owned(target, rescue + 'watch.sh', watch, 0o755)
    subprocess.run(['/bin/sh', '-n', str(watch_path)], check=True)
    write_owned(target, rescue + 'watch.sha256', (digest(watch) + '\n').encode(), 0o644)
    display_path = target / 'usr/local/bin/un260_upgrade_display'
    assert display_path.is_file() and not display_path.is_symlink(), 'Missing independent recovery display'
    display = display_path.read_bytes()
    validate_recovery_display(display)
    write_owned(target, rescue + 'display', display, 0o755)
    write_owned(target, rescue + 'display.sha256', (digest(display) + '\n').encode(), 0o644)
    print('UPDATE_PLATFORM_READY engine=' + digest(engine) + ' runtime=' + digest(runtime))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', required=True, type=Path)
    args = parser.parse_args()
    prepare(Path(__file__).resolve().parents[2], args.target.absolute())
