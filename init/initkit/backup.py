# SPDX-License-Identifier: MIT
"""Identify the unit and copy its NAND data to the host, from the dumper prompt.

The backup is the data stock firmware addresses: vendor-logical offsets 0 to the
end of stock's `system` partition (446 MiB), read through the vendor BBT/BMT
translation with the chip's ECC correcting every page, so a file offset is the
offset stock uses. Bad blocks, the reserve pool, OOB areas and ECC parity are
the chip's own state and are not part of it. It is the only copy of the stock
firmware and of the unit's calibration once installation has begun, so it is
taken before anything is written and it is never overwritten.

A corrected read is exact, so two backups of an unchanged unit are identical.
The device's own SHA-256 of the buffer it sent is checked against the received
file.
"""
from pathlib import Path
import re
import shutil

from .config import partitions
from .console import StepFailed
from .log import sha256_file

# The vendor identity window: `key=value` lines ending in 0xff padding, at the
# start of the `calibration` partition. Besides the serial it holds the unit's
# stock credentials, which are never printed or logged here.
IDENTITY_OFFSET = 0x400000
IDENTITY_BYTES = 0x800
IDENTITY_SCRATCH = 0x88000000
FIRST_LOOK = 0x200
SERIAL_KEY = 'serial_number'
SERIAL_FORM = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._-]{3,63}$')
FIELD = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)=(.*)$')
# U-Boot `md.b`: address, colon, ` %02x` per byte, two spaces, ASCII column.
MD_LINE = re.compile(r'(?m)^[0-9a-fA-F]+:(.*)$')
# The unit's radio calibration: the MT7996 EEPROM image (7680 bytes used of
# 16 KiB), also in the `calibration` partition. The stock firmware copies it to
# /lib/firmware/MT7990_EEPROM.bin; the host tools send it on every radio load.
EEPROM_OFFSET = 0x405000
EEPROM_BYTES = 0x4000
EEPROM_NAME = 'eeprom.bin'
EEPROM_CHIP_ID = b'\x90\x79'      # 0x7990, little-endian, at the start of the image
DUMP_BUFFER = 0x90000000
# Vendor-logical 0 to the end of stock's `system` partition, where the vendor
# reserve begins.
DUMP_BYTES = partitions.GEOMETRY['reserved-bmt'][0]
BACKUP_NAME = 'nand.bin'
SHA256_LINE = re.compile(r'==>\s*([0-9a-fA-F]{64})')
TRANSFERRED = re.compile(r'Bytes transferred = (\d+)')
TRANSLATED = re.compile(r'translation:\s*vendor BBT/BMT')


def parse_md(text):
    """`md.b` output back into bytes; the ASCII column is ignored."""
    out = bytearray()
    for match in MD_LINE.finditer(text):
        pairs = match.group(1).split('  ', 1)[0].split()
        if pairs and all(len(pair) == 2 for pair in pairs):
            try:
                out += bytes.fromhex(''.join(pairs))
            except ValueError:
                continue
    return bytes(out)


def parse_identity(raw):
    """The window's `key=value` fields, up to its 0xff padding."""
    end = raw.find(b'\xff\xff\xff\xff')
    fields = {}
    for line in raw[:end if end >= 0 else len(raw)].split(b'\n'):
        match = FIELD.match(line.decode('latin-1').strip())
        if match:
            fields[match.group(1)] = match.group(2)
    return fields


def serial_from(fields):
    serial = fields.get(SERIAL_KEY, '').strip()
    if not SERIAL_FORM.match(serial):
        raise StepFailed(f'the identity window holds no usable serial_number '
                         f'(fields present: {", ".join(sorted(fields)) or "none"})')
    return serial


def serial_in_backup(path):
    with open(path, 'rb') as stream:
        stream.seek(IDENTITY_OFFSET)
        return parse_identity(stream.read(IDENTITY_BYTES)).get(SERIAL_KEY, '').strip()


def read_serial(console, log):
    """The serial from the identity window, through the dumper.

    The chip must be translated first, so the offset is the one stock uses;
    `.dontskipbad` keeps it exact. Only as much of the window is shown on the
    console as it takes to reach the serial.
    """
    check_listing(console, log)
    return read_serial_at(console)


def read_serial_at(console, scratch=IDENTITY_SCRATCH):
    """The serial from the identity window of a translated chip, read through
    RAM at `scratch` (the dumper or the restorer)."""
    console.must_run(f'mtd read.dontskipbad spi-nand0 {scratch:#x} '
                     f'{IDENTITY_OFFSET:#x} {IDENTITY_BYTES:#x}', limit=120)
    raw = parse_md(console.must_run(f'md.b {scratch:#x} {FIRST_LOOK:#x}', limit=60))
    if SERIAL_KEY not in parse_identity(raw):
        raw += parse_md(console.must_run(
            f'md.b {scratch + FIRST_LOOK:#x} {IDENTITY_BYTES - FIRST_LOOK:#x}',
            limit=120))
    return serial_from(parse_identity(raw))


