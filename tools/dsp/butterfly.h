/* SPDX-License-Identifier: MIT */
#ifndef DSP_BUTTERFLY_H
#define DSP_BUTTERFLY_H
static inline void store4(float *a, float *b, float *c, float *d,
                          float ar,float ai,float br,float bi,float cr,float ci,float dr,float di)
{
    float acr=ar+cr, aci=ai+ci, amcr=ar-cr, amci=ai-ci;
    float bdr=br+dr, bdi=bi+di, bmdr=br-dr, bmdi=bi-di;
    a[0]=acr+bdr; a[1]=aci+bdi;
    b[0]=acr-bdr; b[1]=aci-bdi;
    c[0]=amcr+bmdi; c[1]=amci-bmdr;
    d[0]=amcr-bmdi; d[1]=amci+bmdr;
}

/* Combine the first three binary stages in registers. Twiddles are only
 * 1, -i and (+/-1-i)/sqrt(2): no table loads or general complex multiplies. */
static inline void first8(float *x, unsigned n)
{
    const float h=0x1.6a09e6p-1f;
    for(float *p=x;p<x+2*n;p+=16) {
        float ar=p[0]+p[2], ai=p[1]+p[3], ax=p[0]-p[2], ay=p[1]-p[3];
        float cr=p[4]+p[6], ci=p[5]+p[7], cx=p[4]-p[6], cy=p[5]-p[7];
        float br=p[8]+p[10], bi=p[9]+p[11], bx=p[8]-p[10], by=p[9]-p[11];
        float dr=p[12]+p[14], di=p[13]+p[15], dx=p[12]-p[14], dy=p[13]-p[15];
        store4(p,p+8,p+4,p+12,ar,ai,br,bi,cr,ci,dr,di);
        store4(p+2,p+10,p+6,p+14,ax,ay,(bx+by)*h,(by-bx)*h,cy,-cx,(dy-dx)*h,-(dx+dy)*h);
    }
}

#endif
