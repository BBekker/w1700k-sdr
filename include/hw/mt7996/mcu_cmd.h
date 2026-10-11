/* SPDX-License-Identifier: MIT */
/*
 * MT7996 host <-> MCU command / event protocol.
 *
 * This is a PACKET format carried in WFDMA0 host rings (wfdma.h), not a
 * register block. Offsets are byte offsets into the packet as placed in the
 * DMA buffer; all multi-byte fields are little-endian unless stated.
 *
 * Rings:
 *   TX16 (FWDL)   raw FW_SCATTER payload, no header.
 *   TX17 (MCU_WM) download-era commands; the boot ROM answers on RX0.
 *   TX20 (MCU_WA) every command once firmware runs (WA forwards to WM by
 *                 s2d_index); running firmware answers on RX1.   [measured]
 *   Events are dispatched by packet type, not by ring.
 *
 * Generation: connac3 hardware TXD (HDR_FORMAT at dw1 bits 15:14) in front of
 * a connac2 MCU header. Accepted on silicon.                       [measured]
 *
 * Two command header forms:
 *   legacy  64 bytes: 32-byte HW TXD + 32-byte MCU header (8-bit cid)
 *   UNI     48 bytes: 32-byte HW TXD + 16-byte MCU header (16-bit cid,
 *                     option bitmap), body = 4-byte cid header + TLVs
 *                     {le16 tag, le16 len (incl. its 4 bytes), data}.
 */
#ifndef HW_MT7996_MCU_CMD_H
#define HW_MT7996_MCU_CMD_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------ */
/* Hardware TXD words 0..7 (first 32 bytes of every command)                */
/* ------------------------------------------------------------------------ */

#define MT7996_MCU_TXD_HW_BYTES			32u
/* dw0 = 0x41000000 | total length (TXD included).                   [measured] */
#define   MCU_TXD0_TX_BYTES			HW_GENMASK(15, 0)
#define   MCU_TXD0_ETH_TYPE_OFFSET		HW_GENMASK(22, 16)
#define   MCU_TXD0_PKT_FMT			HW_GENMASK(24, 23)
#define     MCU_TXD0_PKT_FMT_CT			0u
#define     MCU_TXD0_PKT_FMT_SF			1u
#define     MCU_TXD0_PKT_FMT_CMD		2u
#define     MCU_TXD0_PKT_FMT_FW			3u
#define   MCU_TXD0_Q_IDX			HW_GENMASK(31, 25)
#define     MCU_TXD0_Q_IDX_MCU_RX_Q0		0x20u
#define     MCU_TXD0_Q_IDX_MCU_RX_Q1		0x21u
#define     MCU_TXD0_Q_IDX_MCU_RX_Q2		0x22u
#define     MCU_TXD0_Q_IDX_MCU_RX_Q3		0x23u
#define     MCU_TXD0_Q_IDX_MCU_RX_FWDL		0x3eu
#define   MCU_TXD0_CMD_BASE			0x41000000u	/* Q_IDX 0x20, PKT_FMT CMD */
/* dw1: connac3 HDR_FORMAT 15:14 = 1 (CMD) -> 0x00004000.          [measured]
 * The stock vendor host instead writes 0x80010000 (connac2 HDR_FORMAT 17:16
 * = 1 plus bit 31); firmware accepts that form as well.             [static] */
#define   MCU_TXD1_HDR_FORMAT			HW_GENMASK(15, 14)
#define     MCU_TXD1_HDR_FORMAT_802_3		0u
#define     MCU_TXD1_HDR_FORMAT_CMD		1u
#define     MCU_TXD1_HDR_FORMAT_802_11		2u
#define   MCU_TXD1_CMD				0x00004000u
#define   MCU_TXD1_CMD_STOCK			0x80010000u
/* dw2..dw7 zero. */

/* ------------------------------------------------------------------------ */
/* Legacy MCU header (bytes 32..63)                                         */
/* ------------------------------------------------------------------------ */

