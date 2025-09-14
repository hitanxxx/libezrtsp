#include "ezrtsp_common.h"
#include "ezcache.h"

#define CACHE_SIZE_MAX  (2*1024*1024)    ///max cache size of each channel

static ezrtsp_queue_t g_cache_frm_queue[2];
static int g_cache_frm_init[2] = {0};
static int g_cache_frm_cnt[2] = {0};
static int g_cache_frm_totaln[2] = {0};
static long long g_cache_frm_seq[2] = {0};
static pthread_mutex_t g_cache_frm_lock = PTHREAD_MUTEX_INITIALIZER;


int ezcache_init(int channel_id) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if (g_cache_frm_init[channel_id]) {
        err("channel_id [%d] cc already init\n", channel_id);
        return -1;
    }
    ezrtsp_queue_init(&g_cache_frm_queue[channel_id]);
    return 0;
}

int ezcache_exit(int channel_id) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if (!g_cache_frm_init[channel_id]) {
        err("channle_id [%d] cc not init\n", channel_id);
        return -1;
    }
    
    ezrtsp_queue_t * s = NULL;
    ezrtsp_queue_t * q = ezrtsp_queue_head(&g_cache_frm_queue[channel_id]);
    while (q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id])) {
        s = ezrtsp_queue_next(q);
        ezcache_frm_t * frm = ptr_get_struct(q, ezcache_frm_t, queue);
        ez_free(frm);
        q = s;
    }
    g_cache_frm_cnt[channel_id] = 0;
    g_cache_frm_totaln[channel_id] = 0;
    g_cache_frm_seq[channel_id] = 0;
    return 0;
}

int ezcache_frm_add(int channel_id, unsigned char *data, int datan, int typ, unsigned long long ts) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if ((typ < 0) || (typ > 2)) {
        err("frm typ [%d] not support\n", typ);
        return -1;
    }

    if (!g_cache_frm_init[channel_id]) { ///init until video idr 
        if (typ != 1)
            return 0;
        
        ezcache_init(channel_id);
        g_cache_frm_init[channel_id] = 1;
    }
    pthread_mutex_lock(&g_cache_frm_lock);
    while (g_cache_frm_totaln[channel_id] >= CACHE_SIZE_MAX) { ///del oldest
        ezrtsp_queue_t *q = ezrtsp_queue_head(&g_cache_frm_queue[channel_id]);
        ezcache_frm_t *oldest_frm = ptr_get_struct(q, ezcache_frm_t, queue);
        ezrtsp_queue_remove(q);
        int freen = oldest_frm->datan;
        ez_free(oldest_frm);
        
        g_cache_frm_totaln[channel_id] -= freen;
        g_cache_frm_cnt[channel_id] --;
    }

    ezcache_frm_t *new_frm = ez_alloc(sizeof(ezcache_frm_t) + datan);
    if (!new_frm) {
        err("alloc frmn [%ld] failed. [%d]\n", sizeof(ezcache_frm_t) + datan, errno);
        pthread_mutex_unlock(&g_cache_frm_lock);
        return -1;
    }
    ezrtsp_queue_insert_tail(&g_cache_frm_queue[channel_id], &new_frm->queue);
    memcpy(new_frm->data, data, datan);
    new_frm->datan = datan;
    new_frm->seq = g_cache_frm_seq[channel_id]++;
    new_frm->ts = ts;
    new_frm->typ = typ;
    //dbg("channel [%d] add frm len [%d]. frm cnt [%d] totaln [%d]\n", channel_id, datan, g_cache_frm_cnt[channel_id], g_cache_frm_totaln[channel_id]);
    g_cache_frm_totaln[channel_id] += datan;
    g_cache_frm_cnt[channel_id] ++;
    pthread_mutex_unlock(&g_cache_frm_lock);

    return 0;   
}

