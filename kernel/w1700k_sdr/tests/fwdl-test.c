// SPDX-License-Identifier: GPL-2.0-only
/*
 * fwdl.c over the real rings and wire encoder, with a simulated MCU: it reads
 * every packet the executor publishes, checks it byte for byte against the
 * expected command, completes DMA and answers on the event ring.
 */
#include "dma-mocks.h"

#define NSEC_PER_SEC UINT64_C(1000000000)
struct mutex { bool held; };
static inline void mutex_init(struct mutex *m) { m->held = false; }
static inline int mutex_trylock(struct mutex *m)
{
	if (m->held)
		return 0;
	m->held = true;
	return 1;
}
static inline void mutex_unlock(struct mutex *m) { CHECK(m->held); m->held = false; }
static bool interrupted;
#define current NULL
static inline int signal_pending(void *task) { (void)task; return interrupted; }
static inline int pci_channel_offline(struct pci_dev *p) { return p->offline; }
static u64 now, time_step;
static unsigned int sleeps, ticks, interrupt_at, offline_at;
static unsigned int state_reads, state_sleeps, state_ready_at;
static unsigned int state_interrupt_at, state_offline_at;
static u32 state_before, state_after;
static void simulate(void);
static inline u64 ktime_get_ns(void) { simulate(); return now; }
static inline void usleep_range(unsigned long min, unsigned long max)
{
	CHECK((min == 1000 && max == 2000) || (min == 10000 && max == 20000));
	if (min == 10000) {
		state_sleeps++;
		if (state_sleeps == state_interrupt_at)
			interrupted = true;
		if (state_sleeps == state_offline_at)
			test_pdev.offline = true;
	}
	now += time_step;
	sleeps++;
}
#define dev_warn_ratelimited(dev, fmt, ...) ((void)sizeof(printf(fmt, ##__VA_ARGS__)))
#define dev_dbg_ratelimited(dev, fmt, ...) ((void)sizeof(printf(fmt, ##__VA_ARGS__)))
#include "fwdl_wire.c"
#include "fwdl-image.c"
#include "fwdl.c"

static u32 state_read(void *ctx, u32 reg)
{
	CHECK(ctx == &test_pdev && reg == 0xe00f0);	/* FW_STATE */
	state_reads++;
	return state_ready_at && state_reads >= state_ready_at ? state_after : state_before;
}
static const struct sdr_mmio_ops state_ops = { .read32 = state_read };
static const struct sdr_mmio state_io = { &state_ops, &test_pdev, 0xe00f4 };

#define RX_MAPS 1024U	/* the stream ring's slices stay mapped */
static struct sdr_dma_ring *fw, *wm, *rx;
static struct sdr_fwdl *loader;
static struct sdr_fwdl_region regions[SDR_FWDL_MAX_REGIONS];
static unsigned int region_count, next_region, capture_region, capture_offset;
static unsigned int seen_wm, seen_fw, sequence, command_count;
static unsigned int command_age, command_seq, tx_delay, reply_delay, reply_status;
static unsigned int bad_events, fail_command, sems;
static bool freeze_tx, no_reply, flood, bad_index, stall_fw, stale_at_start;
static bool command_active, replied;
static unsigned int start_working_pda, sem_reply_get, sem_reply_release;
static u8 cmd_log[64];
static unsigned int cmd_log_count;
static u8 raw[8193];
static unsigned int reply_eid;

static void emit(struct sdr_dma_ring *ring, unsigned int seq, unsigned int eid,
		 unsigned int status, unsigned int bad)
{
	u8 packet[100] = { 0 };
	unsigned int bytes = 44, flags = SDR_DMA_LAST;

	put_unaligned_le32(0x3800002c, packet);
	packet[32] = 12;
	packet[34] = 7;
	packet[36] = (u8)eid;
	packet[37] = (u8)seq;
	packet[40] = (u8)status;
	if (bad == 1)
		packet[37] = seq == 15 ? 1 : seq + 1;	/* unrelated */
	if (bad == 2)
		packet[38] = 1;				/* malformed */
	if (bad == 3)
		flags = 0;				/* fragment */
	if (bad == 4)
		bytes = 100;				/* a larger event */
	complete_rx(ring, packet, bytes, (bytes << 16) | flags);
}

