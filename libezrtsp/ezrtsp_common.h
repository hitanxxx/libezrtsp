#ifndef __EZRTSP_COMMON_H__
#define __EZRTSP_COMMON_H__

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


/* This uses that the expression (n+(k-1))/k means the smallest
   integer >= n/k, i.e., the ceiling of n/k.  */
#ifndef EZRTSP_BASE64_LENGTH
#define EZRTSP_BASE64_LENGTH(inlen) ((((inlen) + 2) / 3) * 4)
#endif

#ifndef sys_msleep
#define sys_msleep(msec) (usleep(1000*msec))
#endif

/// improve compile performance
#ifndef LIKELY
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#endif

#ifndef err
#define err(format, ...) \
do { \
    printf("[ERROR] %lu (%s)%s:%d "format, time(NULL), __FILE__, __func__, __LINE__, ##__VA_ARGS__);\
} while(0);
#endif

#ifndef dbg
#define dbg(format, ...) \
do { \
    printf("[DEBUG] %lu (%s)%s:%d "format, time(NULL), __FILE__, __func__, __LINE__, ##__VA_ARGS__);\
} while(0);
#endif

#ifndef EZRTSP_THNAME
#define EZRTSP_THNAME(name) (prctl(PR_SET_NAME, (unsigned long)name))
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef SYS_ABS
#define SYS_ABS(a) ( ((a)>=0)?(a):(0-(a)) )
#endif

#ifndef SYS_MAX
#define SYS_MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef SYS_MIN
#define SYS_MIN(a,b) ((a)>(b)?(b):(a))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

#ifndef ptr_get_struct
#define ptr_get_struct( ptr, struct_type, struct_member ) \
(\
    (struct_type *)\
    (((unsigned char*)ptr)-offsetof(struct_type,struct_member))\
)
#endif

/// macro append after struct data for align with 1 Byte
#ifndef __packed
#define __packed  __attribute__((packed))
#endif


typedef struct queue_s ezrtsp_queue_t;
struct queue_s {
    ezrtsp_queue_t  *prev;
    ezrtsp_queue_t  *next;
};

typedef struct ezrtsp_meta_t ezrtsp_meta_t;
struct ezrtsp_meta_t {
	ezrtsp_meta_t * next;
	char * start;
	char * end;
	char * pos;
	char * last;
	char data[0];
};

typedef struct ezrtsp_data {
	int datan;
	unsigned char data[0];
} ezrtsp_data_t;

#ifndef meta_getfree
#define meta_getfree(x) ((x)->end - (x)->last)
#endif

#ifndef meta_getlen
#define meta_getlen(x) ((x)->last - (x)->pos)
#endif

#ifndef meta_cap
#define meta_cap(x) ((x)->end - (x)->start)
#endif

#ifndef meta_clr
#define meta_clr(x) \
do { \
	(x)->pos = (x)->last = (x)->start; \
	memset(x->start, 0x0, meta_cap(x)); \
} while(0); 
#endif

int ezrtsp_base64_encode(const char *in, int inlen, char *out, int outlen);
int ezrtsp_base64_decode(const char *in, int inlen, char *out, int outlen);

// common function
void ezrtsp_free(void * addr);
void * ezrtsp_alloc(int size);
unsigned long long ezrtsp_ts_msec(void);

void ezrtsp_queue_init(ezrtsp_queue_t * q);
void ezrtsp_queue_insert(ezrtsp_queue_t * h, ezrtsp_queue_t * q);
void ezrtsp_queue_insert_tail(ezrtsp_queue_t * h, ezrtsp_queue_t * q);
void ezrtsp_queue_remove(ezrtsp_queue_t * q);
int ezrtsp_queue_empty(ezrtsp_queue_t * h);
ezrtsp_queue_t * ezrtsp_queue_head(ezrtsp_queue_t * h);
ezrtsp_queue_t * ezrtsp_queue_next(ezrtsp_queue_t * q);
ezrtsp_queue_t * ezrtsp_queue_prev(ezrtsp_queue_t * q);
ezrtsp_queue_t * ezrtsp_queue_tail(ezrtsp_queue_t * h);

int ezrtsp_meta_alloc(ezrtsp_meta_t ** meta, int size);
void ezrtsp_meta_free(ezrtsp_meta_t * meta);

#endif
