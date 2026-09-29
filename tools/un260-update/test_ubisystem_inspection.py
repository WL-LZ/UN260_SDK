#!/usr/bin/env python3
import struct
import unittest
import zlib
from inspect_ubisystem_backup import inspect, BLOCK, PAGE, OOB, SIZE


def header():
    prefix = b'UBI#\x01\0\0\0' + struct.pack('>QIII', 2, PAGE, PAGE * 2, 0) + b'\0' * 32
    return prefix + struct.pack('>I', zlib.crc32(prefix) ^ 0xffffffff)


class InspectionTests(unittest.TestCase):
    def test_device_example_crc(self):
        self.assertEqual(header()[-4:].hex(), '40932d8c')

    def test_headers_only(self):
        data = (header() + b'\xff' * (BLOCK - 64)) * (SIZE // BLOCK)
        report = inspect(data)
        self.assertTrue(report['headers_only_candidate'])
        self.assertEqual(report['valid_ec_headers'], 304)

    def test_oob_not_mistaken_for_user_content(self):
        data = (header() + b'\xff' * (BLOCK - 64)) * (SIZE // BLOCK)
        raw = b''.join(data[p:p + PAGE] + b'\x00' * OOB for p in range(0, SIZE, PAGE))
        report = inspect(raw)
        self.assertEqual(len(raw), 41091072)
        self.assertTrue(report['headers_only_candidate'])

    def test_any_content_prevents_blank_conclusion(self):
        data = bytearray(b'\xff' * SIZE)
        data[:64] = header(); data[BLOCK * 303 + 123] = 0
        self.assertFalse(inspect(data)['headers_only_candidate'])

    def test_corrupt_ec_prevents_blank_conclusion(self):
        data = bytearray(b'\xff' * SIZE)
        data[:64] = header(); data[12] ^= 1
        self.assertFalse(inspect(data)['headers_only_candidate'])

    def test_truncated_dump_rejected(self):
        with self.assertRaises(ValueError): inspect(b'\xff' * (SIZE - 1))


if __name__ == '__main__':
    unittest.main(verbosity=2)
