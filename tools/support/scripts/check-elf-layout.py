#!/usr/bin/env python3
"""Read-only ELF layout gate for binaries processed by strip / patchelf."""
import argparse
import json
from pathlib import Path
import struct


class LayoutError(ValueError):
    pass


def check_elf(path):
    data = Path(path).read_bytes()
    if len(data) < 16 or data[:4] != b'\x7fELF':
        raise LayoutError('not an ELF file')
    elf_class, byte_order = data[4:6]
    if elf_class not in (1, 2) or byte_order not in (1, 2):
        raise LayoutError('unsupported ELF class or byte order')
    endian = '<' if byte_order == 1 else '>'

    def unpack(fmt, offset):
        size = struct.calcsize(endian + fmt)
        if offset < 0 or offset + size > len(data):
            raise LayoutError('truncated ELF structure')
        return struct.unpack_from(endian + fmt, data, offset)

    def span(offset, size):
        if offset < 0 or size < 0 or offset + size > len(data):
            raise LayoutError('section or segment extends beyond file')
        return data[offset:offset + size]

    header_fmt = '16sHHIQQQIHHHHHH' if elf_class == 2 else '16sHHIIIIIHHHHHH'
    ph_fmt = 'IIQQQQQQ' if elf_class == 2 else 'IIIIIIII'
    sh_fmt = 'IIQQQQIIQQ' if elf_class == 2 else 'IIIIIIIIII'
    dyn_fmt = 'qQ' if elf_class == 2 else 'iI'
    header = unpack(header_fmt, 0)
    phoff, shoff, phsize, phnum, shsize, shnum, names_index = (
        header[5], header[6], header[9], header[10],
        header[11], header[12], header[13])
    if header[3] != 1:
        raise LayoutError('invalid ELF version')
    if phnum == 0xffff or (shoff and shnum == 0) or names_index == 0xffff:
        raise LayoutError('extended header numbering is not supported by this gate')
    if phnum and phsize != struct.calcsize(endian + ph_fmt):
        raise LayoutError('invalid program header entry size')
    if shnum and shsize != struct.calcsize(endian + sh_fmt):
        raise LayoutError('invalid section header entry size')
    if not phnum:
        raise LayoutError('expected loadable executable or shared library')
    programs = []
    for i in range(phnum):
        p = unpack(ph_fmt, phoff + i * phsize)
        if elf_class == 1:
            p = (p[0], p[6], p[1], p[2], p[3], p[4], p[5], p[7])
        if p[0] == 1:
            span(p[2], p[5])
            if p[6] < p[5]:
                raise LayoutError('PT_LOAD memory size is smaller than file size')
        programs.append(p)
    loads = [p for p in programs if p[0] == 1]
    if not loads:
        raise LayoutError('no PT_LOAD segments')

    def load_offset(address, size):
        for p in loads:
            if p[3] <= address and address + size <= p[3] + p[5]:
                return p[2] + address - p[3]
        return None

    errors = []
    sections = [unpack(sh_fmt, shoff + i * shsize) for i in range(shnum)]
    names = b''
    if names_index:
        if names_index >= len(sections):
            raise LayoutError('invalid section-name table index')
        names_section = sections[names_index]
        names = span(names_section[4], names_section[5])
    for i, section in enumerate(sections):
        name = f'section[{i}]'
        if names:
            if section[0] >= len(names) or b'\0' not in names[section[0]:]:
                raise LayoutError('invalid section name')
            name = names[section[0]:].split(b'\0', 1)[0].decode('ascii', 'replace')
        if section[1] != 8:
            span(section[4], section[5])
        if section[2] & 2 and section[5] and section[1] != 8:
            offset = load_offset(section[3], section[5])
            if offset != section[4]:
                errors.append(f'{name}: not wholly file-backed by a matching PT_LOAD')

    strings = []
    for p in programs:
        if p[0] != 2:
            continue
        dynamic = span(p[2], p[5])
        dynsize = struct.calcsize(endian + dyn_fmt)
        if load_offset(p[3], p[5]) != p[2]:
            errors.append('PT_DYNAMIC: not wholly mapped by PT_LOAD')
        entries = []
        for off in range(0, len(dynamic) - dynsize + 1, dynsize):
            tag, value = unpack(dyn_fmt, p[2] + off)
            if tag == 0:
                break
            entries.append((tag, value))
        else:
            raise LayoutError('PT_DYNAMIC has no bounded DT_NULL terminator')
        tags = dict(entries)
        if 5 not in tags or 10 not in tags:
            raise LayoutError('dynamic string table address or size is missing')
        offset = load_offset(tags[5], tags[10])
        if offset is None:
            errors.append('DT_STRTAB: string table is outside a complete PT_LOAD')
            continue
        table = span(offset, tags[10])
        for tag, value in entries:
            if tag not in (1, 14, 15, 29):
                continue
            if value >= len(table) or b'\0' not in table[value:]:
                errors.append(f'dynamic tag {tag}: invalid string offset {value}')
            else:
                strings.append({'tag': tag, 'value': table[value:].split(b'\0', 1)[0]
                                .decode('utf-8', 'replace')})
    return {'path': str(path), 'ok': not errors, 'errors': errors,
            'elf_class': elf_class * 32, 'dynamic_strings': strings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true', help='emit structured results')
    parser.add_argument('files', nargs='+', type=Path)
    args = parser.parse_args()
    results = []
    for path in args.files:
        try:
            result = check_elf(path)
        except (OSError, LayoutError) as error:
            result = {'path': str(path), 'ok': False, 'errors': [str(error)]}
        results.append(result)
    if args.json:
        print(json.dumps(results, indent=2))
    else:
        for result in results:
            print(('PASS ' if result['ok'] else 'FAIL ') + result['path'])
            for error in result['errors']:
                print('  ' + error)
    return 0 if all(result['ok'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
