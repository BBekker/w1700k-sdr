/* SPDX-License-Identifier: MIT */
/* Continuous wideband ICAP -> host DMA, plus three host-channelized IQ mailboxes
 * -> MEM_DMA -> dedicated BLE DSP decoders. Header/payload ownership is req/ack.
 * The CPU does channel selection; the DSPs perform demodulation and decoding. */
#include "../../host/ble/decode.h"
#include "wm-output.c"
#define MEM(o) R(0x58000000u + (o))
#define TX ((volatile uint32_t *)0x00910800)
#define RX ((volatile uint32_t *)0x00910900)
#define BOX(b) ((volatile uint32_t *)(0x86200000u + ((b) << 22)))
#define META(b) ((volatile uint32_t *)(0x0090fe90u + 32 * (b)))
#define ST ((volatile uint32_t *)0x0090ec00u)
#define C0 R(0x80023090)
#define GRANT R(0x88009004)
#define RING 0x100000u
#define CHUNK 8192u
static inline void fence(void) { __asm__ volatile("fence iorw,iorw" ::: "memory"); }
static inline uint32_t head(void) { return ((R(0x800230b4) << 2) + 16u) & (RING - 1); }

__attribute__((section(".text.entry.pipeline"))) uint32_t ble_stream(uint32_t unused)
{
    (void)unused;
    uint32_t cpu = R(MT7996_MCU_DMA0_TX0_CIDX), retired = cpu;
    uint32_t desc = R(MT7996_MCU_DMA0_TX0_BASE), slots = R(MT7996_MCU_DMA0_TX0_ADAPTER + 8);
    uint32_t cfg = MEM(0x208), pending[3] = {0}, req[3] = {0}, hostreq[3] = {0}, started[3] = {0};
    uint32_t dma = 0, dma_core = 0, dma_seq = 0, dma_time = 0, prev = 0, status = 0, stopping = 0;
    uint32_t maxlag = 0, last = cycles();
    uint64_t sent = 0, done = 0, written = 0, lap = 0;
    for (unsigned k = 0; k < 64; k++)
        ST[k] = 0;
    ST[0] = 0x424c4531;
    ST[1] = 1;
    if (cpu != R(MT7996_MCU_DMA0_TX0_DIDX) || cpu != R(MT7996_MCU_DMA0_TX0_ADAPTER + 0x2c) ||
        (cfg & 10) || MEM(0x308) != MEM(0x30c) || (C0 & 3) || GRANT)
        return ST[2] = 1;
    for (unsigned b = 0; b < 3; b++) {
        if (BOX(b)[0] != 0x44535031 || BOX(b)[10] != 1 || BOX(b)[1] != BOX(b)[2])
            return ST[2] = 2;
        req[b] = BOX(b)[1];
        for (unsigned j = 0; j < 8; j++)
            META(b)[j] = 0;
        R(0x0090e004u + 1024 * b) = 0;
    }
    MEM(0x208) = cfg & ~5u;
    for (unsigned i = 0; i < 8; i++) {
        unsigned b = i % 3;
        TX[4 * i] = 0x0090b000u + 4096 * b;
        TX[4 * i + 1] = 0x80000000u;
        TX[4 * i + 2] = TX[4 * i + 3] = 0;
        RX[4 * i] = 0x86204000u + (b << 22);
        RX[4 * i + 1] = 4096u << 16;
        RX[4 * i + 2] = RX[4 * i + 3] = 0;
    }
    fence();
    MEM(0x300) = 0x00910800;
    MEM(0x304) = 8;
    MEM(0x308) = 0;
    MEM(0x500) = 0x00910900;
    MEM(0x504) = 8;
    MEM(0x508) = 0;
    MEM(0x20c) = 1;
    MEM(0x280) = 1;
    MEM(0x508) = 7;
    MEM(0x208) = (cfg & ~10u) | 5;
    uint32_t wait = cycles();
    while (R(GO) != 0x474f474f)
        if (cycles() - wait > 1440000000u) {
            status = 3;
            goto finish;
        }
    R(GO) = 0;
    C0 = 0;
    R(0x80023098) = 0;
    R(0x8002309c) = RING / 4 - 4;
    C0 = 0x2400;
    GRANT = 0xff000000;
    C0 = 0x2401;
    C0 = 0x2403;
    ST[1] = 2;
    last = cycles();
    for (;;) {
        uint32_t now = cycles(), gap = now - last;
        last = now;
        if (gap > 720000u) {
            status = 4;
            break;
        }
        if (!stopping) {
            uint32_t pos = head();
            if (pos < prev)
                lap += RING;
            prev = pos;
            written = lap + pos;
            uint64_t lag = written - (uint64_t)done * CHUNK;
            if (lag > maxlag)
                maxlag = (uint32_t)lag;
            if (lag > RING - 0x20000u) {
                status = 5;
                break;
            }
            if (R(GO) == 0x53544f50u) {
                C0 = 0x400;
                pos = head();
                if (pos < prev)
                    lap += RING;
                written = lap + pos;
                stopping = 1;
            }
        }
        uint32_t didx = R(MT7996_MCU_DMA0_TX0_DIDX);
        while (retired != didx) {
            retired = (retired + 1) & 7;
            done++;
        }
        if ((uint64_t)(sent + 1) * CHUNK <= written && ((cpu - retired) & 7) < 7) {
            R(slots + 4 * cpu) = 0;
            R(desc + 16 * cpu) = 0xe0400000u + ((sent * CHUNK) & (RING - 1));
            H(desc + 16 * cpu + 6) = 0x4000u | CHUNK;
            fence();
            cpu = (cpu + 1) & 7;
            R(MT7996_MCU_DMA0_TX0_CIDX) = cpu;
            sent++;
        }
        if (dma) {
            unsigned slot = dma_seq & 7, next = (slot + 1) & 7;
            if (MEM(0x30c) == next && MEM(0x50c) == next && (RX[4 * slot + 1] & 0x80000000u)) {
                fence();
                if (((RX[4 * slot + 1] >> 16) & 0x3fff) != 4096) {
                    status = 6;
                    break;
                }
                unsigned b = dma_core;
                req[b]++;
                if (!req[b])
                    req[b] = 1;
                BOX(b)[3] = 8;
                BOX(b)[4] = 2048;
                BOX(b)[11] = 37 + b + 512 + (META(b)[2] ? 256 : 0);
                fence();
                BOX(b)[1] = req[b];
                pending[b] = 1;
                started[b] = now;
                /* Host requests are serviced round-robin, including descriptor reuse. */
                unsigned nextb = (dma_seq + 8) % 3;
                TX[4 * slot] = 0x0090b000u + 4096 * nextb;
                RX[4 * slot] = 0x86204000u + (nextb << 22);
                RX[4 * slot + 1] = 4096u << 16;
                fence();
                MEM(0x508) = slot;
                dma = 0;
                dma_seq++;
            } else if (now - dma_time > 720000u) {
                status = 7;
                break;
            }
        }
        for (unsigned b = 0; b < 3; b++)
            if (pending[b]) {
                if (BOX(b)[2] == req[b]) {
                    fence();
                    if (BOX(b)[6] || BOX(b)[9] || BOX(b)[10] != 1) {
                        status = 8;
                        goto finish;
                    }
                    const volatile struct ble_output *o =
                        (const volatile struct ble_output *)(0x86208000u + (b << 22));
                    volatile uint32_t *out = (volatile uint32_t *)(0x0090e000u + 1024 * b);
                    unsigned at = 8, count = o->count;
                    if (count > BLE_MAX_EVENTS) {
                        status = 9;
                        goto finish;
                    }
                    out[0] = 0x424c4550;
                    out[2] = count;
                    out[3] = o->dropped;
                    out[4] = o->samples;
                    out[5] = o->crc_fail;
                    out[6] = 0;
                    out[7] = BOX(b)[5];
                    for (unsigned j = 0; j < count; j++) {
                        unsigned n = o->packets[j].length;
                        if (n < 2 || n > BLE_MAX_PDU || at + 2 + (n + 6) / 4 > 256) {
                            status = 10;
                            goto finish;
                        }
                        out[at++] = o->packets[j].sample;
                        out[at++] = n;
                        volatile unsigned char *dst = (volatile unsigned char *)(out + at);
                        for (unsigned k = 0; k < n + 3; k++)
                            dst[k] = o->packets[j].pdu[k];
                        for (unsigned k = n + 3; k < ((n + 6) & ~3u); k++)
                            dst[k] = 0;
                        at += (n + 6) / 4;
                    }
                    out[6] = at * 4;
                    fence();
                    out[1] = (hostreq[b] << 4) | count;
                    META(b)[4] = at * 4;
                    META(b)[5] = BOX(b)[5];
                    META(b)[6] = 0;
                    fence();
                    META(b)[1] = hostreq[b];
                    pending[b] = 0;
                    ST[8 + b]++;
                } else if (now - started[b] > 7200000u) {
                    status = 11;
                    goto finish;
                }
            }
        if (!dma && !stopping) {
            unsigned b = dma_seq % 3;
            if (!pending[b] && META(b)[0] != META(b)[1]) {
                if (META(b)[3] != 2048) {
                    status = 12;
                    break;
                }
                hostreq[b] = META(b)[0];
                unsigned slot = dma_seq & 7;
                TX[4 * slot + 1] = 0x50000000u;
                fence();
                dma_time = cycles();
                dma_core = b;
                dma = 1;
                MEM(0x308) = (slot + 1) & 7;
            }
        }
        ST[3] = sent;
        ST[4] = done;
        ST[5] = maxlag;
        if (stopping && sent == written / CHUNK && done == sent && !dma && !pending[0] &&
            !pending[1] && !pending[2])
            break;
    }
finish:
    wait = cycles();
    while (R(MT7996_MCU_DMA0_TX0_DIDX) != cpu)
        if (cycles() - wait > 7200000u) {
            status = 13;
            break;
        }
    MEM(0x208) = cfg & ~5u;
    C0 = 0x400;
    GRANT = 0;
    if (!status) {
        R(MT7996_MCU_DMA0_TX0_ADAPTER + 0x2c) = cpu;
        R(MT7996_MCU_DMA0_TX0_ADAPTER + 0x34) += sent;
        if (sent >= 8)
            R(MT7996_MCU_DMA0_TX0_ADAPTER + 0x38) += sent - 8;
        reclaim();
    }
    ST[2] = status;
    ST[3] = sent;
    ST[4] = done;
    ST[5] = maxlag;
    ST[6] = MEM(0x110);
    ST[7] = R(MT7996_MCU_DMA0_INT_STA);
    fence();
    ST[1] = 3;
    return status;
}
