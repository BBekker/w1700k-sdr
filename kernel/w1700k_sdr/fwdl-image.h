/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef W1700K_SDR_FWDL_IMAGE_H
#define W1700K_SDR_FWDL_IMAGE_H
#include "fwdl.h"
#include "hw/mt7996/fw_format.h"

#define SDR_FWDL_IMAGE_MAX_BYTES (16U * 1024U * 1024U)

struct sdr_fwdl_image {
	struct sdr_fwdl_region regions[SDR_FWDL_MAX_REGIONS];
	u32 count;
	u8 chip_id, eco_code;
	/* raw, not NUL-terminated */
	u8 version[MT7996_FW_RAM_TRL_FW_VER_BYTES];
	u8 build_date[MT7996_FW_RAM_TRL_BUILD_DATE_BYTES];
};

/*
 * Parse a RAM container (format 2, flag 1): 1..14 regions, the release
 * envelope and the trailer CRC32. Regions point into `data`. Returns 0,
 * -EINVAL (arguments), -EFBIG, -EBADMSG (structure or CRC) or -EOPNOTSUPP
 * (format, region count or a feature the INIT encoder refuses); `out` is
 * untouched on error. The CRC detects corruption; it is not authentication.
 */
int sdr_fwdl_parse_image(const void *data, size_t bytes,
			 struct sdr_fwdl_image *out);

/*
 * Parse a ROM patch: header at the front, big-endian, INFO sections only.
 * No checksum is verified (mt76 verifies none). Returns as above;
 * -EOPNOTSUPP also when the header is not a patch header at all.
 */
int sdr_fwdl_parse_patch(const void *data, size_t bytes,
			 struct sdr_fwdl_image *out);
#endif