#define MT7996_MCU_TXD_BYTES			64u
#define MT7996_MCU_TXD_LEN			0x20u	/* le16 total - 32 */
#define MT7996_MCU_TXD_PQ_ID			0x22u	/* le16 (port << 15) | (queue << 10) */
#define   MCU_TXD_PQ_ID_MCU_RX_Q0		0x8000u	/* port 1 (MCU), queue 0x20 */
#define MT7996_MCU_TXD_CID			0x24u	/* u8 command id */
#define MT7996_MCU_TXD_PKT_TYPE			0x25u	/* u8, always 0xa0 */
#define   MCU_TXD_PKT_TYPE_CMD			0xa0u
#define MT7996_MCU_TXD_SET_QUERY		0x26u	/* u8 */
#define   MCU_TXD_SET_QUERY_QUERY		0u
#define   MCU_TXD_SET_QUERY_SET			1u
#define   MCU_TXD_SET_QUERY_NA			3u	/* download and plain commands */
#define MT7996_MCU_TXD_SEQ			0x27u	/* u8 token 1..15 */
#define MT7996_MCU_TXD_D2B0_REV			0x28u	/* u8, 0 */
#define MT7996_MCU_TXD_EXT_CID			0x29u	/* u8, real id when cid = 0xed */
#define MT7996_MCU_TXD_S2D_INDEX		0x2au	/* u8, MCU_S2D_* */
#define MT7996_MCU_TXD_EXT_CID_ACK		0x2bu	/* u8, 1 = reply requested */
/* 0x2c..0x3f reserved, zero. Extended form: cid 0xed, ext_cid = id,
 * ext_cid_ack 1, set_query QUERY/SET. */

/* ------------------------------------------------------------------------ */
/* UNI MCU header (bytes 32..47)                                            */
/* ------------------------------------------------------------------------ */

#define MT7996_MCU_UNI_TXD_BYTES		48u
#define MT7996_MCU_UNI_TXD_LEN			0x20u	/* le16 total - 32 */
#define MT7996_MCU_UNI_TXD_CID			0x22u	/* le16 UNI command id */
#define MT7996_MCU_UNI_TXD_PKT_TYPE		0x25u	/* u8 0xa0 */
#define MT7996_MCU_UNI_TXD_FRAG_N		0x26u	/* u8 (frag_num << 4) | frag_total; 0 */
#define MT7996_MCU_UNI_TXD_SEQ			0x27u	/* u8 token 1..15 */
#define MT7996_MCU_UNI_TXD_CHECKSUM		0x28u	/* le16, 0 = none */
#define MT7996_MCU_UNI_TXD_S2D_INDEX		0x2au	/* u8 */
#define MT7996_MCU_UNI_TXD_OPTION		0x2bu	/* u8 */
#define   MCU_UNI_OPTION_ACK			HW_BIT(0)	/* request reply */
#define   MCU_UNI_OPTION_UNI			HW_BIT(1)	/* unified command */
#define   MCU_UNI_OPTION_SET			HW_BIT(2)	/* 0 = query */
#define   MCU_UNI_OPTION_SET_ACK		0x07u
#define   MCU_UNI_OPTION_QUERY_ACK		0x03u
#define   MCU_UNI_OPTION_SET_NOACK		0x06u
#define   MCU_UNI_OPTION_QUERY_NOACK		0x02u
/* 0x24 and 0x2c..0x2f reserved. */
#define MT7996_MCU_UNI_BODY_HDR_BYTES		4u	/* cid-specific, usually zero */
#define MT7996_MCU_UNI_TLV_HDR_BYTES		4u	/* le16 tag, le16 len (incl. hdr) */

/* Source/destination index (both header forms).                    [static] */
#define MCU_S2D_H2N				0u	/* host -> WM */
#define MCU_S2D_C2N				1u	/* WA -> WM */
#define MCU_S2D_H2C				2u	/* host -> WA */
#define MCU_S2D_H2CN				3u	/* host -> WA and WM */
/* No DSP destination exists; the DSP has no command path. */

/*
 * Sequence token: 4 bits, 1..15 cyclic, 0 skipped. Every submitted command
 * AND every FW_SCATTER consumes one. The firmware attributes replies by token
 * alone; a reused token is ambiguous. Body limit per descriptor: 4096 bytes
 * including the header.                                     [static, measured]
 * Unsolicited events (e.g. capture data) carry token 0.             [measured]
 */
#define MT7996_MCU_SEQ_MIN			1u
#define MT7996_MCU_SEQ_MAX			15u
#define MT7996_MCU_CMD_MAX_BYTES		4096u

/* ------------------------------------------------------------------------ */
/* Event (RX) header                                                        */
/* ------------------------------------------------------------------------ */