def check_read_only(listing):
    """The dumper must report the chip read-only before it is read."""
    flags = re.search(r'flags:\s*(0x[0-9a-fA-F]+)\s*\((\w[\w-]*)\)', listing)
    if not flags or flags.group(2) != 'read-only' or int(flags.group(1), 16) & 0x400:
        raise StepFailed('the dumper does not report the chip read-only; refusing to '
                         'continue:\n' + listing[-600:])


def check_translated(listing):
    """The dumper or the restorer must address the chip through the vendor
    tables. A chip whose tables did not validate is shown untranslated: its
    offsets are not the ones stock uses.
    """
    if not TRANSLATED.search(listing):
        raise StepFailed('U-Boot could not validate this chip\'s vendor bad-block '
                         'tables, so it cannot address the data as stock does; '
                         'nothing was written:\n' + listing[-600:])


def check_listing(console, log):
    listing = console.must_run('mtd list', limit=30)
    log.text('mtd-list.txt', listing)
    check_read_only(listing)
    check_translated(listing)


def dump(console, log, settings, served_dir, unit_directory, serial):
    """Read the NAND data, hash it on the device, receive it, verify it.

    The transfer server writes into `served_dir`; the verified file, which must
    hold the `serial` the unit reported, moves to `unit_directory`.
    """
    target = Path(unit_directory) / BACKUP_NAME
    if target.exists():
        raise StepFailed(f'{target} exists; a backup is never overwritten')
    console.must_run('; '.join((f'setenv ipaddr {settings.device_ip}',
                                f'setenv serverip {settings.host_ip}',
                                f'setenv tftpdstp {settings.transfer_port}',
                                'setenv tftpblocksize 1468')), limit=20)
    check_listing(console, log)
    print('  reading the NAND data (about a minute, silent)')
    text = console.must_run(f'mtd read.dontskipbad spi-nand0 {DUMP_BUFFER:#x} 0 '
                            f'{DUMP_BYTES:#x}', limit=900, heartbeat=15, about=55)
    if 'Failure while reading' in text or 'failed with error' in text:
        raise StepFailed('the NAND read failed (a page the ECC could not correct?); '
                         'no backup was made:\n' + text[-600:])
    corrected = text.count('Corrected bitflips')
    log.event('read', corrected_pages=corrected)
    text = console.must_run(f'hash sha256 {DUMP_BUFFER:#x} {DUMP_BYTES:#x}',
                            limit=900, heartbeat=15, about=10)
    device = SHA256_LINE.search(text)
    if not device:
        raise StepFailed('the device printed no SHA-256 for the dump buffer')
    print('  sending it to the host (about 35 s)')
    partial = Path(served_dir) / BACKUP_NAME
    partial.unlink(missing_ok=True)
    text = console.must_run(f'tftpput {DUMP_BUFFER:#x} {DUMP_BYTES:#x} {BACKUP_NAME}',
                            limit=1800, heartbeat=15, about=34)
    if not TRANSFERRED.search(text) or not partial.is_file():
        raise StepFailed('tftpput did not complete:\n' + text[-600:])
    if partial.stat().st_size != DUMP_BYTES:
        raise StepFailed(f'the backup is {partial.stat().st_size} bytes, not {DUMP_BYTES}')
    received = sha256_file(partial)
    if received != device.group(1).lower():
        raise StepFailed(f'the backup was corrupted in transfer: device {device.group(1)}, '
                         f'host {received}; it was kept as {partial}')
    if serial_in_backup(partial) != serial:
        raise StepFailed(f'the backup does not hold serial {serial}; it was kept as {partial}')
    partial.chmod(0o400)
    target.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    shutil.move(partial, target)
    log.event('backup', path=str(target), sha256=received)
    return target, received


def extract_eeprom(backup_path, unit_directory):
    """Save the unit's radio calibration from its backup (vendor-logical, so the
    partition offset is the file offset)."""
    with open(backup_path, 'rb') as stream:
        stream.seek(EEPROM_OFFSET)
        data = stream.read(EEPROM_BYTES)
    if len(data) != EEPROM_BYTES or data[:2] != EEPROM_CHIP_ID:
        raise StepFailed(f'{backup_path} holds no MT7996 calibration at {EEPROM_OFFSET:#x}')
    target = Path(unit_directory) / EEPROM_NAME
    if not target.is_file() or target.read_bytes() != data:
        partial = target.with_suffix('.part')
        partial.write_bytes(data)
        partial.chmod(0o600)
        partial.replace(target)
    return target
