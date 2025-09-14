#include "ezrtsp_common.h"
#include "ezevt.h"

void ev_timer_del(ev_ctx_t *ev_ctx, ev_t *evt) {
    evt->exp_cb = NULL;
    evt->exp_ts = 0;
    return;
}

void ev_timer_add(ev_ctx_t *ev_ctx, ev_t *evt, ev_cb exp_cb, int exp_ms) {
    evt->exp_ts = ezrtsp_ts_msec() + exp_ms;
    evt->exp_cb = exp_cb;
    return;
}

void ev_opt(ev_ctx_t *ev_ctx, ev_t *evt, int op) {
    /// some assert
    schk(ev_ctx, return);
    schk(evt, return);
    schk(evt->fd > 0, return);

    if (evt->op != op) {
        if (op == EV_R) {
            FD_CLR(evt->fd, &ev_ctx->cache_wfds);
            FD_SET(evt->fd, &ev_ctx->cache_rfds);
        } 
        if (op == EV_W) {
            FD_SET(evt->fd, &ev_ctx->cache_wfds);
            FD_CLR(evt->fd, &ev_ctx->cache_rfds);
        }
        if (op == EV_RW) {
            FD_SET(evt->fd, &ev_ctx->cache_wfds);
            FD_SET(evt->fd, &ev_ctx->cache_rfds);
        }
        if (op == EV_NONE) {
            FD_CLR(evt->fd, &ev_ctx->cache_wfds);
            FD_CLR(evt->fd, &ev_ctx->cache_rfds);
        }
        evt->op = op;
    }
    return;
}

void ev_loop(ev_ctx_t *ev_ctx) {
    /// round-robin chk. interval default 20ms
    int max_fd = -1;
    int actall = 0;
    unsigned long long cur_msec = ezrtsp_ts_msec();

    fd_set rfds;
    fd_set wfds;

    struct timeval ts;
    memset(&ts, 0, sizeof(ts));
    ts.tv_sec = 0;
    ts.tv_usec = 20 * 1000;

    ev_t *evt = NULL;
    ezrtsp_queue_t *q = ezrtsp_queue_head(&ev_ctx->queue);
    ezrtsp_queue_t *n = NULL;
    while (q != ezrtsp_queue_tail(&ev_ctx->queue)) {
        n = ezrtsp_queue_next(q);
        
        evt = ptr_get_struct(q, ev_t, queue);
        if (evt->fd > max_fd) 
            max_fd = evt->fd;
        
        if (evt->exp_ts > 0) {
            if (cur_msec >= evt->exp_ts) {
                dbg("evt [%p:%d] timeout\n", evt, evt->fd);
                evt->exp_ts = 0;
                if (evt->exp_cb) evt->exp_cb(ev_ctx, evt);
            }
        }
        
        q = n;
    }
    
    memcpy(&rfds, &ev_ctx->cache_rfds, sizeof(fd_set));
    memcpy(&wfds, &ev_ctx->cache_wfds, sizeof(fd_set));
    
    actall = select(((max_fd == -1) ? 1 : max_fd+1), &rfds, &wfds, NULL, &ts);
    if (actall < 0) {
        err("ev select failed. [%d]\n", errno);
        return;
    } else if (actall == 0) { ///timeout. do nothing
        return;
    } else {
        int actn = 0;
        ezrtsp_queue_t *q = ezrtsp_queue_head(&ev_ctx->queue);
        ezrtsp_queue_t *n = NULL;
        while (q != ezrtsp_queue_tail(&ev_ctx->queue)) {
            n = ezrtsp_queue_next(q);

            evt = ptr_get_struct(q, ev_t, queue);
            int rw = 0;
            if (FD_ISSET(evt->fd, &rfds)) rw |= EV_R;
            if (FD_ISSET(evt->fd, &wfds)) rw |= EV_W;
            if(rw > 0) {
                if ((rw & EV_R) && evt->read_cb)
                    evt->read_cb(ev_ctx, evt);
                if ((rw & EV_W) && evt->write_cb)
                    evt->write_cb(ev_ctx, evt);
                actn++;
                if(actn >= actall) break;
            }
            
            q = n;
        }
    }
    return;
}

void ev_free_evt(ev_ctx_t *ev_ctx, ev_t *evt) {
    ezrtsp_queue_remove(&evt->queue);

    if (evt->fd > 0) {
        ev_opt(ev_ctx, evt, EV_NONE);
        close(evt->fd);
    }
    ez_free(evt);
}

ev_t *ev_alloc_evt(ev_ctx_t *ev_ctx) {
    ev_t *ev = ez_alloc(sizeof(ev_t));
    if (ev) {
        memset(ev, 0x0, sizeof(ev_t));
        
        ev->ev_ctx = ev_ctx;
        ev->data = NULL;
        ev->read_cb = NULL;
        ev->write_cb = NULL;
        ev->fd = 0;
        ev->op = 0;
        ev->exp_cb = NULL;
        ev->exp_ts = 0;
        ezrtsp_queue_insert_tail(&ev_ctx->queue, &ev->queue);
        return ev;
    }
    return NULL;
}

int ev_create(ev_ctx_t **ctx) {
    ev_ctx_t * nctx = ez_alloc(sizeof(ev_ctx_t));
    if (!nctx) {
        err("ev ctx alloc failed. [%d]\n", errno);
        return -1;
    }

    FD_ZERO(&nctx->cache_rfds);
    FD_ZERO(&nctx->cache_wfds);
    ezrtsp_queue_init(&nctx->queue);
    *ctx = nctx;
    return 0;
}
 
void ev_free(ev_ctx_t *ev_ctx) {
    if (ev_ctx) {
        FD_ZERO(&ev_ctx->cache_rfds);
        FD_ZERO(&ev_ctx->cache_wfds);

        
        ezrtsp_queue_t *q = ezrtsp_queue_head(&ev_ctx->queue);
        ezrtsp_queue_t *n = NULL;
        while (q != ezrtsp_queue_tail(&ev_ctx->queue)) {
            n = ezrtsp_queue_next(q);

            ev_t *evt = ptr_get_struct(q, ev_t, queue);
            ezrtsp_queue_remove(&evt->queue);
            ez_free(evt);
            
            q = n;
        }

        
        ez_free(ev_ctx);
    }
}


