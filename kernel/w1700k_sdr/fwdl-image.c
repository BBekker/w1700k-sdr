// SPDX-License-Identifier: GPL-2.0-only
/*
 * Firmware container parsing. RAM container: regions from offset 0, then a
 * release envelope, a table of 40-byte region descriptors and a 36-byte
 * trailer whose CRC32 covers the rest of the file (mt76 mt7996/mcu.c,
 * mt7996_fw_trailer/mt7996_fw_region). ROM patch: a 96-byte big-endian header
 * and 64-byte section records (mt7996_patch_hdr/mt7996_patch_sec).
 */
#include <linux/crc32.h>
#include <linux/errno.h>
#include "fwdl-image.h"
#include "fwdl_wire.h"
#include "hw/mt7996/fw_format.h"

/* The smallest release envelope: both marks and the body length, no body. */
#define SDR_RELEASE_MIN_BYTES (MT7996_FW_RAM_RELINFO_HDR_BYTES + \
			       MT7996_FW_RAM_RELINFO_MARK_BYTES)

static u32 sdr_image_le32(const u8 *p)
{
	return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}

/* The patch container is big-endian throughout, unlike the RAM container. */
static u32 sdr_image_be32(const u8 *p)
{
	return (u32)p[3] | (u32)p[2] << 8 | (u32)p[1] << 16 | (u32)p[0] << 24;
}

/* The release envelope between the last region and the descriptor table:
 * 16 '#' at each end, LE32 at +16 = file length - 36, then length-prefixed
 * records. Only the framing is checked.
 */
static int sdr_image_release(const u8 *data, size_t bytes)
{
	size_t pos, end, length;
	u32 i;

	if (bytes < SDR_RELEASE_MIN_BYTES ||
	    sdr_image_le32(data + MT7996_FW_RAM_RELINFO_MARK_BYTES) !=
	    bytes - SDR_RELEASE_MIN_BYTES)
		return -EBADMSG;
	for (i = 0; i < MT7996_FW_RAM_RELINFO_MARK_BYTES; i++)
		if (data[i] != MT7996_FW_RAM_RELINFO_MARK ||
		    data[bytes - MT7996_FW_RAM_RELINFO_MARK_BYTES + i] !=
		    MT7996_FW_RAM_RELINFO_MARK)
			return -EBADMSG;
	end = bytes - MT7996_FW_RAM_RELINFO_MARK_BYTES;
	for (pos = MT7996_FW_RAM_RELINFO_HDR_BYTES; pos < end; pos += length) {
		if (end - pos < 4)
			return -EBADMSG;
		length = 4U + data[pos] + ((u32)data[pos + 1] << 8) + data[pos + 2];
		if (length > end - pos)
			return -EBADMSG;
	}
	return 0;
}

int sdr_fwdl_parse_image(const void *data, size_t bytes,
			 struct sdr_fwdl_image *out)
{
	struct sdr_fwdl_image parsed = {};
	const u8 *image = data, *trailer, *desc;
	u8 packet[MT7996_FWDL_INIT_BYTES];
	size_t table, offset = 0;
	unsigned long start = (unsigned long)data, dst = (unsigned long)out;
	u32 i, length, address, feature;
	int ret;

	if (!data || !out || !bytes || bytes - 1 > ~0UL - start ||
	    sizeof(*out) - 1 > ~0UL - dst || dst % __alignof__(*out))
		return -EINVAL;
	if (bytes > SDR_FWDL_IMAGE_MAX_BYTES)
		return -EFBIG;
	if (start <= dst + sizeof(*out) - 1 && dst <= start + bytes - 1)
		return -EINVAL;
	if (bytes < MT7996_FW_RAM_TRAILER_BYTES)
		return -EBADMSG;
	trailer = image + bytes - MT7996_FW_RAM_TRAILER_BYTES;
	if (trailer[MT7996_FW_RAM_TRL_FORMAT_VER] != MT7996_FW_RAM_FORMAT_VER_CONNAC3 ||
	    trailer[MT7996_FW_RAM_TRL_FORMAT_FLAG] != MT7996_FW_RAM_FORMAT_FLAG_RELEASE_INFO)
		return -EOPNOTSUPP;
	parsed.count = trailer[MT7996_FW_RAM_TRL_N_REGION];
	if (!parsed.count)
		return -EBADMSG;
	if (parsed.count > SDR_FWDL_MAX_REGIONS)
		return -EOPNOTSUPP;
	if (parsed.count * MT7996_FW_RAM_REGION_DESC_BYTES > bytes - MT7996_FW_RAM_TRAILER_BYTES)
		return -EBADMSG;
	table = bytes - MT7996_FW_RAM_TRAILER_BYTES -
		parsed.count * MT7996_FW_RAM_REGION_DESC_BYTES;
	for (i = 0; i < parsed.count; i++) {
		desc = image + table + i * MT7996_FW_RAM_REGION_DESC_BYTES;
		address = sdr_image_le32(desc + MT7996_FW_RAM_RGN_ADDR);
		length = sdr_image_le32(desc + MT7996_FW_RAM_RGN_LEN);
		feature = desc[MT7996_FW_RAM_RGN_FEATURE_SET];
		if (!length || length > table - offset)
			return -EBADMSG;
		/* Admit exactly what the INIT encoder accepts. */
		ret = mt7996_fwdl_init(packet, sizeof(packet), 1, address, length, feature);
		if (ret < 0)
			return -EOPNOTSUPP;
		parsed.regions[i] = (struct sdr_fwdl_region){
			.data = image + offset, .address = address,
			.length = length, .feature_set = feature,
		};
		offset += length;
	}
	ret = sdr_image_release(image + offset, table - offset);
	if (ret)
		return ret;
	if ((crc32_le(~0U, image, bytes - 4) ^ ~0U) !=
	    sdr_image_le32(trailer + MT7996_FW_RAM_TRL_CRC32))
		return -EBADMSG;
	parsed.chip_id = trailer[MT7996_FW_RAM_TRL_CHIP_ID];
	parsed.eco_code = trailer[MT7996_FW_RAM_TRL_ECO_CODE];
	for (i = 0; i < sizeof(parsed.version); i++)
		parsed.version[i] = trailer[MT7996_FW_RAM_TRL_FW_VER + i];
	for (i = 0; i < sizeof(parsed.build_date); i++)
		parsed.build_date[i] = trailer[MT7996_FW_RAM_TRL_BUILD_DATE + i];
	*out = parsed;
	return 0;
}