#define MT7996_MCU_RXD_HW_BYTES			32u
/* rxd dw0: byte count 15:0 (whole packet), packet type 31:27. */
#define   MCU_RXD0_RX_BYTES			HW_GENMASK(15, 0)
#define   MCU_RXD0_PKT_TYPE			HW_GENMASK(31, 27)
#define     MCU_RXD0_PKT_TYPE_TXS		0u
#define     MCU_RXD0_PKT_TYPE_TXRXV		1u
#define     MCU_RXD0_PKT_TYPE_NORMAL		2u	/* 802.11 frame */
#define     MCU_RXD0_PKT_TYPE_RX_DUP_RFB	3u
#define     MCU_RXD0_PKT_TYPE_RX_TMR		4u
#define     MCU_RXD0_PKT_TYPE_RETRIEVE		5u
#define     MCU_RXD0_PKT_TYPE_TXRX_NOTIFY	6u
#define     MCU_RXD0_PKT_TYPE_RX_EVENT		7u	/* MCU event */
#define     MCU_RXD0_PKT_TYPE_NORMAL_MCU	8u
#define     MCU_RXD0_PKT_TYPE_FW_MONITOR	0x0cu
#define MT7996_MCU_EVT_LEN			0x20u	/* le16 total - 32 */
#define MT7996_MCU_EVT_PKT_TYPE_ID		0x22u	/* le16 */
#define MT7996_MCU_EVT_EID			0x24u	/* u8 event id */
#define MT7996_MCU_EVT_SEQ			0x25u	/* u8 token of the command */
#define MT7996_MCU_EVT_OPTION			0x26u	/* u8 */
#define   MCU_EVT_OPTION_UNI			HW_BIT(1)
#define   MCU_EVT_OPTION_UNSOLICITED		HW_BIT(2)
#define MT7996_MCU_EVT_EXT_EID			0x28u	/* u8 */
#define MT7996_MCU_EVT_S2D_INDEX		0x2bu	/* u8, 0 from WM, 2 from WA */
#define MT7996_MCU_EVT_BODY			0x2cu	/* 44: event payload */

/*
 * ROM download reply: exactly 44 bytes, rx_bytes 44, event len 12,
 * pkt_type_id 0x0007, bytes 38/39/41/42/43 zero, STATUS at byte 40 (0 = ok).
 *                                                                 [measured]
 */
#define MT7996_MCU_ROM_REPLY_BYTES		44u
#define MT7996_MCU_ROM_REPLY_PKT_TYPE_ID	0x0007u
#define MT7996_MCU_ROM_REPLY_STATUS		40u
#define   MT7996_MCU_ROM_REPLY_STATUS_OK	0u

/* UNI result event (eid 0x01) body: le16 cid echo, 2 reserved, le32 status. */
#define MT7996_MCU_UNI_RESULT_CID		0u
#define MT7996_MCU_UNI_RESULT_STATUS		4u
/* Firmware status words.                                    [measured, static] */
#define MCU_STATUS_OK				0x00000000u
#define MCU_STATUS_INVALID_PARAM		0xc0000001u	/* bad length / parameters */
#define MCU_STATUS_UNSUPPORTED			0xc00000bbu	/* unknown cid / function */
#define MCU_STATUS_TM_SAME_MODE			0x00010003u	/* op_mode already current */

/* ------------------------------------------------------------------------ */
/* Legacy command ids                                                       */
/* ------------------------------------------------------------------------ */

/* Download / ROM commands (legacy header, TX17, s2d 0, set_query NA).
 *                                                         [static, measured] */
#define MCU_CMD_TARGET_ADDRESS_LEN_REQ		0x01u	/* INIT: {addr, len, mode} */
#define MCU_CMD_FW_START_REQ			0x02u	/* {option, entry} -- option FIRST */
#define MCU_CMD_INIT_ACCESS_REG			0x03u	/* ROM pre-auth register access */
#define MCU_CMD_NIC_POWER_CTRL			0x04u
#define MCU_CMD_PATCH_START_REQ			0x05u	/* INIT to a patch destination */
#define MCU_CMD_PATCH_FINISH_REQ		0x07u	/* {u8 check_crc = 0, 3 zero} */
#define MCU_CMD_PATCH_SEM_CONTROL		0x10u	/* {le32 op} */
#define MCU_CMD_WA_PARAM			0xc4u	/* WA "CR4" param, s2d 2 */
#define MCU_CMD_EXT_CID				0xedu	/* extended command */
#define MCU_CMD_FW_SCATTER			0xeeu	/* payload on TX16, no header */
#define MCU_CMD_RESTART_DL_REQ			0xefu

