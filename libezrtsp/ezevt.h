#ifndef __EZEVT_H__
#define __EZEVT_H__

#include "ezrtsp_common.h"

/// evt datas 
#define EV_FD_MAX  256

#define EV_NONE     0x0
#define EV_R        0x1
#define EV_W        0x2
#define EV_RW       0X3

typedef struct ev_ctx_t ev_ctx_t;
typedef struct ev_t ev_t;

typedef void (*ev_cb) (ev_ctx_t * ctx, ev_t *evt);

/// @brief evt obj data
struct ev_t {
    ezrtsp_queue_t queue;
    ev_ctx_t *ev_ctx;
    void *data;
    ev_cb read_cb;
    ev_cb write_cb;
    int fd;
    int op;

    ev_cb exp_cb;
    unsigned long long exp_ts;
};

/// @brief evt mgr data
struct ev_ctx_t {
    ezrtsp_queue_t queue;
    fd_set cache_rfds;
    fd_set cache_wfds;
};

/// evt api
void ev_timer_del(ev_ctx_t *ev_ctx, ev_t *evt);
void ev_timer_add(ev_ctx_t *ev_ctx, ev_t *evt, ev_cb exp_cb, int exp_ms);

void ev_opt(ev_ctx_t *ev_ctx, ev_t *evt, int op);
void ev_loop(ev_ctx_t *ev_ctx);
void ev_free_evt(ev_ctx_t *ev_ctx, ev_t *evt);
ev_t *ev_alloc_evt(ev_ctx_t *ev_ctx);
int ev_create(ev_ctx_t **ctx);
void ev_free(ev_ctx_t * ctx);

#endif


