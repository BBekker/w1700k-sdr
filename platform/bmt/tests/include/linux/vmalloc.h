/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdlib.h>
extern int fail_alloc;
static inline void *kvzalloc(size_t n,int flags) { (void)flags; return fail_alloc ? NULL : calloc(1,n); }
#define kvfree(p) free(p)
