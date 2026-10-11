// SPDX-License-Identifier: GPL-2.0-only
/* wfdma.c: register effects against mt76, waits, faults and the session states. */
#include "harness.h"
#include "wfdma.c"
#include "mt76_vectors.h"

struct trace { char op; u32 reg, value; };
static struct trace trace[4096];
static u32 registers[0x8000 / 4];
static unsigned int count, reads, writes, hif_reads, waits, ready_at;
static unsigned int fail_read, fail_write;
static u32 xor_read_reg, xor_read_bits;
static int wait_error;

static void record(char op, u32 reg, u32 value)
{
	CHECK(count < ARRAY_SIZE(trace));
	CHECK(!(reg & 3) && reg >= 0xd0000 && reg < 0xd8000);
	trace[count++] = (struct trace){ op, reg, value };
}

static u32 *regptr(u32 reg)
{
	return &registers[(reg - 0xd0000) / 4];
}

static u32 read_reg(void *ctx, u32 reg)
{
	u32 value = *regptr(reg);

	(void)ctx;
	if (reg == WFDMA_HIF_MISC) {
		hif_reads++;
		value = (value & ~1U) | (ready_at && hif_reads >= ready_at ? 0U : 1U);
	}
	if (++reads == fail_read)
		value = U32_MAX;
	if (reg == xor_read_reg)
		value ^= xor_read_bits;
	record('r', reg, value);
	return value;
}

static void write_reg(void *ctx, u32 reg, u32 value)
{
	(void)ctx;
	record('w', reg, value);
	if (++writes != fail_write)
		*regptr(reg) = value;
}

static int wait_poll(void *ctx)
{
	(void)ctx;
	waits++;
	return wait_error;
}

static const struct sdr_mmio_ops ops = { .read32 = read_reg, .write32 = write_reg };
static struct sdr_mmio io = { &ops, NULL, WFDMA_HIF_MISC + 4 };
static struct sdr_wfdma session;

static void reset_trace(void)
{
	count = reads = writes = hif_reads = waits = 0;
	fail_read = fail_write = 0;
	xor_read_reg = xor_read_bits = 0;
	wait_error = 0;
}

/* The register file mt76's traces start from. */
static void init(u32 pattern)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(registers); i++)
		registers[i] = pattern;
	*regptr(WFDMA_IRQ_MASK) = 0;
	*regptr(WFDMA_GLO_CFG) &= ~WFDMA_DISABLE_BITS;
	reset_trace();
	ready_at = 1;
}

/* Our writes, in order, equal mt76's; `skip_mask` drops our own interrupt
 * mask writes (mt76 masks interrupts elsewhere).
 */
static void compare_writes(const struct mt76_reg_write *want, unsigned int n, bool skip_mask)
{
	unsigned int i, j = 0;

	for (i = 0; i < count; i++) {
		if (trace[i].op != 'w' || (skip_mask && trace[i].reg == WFDMA_IRQ_MASK))
			continue;
		CHECK(j < n);
		CHECK(trace[i].reg == want[j].reg && trace[i].value == want[j].value);
		j++;
	}
	CHECK(j == n);
}

