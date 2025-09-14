#ifndef __EZRTSP_H__
#define __EZRTSP_H__

#include "ezrtsp_common.h"
#include "ezevt.h"

#define METHOD_OPTIONS      1
#define METHOD_DESCRIBE     2
#define METHOD_SETUP        3
#define METHOD_PLAY         4

#define EZRTSP_INIT 0x1
#define EZRTP_INIT  0x1

#define EZRTSP_VPS  1
#define EZRTSP_SPS  2
#define EZRTSP_PPS  3

#define PUT_16(p, v) ((p)[0] = ((v) >> 8) & 0xff, (p)[1] = (v)&0xff)
#define PUT_32(p, v) ((p)[0] = ((v) >> 24) & 0xff, (p)[1] = ((v) >> 16) & 0xff, (p)[2] = ((v) >> 8) & 0xff, (p)[3] = (v) & 0xff)
#define GET_16(p)    (((p)[0] << 8) | (p)[1])
#define GET_32(p)    (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])


typedef struct {
    unsigned int count:5;
    unsigned int pad:1;
    unsigned int ver:2;
    unsigned int pt:8;
    unsigned int len:16;

    unsigned int ssrc;
    unsigned int ntp_sec;
    unsigned int ntp_frac;
    unsigned int rtp_ts;
    unsigned int psent;
    unsigned int osent;
} ezrtcp_sr_t;

typedef struct rtsp_con rtsp_con_t;

typedef struct {
    int trackid;
    unsigned short seq;
    unsigned int ssrc;
    unsigned int ts;
    int send_err;

    unsigned long long ts_prev;

    int fd_rtp;
    int fd_rtcp;
    unsigned long long rtcp_ts;
    int rtpn;
    int rtplen;
    
    rtsp_con_t * c;
} rtsp_session_t;


/// @brief rtsp connection info
struct rtsp_con {
    ev_t *evt;
    ev_ctx_t *ev_ctx;
    
    int rtp_port_audio;
    int rtp_port_video;
    int rtcp_port_audio;
    int rtcp_port_video;

    char client_ip[64];
    ezrtsp_meta_t *meta;

    int state;
    char *method;
    int methodn;
    char *url;
    int urln;
    char *reqfin;
   
    char fuse:1;        ///
    char fcomplete:1;
    char fplay:1;
    char fovertcp:1;
    char faudioenb:1;
    char fvideoenb:1;
    
    long long ichseq;
    int ichn;
    int icseq;
    int imethod;
    int itrack;
    int irtp_port;
    int irtcp_port;

    rtsp_session_t  session_audio;
    rtsp_session_t  session_video;
    char session_str[32];
    
    pthread_t rtp_pid;
    int rtp_stat;
};

int ezrtsp_serv_start(void);
int ezrtsp_serv_stop(void);

int ezrtsp_con_init();
int ezrtsp_con_alloc(rtsp_con_t ** c);
void ezrtsp_con_free(rtsp_con_t * c);
void ezrtsp_con_expire(ev_ctx_t * ctx, ev_t *evt);

int ezrtp_start(rtsp_con_t * c);
int ezrtp_packet_send(int fd, char * data, int datan);
int ezrtcp_sr_send(rtsp_con_t *c);

int ezrtsp_video_codec_typ();
int ezrtsp_video_sequence_parament_set_ready(int ch);
int ezrtsp_video_sequence_parament_set_get(int ch, ezrtsp_data_t ** vps, ezrtsp_data_t ** sps, ezrtsp_data_t ** pps);

int ezrtsp_audio_codec_typ();
int ezrtsp_audio_enb();
int ezrtsp_audio_aacadts_ready();
unsigned char * ezrtsp_audio_aacadts_get();


#endif