static void check_command(const u8 *data, unsigned int bytes)
{
	u8 expected[MT7996_FWDL_INIT_BYTES];
	unsigned int i, cid = data[36];
	u32 entry = 0;
	int len = -1;

	sequence = mt7996_fwdl_next_seq(sequence);
	CHECK(data[39] == sequence);
	CHECK(cmd_log_count < ARRAY_SIZE(cmd_log));
	cmd_log[cmd_log_count++] = (u8)cid;
	switch (cid) {
	case MT7996_FWDL_CMD_PATCH_SEM:
		len = mt7996_fwdl_patch_sem(expected, sizeof(expected), sequence,
					    sems++ ? MT7996_FWDL_SEM_RELEASE : MT7996_FWDL_SEM_GET);
		break;
	case MT7996_FWDL_CMD_PATCH_START:
	case MT7996_FWDL_CMD_TARGET_ADDRESS_LEN:
		CHECK(next_region < region_count);
		CHECK(!next_region || capture_offset == regions[capture_region].length);
		capture_region = next_region++;
		capture_offset = 0;
		len = mt7996_fwdl_init(expected, sizeof(expected), sequence,
				       regions[capture_region].address,
				       regions[capture_region].length,
				       regions[capture_region].feature_set);
		break;
	case MT7996_FWDL_CMD_PATCH_FINISH:
		CHECK(next_region == region_count &&
		      capture_offset == regions[capture_region].length);
		len = mt7996_fwdl_patch_finish(expected, sizeof(expected), sequence, 0);
		break;
	case MT7996_FWDL_CMD_FW_START:
		CHECK(next_region == region_count &&
		      capture_offset == regions[capture_region].length);
		for (i = 0; i < region_count; i++)
			if (regions[i].feature_set & BIT(5))
				entry = regions[i].address;
		len = mt7996_fwdl_start(expected, sizeof(expected), sequence, entry,
					start_working_pda);
		break;
	}
	CHECK(len == (int)bytes && !memcmp(expected, data, bytes));
}

/* The status and event id the device answers command `n` (1-based) with. */
static void answer(unsigned int n, const u8 *data, unsigned int *status, unsigned int *eid)
{
	*status = n == fail_command ? reply_status : 0;
	*eid = MT7996_FWDL_EID_GENERIC;
	if (data[36] == MT7996_FWDL_CMD_PATCH_SEM) {
		*eid = MT7996_FWDL_EID_PATCH_SEM;
		*status = get_unaligned_le32(data + 64) == MT7996_FWDL_SEM_GET ?
			  sem_reply_get : sem_reply_release;
	}
}

static u8 last_command[MT7996_FWDL_INIT_BYTES];

