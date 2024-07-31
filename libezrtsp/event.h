#ifndef __EVENT_H__
#define __EVENT_H__

#include "ezrtsp_common.h"

/// evt datas 
#define EV_FD_MAX  256

#define EV_NONE	0x0
#define EV_R	0x1
#define EV_W	0x2
#define EV_RW	0X3

typedef struct ev_ctx_t ev_ctx_t;
typedef struct ev_t ev_t;

typedef void (*ev_cb) (ev_ctx_t * ctx, int fd, void * user_data, int rw);
typedef void (*ev_exp_cb) (ev_ctx_t * ctx, int fd, void * user_data);

/// @brief evt obj data
struct ev_t {
    ezrtsp_queue_t     queue;
    ev_ctx_t *  ctx;
    ev_cb       cb;
    int         fd;
    int 	op;
    void * ext_data;

    unsigned long long exp_ts;
    ev_exp_cb exp_cb;

    char active:1;
};

/// @brief evt mgr datas
struct ev_ctx_t {
    fd_set cache_rfds;
    fd_set cache_wfds;
    ezrtsp_queue_t queue;
};

/// evt api
void ev_timer_add(ev_ctx_t * ctx, int fd, void * user_data, ev_exp_cb cb, int delay_msec);
void ev_timer_del(ev_ctx_t * ctx, int fd);

int ev_create(ev_ctx_t ** ev_ctx);
void ev_free(ev_ctx_t * evt);
void ev_loop(ev_ctx_t * evt);

void ev_opt(ev_ctx_t * ctx, int fd, void * user_data, ev_cb cb, int rw);
ev_t * ev_find(ev_ctx_t * ctx, int fd);

#endif