/* INIT payload (76-byte packet): +64 le32 addr, +68 le32 len, +72 le32 mode.
 * Patch destinations 0x00200000 / 0x00900000 select PATCH_START_REQ.
 *                                                         [static, measured] */
#define MT7996_MCU_INIT_BYTES			76u
#define MT7996_MCU_INIT_ADDR			0x40u	/* le32 destination */
#define MT7996_MCU_INIT_LEN			0x44u	/* le32 bytes */
#define MT7996_MCU_INIT_MODE			0x48u	/* le32, MCU_DL_MODE_* */
#define   MCU_DL_MODE_ENCRYPT			HW_BIT(0)
#define   MCU_DL_MODE_KEY_IDX			HW_GENMASK(2, 1)
#define   MCU_DL_MODE_RESET_SEC_IV		HW_BIT(3)
#define   MCU_DL_MODE_WORKING_PDA_CR4		HW_BIT(4)	/* WA and DSP regions */
#define   MCU_DL_MODE_VALID_RAM_ENTRY		HW_BIT(5)
#define   MCU_DL_MODE_ENCRY_MODE_SEL		HW_BIT(6)	/* connac3 encryption mode */
#define   MCU_DL_MODE_NEED_RSP			HW_BIT(31)
/* Region feature bit 0 -> ENCRYPT | RESET_SEC_IV; bits 2:1 -> KEY_IDX;
 * bit 4 -> ENCRY_MODE_SEL. Stock encrypted WM region: 0x80000009. */

/* FW_START_REQ payload (72-byte packet): +64 le32 option, +68 le32 entry.
 *                                                         [static, measured] */
#define MT7996_MCU_START_BYTES			72u
#define MT7996_MCU_START_OPTION			0x40u	/* le32, MCU_FW_START_* */
#define MT7996_MCU_START_ADDR			0x44u	/* le32 entry */
#define   MCU_FW_START_OVERRIDE			HW_BIT(0)	/* entry address valid */
#define   MCU_FW_START_WORKING_PDA_CR4		HW_BIT(2)	/* starting the WA */
#define   MCU_FW_START_WORKING_PDA_DSP		HW_BIT(3)	/* starting the DSP */
/*
 * FW_START status 2 = the image failed the ROM's verification. Any extra
 * scatter (INIT + FW_SCATTER to a destination of the host's choosing) issued
 * before a START falls inside that START's verification scope and makes it
 * return 2. Download commands are only accepted while FW_STATE == 1; after
 * the firmware is up the same sequence fails without any TX completing.
 *                                                                 [measured]
 * HAZARD: a UNI command that times out stalls the mailbox: every later
 *         command gets no reply until the WF subsystem reset and a fresh
 *         download. Treat an unanswered command as consuming the session.
 *                                                                 [measured]
 */
#define MCU_FW_START_STATUS_VERIFY_FAIL		2u

/* PATCH_SEM_CONTROL op and reply outcomes (reply eid 0x04). A nonzero reply
 * is an OUTCOME, not an error.                               [static, measured] */
#define MT7996_MCU_PATCH_BYTES			68u
#define MT7996_MCU_PATCH_SEM_OP			0x40u	/* le32, MCU_PATCH_SEM_GET / _RELEASE */
#define MT7996_MCU_PATCH_FINISH_CHECK_CRC	0x40u	/* u8, then 3 zero bytes */
#define MCU_PATCH_SEM_RELEASE			0u
#define MCU_PATCH_SEM_GET			1u
#define MCU_PATCH_SEM_NOT_DL_FAIL		0u	/* semaphore not obtained */
#define MCU_PATCH_SEM_IS_DL			1u	/* patch already present: skip regions */
#define MCU_PATCH_SEM_NOT_DL_SUCCESS		2u	/* granted: download the patch */
#define MCU_PATCH_SEM_REL_SUCCESS		3u	/* released */

/*
 * INIT_ACCESS_REG (ROM, before authentication): reads dereference any
 * address; writes are performed only while secure gate 0x74140024 bit 6 is
 * clear, otherwise status 7.                                         [static]
 * Any cid without a ROM handler is dispatched through a writable ROM data
 * pointer (default-handler slot) with the packet in a0, before authentication;
 * FW_SCATTER can write any destination pre-authentication.  [static, measured]
 */
#define MCU_ROM_ACCESS_REG_DENIED		7u

