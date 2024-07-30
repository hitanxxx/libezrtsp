#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <time.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/socket.h>
#include <malloc.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sched.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <asm/types.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <linux/ethtool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>

typedef enum {
	AT_PCM = 0,
	AT_G711A,	
	AT_G711U,	
	AT_AAC,		
	AT_MP3,		
	AT_OPUS,     
	AT_MAX		
} audio_codec_typ;

typedef enum {
    VT_H264 = 0,
    VT_H265,
} video_codec_typ;

// hm2p base64 use
/* This uses that the expression (n+(k-1))/k means the smallest
   integer >= n/k, i.e., the ceiling of n/k.  */
#define HM2P_BASE64_LENGTH(inlen) ((((inlen) + 2) / 3) * 4)

#define sys_msleep(msec) (usleep(1000*msec))

/// improve compile performance
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)

#define err(format, ...) \
do { \
    printf("[ERROR] %lu (%s)%s:%d "format, time(NULL), __FILE__, __func__, __LINE__, ##__VA_ARGS__);\
} while(0);

#define dbg(format, ...) \
do { \
    printf("[DEBUG] %lu (%s)%s:%d "format, time(NULL), __FILE__, __func__, __LINE__, ##__VA_ARGS__);\
} while(0);



#define SET_THREAD_NAME(name) (prctl(PR_SET_NAME, (unsigned long)name))

#define IN
#define OUT

#define SYS_ABS(a) ( ((a)>=0)?(a):(0-(a)) )
#define SYS_MAX(a,b) ((a)>(b)?(a):(b))
#define SYS_MIN(a,b) ((a)>(b)?(b):(a))
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define ptr_get_struct( ptr, struct_type, struct_member ) \
(\
    (struct_type *)\
    (((unsigned char*)ptr)-offsetof(struct_type,struct_member))\
)



/// macro append after struct data for align with 1 Byte
#ifndef __packed
#define __packed  __attribute__((packed))
#endif



typedef struct queue_s queue_t;
struct queue_s
{
    queue_t  *prev;
    queue_t  *next;
};

typedef struct meta_t meta_t;
struct meta_t {
	meta_t * next;
	char * start;
	char * end;
	char * pos;
	char * last;
	char data[0];
};

typedef struct sys_data {
	int datan;
	char data[0];
} sys_data_t;

#define meta_getfree(x) ((x)->end - (x)->last)
#define meta_getlen(x) ((x)->last - (x)->pos)
#define meta_cap(x) ((x)->end - (x)->start)
#define meta_clr(x) \
do { \
	(x)->pos = (x)->last = (x)->start; \
	memset(x->start, 0x0, meta_cap(x)); \
} while(0); 

int sys_base64_encode(const char *in, int inlen, char *out, int outlen);
int sys_base64_decode(const char *in, int inlen, char *out, int outlen);

// common function
void sys_free(void * addr);
void * sys_alloc(int size);
unsigned long long sys_ts_msec(void);

void queue_init(queue_t * q);
void queue_insert(queue_t * h, queue_t * q);
void queue_insert_tail(queue_t * h, queue_t * q);
void queue_remove(queue_t * q);
int queue_empty(queue_t * h);
queue_t * queue_head(queue_t * h);
queue_t * queue_next(queue_t * q);
queue_t * queue_prev(queue_t * q);
queue_t * queue_tail(queue_t * h);

int meta_alloc(meta_t ** meta, int size);
void meta_free(meta_t * meta);

#endif