/* mt7996_patch_sec_mode as a feature_set: all-ones or PLAIN key info means no
 * encryption; otherwise encrypt with the key index from the low byte (AES) or
 * the next byte (other modes). A key index wider than two bits is refused.
 */
static int sdr_patch_feature_set(u32 key_info)
{
	u32 security = HW_GET(PATCH_SEC_ENC_TYPE, key_info), key;

	if (key_info == PATCH_SEC_NOT_SUPPORT || security == PATCH_SEC_ENC_TYPE_PLAIN)
		return 0;
	key = security == PATCH_SEC_ENC_TYPE_AES ?
		HW_GET(PATCH_SEC_ENC_AES_KEY, key_info) :
		HW_GET(PATCH_SEC_ENC_SCRAMBLE_KEY, key_info);
	if (key > HW_GET(FW_FEATURE_SET_KEY_IDX, ~0U))
		return -EOPNOTSUPP;
	return (int)(FW_FEATURE_SET_ENCRYPT | HW_PREP(FW_FEATURE_SET_KEY_IDX, key));
}

int sdr_fwdl_parse_patch(const void *data, size_t bytes,
			 struct sdr_fwdl_image *out)
{
	struct sdr_fwdl_image parsed = {};
	const u8 *image = data, *sec;
	u8 packet[MT7996_FWDL_INIT_BYTES];
	unsigned long start = (unsigned long)data, dst = (unsigned long)out;
	u32 i, count, type, offset, size, address, length, feature;
	int ret;

	if (!data || !out || !bytes || bytes - 1 > ~0UL - start ||
	    sizeof(*out) - 1 > ~0UL - dst || dst % __alignof__(*out))
		return -EINVAL;
	if (bytes > SDR_FWDL_IMAGE_MAX_BYTES)
		return -EFBIG;
	if (start <= dst + sizeof(*out) - 1 && dst <= start + bytes - 1)
		return -EINVAL;
	if (bytes < MT7996_FW_PATCH_HDR_BYTES)
		return -EOPNOTSUPP;
	/* Detection: hdr.platform is four printable characters ("ALPS"). */
	for (i = 0; i < MT7996_FW_PATCH_HDR_PLATFORM_BYTES; i++)
		if (image[MT7996_FW_PATCH_HDR_PLATFORM + i] < 0x20 ||
		    image[MT7996_FW_PATCH_HDR_PLATFORM + i] > 0x7e)
			return -EOPNOTSUPP;

	count = sdr_image_be32(image + MT7996_FW_PATCH_HDR_DESC_N_REGION);
	if (!count)
		return -EBADMSG;
	if (count > SDR_FWDL_MAX_REGIONS)
		return -EOPNOTSUPP;
	if ((u64)count * MT7996_FW_PATCH_SEC_BYTES > bytes - MT7996_FW_PATCH_HDR_BYTES)
		return -EBADMSG;

	for (i = 0; i < count; i++) {
		sec = image + MT7996_FW_PATCH_HDR_BYTES + i * MT7996_FW_PATCH_SEC_BYTES;
		type = sdr_image_be32(sec + MT7996_FW_PATCH_SEC_TYPE);
		offset = sdr_image_be32(sec + MT7996_FW_PATCH_SEC_OFFS);
		size = sdr_image_be32(sec + MT7996_FW_PATCH_SEC_SIZE);
		address = sdr_image_be32(sec + MT7996_FW_PATCH_SEC_ADDR);
		length = sdr_image_be32(sec + MT7996_FW_PATCH_SEC_LEN);
		/* mt76 rejects any other section type. */
		if (HW_GET(PATCH_SEC_TYPE_MASK, type) != PATCH_SEC_TYPE_INFO)
			return -EOPNOTSUPP;
		ret = sdr_patch_feature_set(sdr_image_be32(sec + MT7996_FW_PATCH_SEC_KEY_IDX));
		if (ret < 0)
			return ret;
		feature = (u32)ret;
		/* mt76 downloads info.len bytes from offs; size is only bounded. */
		if (!length || offset < MT7996_FW_PATCH_HDR_BYTES ||
		    offset > bytes || length > bytes - offset)
			return -EBADMSG;
		if (size && (size > bytes - offset))
			return -EBADMSG;
		if (mt7996_fwdl_init(packet, sizeof(packet), 1, address,
				     length, feature) < 0)
			return -EOPNOTSUPP;
		parsed.regions[i] = (struct sdr_fwdl_region){
			.data = image + offset, .address = address,
			.length = length, .feature_set = feature,
		};
	}
	parsed.count = count;
	*out = parsed;
	return 0;
}