/* Legacy event ids (download path).                                  [static] */
#define MCU_EVENT_GENERIC			0x01u	/* TARGET_ADDRESS_LEN, FW_START */
#define MCU_EVENT_ACCESS_REG			0x02u
#define MCU_EVENT_MT_PATCH_SEM			0x04u
#define MCU_EVENT_REG_ACCESS			0x05u
#define MCU_EVENT_EXT				0xedu
#define MCU_EVENT_RESTART_DL			0xefu
#define MCU_EVENT_COREDUMP			0xf0u

/* Legacy runtime: WA_PARAM ext 1 SET {le32 option, le32 value, le32 0};
 * extended (0xed) ids seen: 0x51 AUTO_BA, 0x52 MCAST_CLONE, 0x6c PKT_BUDGET,
 * 0x75 CP_SUPPORT, 0x80 A4, 0xa3 RDD_IPI_HIST.                       [static] */
#define MCU_EXT_CMD_RF_TEST			0x04u
#define MCU_EXT_CMD_RDD_IPI_HIST		0xa3u

/* ------------------------------------------------------------------------ */
/* UNI command ids                                                          */
/* ------------------------------------------------------------------------ */

#define MCU_UNI_CMD_DEV_INFO_UPDATE		0x01u
#define MCU_UNI_CMD_BSS_INFO_UPDATE		0x02u
#define MCU_UNI_CMD_STA_REC_UPDATE		0x03u
#define MCU_UNI_CMD_EDCA_UPDATE			0x04u
#define MCU_UNI_CMD_BAND_CONFIG			0x08u
#define MCU_UNI_CMD_WSYS_CONFIG			0x0bu
#define MCU_UNI_CMD_REG_ACCESS			0x0du
#define MCU_UNI_CMD_CHIP_CONFIG			0x0eu
#define MCU_UNI_CMD_POWER_CTRL			0x0fu
#define MCU_UNI_CMD_RX_HDR_TRANS		0x12u
#define MCU_UNI_CMD_SER				0x13u
#define MCU_UNI_CMD_SCAN_REQ			0x16u
#define MCU_UNI_CMD_RDD_CTRL			0x19u
#define MCU_UNI_CMD_TXPOWER			0x2bu
#define MCU_UNI_CMD_EFUSE_CTRL			0x2du
#define MCU_UNI_CMD_RA				0x2fu
#define MCU_UNI_CMD_SPECTRUM			0x30u	/* vendor capture command */
#define MCU_UNI_CMD_MURU			0x31u
#define MCU_UNI_CMD_TESTMODE_RX_STAT		0x32u
#define MCU_UNI_CMD_BF				0x33u
#define MCU_UNI_CMD_CHANNEL_SWITCH		0x34u
#define MCU_UNI_CMD_THERMAL			0x35u
#define MCU_UNI_CMD_VOW				0x37u
#define MCU_UNI_CMD_TESTMODE_CTRL		0x46u
#define MCU_UNI_CMD_RRO				0x57u
#define MCU_UNI_CMD_OFFCH_SCAN_CTRL		0x58u
#define MCU_UNI_CMD_EXT_EEPROM_CTRL		0x74u

/* UNI event ids.                                            [static, measured] */
#define MCU_UNI_EVENT_RESULT			0x01u
#define MCU_UNI_EVENT_FW_LOG_2_HOST		0x04u
#define MCU_UNI_EVENT_ACCESS_REG		0x06u
#define MCU_UNI_EVENT_SCAN_DONE			0x0eu
#define MCU_UNI_EVENT_RDD_REPORT		0x11u
#define MCU_UNI_EVENT_SPECTRUM			0x30u
#define MCU_UNI_EVENT_THERMAL			0x35u
#define MCU_UNI_EVENT_NIC_CAPAB			0x43u
#define MCU_UNI_EVENT_TESTMODE_CTRL		0x46u

/* Reference reply timeouts: EFUSE_CTRL / BF / TESTMODE_CTRL 20 s,
 * EXT_EEPROM 30 s, everything else 3 s.                              [static] */

