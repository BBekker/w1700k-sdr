/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * MT7996 MCU command wire format: firmware download commands and their
 * replies.
 *
 * Layouts follow mt76 (mt76_connac_mcu.[ch], mt7996/mcu.c),
 * Copyright (C) 2022 MediaTek Inc. See LICENSE.mt76.
 *
 * These functions only encode and parse bytes; the caller owns sequencing,
 * DMA and timeouts. Encoders return the packet length or a negative
 * MT7996_FWDL_* error and leave the output untouched on error.
 */
#ifndef MT7996_FWDL_WIRE_H
#define MT7996_FWDL_WIRE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif
#include "hw/mt7996/fw_format.h"
#include "hw/mt7996/mcu_cmd.h"

#define MT7996_FWDL_COMMAND_HEADER MT7996_MCU_TXD_BYTES	/* mt76_connac2_mcu_txd */
#define MT7996_FWDL_INIT_BYTES MT7996_MCU_INIT_BYTES
#define MT7996_FWDL_START_BYTES MT7996_MCU_START_BYTES
#define MT7996_FWDL_PATCH_BYTES MT7996_MCU_PATCH_BYTES
#define MT7996_FWDL_SCATTER_MAX MT7996_MCU_CMD_MAX_BYTES	/* one TX descriptor */
/* RX prefix + legacy event header */
#define MT7996_FWDL_REPLY_BYTES MT7996_MCU_ROM_REPLY_BYTES

/* Download command ids (mt76_connac_mcu.h). */
#define MT7996_FWDL_CMD_TARGET_ADDRESS_LEN MCU_CMD_TARGET_ADDRESS_LEN_REQ
#define MT7996_FWDL_CMD_FW_START MCU_CMD_FW_START_REQ
#define MT7996_FWDL_CMD_PATCH_START MCU_CMD_PATCH_START_REQ
#define MT7996_FWDL_CMD_PATCH_FINISH MCU_CMD_PATCH_FINISH_REQ
#define MT7996_FWDL_CMD_PATCH_SEM MCU_CMD_PATCH_SEM_CONTROL

/* Destinations that take PATCH_START_REQ instead of TARGET_ADDRESS_LEN_REQ. */
#define MT7996_FWDL_PATCH_ADDRESS MT7996_FW_PATCH_ADDR_LEGACY
#define MT7996_FWDL_PATCH_ADDRESS_799X MT7996_FW_PATCH_ADDR

/* PATCH_SEM_CONTROL operand and reply status. Statuses are outcomes, not
 * pass/fail: IS_DL means the patch is already present.
 */
#define MT7996_FWDL_SEM_RELEASE MCU_PATCH_SEM_RELEASE
#define MT7996_FWDL_SEM_GET MCU_PATCH_SEM_GET
#define MT7996_FWDL_SEM_NOT_DL_FAIL MCU_PATCH_SEM_NOT_DL_FAIL
#define MT7996_FWDL_SEM_IS_DL MCU_PATCH_SEM_IS_DL
#define MT7996_FWDL_SEM_NOT_DL_SUCCESS MCU_PATCH_SEM_NOT_DL_SUCCESS
#define MT7996_FWDL_SEM_REL_SUCCESS MCU_PATCH_SEM_REL_SUCCESS

/* Reply event ids: init and start answer MCU_EVENT_GENERIC, the semaphore
 * MCU_EVENT_MT_PATCH_SEM. ANY skips the check.
 */
#define MT7996_FWDL_EID_ANY 0x00U
#define MT7996_FWDL_EID_GENERIC MCU_EVENT_GENERIC
#define MT7996_FWDL_EID_PATCH_SEM MCU_EVENT_MT_PATCH_SEM

/* FW_START_REQ option: OVERRIDE when an entry address is given, plus the
 * working-PDA bit naming the processor being started (none for the WM).
 */
#define MT7996_FWDL_START_OVERRIDE MCU_FW_START_OVERRIDE
#define MT7996_FWDL_START_PDA_CR4 MCU_FW_START_WORKING_PDA_CR4
#define MT7996_FWDL_START_PDA_DSP MCU_FW_START_WORKING_PDA_DSP

enum mt7996_fwdl_result {
	MT7996_FWDL_INVALID = -1,
	MT7996_FWDL_CAPACITY = -2,
	MT7996_FWDL_MALFORMED_REPLY = -3,
	MT7996_FWDL_UNRELATED_REPLY = -4,
};

/* Next command sequence number: cyclic 1..15, 0 before the first. Every
 * submission, including each raw scatter chunk, consumes one.
 */
int mt7996_fwdl_next_seq(uint32_t previous);

/* The command id an init for `address` carries. */
uint32_t mt7996_fwdl_init_command(uint32_t address);

/* INIT for one region: {address, length, mode}, where mode is NEED_RSP plus
 * the encryption bits derived from feature_set (mt76 FW_FEATURE_*: bit 0
 * encrypt, bits 2:1 key index, bit 4 encryption mode, bit 5 start entry).
 */
int mt7996_fwdl_init(uint8_t *out, size_t capacity, uint32_t seq,
		     uint32_t address, uint32_t length, uint32_t feature_set);

/* PATCH_SEM_CONTROL with MT7996_FWDL_SEM_GET or _RELEASE. */
int mt7996_fwdl_patch_sem(uint8_t *out, size_t capacity, uint32_t seq,
			  uint32_t get);

/* PATCH_FINISH_REQ; check_crc is 0 or 1 (mt76 sends 0). */
int mt7996_fwdl_patch_finish(uint8_t *out, size_t capacity, uint32_t seq,
			     uint32_t check_crc);

/* FW_START_REQ. A nonzero entry sets OVERRIDE; working_pda is 0,
 * MT7996_FWDL_START_PDA_CR4 (WA) or MT7996_FWDL_START_PDA_DSP.
 */
int mt7996_fwdl_start(uint8_t *out, size_t capacity, uint32_t seq,
		      uint32_t entry, uint32_t working_pda);

/* Length of the next headerless scatter chunk at `offset`, 0 at the end. */
int mt7996_fwdl_scatter_length(uint32_t total, uint32_t offset);

/*
 * Status byte of a download-command reply, including its 32-byte RX prefix.
 * The packet must be exactly 44 bytes, an MCU event (RXD0 type 7, pkt type
 * 0x0007) with the expected sequence and, unless MT7996_FWDL_EID_ANY, event
 * id. Returns 0..255 or a negative MT7996_FWDL_* error.
 */
int mt7996_fwdl_reply_status(const uint8_t *packet, size_t bytes,
			     uint32_t expected_seq, uint32_t expected_eid);

#endif
