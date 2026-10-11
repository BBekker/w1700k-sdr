// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * MT7996 MCU command wire format. Layouts follow mt76 (mt76_connac_mcu.[ch],
 * mt7996/mcu.c), Copyright (C) 2022 MediaTek Inc. See LICENSE.mt76.
 *
 * Explicit byte stores: no host alignment, endianness or bitfield layout is
 * assumed. MT7996 uses the connac3 HDR_FORMAT field (TXD1 bits 15:14).
 */
#include "fwdl_wire.h"

static void put16(uint8_t *out, uint32_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *out, uint32_t value)
{
	put16(out, value);
	put16(out + 2, value >> 16);
}

/* TXD0/TXD1 of an MCU command: Q_IDX MT_TX_MCU_PORT_RX_Q0, PKT_FMT CMD,
 * HDR_FORMAT CMD, then the length of everything after the 32-byte TXD.
 */
static void txd(uint8_t *out, uint32_t bytes)
{
	put32(out, MCU_TXD0_CMD_BASE | bytes);
	put32(out + 4, MCU_TXD1_CMD);
	put16(out + MT7996_MCU_TXD_LEN, bytes - MT7996_MCU_TXD_HW_BYTES);
}

/* The 64-byte download command header: pq_id 0x8000, MCU_PKT_ID, MCU_Q_NA,
 * destination WM, no extended id.
 */
static void header(uint8_t *out, uint32_t bytes, uint8_t command, uint8_t seq)
{
	uint32_t i;

	for (i = 0; i < MT7996_FWDL_COMMAND_HEADER; i++)
		out[i] = 0;
	txd(out, bytes);
	put16(out + MT7996_MCU_TXD_PQ_ID, MCU_TXD_PQ_ID_MCU_RX_Q0);
	out[MT7996_MCU_TXD_CID] = command;
	out[MT7996_MCU_TXD_PKT_TYPE] = MCU_TXD_PKT_TYPE_CMD;
	out[MT7996_MCU_TXD_SET_QUERY] = MCU_TXD_SET_QUERY_NA;
	out[MT7996_MCU_TXD_SEQ] = seq;
}

static int valid_seq(uint32_t seq)
{
	return seq >= MT7996_MCU_SEQ_MIN && seq <= MT7996_MCU_SEQ_MAX;
}

int mt7996_fwdl_next_seq(uint32_t previous)
{
	if (previous > MT7996_MCU_SEQ_MAX)
		return MT7996_FWDL_INVALID;
	return previous == MT7996_MCU_SEQ_MAX ? (int)MT7996_MCU_SEQ_MIN : (int)previous + 1;
}

uint32_t mt7996_fwdl_init_command(uint32_t address)
{
	return address == MT7996_FWDL_PATCH_ADDRESS ||
	       address == MT7996_FWDL_PATCH_ADDRESS_799X ?
		MT7996_FWDL_CMD_PATCH_START : MT7996_FWDL_CMD_TARGET_ADDRESS_LEN;
}

int mt7996_fwdl_init(uint8_t *out, size_t capacity, uint32_t seq,
		     uint32_t address, uint32_t length, uint32_t feature_set)
{
	uint32_t mode;

	if (!out || !valid_seq(seq) || length == 0 ||
	    length > 0x7fffffffU || length - 1 > 0xffffffffU - address ||
	    (feature_set & ~(FW_FEATURE_SET_ENCRYPT | FW_FEATURE_SET_KEY_IDX |
			     FW_FEATURE_SET_ENCRY_MODE | FW_FEATURE_SET_OVERRIDE_ADDR)))
		return MT7996_FWDL_INVALID;
	if (capacity < MT7996_FWDL_INIT_BYTES)
		return MT7996_FWDL_CAPACITY;
	mode = MCU_DL_MODE_NEED_RSP |
	       HW_PREP(MCU_DL_MODE_KEY_IDX, HW_GET(FW_FEATURE_SET_KEY_IDX, feature_set));
	if (feature_set & FW_FEATURE_SET_ENCRYPT)
		mode |= MCU_DL_MODE_ENCRYPT | MCU_DL_MODE_RESET_SEC_IV;
	if (feature_set & FW_FEATURE_SET_ENCRY_MODE)
		mode |= MCU_DL_MODE_ENCRY_MODE_SEL;
	header(out, MT7996_FWDL_INIT_BYTES,
	       (uint8_t)mt7996_fwdl_init_command(address), (uint8_t)seq);
	put32(out + MT7996_MCU_INIT_ADDR, address);
	put32(out + MT7996_MCU_INIT_LEN, length);
	put32(out + MT7996_MCU_INIT_MODE, mode);
	return MT7996_FWDL_INIT_BYTES;
}