/* ------------------------------------------------------------------------ */
/* UNI 0x0d REG_ACCESS                                                      */
/* ------------------------------------------------------------------------ */
/*
 * Body: 4 reserved bytes then any mix of BASIC and RF TLVs; one reply slot per
 * TLV, in order, in event 0x06 (body: 4 reserved + slots). Option 0x03 =
 * read, 0x07 = WRITE of every TLV's value field (set/query is per command).
 * The firmware forwards host -> WA -> WM; s2d 0.             [measured, static]
 *
 * BASIC (tag 0), 12-byte TLV: +0 tag, +2 len 12, +4 le32 addr, +8 le32 value.
 * Reply slot 12 bytes {tag 0, len 12, addr echo, value}. Anchor a value on
 * the echoed address: an all-zero slot means REFUSED, and a malformed request
 * yields a well-formed reply full of zeros.                         [measured]
 * Stock WM serves only addresses in [0x70000000, 0xe0000000) (one range
 * check); up to 124 BASIC TLVs per query (reply cap 4 + 12 n <= 0x5db).
 *
 * RF (tag 1), 16-byte TLV: +0 tag 1, +2 len 16, +4 le16 idx, +6 reserved,
 * +8 le32 ofs (12-bit A-die register INDEX, not a byte address),
 * +12 le32 data. idx = (die << 4) | k: die 0..2 selects the A-die SPI master
 * (conn.h), k 0..4 = WF chain (4 = synthesizer bank), k 15 = TOP. Reply slot
 * 16 bytes {tag 1, len 16, idx, rsv, ofs, value}; up to 93 per query.
 * No SPI status is returned: a failed read yields 0.                [static]
 */
#define UNI_REG_ACCESS_TAG_BASIC		0u
#define UNI_REG_ACCESS_TAG_RF			1u
#define UNI_REG_ACCESS_BASIC_TLV_BYTES		12u
#define UNI_REG_ACCESS_RF_TLV_BYTES		16u
#define UNI_REG_ACCESS_MAX_BASIC		124u
#define UNI_REG_ACCESS_MAX_RF			93u
#define UNI_REG_ACCESS_SERVED_LO		0x70000000u
#define UNI_REG_ACCESS_SERVED_HI		0xe0000000u	/* exclusive */
#define   UNI_REG_ACCESS_RF_IDX_DIE		HW_GENMASK(5, 4)
#define   UNI_REG_ACCESS_RF_IDX_K		HW_GENMASK(3, 0)
#define     UNI_REG_ACCESS_RF_K_TOP		15u
#define   UNI_REG_ACCESS_RF_OFS_INDEX		HW_GENMASK(11, 0)
/*
 * HAZARD: option 0x07 on a "read" body WRITES the value field (zero) to every
 *         address in it. Never send SET without explicit values.  [measured]
 * HAZARD: RF idx with k 5..14, idx 0 or idx > 47 silently addresses die 0
 *         chip-select 1 instead of failing.                          [static]
 * HAZARD: a BASIC read of CONN semaphore 12 (bus 0x7c070030) ACQUIRES the
 *         A-die SPI semaphore; every later firmware RF access then fails.
 *         Never read it.                                             [static]
 * HAZARD: with the range check removed, reads of unmapped MCU memory can
 *         fault the WM (recover with the WF subsystem reset).       [measured]
 * HAZARD: A-die TOP index 0x222 (die 1) can wedge that die's SPI read path
 *         past a WF subsystem reset; only a power cycle cleared it. Do not
 *         sweep TOP 0x200..0x2ff.                                   [measured]
 */

/* ------------------------------------------------------------------------ */
/* UNI 0x2d EFUSE_CTRL                                                      */
/* ------------------------------------------------------------------------ */
/*
 * Tag 1 ACCESS, 32-byte body: +4 tag 1, +6 len 0x1c, +8 le32 addr
 * (16-byte block, addr & ~0xf), +12 le32 valid, +16 16 data bytes. Option 0x03
 * reads one block; reply body {rsv4, tag, len, version, addr, valid, size,
 * magic_no, type, rsv[4], data[16]}.                         [static, measured]
 * Block 0x2a0 (logical) carries the FT-recovery gate word, physical 0x2b0
 * (fw_format.h).                                                     [static]
 * Tag 2 BUFFER_MODE loads the EEPROM image into firmware RAM (not a burn):
 * 0x40c-byte body, 0x400 data bytes per command. Tag 3 FREE_BLOCK, tag 4
 * BUFFER_RD.                                                         [static]
 *
 * HAZARD: EFUSE BURN. The ACCESS write is byte-identical to the read except
 *         option 0x07. Tag 1 with the SET bit PROGRAMS eFuse irreversibly.
 *         Only ever send tag 1 with option 0x03.                     [static]
 */
