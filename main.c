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


#include "ezrtsp_utils.h"

typedef struct ffctx ffctx_t;
typedef void (*ff_cb) (ffctx_t * ctx, unsigned char * data, int datan);
struct ffctx {
    ff_cb cb;
    char fname[256];
    int ffd;
    int fps;
    int h265;

    char buf[2*1024*1024];
    char * start;
    char * end;
    char * pos;
    char * last;
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
        nctx = malloc(sizeof(ffctx_t));
            if(!nctx) {
            printf("alloc ffctx err\n");
            break;
        }
        memset(nctx, 0, sizeof(ffctx_t));
        nctx->start = nctx->buf;
        nctx->end = nctx->start + sizeof(nctx->buf);
        nctx->pos = nctx->last = nctx->start;
        nctx->cb = cb;
        nctx->fps = fps;
        nctx->h265 = h265;
        memcpy(nctx->fname, fname, strlen(fname));
        nctx->ffd = open(nctx->fname, O_RDONLY);
        if(0 > nctx->ffd) {
            printf("open ffctx file err. [%d:%s]\n", errno, strerror(errno));
            break;
        }
        ret = 0;
    } while(0);

    if(ret != 0) {
        if(nctx) free(nctx);
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
        free(ctx);
    }
    return 0;
}

int ffnalu_process(ffctx_t * ctx)
{
    int readn = 0;
    for(;;) {
        if(ctx->end > ctx->last) {
            readn = read(ctx->ffd, ctx->last, ctx->end - ctx->last);
            if(readn <= 0) {
                if(readn == 0) {
                    ///eof. 
                    lseek(ctx->ffd, 0L, SEEK_SET);
                    ctx->pos = ctx->last = ctx->start;
                    continue;
                }
                printf("ff read err. [%d:%s]\n", errno, strerror(errno));
                return -1;
            }
            ctx->last += readn;
        }

        char * nalus = NULL;
        char find = 0;
        for(; ctx->last > ctx->pos; ctx->pos++) {
            char * p = ctx->pos;
            if(((ctx->last - ctx->pos) > 3) && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
                find = 1;
                ctx->pos += 3;
            }
            if(((ctx->last - ctx->pos) > 4) && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
                find = 1;
                ctx->pos += 4;
            }

            if(find > 0) {
                if(!nalus) {
                    nalus = p;
                } else {
                    ctx->cb(ctx, (unsigned char*)nalus, p - nalus);
                    usleep((1000/ctx->fps)*1000);
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
    if(argc < 2) {
        printf("need to specify a file\n");
        return -1;
    }
    
    ezrtsp_ctx_t rtspctx;
    rtspctx.vtype = VT_H264;
    rtspctx.aenb = 0;
    ezrtsp_start(&rtspctx);
    
    ffctx_t * ctx = NULL;
    if(0 != ffnalu_alloc(&ctx, argv[1], ffnalu_video, 15, 0)) {
        printf("ffnalu ctx alloc err\n");
        return -1;
    }
    ffnalu_process(ctx);
    ffnalu_free(ctx);
    
	ezrtsp_stop();
    return 0;
}
