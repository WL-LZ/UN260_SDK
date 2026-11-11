"""Small host-only UPK archives for exercising production updater functions."""
import functools
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile

SDK = Path(__file__).resolve().parents[2]
UPDATER_REL = 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
_unpack_dir = tempfile.TemporaryDirectory(prefix='un260-test-unpack-')


@functools.lru_cache()
def native_unpack():
    binary = Path(_unpack_dir.name) / 'un260_unpack'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2',
                    str(SDK / 'source/artinchip/test-lvgl/tools/un260_unpack.c'),
                    '-o', str(binary), '-lz'], check=True)
    return binary


def archive(path, files, corrupt=False, metadata_extra=''):
    checks = ''.join('%s  payload/%s\n' % (hashlib.sha256(d).hexdigest(), p)
                     for p, (d, _) in sorted(files.items())).encode()
    metadata = ('format=UN260_UPGRADE\nschema=1\nproduct=UN260\nversion=test\npackage_id=%s\n%s'
                % (hashlib.sha256(checks).hexdigest(), metadata_extra)).encode()
    entries = {'manifest.ini': (metadata, 0o644), 'checksums.sha256': (checks, 0o644),
               'install.tsv': (''.join('file|%04o|%s\n' % (m, p) for p, (_, m) in files.items()).encode(), 0o644)}
    entries.update({'payload/' + p: (d, m) for p, (d, m) in files.items()})
    if corrupt:
        entries['payload/usr/local/bin/test_lvgl'] = (b'BAD', 0o755)
    with tarfile.open(path, 'w:gz', format=tarfile.USTAR_FORMAT) as out:
        directory = tarfile.TarInfo('payload'); directory.type = tarfile.DIRTYPE; directory.mode = 0o755; out.addfile(directory)
        for name, (data, mode) in entries.items():
            info = tarfile.TarInfo(name); info.mode = mode; info.size = len(data)
            out.addfile(info, io.BytesIO(data))