static void simulate(void)
{
	struct sdr_dma_buffer *b;
	unsigned int status, eid;
	int bytes;

	reset_events();
	ticks++;
	if (ticks == interrupt_at)
		interrupted = true;
	if (ticks == offline_at)
		test_pdev.offline = true;
	if (!loader)
		return;
	if (flood) {
		emit(rx, 1, MT7996_FWDL_EID_GENERIC, 0, 0);
		return;
	}
	if (wm->head != seen_wm) {
		CHECK(wm->queued == 1 && !fw->queued);
		b = &wm->buffers[wm->tail];
		check_command(b->data, b->bytes);
		memcpy(last_command, b->data, sizeof(last_command));
		command_count++;
		seen_wm = wm->head;
		command_active = true;
		replied = false;
		command_age = 0;
		command_seq = sequence;
		if (stale_at_start && command_count == region_count + 1) {
			emit(rx, 1, MT7996_FWDL_EID_GENERIC, 0, 0);
			reply_delay = 3;
		}
	}
	if (fw->head != seen_fw) {
		CHECK(!wm->queued && fw->queued == 1 && replied);
		b = &fw->buffers[fw->tail];
		bytes = mt7996_fwdl_scatter_length(regions[capture_region].length, capture_offset);
		CHECK(bytes == (int)b->bytes);
		CHECK(!memcmp(b->data, (const u8 *)regions[capture_region].data + capture_offset,
			      bytes));
		capture_offset += bytes;
		sequence = mt7996_fwdl_next_seq(sequence);
		seen_fw = fw->head;
		if (!freeze_tx && !stall_fw)
			set_index(fw, bad_index ? fw->ndesc : fw->head);
	}
	if (!command_active)
		return;
	command_age++;
	if (!freeze_tx && command_age >= tx_delay)
		set_index(wm, bad_index ? wm->ndesc : wm->head);
	if (!no_reply && !replied && command_age >= reply_delay) {
		if (bad_events) {
			emit(rx, command_seq, MT7996_FWDL_EID_GENERIC, 0, bad_events--);
		} else {
			answer(command_count, last_command, &status, &eid);
			emit(rx, command_seq, reply_eid ? reply_eid : eid, status, 0);
			replied = true;
		}
	}
}

static void setup_rings(void)
{
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_FWDL_TX, &fw));
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_TX, &wm));
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &rx));
	bind_ring(fw);
	bind_ring(wm);
	bind_ring(rx);
	CHECK(!sdr_fwdl_alloc(&test_pdev, &state_io, fw, wm, rx, &loader));
}

static void setup(unsigned int count, unsigned int bytes)
{
	unsigned int i;

	clean_state();
	now = 0;
	time_step = 1000000;
	ticks = sleeps = 0;
	state_reads = state_sleeps = 0;
	state_ready_at = 1;
	state_interrupt_at = state_offline_at = 0;
	state_before = 0;
	state_after = 1;
	test_pdev.offline = interrupted = false;
	interrupt_at = offline_at = 0;
	seen_wm = seen_fw = sequence = command_count = 0;
	next_region = capture_region = capture_offset = command_age = command_seq = 0;
	tx_delay = reply_delay = 1;
	reply_status = bad_events = sems = 0;
	fail_command = 1;
	stall_fw = stale_at_start = false;
	reply_eid = 0;
	start_working_pda = 0;
	sem_reply_get = MT7996_FWDL_SEM_NOT_DL_SUCCESS;
	sem_reply_release = MT7996_FWDL_SEM_REL_SUCCESS;
	freeze_tx = no_reply = flood = bad_index = command_active = replied = false;
	cmd_log_count = 0;
	region_count = count;
	for (i = 0; i < sizeof(raw); i++)
		raw[i] = (u8)(i * 37 + 11);
	for (i = 0; i < count; i++)
		regions[i] = (struct sdr_fwdl_region){ raw, 0x916400U + i * 0x10000, bytes,
						       i & 1 ? 0 : 32 };
	loader = NULL;
	setup_rings();
	reset_events();
}

static void teardown(void)
{
	unsigned int mapped = map_count;

	sdr_fwdl_free(loader);
	loader = NULL;
	CHECK(map_count == mapped);	/* the session never frees DMA memory */
	sdr_dma_ring_free_quiesced(fw);
	sdr_dma_ring_free_quiesced(wm);
	sdr_dma_ring_free_quiesced(rx);
	no_leaks();
}

/* A new container in the same session: the model restarts its region count. */
static void next_container(void)
{
	next_region = capture_region = capture_offset = 0;
	command_count = sems = cmd_log_count = 0;
}

static int run_as(unsigned int container, unsigned int ram_type, struct sdr_fwdl_result *r)
{
	struct sdr_fwdl_request req = { regions, region_count, container, ram_type };

	return sdr_fwdl_run(loader, &req, r);
}

static int run(struct sdr_fwdl_result *r)
{
	return run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_WM, r);
}

