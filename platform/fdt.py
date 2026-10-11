# SPDX-License-Identifier: MIT
"""A reader for flattened device trees and FIT images, without dtc."""
import struct

MAGIC = 0xd00dfeed
BEGIN_NODE, END_NODE, PROP, NOP, END = 1, 2, 3, 4, 9


class FdtError(ValueError):
    """The blob is not a well-formed version 17 flattened device tree."""


def _require(condition, message):
    if not condition:
        raise FdtError(message)


def nodes(data):
    """{path: {property: bytes}} for every node of the tree."""
    _require(len(data) >= 40, 'truncated FDT header')
    (magic, total, structure, strings_at, reserve, version, compat, _, strings_len,
     structure_len) = struct.unpack_from('>10I', data)
    _require(magic == MAGIC and version == 17 and compat <= 17, 'unsupported FDT header')
    _require(total == len(data) and 40 <= reserve < structure and reserve % 8 == 0 and
             structure % 4 == 0 and structure + structure_len <= strings_at and
             strings_at + strings_len <= total, 'invalid FDT bounds')
    strings = data[strings_at:strings_at + strings_len]
    offset, end = structure, structure + structure_len
    tree, stack = {}, []
    while offset + 4 <= end:
        token, = struct.unpack_from('>I', data, offset)
        offset += 4
        if token == BEGIN_NODE:
            stop = data.find(b'\0', offset, end)
            _require(stop >= 0, 'unterminated FDT node name')
            name = data[offset:stop].decode('ascii')
            _require('/' not in name and (bool(name) if stack else name == ''),
                     'invalid FDT node name')
            path = stack[-1].rstrip('/') + '/' + name if stack else '/'
            _require(path not in tree, f'duplicate FDT node {path}')
            tree[path] = {}
            stack.append(path)
            offset = (stop + 4) & ~3
        elif token == END_NODE:
            _require(bool(stack), 'unbalanced FDT node')
            stack.pop()
        elif token == PROP:
            _require(bool(stack) and offset + 8 <= end, 'truncated FDT property')
            length, name_at = struct.unpack_from('>II', data, offset)
            offset += 8
            stop = strings.find(b'\0', name_at)
            _require(name_at < len(strings) and stop >= 0 and offset + length <= end,
                     'invalid FDT property bounds')
            name = strings[name_at:stop].decode('ascii')
            _require(name not in tree[stack[-1]], f'duplicate FDT property {name}')
            tree[stack[-1]][name] = data[offset:offset + length]
            offset = (offset + length + 3) & ~3
        elif token == NOP:
            continue
        elif token == END:
            _require(not stack and '/' in tree and offset == end, 'invalid FDT end')
            return tree
        else:
            raise FdtError(f'unknown FDT token {token}')
    raise FdtError('missing FDT end token')


def reservations(data):
    """The memory reservation block as [(address, size)]."""
    offset = struct.unpack_from('>I', data, 16)[0]
    found = []
    while True:
        pair = struct.unpack_from('>QQ', data, offset)
        offset += 16
        if pair == (0, 0):
            return found
        found.append(pair)


def strings(value):
    _require(value.endswith(b'\0'), 'expected a string property')
    return value[:-1].decode('ascii').split('\0')


def string(value):
    return strings(value)[0]


def cells(value):
    _require(len(value) % 4 == 0, 'expected a cell property')
    return list(struct.unpack(f'>{len(value) // 4}I', value))
