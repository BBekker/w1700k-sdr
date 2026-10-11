# SPDX-License-Identifier: MIT
"""Seal code into an MT7996 RAM container (include/hw/mt7996/fw_format.h).

Layout: the regions' payloads from offset 0, an empty release envelope, one 40-byte descriptor
per region, and the 36-byte trailer closed by the CRC32 of everything before it. An encrypted
region (feature bit 0) is AES-128-CBC with a zero key and an IV reset per region, over all but
its last 16 bytes, a plaintext trailer of zeros. The region carrying feature bit 5 is the entry
FW_START names (a DSP container has none).

The ROM checks each region's PDA CRC residue, which a plain trailer does not satisfy; the
PDA-CRC hook of our ROM patch makes that check pass.

read_ram_container reads a container back into its regions; only the tests use it.
"""
import struct
import zlib

import aes

FEATURE_ENCRYPT, FEATURE_ENTRY = 0x01, 0x20
TAIL = 16
CHIP_ID = 0x16                    # as the vendor WM's trailer; the ROM never sees it


def _region(payload, feature):
    body = bytes(payload) + bytes(-len(payload) % 16)
    if not body:
        raise ValueError('an empty region')
    if feature & FEATURE_ENCRYPT:
        body = aes.cbc_zero(body, encrypt=True)
    return body + bytes(TAIL)


def release_envelope():
    """The release-information block with an empty body."""
    return b'#' * 16 + struct.pack('<I', 0) + b'#' * 16


def ram_container(regions, version, build_date):
    """regions: [(MCU address, payload bytes, feature byte)]; version 10 and build_date at most
    15 ASCII characters."""
    if not 1 <= len(regions) <= 14:
        raise ValueError('1..14 regions')
    if sum(1 for _, _, feature in regions if feature & FEATURE_ENTRY) > 1:
        raise ValueError('more than one region carries the entry (feature bit 5)')
    version, build_date = version.encode('ascii'), build_date.encode('ascii')
    if len(version) != 10 or len(build_date) > 15:
        raise ValueError('version is 10 characters, the build date at most 15')
    bodies = [_region(payload, feature) for _, payload, feature in regions]
    descriptors = b''.join(struct.pack('<IIII II BB 14x', 0, 0, 0, 0, address, len(body), feature, 0)
                           for (address, _, feature), body in zip(regions, bodies))
    trailer = struct.pack('<BBBBB2x', CHIP_ID, 0, len(regions), 2, 1) + version + build_date.ljust(15, b'\0')
    image = b''.join(bodies) + release_envelope() + descriptors + trailer
    return image + struct.pack('<I', zlib.crc32(image))


def read_ram_container(image):
    """The inverse of ram_container, for the tests: [(MCU address, payload, feature byte)] in table
    order, each payload decrypted where encrypted and padded to 16 bytes as sealed. Raises
    ValueError unless the CRC holds and the regions, each with its blank tail, fill the image up to
    an empty release envelope and the descriptor table."""
    if len(image) < 36 + 40 or zlib.crc32(image[:-4]) != struct.unpack_from('<I', image, len(image) - 4)[0]:
        raise ValueError('not a RAM container: short, or its CRC does not hold')
    count = image[-36 + 2]
    table = len(image) - 36 - 40 * count
    out, offset = [], 0
    for i in range(count):
        address, length, feature = struct.unpack_from('<IIB', image, table + 40 * i + 0x10)
        body = image[offset:offset + length]
        if length <= TAIL or length % 16 or len(body) != length or body[-TAIL:] != bytes(TAIL):
            raise ValueError(f'region {i}: not AES blocks and a blank tail')
        payload = body[:-TAIL]
        out.append((address, aes.cbc_zero(payload, encrypt=False) if feature & FEATURE_ENCRYPT else payload, feature))
        offset += length
    if image[offset:table] != release_envelope():
        raise ValueError('the regions do not end at an empty release envelope')
    return out