static void success(void)
{
	struct sdr_fwdl_result r, saved;
	unsigned int count, events_before;
	u8 *large;

	for (count = 1; count <= 3; count++) {
		setup(count, sizeof(raw));
		tx_delay = count & 1 ? 3 : 1;	/* reply before or after DMA */
		reply_delay = count & 1 ? 1 : 3;
		CHECK(!run(&r));
		CHECK(r.phase == SDR_FWDL_COMPLETE && !r.result && r.region == count);
		CHECK(r.offset == sizeof(raw) && r.replies == count + 1);
		CHECK(r.tx_submitted == count * 4 + 1 && r.tx_completed == r.tx_submitted);
		CHECK(r.last_seq == sequence && !fw->queued && !wm->queued);
		CHECK(map_count == RX_MAPS && command_count == count + 1);
		saved = r;
		events_before = event_count;
		CHECK(run(&r) == -EALREADY && !memcmp(&r, &saved, sizeof(r)));
		CHECK(event_count == events_before);
		teardown();
	}
	/* Bad events first: a larger one, two of a fragment chain, an
	 * unrelated reply.
	 */
	setup(3, 4096);
	bad_events = 4;
	CHECK(!run(&r) && r.malformed_events == 3 && r.discarded_events == 1);
	teardown();
	/* Sequence numbers wrap through the scatter chunks. */
	setup(1, 16 * 4096);
	large = malloc(16 * 4096);
	CHECK(large);
	memset(large, 0x5a, 16 * 4096);
	regions[0].data = large;
	CHECK(!run(&r) && r.last_seq == 3 && r.tx_submitted == 18);
	free(large);
	teardown();
	/* A stale reply queued before START is discarded, not taken as its answer. */
	setup(14, 1);
	stale_at_start = true;
	CHECK(!run(&r) && r.replies == 15 && r.discarded_events == 1 && r.last_seq == 14);
	teardown();
	setup(1, 1);
	regions[0].feature_set = 0;	/* default WM entry */
	CHECK(!run(&r));
	teardown();
}

static void admission(void)
{
	struct sdr_fwdl_result r;
	struct sdr_fwdl *untouched = (void *)1;
	struct sdr_fwdl_request bad = { regions, 1, SDR_FWDL_RAM, 0 };
	struct device other;

	setup(2, 4096);
	CHECK(sdr_fwdl_alloc(&test_pdev, &state_io, wm, fw, rx, &untouched) ==
	      -EINVAL && untouched == (void *)1);
	CHECK(sdr_dma_ring_check(fw, DEVICE, 77) == -EINVAL);
	CHECK(sdr_dma_ring_check(fw, &other, SDR_DMA_FWDL_TX) == -EINVAL);
	wm->bound = false;
	CHECK(run(&r) == -ENXIO);
	wm->bound = true;
	wm->fault = true;
	CHECK(run(&r) == -EIO);
	wm->fault = false;
	wm->queued = 1;
	CHECK(run(&r) == -EBUSY);
	wm->queued = 0;
	regions[1].feature_set = 8;	/* undefined feature bit */
	CHECK(run(&r) == -EINVAL);
	regions[1].feature_set = 0;
	regions[1].length = 0;
	CHECK(run(&r) == -EINVAL);
	regions[1].length = 4096;
	CHECK(sdr_fwdl_run(loader, NULL, &r) == -EINVAL);
	bad.count = 15;
	CHECK(sdr_fwdl_run(loader, &bad, &r) == -EINVAL);
	bad.count = 1;
	bad.container = 3;
	CHECK(sdr_fwdl_run(loader, &bad, &r) == -EINVAL);
	bad.container = SDR_FWDL_RAM;
	bad.ram_type = 3;
	CHECK(sdr_fwdl_run(loader, &bad, &r) == -EINVAL);
	loader->lock.held = true;
	CHECK(run(&r) == -EBUSY);
	loader->lock.held = false;
	/* None of that touched the device or spent the session. */
	CHECK(!event_count && !ticks && !state_reads);
	CHECK(!run(&r));
	teardown();
}

