/* SPDX-License-Identifier: GPL-2.0-only */
/* Host adapters for the actual mtdchar MEMREAD ioctl, not a rewritten loop. */
#define noinline_for_stack
#define __user
#define check_add_overflow(a,b,c) __builtin_add_overflow(a,b,c)
#define max(a,b) ((a)>(b)?(a):(b))
#define kvmalloc(n,f) test_alloc(n)
#define kvfree(p) free(p)
struct mtd_read_req {
 u64 start,len,ooblen,usr_data,usr_oob;
 u8 mode;
 struct { u32 uncorrectable_errors,corrected_bitflips,max_bitflips; } ecc_stats;
};
static struct mtd_info *mtd_get_master(struct mtd_info *d) { return d; }
static int copy_from_user(void *to,const void *from,size_t n)
{ if(n) memcpy(to,from,n);return 0; }
#define copy_to_user copy_from_user
static unsigned mtd_mod_by_ws(u64 n,struct mtd_info *d) { return n%d->writesize; }
static int mtd_is_bitflip_or_eccerr(int rc) { return rc==-EUCLEAN || rc==-EBADMSG; }
static int mtd_read_oob(struct mtd_info *d,loff_t a,struct mtd_oob_ops *o)
{
 memset(o->stats,0,sizeof *o->stats);
 int rc=d->_read_oob(d,a,o);
 o->stats->uncorrectable_errors=rc==-EBADMSG;
 o->stats->corrected_bitflips=rc==-EUCLEAN;
 o->stats->max_bitflips=rc>0 ? rc : 0;
 return rc;
}
