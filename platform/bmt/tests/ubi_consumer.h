/* SPDX-License-Identifier: GPL-2.0-only */
/* Adapters for actual UBI I/O and the wear-leveling bad-PEB failure tail.
 * Debug self-checks are disabled as in the target; MTD calls use our binding. */
#define ubi_assert assert
#define dbg_io(...) ((void)0)
#define ubi_err(...) ((void)0)
#define ubi_warn(...) ((void)0)
#define ubi_msg(...) ((void)0)
#define dump_stack() ((void)0)
#define ubi_dump_flash(...) ((void)0)
#define self_check_not_bad(...) 0
#define ubi_self_check_all_ff(...) 0
#define self_check_peb_ec_hdr(...) 0
#define self_check_peb_vid_hdr(...) 0
#define self_check_write(...) 0
#define ubi_dbg_is_write_failure(...) 0
#define ubi_dbg_is_erase_failure(...) 0
#define ubi_calculate_reserved(...) ((void)0)
#define spin_lock(...) ((void)0)
#define spin_unlock(...) ((void)0)
#define ubi_ro_mode(u) ((u)->ro_mode=1)
#define yield() ((void)0)
#define UBI_IO_RETRIES 3
#define EINTR 4
#ifndef TEST_LINUX
/* include/linux/compat.h: synchronous U-Boot wait-queue compatibility. */
typedef int wait_queue_head_t;
#define init_waitqueue_head(p) ((void)(p))
#define wait_event_interruptible(...) 0
#endif
struct ubi_device {
 struct mtd_info *mtd;
 int peb_count,peb_size,hdrs_min_io_size,leb_start,ro_mode,bad_allowed;
 int beb_rsvd_pebs,avail_pebs,bad_peb_count,good_peb_count;
};
static unsigned erase_calls,write_calls,markbad_calls;
static size_t last_written;
static int mtd_write(struct mtd_info *dev,loff_t to,size_t len,size_t *written,const u8 *buf)
{
 struct mtd_oob_ops op={.mode=MTD_OPS_AUTO_OOB,.len=len,.datbuf=(u8 *)buf};
 write_calls++;
 int rc=dev->_write_oob(dev,64ull*BS+to,&op);
 *written=last_written=op.retlen;return rc;
}
static int mtd_erase(struct mtd_info *dev,struct erase_info *e)
{
 struct erase_info master=*e;master.addr+=64ull*BS;erase_calls++;
 int rc=dev->_erase(dev,&master);e->state=master.state;return rc;
}
static int mtd_block_markbad(struct mtd_info *dev,loff_t from)
{ markbad_calls++;return dev->_block_markbad(dev,64ull*BS+from); }
