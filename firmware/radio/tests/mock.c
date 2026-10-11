// SPDX-License-Identifier: MIT
/* hal.h for the host test: see mock.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock.h"

enum kind { W, R, AW, AR, E };
static const char *const kind_name[] = { "W", "R", "AW", "AR", "E" };

struct rec {
	enum kind kind;
	uint32_t addr, val;	/* addr: D-die address or HAL_AREG(die, k, idx) */
	unsigned line;
};

static struct rec *recs;
static size_t nrecs, cur;
static int free_run;
static uint32_t clock_us;
unsigned long mock_accesses;
struct hal_fail hal_fail_rec;
uint8_t radio_ee[RADIO_EE_BYTES];
struct radio_id radio_id;

/* Free-run register file: (key, value) with A-die keys tagged in bit 63's place (bit 32). */
struct cell {
	uint64_t key;
	uint32_t val;
};
static struct cell *cells;
static size_t ncells, capcells;

static struct cell *cell(uint64_t key, int create)
{
	for (size_t i = 0; i < ncells; i++)
		if (cells[i].key == key)
			return &cells[i];
	if (!create)
		return NULL;
	if (ncells == capcells) {
		capcells = capcells ? 2 * capcells : 1024;
		cells = realloc(cells, capcells * sizeof(*cells));
	}
	cells[ncells] = (struct cell){ key, 0 };
	return &cells[ncells++];
}

#define AKEY(areg) ((uint64_t)1 << 32 | (areg))

void mock_preset(uint32_t addr, uint32_t value)
{
	cell(addr, 1)->val = value;
}

void mock_apreset(uint32_t die, uint32_t k, uint32_t idx, uint32_t value)
{
	cell(AKEY(HAL_AREG(die, k, idx)), 1)->val = value;
}

static void print_rec(FILE *f, enum kind kind, uint32_t addr, uint32_t val)
{
	if (kind == AW || kind == AR)
		fprintf(f, "%s %x %x %03x %08x", kind_name[kind], addr >> 16, addr >> 12 & 15, addr & 0xfff, val);
	else
		fprintf(f, "%s %08x %08x", kind_name[kind], addr, val);
}

void mock_open(const char *path)
{
	char line[256];
	unsigned n = 0;
	FILE *f;

	if (!strcmp(path, "-")) {
		free_run = 1;
		return;
	}
	if (!(f = fopen(path, "r"))) {
		perror(path);
		exit(2);
	}
	while (fgets(line, sizeof(line), f)) {
		char k[4];
		unsigned a, b, c, d;
		char *hash = strchr(line, '#');
		struct rec r = { .line = ++n };

		if (hash)
			*hash = 0;
		if (sscanf(line, "%3s", k) != 1)
			continue;
		if (!strcmp(k, "W") || !strcmp(k, "R")) {
			if (sscanf(line, "%*s %x %x", &a, &b) != 2)
				goto bad;
			r.kind = k[0] == 'W' ? W : R;
			r.addr = a;
			r.val = b;
		} else if (!strcmp(k, "E")) {
			if (sscanf(line, "%*s %x %x", &a, &b) != 2)
				goto bad;
			r.kind = E;
			r.addr = a;
			r.val = b;
		} else if (!strcmp(k, "AW") || !strcmp(k, "AR")) {
			if (sscanf(line, "%*s %x %x %x %x", &a, &b, &c, &d) != 4)
				goto bad;
			r.kind = k[1] == 'W' ? AW : AR;
			r.addr = HAL_AREG(a, b, c);
			r.val = d;
		} else {
			goto bad;
		}
		recs = realloc(recs, (nrecs + 1) * sizeof(*recs));
		recs[nrecs++] = r;
	}
	fclose(f);
	return;
bad:
	fprintf(stderr, "%s:%u: bad line: %s", path, n, line);
	exit(2);
}

static void mismatch(enum kind kind, uint32_t addr, uint32_t val)
{
	fprintf(stderr, "MISMATCH after %lu accesses: got ", mock_accesses);
	print_rec(stderr, kind, addr, val);
	if (cur < nrecs) {
		fprintf(stderr, ", expected (line %u) ", recs[cur].line);
		print_rec(stderr, recs[cur].kind, recs[cur].addr, recs[cur].val);
	} else {
		fprintf(stderr, ", expected list exhausted");
	}
	fprintf(stderr, "\n");
	for (size_t i = cur > 4 ? cur - 4 : 0; i < cur; i++) {
		fprintf(stderr, "  matched (line %u) ", recs[i].line);
		print_rec(stderr, recs[i].kind, recs[i].addr, recs[i].val);
		fprintf(stderr, "\n");
	}
	exit(1);
}

