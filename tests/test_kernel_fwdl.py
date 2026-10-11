# SPDX-License-Identifier: MIT
"""Firmware download: wire format, container parsing and the command executor."""
import ctypes as C
import errno
import json
import struct
import unittest
import zlib

from kernel_build import ROOT, CProgram, have_compiler

FIRMWARE = ROOT / 'firmware' / 'build'


class Region(C.Structure):
    _fields_ = [('data', C.c_void_p), ('address', C.c_uint32),
                ('length', C.c_uint32), ('feature_set', C.c_uint32)]


class Image(C.Structure):
    _fields_ = [('regions', Region * 14), ('count', C.c_uint32),
                ('chip_id', C.c_uint8), ('eco_code', C.c_uint8),
                ('version', C.c_uint8 * 10), ('build_date', C.c_uint8 * 15)]


def with_crc(data):
    struct.pack_into('<I', data, len(data) - 4, zlib.crc32(data[:-4]))
    return data


def ram_container(lengths=(17, 4097), features=(0x21, 0x37), envelope=b''):
    """A RAM container: regions, release envelope, descriptor table, trailer."""
    payload, table = bytearray(), bytearray()
    for i, length in enumerate(lengths):
        payload.extend((j * 37 + i * 11) & 255 for j in range(length))
        desc = bytearray(40)
        struct.pack_into('<III4xIIB', desc, 0, 0x12345678, 9, 7,
                         0x916400 + i * 0x10000, length, features[i % len(features)])
        table.extend(desc)
    trailer = bytearray(36)
    trailer[:5] = bytes((0x16, 0, len(lengths), 2, 1))
    trailer[7:17] = b'0123456789'
    trailer[17:32] = b'20260101000000\0'
    return with_crc(payload + b'#' * 16 + struct.pack('<I', len(envelope)) + envelope +
                    b'#' * 16 + table + trailer)


def patch_container(sections):
    """A ROM patch: 96-byte big-endian header, 64-byte sections, then data.
    sections: (address, data, key_info)."""
    header = bytearray(96)
    header[0:14] = b'20260101000000'
    header[16:20] = b'ALPS'
    struct.pack_into('>I', header, 44, len(sections))
    body, records = bytearray(), bytearray()
    offset = 96 + 64 * len(sections)
    for address, data, key_info in sections:
        record = bytearray(64)
        struct.pack_into('>IIIIII', record, 0, 0x00030002, offset + len(body),
                         len(data), address, len(data), key_info)
        records.extend(record)
        body.extend(data)
    return header + records + body


@unittest.skipUnless(have_compiler(), 'needs a C compiler')
class KernelFirmwareWireTests(unittest.TestCase):
    def test_packets_bounds_and_replies(self):
        program = CProgram('wire-test.c')
        self.addCleanup(program.close)
        result = program.run()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)


@unittest.skipUnless(have_compiler(), 'needs a C compiler')
class KernelExecutorTests(unittest.TestCase):
    def test_download_schedules_and_stream_handover(self):
        program = CProgram('fwdl-test.c')
        self.addCleanup(program.close)
        result = program.run()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)