ezcache_frm_t *ezcache_frm_get(int channel_id, long long seq) {
    ezcache_frm_t *frm = NULL;
    int find = 0;

    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return NULL;
    }
    if (!g_cache_frm_init[channel_id]) 
        return NULL;

    if ((seq < 0) || (seq >= g_cache_frm_seq[channel_id])) {
        return NULL;
    }
        
    pthread_mutex_lock(&g_cache_frm_lock);
    if ((seq * 2) <= g_cache_frm_seq[channel_id]) {  ///head start 
        ezrtsp_queue_t * q = ezrtsp_queue_head(&g_cache_frm_queue[channel_id]);
        for (; q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); q = ezrtsp_queue_next(q)) {
            frm = ptr_get_struct(q, ezcache_frm_t, queue);
            if (frm->seq == seq) {
                find = 1;
                break;
            }
        }
    } else { ///tail start
        ezrtsp_queue_t * q = ezrtsp_queue_prev(&g_cache_frm_queue[channel_id]);
        for (; q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); q = ezrtsp_queue_prev(q)) {
            frm = ptr_get_struct(q, ezcache_frm_t, queue);
            if (frm->seq == seq) {
                find = 1;
                break;
            }
        }
    }
    if (find) {
        ezcache_frm_t * new_frm = ez_alloc(sizeof(ezcache_frm_t) + frm->datan);
        if (new_frm) {
            memcpy(new_frm, frm, sizeof(ezcache_frm_t) + frm->datan);
            pthread_mutex_unlock(&g_cache_frm_lock);
            return new_frm;
        } else {
            err("alloc new frm failed. [%d]\n", errno);
        }
    }
    pthread_mutex_unlock(&g_cache_frm_lock);
    return NULL;
}

long long ezcache_idr_prev(int channel_id, long long seq) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if (!g_cache_frm_init[channel_id]) 
        return -1;
    
    if ((seq < 0) || (seq >= g_cache_frm_seq[channel_id])) {
        err("seq [%lld] out of range. current seq max [%lld]\n", seq, g_cache_frm_seq[channel_id]);
        return -1;
    }

    ezrtsp_queue_t * q = ezrtsp_queue_head(&g_cache_frm_queue[channel_id]);
    for (; q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); q = ezrtsp_queue_next(q)) {
        ezcache_frm_t * frm = ptr_get_struct(q, ezcache_frm_t, queue);
        if (frm->seq == seq) {
            ezrtsp_queue_t * t = ezrtsp_queue_prev(q); ///forward traserval find previously idr frame
            for (; t != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); t = ezrtsp_queue_prev(t)) {
                ezcache_frm_t * frmm = ptr_get_struct(t, ezcache_frm_t, queue);
                if (frmm->typ == 1) {
                    return frmm->seq;
                }
            }
        }
    }
    err("idr prev by specify not found\n");
    return -1;
}

long long ezcache_idr_next(int channel_id, long long seq) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel [%d] not support\n", channel_id);
        return -1;
    }
    if (!g_cache_frm_init[channel_id]) {
        return -1;
    }
    if ((seq < 0) || (seq >= g_cache_frm_seq[channel_id])) {
        err("seq [%lld] out of range. current seq max [%lld]\n", seq, g_cache_frm_seq[channel_id]);
        return -1;
    }

    ezrtsp_queue_t * q = NULL;
    for (q = ezrtsp_queue_head(&g_cache_frm_queue[channel_id]); q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); q = ezrtsp_queue_next(q)) {
        ezcache_frm_t * frm = ptr_get_struct(q, ezcache_frm_t, queue);
        if (frm->seq == seq) {
            q = ezrtsp_queue_next(&frm->queue);
            while (q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id])) {
                frm = ptr_get_struct(q, ezcache_frm_t, queue);
                if (frm->typ == 1) {
                    return frm->seq;
                }
                q = q->next;
            }
        }
    }
    err("idr next by specify not found\n");
    return -1;
}

long long ezcache_idr_last(int channel_id) {
    /// forward traserval, find the last dir frame seq number 
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if (!g_cache_frm_init[channel_id])
        return -1;

    ezrtsp_queue_t *q = ezrtsp_queue_prev(&g_cache_frm_queue[channel_id]);
    for (; q != ezrtsp_queue_tail(&g_cache_frm_queue[channel_id]); q = ezrtsp_queue_prev(q)) {
        ezcache_frm_t * frm = ptr_get_struct(q, ezcache_frm_t, queue);
        if(frm->typ == 1) {
            return frm->seq;
        }
    }
    return -1;
}

long long ezcache_seq_last(int channel_id) {
    if ((channel_id < 0) || (channel_id > 1)) {
        err("channel_id [%d] not support\n", channel_id);
        return -1;
    }
    if (!g_cache_frm_init[channel_id])
        return -1;

    return g_cache_frm_seq[channel_id];
}

