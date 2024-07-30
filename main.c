#include "ezrtsp_utils.h"

typedef struct ffctx ffctx_t;
typedef void (*ff_cb) (ffctx_t * ctx, unsigned char * data, int datan);
struct ffctx {
    char fname[256];
    int ffd;
    meta_t * meta;
    ff_cb cb;
    int fps;
    int h265;
};

void ffnalu_video(ffctx_t * ctx, unsigned char *data, int datan)
{
    if(ctx->h265) {
        ezrtsp_push_vfrm(0, (char*)data, datan);
    } else {
        ///int nalu_type = data[0] & 0b00011111;
        ///dbg("H264. naltype [%d] <%p:%d>\n", nalu_type, data, datan);
        ezrtsp_push_vfrm(0, (char*)data, datan);
    }
    return;
}

int ffnalu_alloc(ffctx_t ** ctx, char * fname, ff_cb cb, int fps, int h265)
{
    int ret = -1;
    ffctx_t * nctx = NULL;
    do {
        nctx = sys_alloc(sizeof(ffctx_t));
            if(!nctx) {
            err("alloc ffctx err\n");
            break;
        }
        if(0 != meta_alloc(&nctx->meta, 2*1024*1024)) {
            err("alloc ffctx meta err\n");
            break;
        }
        nctx->cb = cb;
        nctx->fps = fps;
        nctx->h265 = h265;
        memcpy(nctx->fname, fname, strlen(fname));
        nctx->ffd = open(nctx->fname, O_RDONLY);
        if(0 > nctx->ffd) {
            err("open ffctx file err. [%d:%s]\n", errno, strerror(errno));
            break;
        }
        ret = 0;
    } while(0);

    if(ret != 0) {
        if(nctx) {
            if(nctx->meta) meta_free(nctx->meta);
            sys_free(nctx);
        }
        return -1;
    }
    *ctx = nctx;
    return 0;
}

int ffnalu_free(ffctx_t * ctx)
{
    if(ctx) {
        if(ctx->ffd) {
            close(ctx->ffd);
            ctx->ffd = 0;
        }
        if(ctx->meta) {
            meta_free(ctx->meta);
        }
        sys_free(ctx);
    }
    return 0;
}

int ffnalu_process(ffctx_t * ctx)
{
    int readn = 0;
    for(;;) {
        if(meta_getfree(ctx->meta) > 0) {
            readn = read(ctx->ffd, ctx->meta->last, meta_getfree(ctx->meta));
            if(readn <= 0) {
                if(readn == 0) {
                    ///eof. 
                    lseek(ctx->ffd, 0L, SEEK_SET);
                    meta_clr(ctx->meta);
                    continue;
                }
                err("ff read err. [%d:%s]\n", errno, strerror(errno));
                return -1;
            }
            ctx->meta->last += readn;
        }

        char * nalus = NULL;
        char find = 0;
        for(; meta_getlen(ctx->meta) > 0; ctx->meta->pos++) {
            char * p = ctx->meta->pos;
            if(meta_getlen(ctx->meta) > 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
                find = 1;
                ctx->meta->pos += 3;
            }
            if(meta_getlen(ctx->meta) > 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
                find = 1;
                ctx->meta->pos += 4;
            }

            if(find > 0) {
                if(!nalus) {
                    nalus = p;
                } else {
                    ctx->cb(ctx, (unsigned char*)nalus, p - nalus);
                    sys_msleep(1000/ctx->fps);
                    nalus = p;
                }
                find = 0;
            }
        }
    }
    return 0;
}

int main(int argc, char ** argv)
{
    ezrtsp_ctx_t rtspctx;
    rtspctx.vtype = VT_H264;
    rtspctx.aenb = 0;

    ezrtsp_start(&rtspctx);
    ffctx_t * ctx = 0;
    if(argc < 2) {
        err("need to specify a file\n");
        return -1;
    }
    ffnalu_alloc(&ctx, argv[1], ffnalu_video, 25, 0);
    ffnalu_process(ctx);
	ezrtsp_stop();
    return 0;
}
