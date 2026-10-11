#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the write-free NAND dumper that backs up a unit before installation.

    platform/dumper/build.py      -> platform/build/images/w1700k-dumper.itb

Stock U-Boot receives this FIT over TFTP and boots it from RAM. It reads the
whole chip with OOB into RAM, hashes it and sends it to the host with `tftpput`.
That raw image is the only way back to stock firmware, and it is taken before
anything is written, so the dumper cannot write: there is no UBI, no saved
environment, no autoboot, and the NAND write, erase and mark-bad paths are not
compiled in at all. The build checks the linked image for each of them.
"""
import gzip
from pathlib import Path
import re
import sys

UNIT = Path(__file__).resolve().parent
sys.path.insert(0, str(UNIT.parent))
from buildlib import BUILD, IMAGES, LOGS, kconfig, main_guard, require, write_if_changed
import uboot

FIT = IMAGES / 'w1700k-dumper.itb'
DEFCONFIG = 'an7581_w1700k_dumper_defconfig'
FILES = {
    f'configs/{DEFCONFIG}': UNIT / DEFCONFIG,
    'defenvs/an7581_w1700k_dumper_env': UNIT / 'an7581_w1700k_dumper_env',
    'dts/upstream/src/arm64/airoha/an7581-w1700k-dumper.dts': UNIT / 'an7581-w1700k-dumper.dts',
    'arch/arm/dts/an7581-w1700k-dumper-u-boot.dtsi': UNIT / 'an7581-w1700k-dumper-u-boot.dtsi',
}
# The raw image is read into one buffer: 512 MiB of pages followed by 32 MiB of OOB.
DUMP_BUFFER = 0x90000000
DUMP_BYTES = 0x22000000
ENABLED = ('AN7581_W1700K_SDR', 'POSITION_INDEPENDENT', 'LINUX_KERNEL_IMAGE_HEADER',
           'OF_SEPARATE', 'ENV_IS_NOWHERE', 'CMD_MTD', 'CMD_HASH', 'SHA256',
           'CMD_TFTPBOOT', 'CMD_TFTPPUT', 'MTD_SPI_NAND', 'MTD_SPI_NAND_READ_ONLY',
           'AIROHA_SNFI_SPI', 'AIROHA_ETH')
DISABLED = ('MTD_UBI', 'CMD_UBI', 'AUTOBOOT', 'CMD_SAVEENV', 'CMD_EDITENV',
            'AIROHA_BMT', 'AIROHA_BMT_WRITE', 'CMD_MTD_MARKBAD', 'CMD_MTD_NAND_WRITE_TEST',
            'CMD_MTD_OTP', 'BOOTCOUNT_LIMIT', 'USE_PREBOOT', 'ENV_IS_IN_MTD', 'ENV_IS_IN_UBI')
# Every NAND mutation implementation, and environment saving. Each must be
# absent from the linked image, not merely unreachable.
ABSENT = ('mtd_erase', 'mtd_write', 'mtd_write_oob', 'mtd_panic_write', 'mtd_writev',
          'mtd_block_markbad', 'spinand_mtd_write', 'spinand_mtd_erase',
          'spinand_mtd_block_markbad', 'spinand_write_page', 'spinand_erase_op',
          'do_mtd_erase', 'spinand_erase', 'spinand_markbad', 'env_save', 'env_export')
ABSENT_PREFIXES = ('ubi_', 'ubiblock_', 'do_ubi', 'airoha_bmt_')
# The read path, and the refusing nand_ops stubs without which the chip does not
# probe at all (nanddev_init requires erase, markbad and isbad entries).
REQUIRED = ('mtd_read_oob', 'spinand_mtd_read', 'mtd_block_isbad', 'do_mtd_io',
            'spinand_erase_refused', 'spinand_markbad_refused')


def patches():
    return [uboot.BOARD_PATCH, *uboot.SHARED_PATCHES,
            *sorted((UNIT / 'patches').glob('*.patch'))]


def check_write_free(linked, mapfile):
    present = sorted(name for name in ABSENT if name in linked)
    present += sorted(name for name in linked if name.startswith(ABSENT_PREFIXES))
    require(not present, 'dumper: NAND mutation code is linked: ' + ', '.join(present[:8]))
    allocated = mapfile[:mapfile.index('Discarded input sections')] + \
        mapfile[mapfile.index('Memory Configuration'):]
    for name in ABSENT:
        require(not re.search(rf'^\s*\.(?:text|rodata|data)\.{re.escape(name)}\b',
                              allocated, re.M),
                f'dumper: {name} is laid out in the image')
    missing = [name for name in REQUIRED if name not in linked]
    require(not missing, 'dumper: the read path is incomplete: ' + ', '.join(missing))


def main():
    src = uboot.prepare('dumper', patches(), FILES)
    uboot.mkimage(src)
    out = uboot.build('dumper', src, DEFCONFIG)
    config = kconfig(out / '.config')
    uboot.require_config('dumper', config, ENABLED, DISABLED)
    require(int(config['CONFIG_SYS_LOAD_ADDR'], 0) == DUMP_BUFFER,
            'dumper: the load address is not the dump buffer')
    image, dtb, linked = uboot.check_image('dumper', out, config, DUMP_BUFFER)
    require(DUMP_BUFFER + DUMP_BYTES <= uboot.RAM_END, 'dumper: the dump buffer passes RAM')
    check_write_free(linked, (out / 'u-boot.map').read_text(errors='replace'))

    # gzip shrinks the FIT to six 32 KiB pieces, the most stock's TFTP is relied
    # on to move: its ten-timeout budget per transfer is never refilled.
    payload = BUILD / 'dumper/u-boot.bin.gz'
    write_if_changed(payload, gzip.compress(image, 9, mtime=0))
    IMAGES.mkdir(parents=True, exist_ok=True)
    its = uboot.stock_fit('W1700K write-free NAND dumper (gzip)', 'dumper', payload, 'gzip',
                          out / 'u-boot.dtb')
    uboot.make_fit(its, FIT, out / 'scripts/dtc', LOGS / 'dumper.log')
    print(f'dumper: {FIT} ({FIT.stat().st_size} bytes)')
    return 0


if __name__ == '__main__':
    main_guard(main)
