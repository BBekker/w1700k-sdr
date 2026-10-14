/* SPDX-License-Identifier: MIT */
#include "mac.h"
/* Deterministic on-core check against integer arithmetic, including both
 * accumulators, signs, rounding ties and products wider than 32 bits. */
uint32_t dsp_mac_check(void)
{
    static const int32_t cases[][5] = {
        {3, 4, 5, -6, 0}, {-3, 4, 5, 6, 0},
        {1, -1, 1, 0, 1}, {-1, 1, 1, 0, 1},
        {131071, -131072, 32768, 0, 15},
        {-131072, 131071, 23170, -23170, 15},
        {262143, 262143, 32767, 32767, 15},
        {-262143, -262143, -32768, -32768, 15},
    };
    for (unsigned k=0;k<sizeof(cases)/sizeof(cases[0]);k++) {
        int32_t a=cases[k][0], b=cases[k][1], c=cases[k][2], d=cases[k][3];
        unsigned s=(unsigned)cases[k][4];
        int32_t re, im;
        int64_t bias=s ? (int64_t)1<<(s-1) : 0;
        int32_t rr=(int32_t)(((int64_t)a*c-(int64_t)b*d+bias)>>s);
        int32_t ri=(int32_t)(((int64_t)a*d+(int64_t)b*c+bias)>>s);
        mrv_cmul(a,b,c,d,s,&re,&im);
        if (re!=rr || im!=ri) return k+1;
    }
    return 0;
}
