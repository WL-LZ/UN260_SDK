#!/usr/bin/env python3
"""Read-only inspection of this board's 38 MiB, 2K/128K NAND dump.

Accepts corrected data-only or per-page data+64-byte-OOB dumps. Never writes,
attaches, creates volumes or authorizes erasure. Preserve the original dump.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

PAGE = 2048
OOB = 64
BLOCK = 131072
SIZE = 38 * 1024 * 1024


def inspect(raw):
    if len(raw) == SIZE:
        data = raw
        form = 'data-only'
    elif len(raw) == SIZE // PAGE * (PAGE + OOB):
        data = b''.join(raw[p:p + PAGE] for p in range(0, len(raw), PAGE + OOB))
        form = 'interleaved-data-plus-oob'
    else:
        raise ValueError('Unexpected size; no blank-space conclusion is permitted: %d' % len(raw))
    report = dict(raw_bytes=len(raw), raw_sha256=hashlib.sha256(raw).hexdigest(), format=form,
                  data_bytes=len(data), physical_blocks=SIZE // BLOCK, erased_blocks=0,
                  valid_ec_headers=0, invalid_ec_blocks=[], content_blocks=[], vid_headers=[])
    blank_block = b'\xff' * BLOCK
    blank_tail = blank_block[64:]
    for index in range(SIZE // BLOCK):
        block = data[index * BLOCK:(index + 1) * BLOCK]
        if block == blank_block:
            report['erased_blocks'] += 1
            continue
        ec = block[:64]
        valid = (ec[:4] == b'UBI#' and ec[4] == 1 and
                 struct.unpack_from('>II', ec, 16) == (PAGE, PAGE * 2) and
                 (zlib.crc32(ec[:60]) ^ 0xffffffff) == struct.unpack_from('>I', ec, 60)[0])
        if valid:
            report['valid_ec_headers'] += 1
        else:
            report['invalid_ec_blocks'].append(index)
        if block[64:] != blank_tail:
            report['content_blocks'].append(index)
        vid = block[PAGE:PAGE + 64]
        if vid[:4] == b'UBI!' and (zlib.crc32(vid[:60]) ^ 0xffffffff) == struct.unpack_from('>I', vid, 60)[0]:
            report['vid_headers'].append(dict(block=index, volume_id=struct.unpack_from('>I', vid, 8)[0],
                                              logical_block=struct.unpack_from('>I', vid, 12)[0]))
    report['headers_only_candidate'] = (not report['invalid_ec_blocks'] and not report['content_blocks'] and
                                         report['erased_blocks'] + report['valid_ec_headers'] == SIZE // BLOCK)
    report['warning'] = 'Dump inspection only. No ECC/OOB authenticity or current-device-state guarantee; no write performed.'
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('backup', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.backup.read_bytes()), indent=2))
