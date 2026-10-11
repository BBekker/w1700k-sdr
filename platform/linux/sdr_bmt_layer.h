/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Attach/detach for the vendor BMT translation layer, called from the kernel's
 * spinand_probe. platform/linux/build.py copies it to drivers/mtd/nand/.
 */
#ifndef SDR_BMT_LAYER_H
#define SDR_BMT_LAYER_H

struct mtd_info;

/* Attach vendor-logical translation, and recover persistent transactions when
 * configured. Failure refuses probe/registration; never expose raw fallback. */
int sdr_bmt_attach(struct mtd_info *mtd);
void sdr_bmt_detach(struct mtd_info *mtd);

#endif /* SDR_BMT_LAYER_H */