#define UNI_EFUSE_TAG_ACCESS			1u
#define UNI_EFUSE_TAG_BUFFER_MODE		2u
#define UNI_EFUSE_TAG_FREE_BLOCK		3u
#define UNI_EFUSE_TAG_BUFFER_RD			4u
#define UNI_EFUSE_ACCESS_TLV_LEN		0x1cu
#define UNI_EFUSE_BLOCK_BYTES			16u
#define UNI_EFUSE_FT_GATE_BLOCK			0x2a0u

/* ------------------------------------------------------------------------ */
/* UNI 0x0f POWER_CTRL                                                      */
/* ------------------------------------------------------------------------ */
/* Tag 0 POWER_OFF {u8 power_mode = 1, 3 rsv}: firmware restart back to the
 * download state (FW_STATE 1); reference driver sends it unacked.    [static] */
#define UNI_POWER_TAG_OFF			0u

/* ------------------------------------------------------------------------ */
/* UNI 0x34 CHANNEL_SWITCH                                                  */
/* ------------------------------------------------------------------------ */
/*
 * TLV (tag 0 SWITCH or 1 RX_PATH, len 76 / stock 0x50), offsets from TLV
 * start: +4 control_ch, +5 center_ch, +6 bw (CMD_CBW), +7 tx_path_num,
 * +8 rx (tag 0: stream count; tag 1: path bitmap), +9 switch_reason,
 * +10 band_idx, +11 center_ch2, +12 le16 cac_case, +14 channel_band
 * (0 2.4 / 1 5 / 2 6 GHz), +16 le32 outband_freq (0), +20 txpower_drop,
 * +21 ap_bw, +22 ap_center_ch; stock tag 0 appends SKU bytes.        [static]
 *
 * HAZARD: RF TRANSMIT. A firmware channel switch (and any radio reload)
 *         can emit a full-scale burst from the PA (measured +1 dBFS on an
 *         instrument cabled to the antenna port). Disconnect cabled
 *         instruments first.                                        [measured]
 * HAZARD: an RX path mask of 0x10 (monitor chain) crashes the firmware;
 *         recover by reload.                                        [measured]
 * Firmware recalibrates on a switch unless reason SCAN_BYPASS_DPD or the
 * test-mode CAL_BITMAP skip bits are set.                            [static]
 */
#define UNI_CHSW_TAG_SWITCH			0u
#define UNI_CHSW_TAG_RX_PATH			1u
#define UNI_CHSW_CBW_20				0u
#define UNI_CHSW_CBW_40				1u
#define UNI_CHSW_CBW_80				2u
#define UNI_CHSW_CBW_160			3u
#define UNI_CHSW_CBW_320			7u
#define UNI_CHSW_REASON_NORMAL			0u
#define UNI_CHSW_REASON_SCAN			3u
#define UNI_CHSW_REASON_DFS			5u
#define UNI_CHSW_REASON_SCAN_BYPASS_DPD		9u
#define UNI_CHSW_REASON_THERMAL_RECAL		10u
#define UNI_CHSW_REASON_RECAL_TRIGGER		11u
#define UNI_CHSW_REASON_TEST_MODE_RECAL		13u

/* ------------------------------------------------------------------------ */
/* UNI 0x46 TESTMODE_CTRL                                                   */
/* ------------------------------------------------------------------------ */
/*
 * Tag 0 RF_TEST_CTRL. Offsets from TLV start: +4 action, +5 icap_len,
 * +6 icap_attr, +8 le32 op_mode / func_idx, +12 function data.
 * TLV length is firmware-build dependent: the stock TESTMODE WM requires
 * 92 (100 returns INVALID_PARAM); the vendor TESTMODE WM requires 100.
 * The release (non-testmode) WM has no handler (UNSUPPORTED).  [measured, static]
 * Replies: eid 0x46.
 */
#define UNI_TM_TAG_RF_TEST_CTRL			0u
#define UNI_TM_TLV_LEN_STOCK			92u
#define UNI_TM_TLV_LEN_VENDOR			100u
#define UNI_TM_ACTION_SWITCH			0u	/* op_mode below */
#define UNI_TM_ACTION_IN_RFTEST			1u	/* func_idx 0x01..0x15 */
#define UNI_TM_ACTION_SET			3u	/* RF_AT {func, value} */
#define UNI_TM_ACTION_GET			4u
#define UNI_TM_OPMODE_NORMAL			0u
#define UNI_TM_OPMODE_RFTEST			1u
#define UNI_TM_OPMODE_ICAP			2u
#define UNI_TM_OPMODE_ICAP_OVERLAP		3u
#define UNI_TM_OPMODE_SPECTRUM			4u
/* RF_AT function ids used with ACTION_SET (value at +12 of the data).
 *                                                                   [static] */
