// mmaptrack.c — LD_PRELOAD: track net mmap/munmap bytes per call-site, plus
// periodically print total RSS and mmap-live so we can see the mmap-level leak
// the malloc interposer misses. Anonymous mmaps only (the ones that grow RSS).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

static void *(*real_mmap)(void*,size_t,int,int,int,off_t)=0;
static int (*real_munmap)(void*,size_t)=0;
static __thread int in_hook=0;
#define NSITE 16384
typedef struct { void*k[3]; long long bytes; long long allocs; } Site;
static Site sites[NSITE]; static int nsite=0;
static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;
#define NREC (1<<20)
typedef struct { void*p; size_t sz; int sidx; } Rec;
static Rec recs[NREC];
static long long g_mmapLive=0, g_mmapTotal=0, g_munmapTotal=0;

static unsigned long hp(void*p){unsigned long x=(unsigned long)p;x^=x>>33;x*=0xff51afd7ed558ccdUL;x^=x>>33;return x;}
static int find_site(void**bt){unsigned long h=hp(bt[0])&(NSITE-1);
    for(int i=0;i<NSITE;i++){unsigned long j=(h+i)&(NSITE-1);
        if(sites[j].allocs==0&&sites[j].k[0]==0){memcpy(sites[j].k,bt,3*sizeof(void*));nsite++;return j;}
        if(memcmp(sites[j].k,bt,3*sizeof(void*))==0)return j;}return -1;}
static void rec_put(void*p,size_t sz,int s){unsigned long h=hp(p)&(NREC-1);
    for(int i=0;i<NREC;i++){unsigned long j=(h+i)&(NREC-1);if(recs[j].p==0||recs[j].p==p){recs[j].p=p;recs[j].sz=sz;recs[j].sidx=s;return;}}}
static int rec_take(void*p,size_t*sz){unsigned long h=hp(p)&(NREC-1);
    for(int i=0;i<NREC;i++){unsigned long j=(h+i)&(NREC-1);if(recs[j].p==0)return -1;
        if(recs[j].p==p){if(sz)*sz=recs[j].sz;int s=recs[j].sidx;recs[j].p=(void*)-1;return s;}}return -1;}

static long rss_kb(){FILE*f=fopen("/proc/self/status","r");if(!f)return -1;char l[256];long r=-1;
    while(fgets(l,sizeof(l),f)){if(!strncmp(l,"VmRSS:",6)){r=atol(l+6);break;}}fclose(f);return r;}

static void dump(){in_hook=1;pthread_mutex_lock(&mtx);
    FILE*f=fopen(getenv("MMAP_OUT")?:"/tmp/mmaptrack.txt","a");if(!f){pthread_mutex_unlock(&mtx);in_hook=0;return;}
    fprintf(f,"=== MMAP dump t=%ld RSS=%ldKB mmapLive=%.2fMB total=%.2fMB unmap=%.2fMB nsite=%d ===\n",
            (long)time(0),rss_kb(),g_mmapLive/1048576.0,g_mmapTotal/1048576.0,g_munmapTotal/1048576.0,nsite);
    Site top[20];memset(top,0,sizeof(top));
    for(int i=0;i<NSITE;i++){if(sites[i].allocs==0)continue;
        for(int k=0;k<20;k++)if(sites[i].bytes>top[k].bytes){for(int m=19;m>k;m--)top[m]=top[m-1];top[k]=sites[i];break;}}
    for(int k=0;k<20;k++){if(top[k].k[0]==0)break;
        fprintf(f,"  liveMmapB=%.2fMB allocs=%lld:\n",top[k].bytes/1048576.0,top[k].allocs);
        char**s=backtrace_symbols(top[k].k,3);for(int q=0;q<3;q++)fprintf(f,"      %s\n",s?s[q]:"?");if(s)free(s);}
    fclose(f);pthread_mutex_unlock(&mtx);in_hook=0;}
static void*dumper(void*a){(void)a;int per=getenv("MMAP_PERIOD")?atoi(getenv("MMAP_PERIOD")):25;
    for(;;){struct timespec ts={per,0};nanosleep(&ts,0);dump();}}
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void spawn(){pthread_t t;if(pthread_create(&t,0,dumper,0)==0)pthread_detach(t);}

void*mmap(void*a,size_t len,int prot,int flags,int fd,off_t off){
    if(!real_mmap)real_mmap=dlsym(RTLD_NEXT,"mmap");
    void*p=real_mmap(a,len,prot,flags,fd,off);
    if(!in_hook && p!=MAP_FAILED && (flags&MAP_ANONYMOUS) && fd<0){
        in_hook=1;pthread_once(&once,spawn);
        void*bt[5];int n=backtrace(bt,5);void*fr[3]={n>2?bt[2]:0,n>3?bt[3]:0,n>4?bt[4]:0};
        pthread_mutex_lock(&mtx);int s=find_site(fr);if(s>=0){sites[s].allocs++;sites[s].bytes+=len;rec_put(p,len,s);}
        g_mmapLive+=len;g_mmapTotal+=len;pthread_mutex_unlock(&mtx);in_hook=0;}
    return p;}
int munmap(void*p,size_t len){
    if(!real_munmap)real_munmap=dlsym(RTLD_NEXT,"munmap");
    if(!in_hook && p){in_hook=1;pthread_mutex_lock(&mtx);size_t sz=0;int s=rec_take(p,&sz);
        if(s>=0){sites[s].bytes-=sz;sites[s].allocs--;g_mmapLive-=sz;g_munmapTotal+=sz;}pthread_mutex_unlock(&mtx);in_hook=0;}
    return real_munmap(p,len);}
