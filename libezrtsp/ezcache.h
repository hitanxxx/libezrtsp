#ifndef __EZCACHE_H__
#define __EZCACHE_H__

#include "ezrtsp_common.h"

enum {
    SYS_CACHE_VIDEO_MAIN = 0,
    SYS_CACHE_VIDEO_SUB,
    SYS_CACHE_AUDIO,
    SYS_CACHE_COUNT
};


typedef struct ezcache_frm_s
{
    ezrtsp_queue_t   queue;
    long long  seq;
    unsigned long long  ts;

    char                typ;    // 0:audio  1:iframe  2:pframe
    char                nalu_fin;
    int                 datan;
    unsigned char       data[0];
} ezcache_frm_t;


int ezcache_exit(int channel_id);
int ezcache_frm_add(int channel_id, unsigned char * data, int datan, int typ, unsigned long long ts, char nalu_fin);
ezcache_frm_t * ezcache_frm_get(int channel_id, long long seq);
long long ezcache_idr_last(int channel_id);
long long ezcache_idr_prev(int channel_id, long long seq);
long long ezcache_idr_next(int channel_id, long long seq);
long long ezcache_seq_last(int channel_id);


#endif