#define UNI_TM_AT_COMMAND			1u	/* UNI_TM_CMD_* */
#define UNI_TM_AT_CHNL_FREQ			18u	/* kHz */
#define UNI_TM_AT_WRITE_EFUSE			67u	/* HAZARD: eFuse burn */
#define UNI_TM_AT_CBW				71u
#define UNI_TM_AT_DBW				72u
#define UNI_TM_AT_PRIMARY_CH			73u
#define UNI_TM_AT_BAND				90u
#define UNI_TM_AT_DBDC_BAND_IDX			104u
#define UNI_TM_AT_RX_PATH			106u	/* mask in bits 20:16 */
#define UNI_TM_AT_ANT_MASK			115u
#define UNI_TM_AT_CAL_BITMAP			125u	/* b20 full/none, b21 skip TX, b22 skip DPD */
#define UNI_TM_CMD_STOP				0u
#define UNI_TM_CMD_START_TX			1u	/* HAZARD: transmits */
#define UNI_TM_CMD_START_RX			2u
#define UNI_TM_CMD_CW				10u	/* HAZARD: transmits */
#define UNI_TM_CMD_SINGLE_TONE			15u	/* HAZARD: transmits */
#define UNI_TM_CMD_TX_COMMIT			18u
#define UNI_TM_CMD_RX_COMMIT			19u
/* HAZARD: RF TRANSMIT -- COMMAND 1/5/6/10/15 and IN_RFTEST functions 0x03
 *         (TX_TONE_START) / 0x05 (CONT_TX_START) drive the PA.        [static]
 * HAZARD: EFUSE BURN -- RF_AT 67 writes eFuse.                         [static] */

/* ------------------------------------------------------------------------ */
/* UNI 0x30 SPECTRUM (capture)                                              */
/* ------------------------------------------------------------------------ */
/* Option 0x07, s2d 0, replies eid 0x30; dump data events arrive unsolicited
 * with token 0. Body/tag layout: spectrum_cmd.h.          [measured, static] */

/* ------------------------------------------------------------------------ */
/* UNI 0x19 RDD_CTRL and 0x58 OFFCH_SCAN_CTRL                               */
/* ------------------------------------------------------------------------ */
/* RDD_CTRL tag 0, TLV len 12 (required): +4 ctrl, +5 rdd_idx (0 band 2,
 * 1 band 1, 2 background/dedicated), +6 rx_sel, +7 value.   [measured, static] */
#define UNI_RDD_TLV_LEN				12u
#define UNI_RDD_CTRL_STOP			0u
#define UNI_RDD_CTRL_START			1u
#define UNI_RDD_CTRL_DET_MODE			2u
#define UNI_RDD_CTRL_CAC_START			50u
#define UNI_RDD_CTRL_CAC_END			51u
/* OFFCH_SCAN_CTRL tag 0, TLV len 0x14: +4 work ch, +5 central, +6 bw, +7 tx,
 * +8 rx, +9 monitor ch, +10 monitor central, +11 monitor bw (<= 3),
 * +12 mon tx, +13 mon rx, +14 scan_mode (0 stop, 1 start, 2 running),
 * +15 band_idx, +16 scan_type (0 off, 1 balanced, 2 simple RX), +17 band
 * (0 2.4, 1 5 GHz). Programs the dedicated monitor receiver PLL. Start with
 * OFFCH start then RDD START idx 2; stop in reverse.       [measured, static] */
#define UNI_OFFCH_TLV_LEN			0x14u
#define UNI_OFFCH_SCAN_TYPE_SIMPLE_RX		2u

/* Other UNI tags used: TXPOWER (0x2b) 12-byte {tag, 8, tag, value, band, 0}
 * with tag 6 = ATE mode on/off; TESTMODE_RX_STAT (0x32) tag 7 user ctrl,
 * tag 9 get all; CHIP_CONFIG (0x0e) tag 3 NIC capability (query, eid 0x43);
 * BAND_CONFIG (0x08) tag 0 radio on/off, tag 0x0c MAC enable.         [static] */
#define UNI_TXPOWER_TAG_ATEMODE_CTRL		6u

#endif /* HW_MT7996_MCU_CMD_H */