static void failures(void)
{
	struct sdr_fwdl_result r, saved;
	unsigned int status, mode, call;

	for (status = 1; status <= 255; status++) {
		setup(1, 1);
		reply_status = status;
		freeze_tx = true;
		CHECK(run(&r) == (int)status && r.result == (int)status);
		CHECK(r.phase == SDR_FWDL_INIT && r.replies == 1 && r.tx_submitted == 1 &&
		      !r.tx_completed && wm->queued == 1 && map_count == RX_MAPS + 1);
		saved = r;
		CHECK(run(&r) == -EALREADY && !memcmp(&saved, &r, sizeof(r)));
		teardown();
	}
	setup(1, 1);
	no_reply = true;
	time_step = SDR_FWDL_TIMEOUT_NS;
	CHECK(run(&r) == -ETIMEDOUT && r.tx_completed == 1 && !r.replies);
	teardown();
	/* A stale success queued before the command does not answer it. */
	setup(1, 1);
	emit(rx, 1, MT7996_FWDL_EID_GENERIC, 0, 0);
	no_reply = true;
	time_step = SDR_FWDL_TIMEOUT_NS / 2;
	CHECK(run(&r) == -ETIMEDOUT && !r.replies && r.discarded_events == 1);
	teardown();
	/* Endless stale events with a stopped clock: the poll bound ends it. */
	setup(1, 1);
	flood = true;
	time_step = 0;
	CHECK(run(&r) == -ETIMEDOUT && !r.tx_submitted);
	CHECK(r.discarded_events == SDR_FWDL_POLL_LIMIT - 1 && sleeps == SDR_FWDL_POLL_LIMIT - 1);
	teardown();
	for (mode = 0; mode < 2; mode++) {
		setup(1, 1);
		no_reply = freeze_tx = true;
		if (mode)
			offline_at = 5;
		else
			interrupt_at = 5;
		CHECK(run(&r) == (mode ? -EIO : -EINTR));
		CHECK(r.tx_submitted == 1 && !r.tx_completed && map_count == RX_MAPS + 1);
		teardown();
	}
	setup(1, 1);
	bad_index = true;
	CHECK(run(&r) == -EIO && r.tx_submitted == 1 && !r.tx_completed && wm->fault);
	teardown();
	setup(1, sizeof(raw));
	stall_fw = true;
	time_step = SDR_FWDL_TIMEOUT_NS;
	CHECK(run(&r) == -ETIMEDOUT && r.phase == SDR_FWDL_SCATTER && r.offset == 0);
	CHECK(r.tx_submitted == 2 && r.tx_completed == 1 && fw->queued == 1);
	teardown();
	setup(2, 4096);
	fail_command = 2;
	reply_status = 5;
	CHECK(run(&r) == 5 && r.phase == SDR_FWDL_INIT && r.region == 1 && r.replies == 2);
	teardown();
	setup(1, 4096);
	fail_command = 2;
	reply_status = 7;
	CHECK(run(&r) == 7 && r.phase == SDR_FWDL_START && r.region == 1 && r.tx_completed == 3);
	teardown();
	setup(1, sizeof(raw));
	fail_at = calls + 5;	/* the second scatter allocation */
	CHECK(run(&r) == -ENOMEM && r.phase == SDR_FWDL_SCATTER && r.offset == 4096);
	CHECK(r.tx_submitted == 2 && r.tx_completed == 2 && map_count == RX_MAPS);
	CHECK(run(&r) == -EALREADY);
	teardown();
	for (call = 1; call <= 2; call++) {
		setup(1, 1);
		fail_at = calls + call;
		CHECK(run(&r) == (call == 1 ? -ENOMEM : -EIO) && !r.tx_submitted);
		CHECK(run(&r) == -EALREADY);
		teardown();
	}
}

