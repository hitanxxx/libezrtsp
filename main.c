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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static char fquit = 0;

void signal_cb(int signal) {
    int err_cc = errno; ///cache errno 

    if (signal == SIGINT) {
	printf("exiting. please wait\n");
        fquit = 1;
    }
    
    errno = err_cc; ///recovery errno
}

int signal_init(void) {
    int i = 0;
    struct sigaction sa;
    int arr[] = {
        SIGINT,
        SIGPIPE,
        0
    };
    for (i = 0; arr[i]; i++) {
        memset(&sa, 0x0, sizeof(struct sigaction));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = signal_cb;
        sa.sa_flags = SA_SIGINFO;
        if (0 != sigaction(arr[i], &sa, NULL)) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    signal_init();

    ezrtsp_ctx_t ctx = {
        .vtype = VT_H264,
        .aenb = 0
    };
    ezrtsp_start(&ctx);

#if 1
    /// cut h264 file frame by frame 
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    
    AVFormatContext *ff_format_ctx = NULL;
    AVPacket *pkt = NULL;
    
    if (avformat_open_input(&ff_format_ctx, "./venc_br1_1024kbps_chn0_2560x1440.h264", NULL, NULL) != 0) {
        printf("ff input file open err\n");
        return -1;
    }
    if (avformat_find_stream_info(ff_format_ctx, NULL) < 0) {
        printf("ff input file get stream info err\n");
        avformat_close_input(&ff_format_ctx);
        return -1;
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        printf("ff pkt alloc err\n");
        avformat_close_input(&ff_format_ctx);
        return -1;
    }
    while (!fquit) {
        int ret = av_read_frame(ff_format_ctx, pkt);
        if (ret < 0) {
            printf("reset stream\n");
            ret = av_seek_frame(ff_format_ctx, 0, 0, AVSEEK_FLAG_BYTE);  ///AVSEEK_FLAG_BACKWARD 
            if (ret < 0) {
                printf("stream seek to start err [%s]\n", av_err2str(ret));
                break;
            }
        } else {
            ///printf("frame data len [%d] stream idx [%d] key frame [%s]\n",
            ///    pkt->size,
            ///    pkt->stream_index,
            ///    pkt->flags == AV_PKT_FLAG_KEY ? "yes" : "no"
            ///);
            ezrtsp_push_vfrm(0, pkt->data, pkt->size, pkt->flags == AV_PKT_FLAG_KEY ? 1 : 2);
            ezrtsp_push_vfrm(1, pkt->data, pkt->size, pkt->flags == AV_PKT_FLAG_KEY ? 1 : 2);
        }
        
        av_packet_unref(pkt);
        usleep((1000/15)*1000);
    }
    av_packet_free(&pkt);
    avformat_close_input(&ff_format_ctx);
#else
    while(!fquit) {
        sleep(1);
    }
#endif
    
    ezrtsp_stop();
    printf("exited\n");
    return 0;
}
