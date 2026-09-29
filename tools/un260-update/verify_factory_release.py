#!/usr/bin/env python3
"""Read-only IMG/UPK consistency gate; --deep additionally uses ubi_reader.

No flashing, mounting, MTD access, or customer data. Deep mode decodes the
actual UBIFS inodes from the IMG, without extracting their paths to the host.
"""
import argparse
import hashlib
import json
from pathlib import Path
import stat
import struct
import tempfile
import zlib
from build_delta import load
from factory_layout import APP, APP_IMAGE, LAUNCHER, OWNER, STATE

def text_field(data): return data.split(b'\0', 1)[0].decode('ascii')

def read_image(path):
    blob = Path(path).read_bytes()
    assert blob[:8] == b'AIC.FW\0\0', 'Not an AIC full firmware image'
    assert text_field(blob[8:72]) == 'd211'
    assert text_field(blob[72:136]) == 'd213_devkitf_page_2k_block_128k'
    assert text_field(blob[200:264]) == 'spi-nand'
    assert text_field(blob[268:332]) == 'P=2K,B=128K', 'Unsupported factory NAND geometry'
    offset, size = struct.unpack_from('<II', blob, 332)
    assert size % 512 == 0 and offset + size <= len(blob)
    result = {}
    for address in range(offset, offset + size, 512):
        record = blob[address:address + 512]
        if record[:8] == bytes(8): continue
        assert text_field(record[:8]) == 'META'
        name, part = text_field(record[8:72]), text_field(record[72:136])
        start, length, crc = struct.unpack_from('<III', record, 136)
        assert name not in result and start + length <= len(blob), name
        data = blob[start:start + length]
        assert zlib.crc32(data) & 0xffffffff == crc, 'IMG component CRC: ' + name
        result[name] = dict(part=part, attrs=text_field(record[152:216]).split(';'),
                            filename=text_field(record[216:280]), data=data)
    return result

def read_ubifs(path):
    # Optional release QA dependency, isolated from both firmware and builds.
    from ubireader.ubi_io import ubi_file
    from ubireader.ubifs import ubifs, walk
    from ubireader.ubifs.output import _process_reg_file
    source = ubi_file(str(path), 126976)
    try:
        fs = ubifs(source)
        assert fs.superblock_node.min_io_size == 2048
        assert fs.superblock_node.leb_size == 126976
        inodes, bad = {}, []
        walk.index(fs, fs.master_node.root_lnum, fs.master_node.root_offs, inodes, bad)
        assert not bad, 'UBIFS has unreadable index blocks'
        paths = {}
        def visit(number, parent, ancestors):
            assert number not in ancestors, 'Cyclic UBIFS directory'
            for dent in inodes[number].get('dent', []):
                name = dent.name
                assert name not in ('.', '..') and '/' not in name
                path = parent + name
                assert path not in paths, 'Duplicate UBIFS path'
                node = inodes[dent.inum]; info = node['ino']
                data = _process_reg_file(fs, node, path) if stat.S_ISREG(info.mode) else None
                if data is not None: assert len(data) == info.size, path
                paths[path] = (data, info.mode & 0o7777, info.uid, info.gid)
                if stat.S_ISDIR(info.mode): visit(dent.inum, path + '/', ancestors + [number])
        visit(1, '', [])
        return paths
    finally:
        source._fhandle.close()

def verify(image, application, target, deep=False):
    image, application, target = Path(image), Path(application), Path(target)
    report = json.loads((image.parent / 'un260-factory-layout.json').read_text())
    metadata, files = load(application)
    assert report['release'] == metadata['version'], 'Factory and UPK versions differ'
    assert report['requires_usb'] is False and report['first_boot_formats'] is False
    assert hashlib.sha256(files[APP][0]).hexdigest() == report['app_sha256']
    assert (target / APP).read_bytes() == files[APP][0], 'UPK was built from launcher instead of ELF'
    records = read_image(image)
    for name, part, filename in [('rootfs', 'ubiroot:rootfs', 'rootfs_page_2k_block_128k.ubifs'),
                                  ('app', 'ubisystem:un260_app', APP_IMAGE)]:
        component = records['image.target.' + name]
        assert component['part'] == part and set(component['attrs']) == {'ubi', 'required'}
        assert component['filename'] == filename
        assert component['data'] == (image.parent / filename).read_bytes(), name
    assert hashlib.sha256(records['image.target.app']['data']).hexdigest() == report['app_ubifs_sha256']
    env = records['image.target.env']['data']
    assert b'UBI=ubiroot:-(rootfs);ubisystem:-(un260_app)\0' in env
    assert b'64m(ubiroot),-(ubisystem)\0' in env
    if deep:
        with tempfile.TemporaryDirectory(prefix='un260-img-readback-') as directory:
            for name in ['rootfs', 'app']:
                (Path(directory) / name).write_bytes(records['image.target.' + name]['data'])
            root = read_ubifs(Path(directory) / 'rootfs')
            app = read_ubifs(Path(directory) / 'app')
        assert root[APP] == (LAUNCHER, 0o755, 0, 0), 'Root launcher or ownership differs'
        assert root[STATE + '/active'] == (OWNER, 0o644, 0, 0)
        assert app['.un260-owner'] == (OWNER, 0o644, 0, 0)
        for forbidden in ['etc/un260/unified-request', 'etc/ui_state', 'var/lib/un260-updater', STATE + '/pending']:
            assert not any(path == forbidden or path.startswith(forbidden + '/') for path in root), forbidden
        for name, (data, mode) in files.items():
            actual = app['test_lvgl'] if name == APP else root[name]
            assert actual == (data, mode, 0, 0), 'Factory/UPK bytes, mode, or ownership differ: ' + name
        assert set(app) == {'.un260-owner', 'test_lvgl'}, 'Unexpected application-volume contents'
        print('PASS actual IMG UBIFS readback: root launcher/active owner; app ELF; all %d UPK files match; no customer/update state' % len(files))
    print('PASS factory IMG: required root+app, component CRCs, geometry, environment, ELF and release identity match UPK')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', required=True)
    parser.add_argument('--application', required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--deep', action='store_true')
    args = parser.parse_args()
    verify(args.image, args.application, args.target, args.deep)