static void ram_image(void)
{
	struct sdr_fwdl_result r, saved;
	unsigned int payload = 2 * 4097, table = payload + 36, bytes = table + 80 + 36, i;
	u8 *image, *copy;

	setup(2, 4097);
	image = calloc(1, bytes);
	copy = malloc(bytes);
	CHECK(image && copy);
	for (i = 0; i < 2; i++) {
		memcpy(image + i * 4097, raw, 4097);
		regions[i].data = image + i * 4097;
		put_unaligned_le32(regions[i].address, image + table + i * 40 + 16);
		put_unaligned_le32(4097, image + table + i * 40 + 20);
		image[table + i * 40 + 24] = (u8)regions[i].feature_set;
	}
	memset(image + payload, '#', 16);
	put_unaligned_le32(0, image + payload + 16);
	memset(image + table - 16, '#', 16);
	image[bytes - 36] = 0x16;
	image[bytes - 34] = 2;
	image[bytes - 33] = 2;
	image[bytes - 32] = 1;
	put_unaligned_le32(crc32_le(~0U, image, bytes - 4) ^ ~0U, image + bytes - 4);
	memset(&r, 0xa5, sizeof(r));
	saved = r;
	image[table + 40 + 24] = 8;	/* a bad region is refused before any I/O */
	CHECK(sdr_fwdl_run_image(loader, image, bytes, SDR_FWDL_RAM_WM, &r) == -EOPNOTSUPP);
	CHECK(!memcmp(&r, &saved, sizeof(r)) && !ticks && !event_count);
	image[table + 40 + 24] = (u8)regions[1].feature_set;
	image[bytes - 1] ^= 1;
	CHECK(sdr_fwdl_run_image(loader, image, bytes, SDR_FWDL_RAM_WM, &r) == -EBADMSG);
	image[bytes - 1] ^= 1;
	memcpy(copy, image, bytes);
	CHECK(!sdr_fwdl_run_image(loader, image, bytes, SDR_FWDL_RAM_WM, &r));
	CHECK(r.container == SDR_FWDL_RAM && r.phase == SDR_FWDL_COMPLETE && r.replies == 3);
	CHECK(!memcmp(image, copy, bytes));
	teardown();
	free(image);
	free(copy);
}

static void put_be32(u8 *out, u32 value)
{
	out[0] = (u8)(value >> 24);
	out[1] = (u8)(value >> 16);
	out[2] = (u8)(value >> 8);
	out[3] = (u8)value;
}

