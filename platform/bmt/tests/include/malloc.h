/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdlib.h>
extern int fail_alloc;
static inline void *test_alloc(size_t n) { return fail_alloc ? NULL : malloc(n); }
static inline void *test_calloc(size_t n,size_t size) { return fail_alloc ? NULL : calloc(n,size); }
#define malloc(n) test_alloc(n)
#define calloc(n,s) test_calloc(n,s)