static void matches_mt76(void)
{
	static const struct { const struct mt76_reg_write *reset, *enable;
			      unsigned int nreset, nenable; } ref[4] = {
		{ mt76_wfdma_reset_writes_0, mt76_wfdma_enable_writes_0,
		  ARRAY_SIZE(mt76_wfdma_reset_writes_0), ARRAY_SIZE(mt76_wfdma_enable_writes_0) },
		{ mt76_wfdma_reset_writes_1, mt76_wfdma_enable_writes_1,
		  ARRAY_SIZE(mt76_wfdma_reset_writes_1), ARRAY_SIZE(mt76_wfdma_enable_writes_1) },
		{ mt76_wfdma_reset_writes_2, mt76_wfdma_enable_writes_2,
		  ARRAY_SIZE(mt76_wfdma_reset_writes_2), ARRAY_SIZE(mt76_wfdma_enable_writes_2) },
		{ mt76_wfdma_reset_writes_3, mt76_wfdma_enable_writes_3,
		  ARRAY_SIZE(mt76_wfdma_reset_writes_3), ARRAY_SIZE(mt76_wfdma_enable_writes_3) },
	};
	unsigned int p;

	for (p = 0; p < 4; p++) {
		init(mt76_wfdma_patterns[p]);
		CHECK(sdr_wfdma_reset_regs(&io) == 0);
		compare_writes(ref[p].reset, ref[p].nreset, true);
		init(mt76_wfdma_patterns[p]);
		CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == 0);
		CHECK(!(*regptr(WFDMA_GLO_CFG) & WFDMA_DMA_ENABLE));
		CHECK(sdr_wfdma_enable_regs(&io) == 0);
		compare_writes(ref[p].enable, ref[p].nenable, false);
		CHECK(*regptr(WFDMA_IRQ_MASK) == 0);
		/* No data rings: TX band 0 and band 2 prefetch stay untouched. */
		CHECK(*regptr(0xd4648) == mt76_wfdma_patterns[p]);
		CHECK(*regptr(0xd4654) == mt76_wfdma_patterns[p]);
		/* Stop masks interrupts, clears the enables as mt76 does, and
		 * enables the FIFO busy sources.
		 */
		reset_trace();
		CHECK(sdr_wfdma_stop_regs(&io, wait_poll) == 0);
		CHECK(writes == 3);
		CHECK(trace[0].op == 'w' && trace[0].reg == WFDMA_IRQ_MASK);
		CHECK(trace[3].op == 'w' && trace[3].reg == mt76_wfdma_stop_first_write[p].reg &&
		      trace[3].value == mt76_wfdma_stop_first_write[p].value);
	}
}

static void bounded_waits(void)
{
	static const unsigned int ready[] = { 1, 2, SDR_WFDMA_IDLE_POLLS };
	static const unsigned int mt76_reads[] = {
		MT76_WFDMA_ENABLE_HIF_READS_1, MT76_WFDMA_ENABLE_HIF_READS_2,
		MT76_WFDMA_ENABLE_HIF_READS_101,
	};
	static const int errors[] = { -EINTR, -ENODEV, -EHOSTDOWN, 1 };
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ready); i++) {
		init(0);
		ready_at = ready[i];
		CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == 0);
		CHECK(hif_reads == mt76_reads[i] && waits == ready[i] - 1);
		init(0);
		ready_at = ready[i];
		CHECK(sdr_wfdma_stop_regs(&io, wait_poll) == 0);
		CHECK(hif_reads == ready[i] && waits == ready[i] - 1);
	}
	/* Never idle: mt76 carries on; we stop before RX setup and fail. */
	init(0);
	ready_at = 0;
	CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == -ETIMEDOUT);
	CHECK(hif_reads == MT76_WFDMA_ENABLE_HIF_READS_NEVER && waits == SDR_WFDMA_IDLE_POLLS - 1);
	CHECK(*regptr(WFDMA_EXT0) == 0 && *regptr(WFDMA_GLO_CFG) == 0);
	CHECK(*regptr(0xd4644) == 0x00200002 && *regptr(WFDMA_EXT1) == 0x80000000);
	init(0);
	ready_at = 0;
	*regptr(WFDMA_GLO_CFG) = WFDMA_START_BITS;
	CHECK(sdr_wfdma_stop_regs(&io, wait_poll) == -ETIMEDOUT);
	CHECK(!(*regptr(WFDMA_GLO_CFG) & WFDMA_DISABLE_BITS));
	for (i = 0; i < ARRAY_SIZE(errors); i++) {
		init(0);
		ready_at = 0;
		wait_error = errors[i];
		CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == (errors[i] < 0 ? errors[i] : -EIO));
		CHECK(hif_reads == 1 && waits == 1 && *regptr(WFDMA_EXT0) == 0);
	}
}

static int invoke(unsigned int op)
{
	if (op == 0)
		return sdr_wfdma_reset_regs(&io);
	if (op == 1)
		return sdr_wfdma_prepare_regs(&io, wait_poll);
	if (op == 2)
		return sdr_wfdma_enable_regs(&io);
	return sdr_wfdma_stop_regs(&io, wait_poll);
}

static void op_init(unsigned int op)
{
	init(0);
	if (op == 0)
		*regptr(WFDMA_RST) = WFDMA_RESET_BITS;
	if (op == 2) {
		CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == 0);
		reset_trace();
	}
	if (op == 3)
		*regptr(WFDMA_GLO_CFG) = WFDMA_START_BITS;
}

/* An all-ones read anywhere stops before any later write; a lost write that
 * changes a checked field is caught by its read-back.
 */
