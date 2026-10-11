/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdlib.h>
#define GFP_KERNEL 0
extern int fail_alloc;
static inline void *test_alloc(size_t n) { return fail_alloc ? NULL : malloc(n); }
#define kmalloc(n,f) test_alloc(n)
#define kfree(p) free(p)
