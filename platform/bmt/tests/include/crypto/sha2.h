/* SPDX-License-Identifier: GPL-2.0-only */
#include <openssl/sha.h>
static inline void sha256(const unsigned char *p,size_t n,unsigned char out[32])
{ SHA256(p,n,out); }