static void patch(void)
{
	static const u8 sem_finish[] = { 0x10, 0x05, 0x05, 0x07, 0x10 };
	struct sdr_fwdl_result r;
	unsigned int pbytes = 96 + 64 + 4096;
	u8 *image;

	/* A patch container through run_image: semaphore, region, finish,
	 * release, and no START.
	 */
	setup(1, 4096);
	regions[0].address = 0x900000;
	regions[0].feature_set = 1;
	image = calloc(1, pbytes);
	CHECK(image);
	memcpy(image + 16, "ALPS", 4);
	put_be32(image + 44, 1);
	put_be32(image + 96, 0x00030002);
	put_be32(image + 96 + 4, 96 + 64);
	put_be32(image + 96 + 8, 4096);
	put_be32(image + 96 + 12, 0x900000);
	put_be32(image + 96 + 16, 4096);
	put_be32(image + 96 + 20, 0x01000000);	/* AES, key 0: encrypted */
	memcpy(image + 96 + 64, raw, 4096);
	regions[0].data = image + 96 + 64;
	CHECK(!sdr_fwdl_run_image(loader, image, pbytes, SDR_FWDL_RAM_WM, &r));
	CHECK(r.container == SDR_FWDL_PATCH && r.phase == SDR_FWDL_COMPLETE);
	CHECK(r.sem_get_status == MT7996_FWDL_SEM_NOT_DL_SUCCESS &&
	      r.sem_release_status == MT7996_FWDL_SEM_REL_SUCCESS && r.replies == 4);
	teardown();
	free(image);

	setup(2, 4096);
	regions[0].address = 0x900000;
	regions[1].address = 0x200000;
	CHECK(!run_as(SDR_FWDL_PATCH, 0, &r) && r.phase == SDR_FWDL_COMPLETE);
	CHECK(cmd_log_count == 5 && !memcmp(cmd_log, sem_finish, 5));
	teardown();

	/* Already present: no region is sent, the semaphore is released. */
	setup(2, 4096);
	sem_reply_get = MT7996_FWDL_SEM_IS_DL;
	CHECK(!run_as(SDR_FWDL_PATCH, 0, &r) && r.phase == SDR_FWDL_COMPLETE);
	CHECK(cmd_log_count == 2 && cmd_log[0] == 0x10 && cmd_log[1] == 0x10 && !fw->head);
	teardown();

	/* Refused semaphore: -EBUSY, still released. */
	setup(1, 4096);
	sem_reply_get = MT7996_FWDL_SEM_NOT_DL_FAIL;
	CHECK(run_as(SDR_FWDL_PATCH, 0, &r) == -EBUSY && cmd_log_count == 2);
	CHECK(r.sem_release_status == MT7996_FWDL_SEM_REL_SUCCESS);
	teardown();

	/* A refused region: the semaphore is still released, as mt76 does. */
	setup(2, 4096);
	regions[0].address = 0x900000;
	fail_command = 3;	/* the second INIT */
	reply_status = 9;
	CHECK(run_as(SDR_FWDL_PATCH, 0, &r) == 9);
	CHECK(cmd_log_count == 4 && cmd_log[3] == 0x10);
	CHECK(r.sem_release_status == MT7996_FWDL_SEM_REL_SUCCESS);
	CHECK(run_as(SDR_FWDL_PATCH, 0, &r) == -EALREADY);
	teardown();

	/* A key index wider than two bits is refused before any I/O. */
	setup(1, 4096);
	image = calloc(1, pbytes);
	CHECK(image);
	memcpy(image + 16, "ALPS", 4);
	put_be32(image + 44, 1);
	put_be32(image + 96, 0x00030002);
	put_be32(image + 96 + 4, 96 + 64);
	put_be32(image + 96 + 12, 0x900000);
	put_be32(image + 96 + 16, 4096);
	put_be32(image + 96 + 20, 0x02000400);
	CHECK(sdr_fwdl_run_image(loader, image, pbytes, SDR_FWDL_RAM_WM, &r) == -EOPNOTSUPP);
	CHECK(!ticks && !event_count);
	teardown();
	free(image);
}

/* One session: the patch, then each RAM type started once. */
static void bring_up_session(void)
{
	struct sdr_fwdl_result r;

	setup(1, 4096);
	regions[0].address = 0x900000;
	CHECK(!run_as(SDR_FWDL_PATCH, 0, &r));
	regions[0].address = 0x916400;
	next_container();
	CHECK(!run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_WM, &r) && r.phase == SDR_FWDL_COMPLETE);
	next_container();
	CHECK(run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_WM, &r) == -EALREADY);
	start_working_pda = MT7996_FWDL_START_PDA_DSP;
	CHECK(!run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_DSP, &r));
	next_container();
	start_working_pda = MT7996_FWDL_START_PDA_CR4;
	CHECK(!run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_WA, &r));
	CHECK(run_as(SDR_FWDL_RAM, SDR_FWDL_RAM_WA, &r) == -EALREADY);
	/* The patch may be sent again (the device answers IS_DL). */
	next_container();
	regions[0].address = 0x900000;
	sem_reply_get = MT7996_FWDL_SEM_IS_DL;
	CHECK(!run_as(SDR_FWDL_PATCH, 0, &r));
	teardown();
}

