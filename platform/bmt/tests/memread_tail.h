/* SPDX-License-Identifier: GPL-2.0-only */
static void memread(void)
{
 struct mtd_read_req req;
 reset();open_mtd();fault=-EBADMSG;partial=PS;
 req=(struct mtd_read_req){.start=5*BS,.len=5*PS,.usr_data=(uintptr_t)data};
 assert(mtdchar_read_ioctl(&m,&req)==-EBADMSG);
 assert(req.len==5*PS && calls==5 && req.ecc_stats.uncorrectable_errors==5);
 for(unsigned i=0;i<req.len;i++) {
   u64 a=5*BS+i;
   assert(data[i]==tag((physical(a/BS)*BS+a%BS)/PS,a%PS));
 }
 reset();open_mtd();fault=-EBADMSG;partial=0;
 req=(struct mtd_read_req){.len=PS,.usr_data=(uintptr_t)data};
 assert(mtdchar_read_ioctl(&m,&req)==-EIO && req.len==0 && calls==1);
 puts("actual Linux MEMREAD: ECC bytes delivered, five advancing calls, zero-progress stops in one call");
}
