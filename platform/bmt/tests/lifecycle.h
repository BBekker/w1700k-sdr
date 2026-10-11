/* SPDX-License-Identifier: GPL-2.0-only */
/* Minimal environment for the REAL spinand_probe/remove functions extracted
 * from the prepared kernel. Registration consumes the installed callbacks. */
#define IS_ENABLED(x) (x)
struct device { void *of_node; };
struct spi_device { struct device dev; };
struct spi_mem { struct spi_device *spi; void *data; };
struct spinand_device { struct mtd_info mtd;struct spi_mem *spimem;int lock; };
static struct spinand_device chip;
static int registrations, registration_failure, unregister_failure, cleaned;
static void *devm_kzalloc(struct device *d,size_t n,int f)
{ (void)d;(void)n;(void)f;memset(&chip,0,sizeof chip);return &chip; }
static void spi_mem_set_drvdata(struct spi_mem *s,void *p) { s->data=p; }
static void *spi_mem_get_drvdata(struct spi_mem *s) { return s->data; }
static void spinand_set_of_node(struct spinand_device *s,void *p) { (void)s;(void)p; }
static void mutex_init(int *p) { *p=0; }
static struct mtd_info *spinand_to_mtd(struct spinand_device *s) { return &s->mtd; }
static int spinand_init(struct spinand_device *s) { init_mtd(&s->mtd);return 0; }
static void spinand_cleanup(struct spinand_device *s) { (void)s;cleaned++; }
static void mtk_bmt_attach(struct mtd_info *d) { (void)d; }
static void mtk_bmt_detach(struct mtd_info *d) { (void)d; }
static int mtd_device_register(struct mtd_info *d,void *p,int n)
{
 (void)p;(void)n;registrations++;
 assert(layer.mtd==d);assert(d->_read_oob!=driver_read);
 /* Registration-time partition bad-block statistics must see the BBT shift:
  * physical 5 is bad; logical 5 is good physical 6. */
 assert(d->_block_isbad(d,5*BS)==0);
 return registration_failure ? -EIO : 0;
}
static int mtd_device_unregister(struct mtd_info *d)
{ assert(layer.mtd==d);return unregister_failure ? -EBUSY : 0; }
