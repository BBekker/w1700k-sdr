/* SPDX-License-Identifier: GPL-2.0-only */
#include <openssl/sha.h>
static inline void sha256_csum_wd(const unsigned char *p,unsigned int n,unsigned char out[32],unsigned int chunk)
{ (void)chunk; SHA256(p,n,out); }
