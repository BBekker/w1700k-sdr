/* SPDX-License-Identifier: GPL-2.0-only */
/* Minimal environment for the SPI-NAND DMA error tails and ready-wait function
 * cut from the prepared U-Boot or Linux tree (see tests/test_platform_bmt.py). */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
typedef unsigned char u8;
static jmp_buf stopped;
static int panics, mode_off, status_value, read_error;
static void panic(const char *fmt, ...) __attribute__((noreturn));
static void panic(const char *fmt, ...) { (void)fmt; panics++; longjmp(stopped,1); }
#define SPI_MODE_MANUAL 0
#define STATUS_BUSY 1
#define SPINAND_WAITRDY_TIMEOUT_MS 3
struct spinand_device { void *spimem; u8 *scratchbuf; };
static int spinand_read_status(struct spinand_device *s, u8 *out)
{ (void)s; *out=status_value; return read_error; }
static int airoha_snand_set_mode(void *ctx, int mode)
{ (void)ctx; (void)mode; mode_off++; return 0; }
#ifdef TEST_UBOOT
static unsigned long ticks;
static void udelay(unsigned long n) { (void)n; ticks++; }
static void schedule(void) { }
static unsigned long get_timer(unsigned long start) { return ++ticks-start; }
#else
struct spi_mem_op { u8 *buf; };
#define REG_STATUS 0xc0
#define SPINAND_GET_FEATURE_1S_1S_1S_OP(reg,p) ((struct spi_mem_op){.buf=p})
static int poll_error;
static int spi_mem_poll_status(void *dev, struct spi_mem_op *op, int mask, int match,
    unsigned long initial, unsigned long delay, unsigned long timeout)
{ (void)dev;(void)mask;(void)match;(void)initial;(void)delay;(void)timeout;
  *op->buf=status_value; return poll_error; }
#endif
