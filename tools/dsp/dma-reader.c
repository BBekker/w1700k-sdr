/* SPDX-License-Identifier: MIT */
/* Consume DSP metadata/power packets from the existing host DMA mmap ring.
 * The pump thread re-arms RX descriptors; this consumer validates every result
 * and publishes the latest spectrum at 20 Hz. No MMIO payload reads, CPU FFT,
 * raw IQ transfer, or per-FFT firmware command. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "../../include/uapi/w1700k_sdr.h"

static const struct w1700k_sdr_stream_ring *ring;
static const unsigned char *data;
static const char *output;
static char temporary[4096];
static double center, rate;
static atomic_int done;
static uint64_t begin_seq, frames, packets, published, invalid, lapped;
static uint32_t max_lag;
static struct {
    char magic[8]; uint32_t n,core_mask; uint64_t sequence;
    double center,rate; uint32_t dsp_cycles,dma_cycles;uint64_t timestamp;
} header={.magic="DSPFFT2",.n=2048,.core_mask=7};
_Static_assert(sizeof(header)==56,"spectrum header layout");
static uint64_t ns(clockid_t clock)
{
    struct timespec t;clock_gettime(clock,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;
}
static uint64_t completed(void) {return __atomic_load_n(&ring->completed,__ATOMIC_ACQUIRE);}
static void fail(const char *why)
{
    fprintf(stderr,"DSP DMA reader: %s\n",why);invalid++;
    kill(getpid(),SIGTERM);
}
static void *consume(void *unused)
{
    (void)unused;
    uint64_t next=begin_seq,last_publish=0,expected=0;
    uint32_t meta[16]={0};int have_meta=0;
    for(;;) {
        uint64_t c=completed();
        if(next==c) {
            if(atomic_load(&done)) break;
            struct timespec delay={.tv_nsec=100000};nanosleep(&delay,NULL);continue;
        }
        if(c-next>ring->slices-ring->slots) {lapped++;fail("host DMA ring overwritten");break;}
        if(c-next>max_lag)max_lag=(uint32_t)(c-next);
        uint32_t slot=next%ring->slices;
        uint32_t length=(ring->ctrl[slot]>>16)&0x3fff;
        const unsigned char *p=data+(size_t)slot*ring->slice_bytes;
        if(!have_meta) {
            if(length!=64) {fail("expected metadata packet");break;}
            memcpy(meta,p,64);
            if(meta[0]!=0x44535050 || meta[1]!=(uint32_t)expected || meta[2]>2 || meta[3]>1 || meta[4]!=2048) {
                fail("invalid metadata or sequence gap");break;
            }
            have_meta=1;
        } else {
            if(length!=8192) {fail("expected power packet");break;}
            const uint32_t *words=(const uint32_t *)p;
            unsigned i;
            for(i=0;i<2048;i++) if((words[i]&0x80000000u) || (words[i]&0x7f800000u)==0x7f800000u) break;
            if(i!=2048) {fail("nonfinite or negative FFT power");break;}
            uint64_t now=ns(CLOCK_MONOTONIC);
            if(now-last_publish>=50000000u) {
                header.sequence=expected+1;header.center=center;header.rate=rate;
                header.dsp_cycles=meta[5];header.dma_cycles=meta[6];header.timestamp=ns(CLOCK_REALTIME);
                FILE *f=fopen(temporary,"wb");
                if(!f) {fail("cannot open output");break;}
                int ok=fwrite(&header,1,sizeof(header),f)==sizeof(header) && fwrite(p,1,8192,f)==8192;
                if(fclose(f))ok=0;
                if(completed()-next>ring->slices-ring->slots) {lapped++;fail("power overwritten during publication");break;}
                if(!ok || rename(temporary,output)) {fail("cannot publish output");break;}
                last_publish=now;published++;
            }
            frames++;expected++;have_meta=0;
        }
        if(completed()-next>ring->slices-ring->slots) {lapped++;fail("packet overwritten during read");break;}
        next++;packets++;
    }
    if(have_meta && !invalid)fail("stream ended with an incomplete result");
    unlink(temporary);
    return NULL;
}
static void on_signal(int sig) {(void)sig;}
int main(int argc,char **argv)
{
    if(argc!=6) {fprintf(stderr,"usage: dsp-dma-reader DEVICE OUTPUT SECONDS CENTER_HZ SAMPLE_RATE\n");return 2;}
    char *end;double seconds=strtod(argv[3],&end);
    if(*end || !isfinite(seconds) || seconds<=0 || seconds>604800)return 2;
    center=strtod(argv[4],&end);if(*end || !isfinite(center) || center<=0)return 2;
    rate=strtod(argv[5],&end);if(*end || rate!=40000000)return 2;
    output=argv[2];if(snprintf(temporary,sizeof(temporary),"%s.partial",output)>=(int)sizeof(temporary))return 2;
    int fd=open(argv[1],O_RDWR|O_CLOEXEC);if(fd<0){perror("open");return 1;}
    struct w1700k_sdr_info info;
    if(ioctl(fd,W1700K_SDR_GET_INFO,&info) || info.abi_version!=W1700K_SDR_ABI_VERSION || !(info.capabilities&W1700K_SDR_CAP_STREAM_RX)) {
        fprintf(stderr,"driver ABI or stream capability mismatch\n");close(fd);return 1;
    }
    struct w1700k_sdr_stream_rx geometry={0};
    if(ioctl(fd,W1700K_SDR_STREAM_RX,&geometry) || !geometry.slices || geometry.slice_bytes!=8192 || geometry.slots>=geometry.slices) {
        fprintf(stderr,"invalid DMA ring geometry\n");close(fd);return 1;
    }
    size_t size=geometry.header_bytes+(size_t)geometry.slices*geometry.slice_bytes;
    void *map=mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);if(map==MAP_FAILED){perror("mmap");close(fd);return 1;}
    ring=map;data=(const unsigned char *)map+geometry.header_bytes;begin_seq=completed();
    struct sigaction sa={.sa_handler=on_signal};sigaction(SIGTERM,&sa,NULL);sigaction(SIGINT,&sa,NULL);sigaction(SIGHUP,&sa,NULL);
    sigset_t stops;sigemptyset(&stops);sigaddset(&stops,SIGTERM);sigaddset(&stops,SIGINT);sigaddset(&stops,SIGHUP);
    pthread_sigmask(SIG_BLOCK,&stops,NULL);
    pthread_t thread;
    if(pthread_create(&thread,NULL,consume,NULL)) {munmap(map,size);close(fd);return 1;}
    pthread_sigmask(SIG_UNBLOCK,&stops,NULL);
    struct w1700k_sdr_stream_rx req={.timeout_ms=(uint32_t)(seconds*1000),.idle_ms=1000,
        .go_address=0x0090f008,.go_value=0x474f474f,.stop_address=0x0090f008,.stop_value=0x53544f50};
    int rc=ioctl(fd,W1700K_SDR_STREAM_RX,&req),saved_errno=rc?errno:0;
    atomic_store(&done,1);pthread_join(thread,NULL);
    printf("{\"fft_frames\":%llu,\"packets\":%llu,\"published\":%llu,\"invalid\":%llu,\"lapped\":%llu,\"max_backlog\":%u,\"pump_buffers\":%llu,\"pump_malformed\":%u,\"pump_errno\":%d}\n",
        (unsigned long long)frames,(unsigned long long)packets,(unsigned long long)published,(unsigned long long)invalid,
        (unsigned long long)lapped,max_lag,(unsigned long long)req.buffers,req.malformed,saved_errno);
    munmap(map,size);close(fd);
    return (saved_errno && saved_errno!=EINTR) || invalid || lapped || req.malformed || req.buffers!=packets;
}
