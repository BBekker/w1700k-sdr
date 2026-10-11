/* SPDX-License-Identifier: GPL-2.0-only */
static void lifecycle(void)
{
 struct spi_device spi={0};struct spi_mem mem={.spi=&spi};
 reset();missing=1;registrations=cleaned=0;
 assert(spinand_probe(&mem)<0);assert(registrations==0 && cleaned==1 && !layer.mtd);
 reset();fail_alloc=1;assert(spinand_probe(&mem)==-ENOMEM);assert(registrations==0 && !layer.mtd);
 reset();registration_failure=1;assert(spinand_probe(&mem)==-EIO);assert(!layer.mtd);
 reset();registration_failure=0;assert(spinand_probe(&mem)==0);
 unregister_failure=1;assert(spinand_remove(&mem)==-EBUSY);assert(layer.mtd==&chip.mtd);
 unregister_failure=0;assert(spinand_remove(&mem)==0);assert(!layer.mtd);
 assert(chip.mtd._read_oob==driver_read);
 assert(spinand_probe(&mem)==0);assert(spinand_remove(&mem)==0);assert(!layer.mtd);
 puts("actual kernel probe/remove: registration mapping, fail-closed, unwind and rebind passed");
}