@unittest.skipUnless(have_compiler(), 'needs a C compiler')
class KernelContainerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.program = CProgram('image-lib.c', shared=True)
        cls.addClassCleanup(cls.program.close)
        cls.lib = C.CDLL(str(cls.program.path))
        for name in ('sdr_fwdl_parse_image', 'sdr_fwdl_parse_patch'):
            getattr(cls.lib, name).argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]
        cls.lib.guarded_parse.argtypes = [C.c_void_p, C.c_size_t]

    def parse(self, data, expected=0, parser='sdr_fwdl_parse_image'):
        source = C.create_string_buffer(bytes(data))
        out = Image()
        C.memset(C.byref(out), 0xa5, C.sizeof(out))
        before = bytes(out)
        self.assertEqual(getattr(self.lib, parser)(source, len(data), C.byref(out)), expected)
        self.assertEqual(source.raw, bytes(data) + b'\0', 'input modified')
        if expected:
            self.assertEqual(bytes(out), before, 'output written on error')
        return source, out

    def check_regions(self, source, out, expected):
        """expected: (file offset, address, length, feature_set) per region."""
        self.assertEqual(out.count, len(expected))
        for region, (offset, address, length, feature) in zip(out.regions, expected):
            self.assertEqual(region.data - C.addressof(source), offset)
            self.assertEqual((region.address, region.length, region.feature_set),
                             (address, length, feature))

    def test_ram_container_regions_and_metadata(self):
        for lengths in ((1,), (17, 4097), (8193, 31, 16), (1,) * 14):
            for features in ((0,), (0x21, 0, 0x37), (0x37, 0x20)):
                data = ram_container(lengths, features, b'\x03\x00\x02\xffABCxy')
                source, out = self.parse(data)
                offsets = [sum(lengths[:i]) for i in range(len(lengths))]
                self.check_regions(source, out, [
                    (offsets[i], 0x916400 + i * 0x10000, n, features[i % len(features)])
                    for i, n in enumerate(lengths)])
                self.assertEqual(bytes(out.version), b'0123456789')
                self.assertEqual(bytes(out.build_date), b'20260101000000\0')
                self.assertEqual((out.chip_id, out.eco_code), (0x16, 0))

    def test_ram_container_reads_stay_in_bounds(self):
        data = ram_container((17, 31))
        buffer = C.create_string_buffer(bytes(data))
        self.assertEqual(self.lib.guarded_parse(buffer, len(data)), 0)

    def test_malformed_ram_containers_are_refused(self):
        original = ram_container((17,))
        table = len(original) - 76
        for size in range(36):
            self.parse(original[:size], -errno.EINVAL if not size else -errno.EBADMSG)
        edits = [(table + 20, '<I', 0, errno.EBADMSG),
                 (table + 20, '<I', table + 1, errno.EBADMSG),
                 (table + 16, '<I', 0xfffffff0, errno.EOPNOTSUPP),
                 (len(original) - 34, 'B', 0, errno.EBADMSG),
                 (len(original) - 34, 'B', 2, errno.EBADMSG),
                 (len(original) - 34, 'B', 255, errno.EOPNOTSUPP),
                 (len(original) - 33, 'B', 1, errno.EOPNOTSUPP),
                 (len(original) - 32, 'B', 0, errno.EOPNOTSUPP),
                 (17, 'B', 0, errno.EBADMSG),
                 (table - 1, 'B', 0, errno.EBADMSG),
                 (17 + 16, '<I', 0xffffffff, errno.EBADMSG)]
        for offset, fmt, value, error in edits:
            data = original.copy()
            struct.pack_into(fmt, data, offset, value)
            self.parse(with_crc(data), -error)
        for feature in range(256):
            data = original.copy()
            data[table + 24] = feature
            self.parse(with_crc(data), -errno.EOPNOTSUPP if feature & ~0x37 else 0)
        for envelope in (b'\x00', b'\x00\x00\x00', b'\xff\xff\x00\x00', b'\x00\x00\xff\x00'):
            self.parse(ram_container((17,), envelope=envelope), -errno.EBADMSG)
        for offset in (0, 16, len(original) - 4, len(original) - 1):
            data = original.copy()
            data[offset] ^= 1
            self.parse(data, -errno.EBADMSG)

    def test_patch_sections_and_key_modes(self):
        cases = [(0xffffffff, 0), (0x00000000, 0), (0x01000000, 1), (0x01000003, 7),
                 (0x02000100, 3), (0x02000300, 7)]
        for key_info, feature in cases:
            data = patch_container([(0x900000, bytes(range(200)), key_info),
                                    (0x200000, b'\x5a' * 33, 0)])
            source, out = self.parse(data, parser='sdr_fwdl_parse_patch')
            self.check_regions(source, out, [(96 + 128, 0x900000, 200, feature),
                                             (96 + 128 + 200, 0x200000, 33, 0)])
        for key_info in (0x01000004, 0x02000400):
            self.parse(patch_container([(0x900000, b'x' * 8, key_info)]),
                       -errno.EOPNOTSUPP, 'sdr_fwdl_parse_patch')

    def test_malformed_patches_are_refused(self):
        good = patch_container([(0x900000, b'x' * 64, 0)])
        not_patch = good.copy()
        not_patch[16:20] = b'\0\0\0\0'
        self.parse(not_patch, -errno.EOPNOTSUPP, 'sdr_fwdl_parse_patch')
        self.parse(good[:95], -errno.EOPNOTSUPP, 'sdr_fwdl_parse_patch')
        for offset, value, error in ((44, 0, errno.EBADMSG), (44, 15, errno.EOPNOTSUPP),
                                     (96, 0x00030003, errno.EOPNOTSUPP),
                                     (96 + 4, 95, errno.EBADMSG),
                                     (96 + 16, 0, errno.EBADMSG),
                                     (96 + 16, 65, errno.EBADMSG),
                                     (96 + 8, 65, errno.EBADMSG)):
            data = good.copy()
            struct.pack_into('>I', data, offset, value)
            self.parse(data, -error, 'sdr_fwdl_parse_patch')

    @unittest.skipUnless((FIRMWARE / 'firmware.json').is_file(), 'firmware not built')
    def test_built_firmware_parses(self):
        """Every container of our firmware set, as DOWNLOAD will parse it."""
        manifest = json.loads((FIRMWARE / 'firmware.json').read_text())
        for entry in manifest['containers']:
            data = (FIRMWARE / entry['file']).read_bytes()
            with self.subTest(container=entry['file']):
                if entry['role'] == 'patch':
                    # DOWNLOAD tries a RAM container first: the patch must not pass as one.
                    self.assertNotEqual(self.lib.sdr_fwdl_parse_image(
                        C.create_string_buffer(data), len(data), C.byref(Image())), 0)
                    source, out = self.parse(data, parser='sdr_fwdl_parse_patch')
                else:
                    source, out = self.parse(data)
                self.assertTrue(1 <= out.count <= 14)


if __name__ == '__main__':
    unittest.main()