int mt7996_fwdl_patch_sem(uint8_t *out, size_t capacity, uint32_t seq,
			  uint32_t get)
{
	if (!out || !valid_seq(seq) ||
	    (get != MT7996_FWDL_SEM_GET && get != MT7996_FWDL_SEM_RELEASE))
		return MT7996_FWDL_INVALID;
	if (capacity < MT7996_FWDL_PATCH_BYTES)
		return MT7996_FWDL_CAPACITY;
	header(out, MT7996_FWDL_PATCH_BYTES, MT7996_FWDL_CMD_PATCH_SEM,
	       (uint8_t)seq);
	put32(out + MT7996_MCU_PATCH_SEM_OP, get);
	return MT7996_FWDL_PATCH_BYTES;
}

int mt7996_fwdl_patch_finish(uint8_t *out, size_t capacity, uint32_t seq,
			     uint32_t check_crc)
{
	if (!out || !valid_seq(seq) || check_crc > 1)
		return MT7996_FWDL_INVALID;
	if (capacity < MT7996_FWDL_PATCH_BYTES)
		return MT7996_FWDL_CAPACITY;
	header(out, MT7996_FWDL_PATCH_BYTES, MT7996_FWDL_CMD_PATCH_FINISH,
	       (uint8_t)seq);
	/* check_crc byte, three reserved */
	put32(out + MT7996_MCU_PATCH_FINISH_CHECK_CRC, check_crc);
	return MT7996_FWDL_PATCH_BYTES;
}

int mt7996_fwdl_start(uint8_t *out, size_t capacity, uint32_t seq,
		      uint32_t entry, uint32_t working_pda)
{
	if (!out || !valid_seq(seq) ||
	    (working_pda != 0 && working_pda != MT7996_FWDL_START_PDA_CR4 &&
	     working_pda != MT7996_FWDL_START_PDA_DSP))
		return MT7996_FWDL_INVALID;
	if (capacity < MT7996_FWDL_START_BYTES)
		return MT7996_FWDL_CAPACITY;
	header(out, MT7996_FWDL_START_BYTES, MT7996_FWDL_CMD_FW_START,
	       (uint8_t)seq);
	put32(out + MT7996_MCU_START_OPTION,
	      (entry ? MT7996_FWDL_START_OVERRIDE : 0U) | working_pda);
	put32(out + MT7996_MCU_START_ADDR, entry);
	return MT7996_FWDL_START_BYTES;
}

int mt7996_fwdl_scatter_length(uint32_t total, uint32_t offset)
{
	uint32_t remaining;

	if (total > 0x7fffffffU || offset > total)
		return MT7996_FWDL_INVALID;
	remaining = total - offset;
	return remaining > MT7996_FWDL_SCATTER_MAX ?
		(int)MT7996_FWDL_SCATTER_MAX : (int)remaining;
}

int mt7996_fwdl_reply_status(const uint8_t *packet, size_t bytes,
			     uint32_t expected_seq, uint32_t expected_eid)
{
	if (!packet || !valid_seq(expected_seq) || expected_eid > 0xff)
		return MT7996_FWDL_INVALID;
	if (bytes != MT7996_FWDL_REPLY_BYTES)
		return MT7996_FWDL_MALFORMED_REPLY;
	/* RX byte count and the 12-byte event extent must match the length. */
	if (packet[0] != MT7996_FWDL_REPLY_BYTES || packet[1] != 0 ||
	    packet[MT7996_MCU_EVT_LEN] != MT7996_FWDL_REPLY_BYTES - MT7996_MCU_RXD_HW_BYTES ||
	    packet[MT7996_MCU_EVT_LEN + 1] != 0)
		return MT7996_FWDL_MALFORMED_REPLY;
	/* RXD0[31:27] packet type 7 (event), not a normal RX frame. */
	if ((packet[3] >> 3) != 7 || (packet[2] & 15) == 1 ||
	    packet[MT7996_MCU_EVT_PKT_TYPE_ID] != MT7996_MCU_ROM_REPLY_PKT_TYPE_ID ||
	    packet[MT7996_MCU_EVT_PKT_TYPE_ID + 1] != 0 ||
	    (expected_eid != MT7996_FWDL_EID_ANY && packet[MT7996_MCU_EVT_EID] != expected_eid) ||
	    packet[MT7996_MCU_EVT_SEQ] != expected_seq)
		return MT7996_FWDL_UNRELATED_REPLY;
	/* A download reply leaves these zero; anything else is a runtime or
	 * extended event and its byte 40 is not a status.
	 */
	if (packet[MT7996_MCU_EVT_OPTION] || packet[MT7996_MCU_EVT_OPTION + 1] ||
	    packet[MT7996_MCU_EVT_EXT_EID + 1] || packet[MT7996_MCU_EVT_EXT_EID + 2] ||
	    packet[MT7996_MCU_EVT_S2D_INDEX])
		return MT7996_FWDL_MALFORMED_REPLY;
	return packet[MT7996_MCU_ROM_REPLY_STATUS];
}