static void state_wait(void)
{
	static const unsigned int ready[] = { 1, 2, 101 };
	struct sdr_fwdl_result r;
	unsigned int value, j, mode;

	/* mt7996_firmware_state(FW_DOWNLOAD): FW_STATE[2:0] read until 1, at
	 * most 101 reads with a 10-20 ms wait between.
	 */
	for (value = 0; value < 8; value++) {
		for (j = 0; j < ARRAY_SIZE(ready); j++) {
			setup(1, 1);
			state_before = 0xabcdeff8U | value;
			state_after = 0x12345679;
			state_ready_at = ready[j];
			CHECK(!run(&r));
			if (value == 1) {
				CHECK(state_reads == 1 && !state_sleeps);
				CHECK(r.last_fw_state ==
				      (ready[j] > 1 ? state_before : state_after));
			} else {
				CHECK(state_reads == ready[j] && state_sleeps == ready[j] - 1);
				CHECK(r.last_fw_state == state_after);
			}
			teardown();
		}
		if (value == 1)
			continue;
		setup(1, 1);
		state_before = 0x87654320 | value;
		state_ready_at = 0;
		time_step = 0;
		CHECK(run(&r) == -ETIMEDOUT && r.phase == SDR_FWDL_WAIT_DOWNLOAD);
		CHECK(state_reads == 101 && state_sleeps == 100 && !r.tx_submitted);
		CHECK(r.last_fw_state == state_before && map_count == RX_MAPS);
		CHECK(run(&r) == -EALREADY && state_reads == 101);
		teardown();
	}
	/* All-ones is a dead link, not state 7. */
	for (j = 0; j < ARRAY_SIZE(ready); j++) {
		setup(1, 1);
		state_ready_at = ready[j];
		state_after = ~0U;
		CHECK(run(&r) == -EIO && r.phase == SDR_FWDL_WAIT_DOWNLOAD);
		CHECK(state_reads == ready[j] && r.last_fw_state == ~0U && !r.tx_submitted);
		teardown();
	}
	for (mode = 0; mode < 2; mode++) {
		setup(1, 1);
		state_ready_at = 0;
		if (mode)
			state_offline_at = 3;
		else
			state_interrupt_at = 3;
		CHECK(run(&r) == (mode ? -EIO : -EINTR));
		CHECK(r.phase == SDR_FWDL_WAIT_DOWNLOAD && state_reads == 3 && !r.tx_submitted);
		teardown();
	}
	/* A rejected request never reads the state. */
	setup(1, 1);
	regions[0].length = 0;
	CHECK(run(&r) == -EINVAL && !state_reads);
	teardown();
}

/* Draining before a command consumes a stream-sized completion as
 * malformed instead of letting it block the event ring; streaming holds the
 * same lock a download does.
 */
static void drain_and_stream(void)
{
	static u8 chunk[3000];
	struct sdr_dma_ring *srx = NULL;
	struct sdr_fwdl_result r;

	setup(1, 1);
	put_unaligned_le32(sizeof(chunk), chunk);
	complete_rx(rx, chunk, sizeof(chunk), ((u32)sizeof(chunk) << 16) | SDR_DMA_LAST);
	emit(rx, 1, MT7996_FWDL_EID_GENERIC, 0, 0);
	sdr_fwdl_begin(loader);
	sdr_fwdl_start_clock(loader, NSEC_PER_SEC);
	CHECK(!sdr_fwdl_discard_events(loader));
	CHECK(loader->progress.malformed_events == 1 && loader->progress.discarded_events == 1);
	CHECK(!loader->progress.tx_submitted && rx->header->completed == 2);
	teardown();

	setup(1, 1);
	CHECK(!sdr_fwdl_stream_begin(loader, &srx) && srx == rx && loader->lock.held);
	CHECK(sdr_fwdl_stream_begin(loader, &srx) == -EBUSY);
	CHECK(run(&r) == -EBUSY);	/* no download while streaming */
	sdr_fwdl_stream_end(loader);
	CHECK(!loader->lock.held && sdr_fwdl_stream_begin(NULL, &srx) == -EINVAL);
	CHECK(!event_count && !ticks);
	teardown();
}

int main(void)
{
	success();
	admission();
	failures();
	ram_image();
	patch();
	bring_up_session();
	state_wait();
	drain_and_stream();
	printf("fwdl: %lu checks\n", checks);
	return 0;
}