/* One access: checked (and a read's value served) or, free running, logged. */
static uint32_t access(enum kind kind, uint32_t addr, uint32_t val)
{
	mock_accesses++;
	clock_us++;		/* so a module's own wait loop on hal_now_us() ends */
	if (free_run) {
		uint64_t key = kind == AW || kind == AR ? AKEY(addr) : addr;

		if (kind == W || kind == AW) {
			cell(key, 1)->val = val;
		} else {
			struct cell *c = cell(key, 0);

			val = c ? c->val : 0;
		}
		print_rec(stdout, kind, addr, val);
		printf("\n");
		return val;
	}
	if (cur >= nrecs || recs[cur].kind != kind || recs[cur].addr != addr ||
	    ((kind == W || kind == AW) && recs[cur].val != val))
		mismatch(kind, addr, val);
	return recs[cur++].val;
}

/* A poll: the expected reads of this register up to the first that satisfies it. */
static uint32_t poll(enum kind kind, uint32_t addr, uint32_t mask, uint32_t want, int *ok)
{
	uint32_t v;

	if (free_run) {
		v = access(kind, addr, 0);
		*ok = (v & mask) == want;
		return v;
	}
	v = access(kind, addr, 0);
	while ((v & mask) != want) {
		if (cur >= nrecs || recs[cur].kind != kind || recs[cur].addr != addr) {
			*ok = 0;
			return v;
		}
		v = access(kind, addr, 0);
	}
	*ok = 1;
	return v;
}

int mock_finish(void)
{
	if (free_run || cur == nrecs) {
		fprintf(stderr, "OK %lu accesses\n", mock_accesses);
		return 0;
	}
	fprintf(stderr, "INCOMPLETE: %lu accesses made, %zu expected left from line %u: ", mock_accesses,
		nrecs - cur, recs[cur].line);
	print_rec(stderr, recs[cur].kind, recs[cur].addr, recs[cur].val);
	fprintf(stderr, "\n");
	return 1;
}

void mock_eeprom(const char *path)
{
	FILE *f = fopen(path, "rb");

	if (!f) {
		perror(path);
		exit(2);
	}
	memset(radio_ee, 0, sizeof(radio_ee));
	if (fread(radio_ee, 1, sizeof(radio_ee), f) == 0) {
		fprintf(stderr, "%s: empty\n", path);
		exit(2);
	}
	fclose(f);
}

void hal_fail(uint32_t code, uint32_t arg)
{
	if (!hal_fail_rec.count++) {
		hal_fail_rec.code = code;
		hal_fail_rec.arg = arg;
	}
}

uint32_t hal_rd(uint32_t addr)
{
	return access(R, addr, 0);
}

void hal_wr(uint32_t addr, uint32_t val)
{
	access(W, addr, val);
}

uint32_t hal_poll(uint32_t addr, uint32_t mask, uint32_t val, uint32_t timeout_us, int soft)
{
	int ok;
	uint32_t v = poll(R, addr, mask, val, &ok);

	if (!ok) {
		clock_us += timeout_us;
		if (!soft)
			hal_fail(HAL_FAIL_POLL, addr);
	}
	return v;
}

uint32_t hal_ard(uint32_t die, uint32_t k, uint32_t idx)
{
	return access(AR, HAL_AREG(die, k, idx), 0);
}

void hal_awr(uint32_t die, uint32_t k, uint32_t idx, uint32_t val)
{
	access(AW, HAL_AREG(die, k, idx), val);
}

uint32_t hal_apoll(uint32_t die, uint32_t k, uint32_t idx, uint32_t mask, uint32_t val, uint32_t timeout_us,
		   int soft)
{
	int ok;
	uint32_t v = poll(AR, HAL_AREG(die, k, idx), mask, val, &ok);

	if (!ok) {
		clock_us += timeout_us;
		if (!soft)
			hal_fail(HAL_FAIL_APOLL, HAL_AREG(die, k, idx));
	}
	return v;
}

int hal_efuse8_soft(uint32_t addr, uint8_t *val)
{
	*val = (uint8_t)access(E, addr, 0);
	return 0;
}

/* ROM routines do not run on the host: the call is printed when free running, else ignored. */
uint32_t hal_rom_call4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
	if (free_run)
		printf("# rom call %08x(%x, %x, %x, %x)\n", fn, a0, a1, a2, a3);
	return 0;
}

void hal_udelay(uint32_t us)
{
	clock_us += us;
}

uint32_t hal_now_us(void)
{
	return clock_us;
}