static void faults(void)
{
	unsigned int op, n, nwrite, r, w;
	int ret;

	for (op = 0; op < 4; op++) {
		op_init(op);
		CHECK(invoke(op) == 0);
		n = reads;
		nwrite = writes;
		for (r = 1; r <= n; r++) {
			op_init(op);
			fail_read = r;
			CHECK(invoke(op) == -EIO && reads == r && trace[count - 1].op == 'r');
		}
		for (w = 1; w <= nwrite; w++) {
			op_init(op);
			fail_write = w;
			ret = invoke(op);
			if (ret)
				CHECK(ret == -EIO && writes == w);
		}
	}
	init(0);
	*regptr(WFDMA_IRQ_MASK) = 1;
	fail_write = 1;
	CHECK(sdr_wfdma_reset_regs(&io) == -EIO && writes == 1);
	op_init(2);
	fail_write = 1;
	CHECK(sdr_wfdma_enable_regs(&io) == -EIO && writes == 1);
}

static void admission(void)
{
	struct sdr_mmio bad = io;
	struct sdr_mmio_ops badops = ops;
	unsigned int bit;

	init(0);
	CHECK(sdr_wfdma_reset_regs(NULL) == -EINVAL);
	CHECK(sdr_wfdma_prepare_regs(&io, NULL) == -EINVAL);
	bad.bar_size--;
	CHECK(sdr_wfdma_reset_regs(&bad) == -EINVAL);
	bad = io;
	bad.ops = &badops;
	badops.read32 = NULL;
	CHECK(sdr_wfdma_reset_regs(&bad) == -EINVAL);
	CHECK(count == 0);
	/* An engine that is already enabled or unmasked is refused untouched. */
	for (bit = 0; bit < 32; bit++) {
		if (!(BIT(bit) & WFDMA_DISABLE_BITS))
			continue;
		init(0);
		*regptr(WFDMA_GLO_CFG) = BIT(bit);
		CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == -EBUSY && writes == 0);
		CHECK(sdr_wfdma_enable_regs(&io) == -EBUSY && writes == 0);
	}
	init(0);
	*regptr(WFDMA_IRQ_MASK) = 0x1000;
	CHECK(sdr_wfdma_prepare_regs(&io, wait_poll) == -EBUSY && writes == 0);
	init(0);
	CHECK(sdr_wfdma_enable_regs(&io) == -EIO && writes == 0);	/* not prepared */
	op_init(2);
	ready_at = 0;
	CHECK(sdr_wfdma_enable_regs(&io) == -EBUSY && writes == 0);
}

static void session_init(unsigned int op)
{
	init(0);
	CHECK(sdr_wfdma_init(&session, &io, wait_poll) == 0 && count == 0);
	if (op > 0)
		CHECK(sdr_wfdma_reset(&session) == 0);
	if (op > 1)
		CHECK(sdr_wfdma_prepare(&session) == 0);
	if (op > 2)
		CHECK(sdr_wfdma_enable(&session) == 0);
	reset_trace();
}

static int session_invoke(unsigned int op)
{
	if (op == 0)
		return sdr_wfdma_reset(&session);
	if (op == 1)
		return sdr_wfdma_prepare(&session);
	if (op == 2)
		return sdr_wfdma_enable(&session);
	return sdr_wfdma_stop_observe(&session);
}

static void reject_restart(void)
{
	unsigned int before = count;
	struct sdr_wfdma saved = session;

	CHECK(sdr_wfdma_reset(&session) == -EALREADY);
	CHECK(sdr_wfdma_prepare(&session) == -EALREADY);
	CHECK(sdr_wfdma_enable(&session) == -EALREADY);
	CHECK(count == before && session.state == saved.state &&
	      session.first_error == saved.first_error);
}

static void lifecycle(void)
{
	struct sdr_wfdma bad = { 0 };
	unsigned int op, n, r;

	CHECK(sdr_wfdma_init(NULL, &io, wait_poll) == -EINVAL);
	CHECK(sdr_wfdma_init(&bad, &io, NULL) == -EINVAL);
	CHECK(sdr_wfdma_reset(&bad) == -EINVAL && sdr_wfdma_stop_observe(&bad) == -EINVAL);
	session_init(0);
	CHECK(session.state == SDR_WFDMA_NEW);
	CHECK(sdr_wfdma_prepare(&session) == -EALREADY);
	CHECK(sdr_wfdma_enable(&session) == -EALREADY);
	CHECK(sdr_wfdma_stop_observe(&session) == -EALREADY && count == 0);
	CHECK(sdr_wfdma_reset(&session) == 0 && session.state == SDR_WFDMA_RESET);
	CHECK(sdr_wfdma_prepare(&session) == 0 && session.state == SDR_WFDMA_PREPARED);
	CHECK(sdr_wfdma_enable(&session) == 0 && session.state == SDR_WFDMA_ENABLED);
	reject_restart();
	CHECK(sdr_wfdma_stop_observe(&session) == 0 && session.state == SDR_WFDMA_STOPPED);
	reject_restart();
	reset_trace();
	CHECK(sdr_wfdma_stop_observe(&session) == -EALREADY && count == 0);
	/* A failure at any read latches FAILED; one stop is still allowed. */
	for (op = 0; op < 4; op++) {
		session_init(op);
		CHECK(session_invoke(op) == 0);
		n = reads;
		for (r = 1; r <= n; r++) {
			session_init(op);
			fail_read = r;
			CHECK(session_invoke(op) == -EIO);
			CHECK(session.state == SDR_WFDMA_FAILED && session.first_error == -EIO);
			reject_restart();
			reset_trace();
			if (op < 3) {
				CHECK(sdr_wfdma_stop_observe(&session) == 0);
				CHECK(session.state == SDR_WFDMA_FAILED &&
				      session.first_error == -EIO);
			}
			reset_trace();
			CHECK(sdr_wfdma_stop_observe(&session) == -EALREADY && count == 0);
		}
	}
	/* A prepare that times out before RX setup can never be enabled. */
	session_init(1);
	ready_at = 0;
	CHECK(sdr_wfdma_prepare(&session) == -ETIMEDOUT);
	CHECK(session.state == SDR_WFDMA_FAILED && *regptr(WFDMA_EXT0) == 0);
	ready_at = 1;
	reject_restart();
}

/* Each preparation field: mt76's value, its documented field checked on
 * read-back, reserved bits ignored, and a failure that keeps earlier writes.
 */
static void prepare_fields(void)
{
	unsigned int k, bit, ordinal, found, i;
	u32 stale;
	int ret;

	session_init(1);
	CHECK(sdr_wfdma_prepare(&session) == 0);
	for (i = 0, found = 0; i < count; i++) {
		/* The self-clearing pointer reset is never read back. */
		CHECK(trace[i].op == 'w' || trace[i].reg != WFDMA_RST_DTX);
		if (trace[i].op != 'w')
			continue;
		if (found < ARRAY_SIZE(mt76_wfdma_setup_fields) &&
		    trace[i].reg == mt76_wfdma_setup_fields[found].reg) {
			CHECK(trace[i].value == mt76_wfdma_setup_fields[found].value);
			CHECK(trace[i + 1].op == 'r' && trace[i + 1].reg == trace[i].reg);
			found++;
		}
	}
	CHECK(found == ARRAY_SIZE(mt76_wfdma_setup_fields));
	for (k = 0; k < ARRAY_SIZE(mt76_wfdma_setup_fields); k++) {
		const struct mt76_setup_field *f = &mt76_wfdma_setup_fields[k];

		for (bit = 0; bit < 32; bit++) {
			session_init(1);
			xor_read_reg = f->reg;
			xor_read_bits = BIT(bit);
			ret = sdr_wfdma_prepare(&session);
			CHECK(*regptr(f->reg) == f->value);
			CHECK(BIT(bit) & f->mask ? ret == -EIO : ret == 0);
		}
		/* A lost write of this field fails its read-back. */
		session_init(1);
		CHECK(sdr_wfdma_prepare(&session) == 0);
		for (i = 0, ordinal = 0; i < count; i++) {
			if (trace[i].op != 'w')
				continue;
			ordinal++;
			if (trace[i].reg == f->reg)
				break;
		}
		session_init(1);
		stale = f->value ^ (f->mask & -f->mask);
		*regptr(f->reg) = stale;
		fail_write = ordinal;
		CHECK(sdr_wfdma_prepare(&session) == -EIO && *regptr(f->reg) == stale);
		CHECK(session.state == SDR_WFDMA_FAILED && writes == ordinal);
		CHECK(!(*regptr(WFDMA_GLO_CFG) & WFDMA_DMA_ENABLE));
	}
}

int main(void)
{
	matches_mt76();
	bounded_waits();
	faults();
	admission();
	lifecycle();
	prepare_fields();
	printf("wfdma: %lu checks\n", checks);
	return 0;
}
