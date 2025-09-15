#include "ezrtsp.h"
#include "ezrtsp_utils.h"

static int listen_fd = 0;
static int rtsp_stat = 0;
static pthread_t rtsp_pid;

static char g_gmtstr[128] = {0};
static char *g_arr_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *g_arr_month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                              "Aug", "Sep", "Oct", "Nov", "Dec"};

static void ezrtsp_req(ev_ctx_t *ev_ctx, ev_t *evt);


static char * ezrtsp_gmtstr(void) {
    struct timeval tv;
    time_t sec;

    memset(&tv, 0, sizeof(struct timeval));
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;

    struct tm gmt;
    gmtime_r(&sec, &gmt);
    memset(g_gmtstr, 0, sizeof(g_gmtstr));
    sprintf((char*)g_gmtstr,
        "%s, %02d %s %04d %02d:%02d:%02d GMT",
        g_arr_week[gmt.tm_wday],
        gmt.tm_mday,
        g_arr_month[gmt.tm_mon],
        gmt.tm_year+1900,
        gmt.tm_hour,
        gmt.tm_min,
        gmt.tm_sec
    );
    return g_gmtstr;
}

void ezrtsp_rsp_send(ev_ctx_t *ev_ctx, ev_t *evt) {
    rtsp_con_t *rtspc = (rtsp_con_t *)evt->data;
    
    while (meta_getlen(rtspc->meta) > 0) {
        int sendn = send(evt->fd, rtspc->meta->pos, meta_getlen(rtspc->meta), 0);
        if (sendn < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return ev_timer_add(ev_ctx, evt, ezrtsp_con_expire, 5000);
            }
            err("ezrtsp [%p:%d] send rsp err. [%d]\n", rtspc, evt->fd, errno);
            return ezrtsp_con_free(rtspc);
        }
        rtspc->meta->pos += sendn;
    }
    ev_timer_del(ev_ctx, evt);
    
    dbg("========>\n");
    dbg("%s\n", rtspc->meta->start);
    meta_clr(rtspc->meta);

    rtspc->methodn = 0;
    rtspc->urln = 0;

    rtspc->fcomplete = 0;
    rtspc->imethod = 0;
    rtspc->icseq = 0;
    rtspc->state = 0;

    evt->write_cb = NULL;
    evt->read_cb = ezrtsp_req;
    ev_opt(ev_ctx, evt, EV_R);
    return evt->read_cb(ev_ctx, evt);
}

static void ezrtsp_rsp_push(rtsp_con_t *rtspc, const char *str, ...) {
    va_list argslist;
    const char * args = str;
    char buf[1024] = {0};

    va_start(argslist, str);
    vsnprintf(buf, sizeof(buf) - 1, args, argslist);
    va_end(argslist);

    ezrtsp_meta_pdata(rtspc->meta, buf, strlen(buf));
}

static int ezrtsp_options(rtsp_con_t *rtspc) {
    ezrtsp_rsp_push(rtspc, "RTSP/1.0 200 OK\r\n");
    ezrtsp_rsp_push(rtspc, "CSeq: %d\r\n", rtspc->icseq);
    ezrtsp_rsp_push(rtspc, "Date: %s\r\n", ezrtsp_gmtstr());
    ezrtsp_rsp_push(rtspc, "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n");
    return 0;
}

static int ezrtsp_describe_video_sdp(int chn, char *str) {
    char vpsb64[128] = {0}, spsb64[512] = {0}, ppsb64[128] = {0};
    ezrtsp_data_t * vps = NULL;
    ezrtsp_data_t * sps = NULL;
    ezrtsp_data_t * pps = NULL;
    ezrtsp_video_sequence_parament_set_get(chn, &vps, &sps, &pps);

    if (ezrtsp_video_codec_typ() == VT_H264) { 
        int level = (sps->data[1]<<16) | (sps->data[2]<<8)|sps->data[3];
        if (sps)
            ezrtsp_base64_encode((char*)sps->data, sps->datan, spsb64, sizeof(spsb64));
        if (pps)
            ezrtsp_base64_encode((char*)pps->data, pps->datan, ppsb64, sizeof(ppsb64));
        sprintf(str, "profile-level-id=%06X;sprop-parameter-sets=%s,%s", level, spsb64, ppsb64);
    } else {
        if (vps)
            ezrtsp_base64_encode((char*)vps->data, vps->datan, vpsb64, sizeof(vpsb64));
        if (pps)
            ezrtsp_base64_encode((char*)pps->data, pps->datan, ppsb64, sizeof(ppsb64));
        if (sps)
            ezrtsp_base64_encode((char*)sps->data, sps->datan, spsb64, sizeof(spsb64));
        sprintf(str, "sprop-vps=%s;sprop-sps=%s;sprop-pps=%s", vpsb64, spsb64, ppsb64);
    }
    return 0;
}

static int ezrtsp_describe_audio_sdp(char * str) {
    if (ezrtsp_audio_codec_typ() == AT_AAC) {
        #if(0)  ///if have't adts header, mabey need this
        int aac_samplerate = 8000;
        unsigned short config = 0;
        unsigned auobjtype = 0x2;
        unsigned aufreqidx = 0xb;
        unsigned auchan = 0x1;
        if(aac_samplerate == 8000) {
            aufreqidx = 0xb;
        } else if (aac_samplerate == 16000) {
            aufreqidx = 0x8;
        } else if (aac_samplerate == 24000) {
            aufreqidx = 0x6;
        } else if (aac_samplerate == 32000) {
            aufreqidx = 0x5;
        } else if (aac_samplerate == 44100) {
            aufreqidx = 0x4;
        } else if (aac_samplerate == 48000) {
            aufreqidx = 0x3;
        }
        config |= ((auobjtype<<11) & 0xf800);
        config |= ((aufreqidx<<7) & 0x780);
        config |= ((auchan<<3) & 0x78);
        ///config 0x4x
        
        sprintf(str,
            "a=fmtp:98 "
            "streamtype=5;profile-level-id=1;"
            "mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;"
            "config=%0x4x\r\n",
            config
        );
        #else
        char config[32] = {0};
        unsigned char * adts_header = ezrtsp_audio_aacadts_get();
        unsigned char profile = (adts_header[2]&0xc0)>>6;
        unsigned char freq = (adts_header[2]&0x3c)>>2;
        unsigned char channel = ((adts_header[2]&0x1)<<2)|((adts_header[3]&0xc0)>>6);

        unsigned char spec_cfg[2] = {0};
        unsigned char audiotyp = profile + 1;
        spec_cfg[0] = (audiotyp << 3) | (freq>>1);
        spec_cfg[1] = (freq<<7) | (channel<<3);
        sprintf(config, "%02X%02X", spec_cfg[0], spec_cfg[1]);
        
        sprintf(str,
            "a=fmtp:98 "
            "streamtype=5;profile-level-id=1;"
            "mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;"
            "config=%s\r\n",
            config
        );
        #endif
    } else {
        err("ezrtsp not support audio codec typ [%d]\n", ezrtsp_audio_codec_typ());
        return -1;
    }
    return 0;
}

static int ezrtsp_describe(rtsp_con_t *rtspc) {
    char sdp[4096] = {0};
    int sdpn = 0;

    rtspc->fvideoenb = 1;
    rtspc->faudioenb = ezrtsp_audio_enb();

    if (rtspc->fvideoenb) {
        char sdp_video[1024] = {0};
        ezrtsp_describe_video_sdp(rtspc->ichn, sdp_video);
        sdpn += snprintf(sdp + sdpn, sizeof(sdp) - sdpn - 1,
            "v=0\r\n"
            "s=ezrtsp\r\n"
            "t=0 0\r\n"
            "a=control:*\r\n"
            "a=range:npt=0-\r\n"
            "a=recvonly\r\n"
            "m=video 0 RTP/AVP %d\r\n"
            "c=IN IP4 0.0.0.0\r\n"
            "b=AS:5000\r\n"
            "a=rtpmap:%d %s/90000\r\n"
            "a=fmtp:%d packetization-mode=1;%s\r\n"
            "a=control:track=0\r\n",
            (ezrtsp_video_codec_typ() == VT_H264) ? 96 : 97,
            (ezrtsp_video_codec_typ() == VT_H264) ? 96 : 97,
            (ezrtsp_video_codec_typ() == VT_H264) ? "H264" : "H265",
            (ezrtsp_video_codec_typ() == VT_H264) ? 96 : 97,
            sdp_video
        );
    }
    if (rtspc->faudioenb) {
        if(ezrtsp_audio_codec_typ() == AT_AAC) {
            char sdp_audio[1024] = {0};
            ezrtsp_describe_audio_sdp(sdp_audio);
            sdpn += snprintf(sdp + sdpn, sizeof(sdp) - sdpn - 1,
                "m=audio 0 RTP/AVP 98\r\n"
                "c=IN IP4 0.0.0.0\r\n"
                "b=AS:50\r\n"
                "a=rtpmap:98 mpeg4-generic/8000\r\n"
                "%s"
                "a=control:track=1\r\n",
                sdp_audio
            );
        } else if (ezrtsp_audio_codec_typ() == AT_G711A) {
            sdpn += snprintf(sdp + sdpn, sizeof(sdp) - sdpn - 1,
                "m=audio 0 RTP/AVP 8\r\n"
                "c=IN IP4 0.0.0.0\r\n"
                "b=AS:50\r\n"
                "a=rtpmap:8 PCMA/8000\r\n"
                "a=control:track=1\r\n"
            );
        }
    }

    ezrtsp_rsp_push(rtspc, "RTSP/1.0 200 OK\r\n");
    ezrtsp_rsp_push(rtspc, "CSeq: %d\r\n", rtspc->icseq);
    ezrtsp_rsp_push(rtspc, "Date: %s\r\n", ezrtsp_gmtstr());
    ezrtsp_rsp_push(rtspc, "Content-Type: application/sdp\r\n");
    ezrtsp_rsp_push(rtspc, "Content-Length: %d\r\n\r\n", sdpn);
    ezrtsp_rsp_push(rtspc, "%s", sdp);
    return 0;
}

static int ezrtsp_setup_udp_connection(int * fd, int localport, char * cli_ip, int cli_port) {
    int ffd = 0;

    int socklen = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    struct sockaddr_in addr_cli;

    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(localport);
    addr.sin_family = AF_INET;

    addr_cli.sin_addr.s_addr = inet_addr(cli_ip);
    addr_cli.sin_port = htons(cli_port);
    addr_cli.sin_family = AF_INET;
    
    ffd = socket(AF_INET, SOCK_DGRAM, 0);
    if(0 > ffd) {
        err("socket err. [%d]\n", errno);
        return -1;
    }
    int flag = 1;
    setsockopt(ffd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    if(0 > bind(ffd, (struct sockaddr*)&addr, socklen)) {
        err("bind err. [%d]\n", errno);
        close(ffd);
        return -1;
    }
    if(0 > connect(ffd, (struct sockaddr*)&addr_cli, socklen)) {
        err("connect err. [%d]\n", errno);
        close(ffd);
        return -1;
    }
    *fd = ffd;
    return 0;
}

static int ezrtsp_setup(rtsp_con_t *rtspc) {

    srand(time(NULL));
    if (strlen(rtspc->session_str) < 1) 
        sprintf(rtspc->session_str, "%06d", rand() % 100000);

    rtsp_session_t *session = ((rtspc->itrack == 0) ? &rtspc->session_video : &rtspc->session_audio);
    session->c = rtspc;
    session->seq = rand() % 10000;
    session->ssrc = rand() % 10000;
    session->ts = rand() % 10000;
    session->trackid = rtspc->itrack;
    if (!rtspc->fovertcp) {
        ezrtsp_setup_udp_connection(&session->fd_rtp,
            session->trackid == 0 ? rtspc->rtp_port_video : rtspc->rtp_port_audio,
            rtspc->client_ip, rtspc->irtp_port);
        ezrtsp_setup_udp_connection(&session->fd_rtcp, 
            session->trackid == 0 ? rtspc->rtcp_port_video : rtspc->rtcp_port_audio,
            rtspc->client_ip, rtspc->irtcp_port);
        dbg("ezrtsp [%p:%d] session track%d. rtprtcp info <%d:%d>\n",
            rtspc, rtspc->evt->fd, rtspc->itrack, session->fd_rtp, session->fd_rtcp);
    }

    ezrtsp_rsp_push(rtspc, "RTSP/1.0 200 OK\r\n");
    ezrtsp_rsp_push(rtspc, "CSeq: %d\r\n", rtspc->icseq);
    ezrtsp_rsp_push(rtspc, "Date: %s\r\n", ezrtsp_gmtstr());
    if (rtspc->fovertcp) {
        ezrtsp_rsp_push(rtspc, "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d;ssrc=%x;mode=\"play\"\r\n",
            (session->trackid * 2),
            (session->trackid * 2 + 1),
            session->ssrc
        );
    } else {
        ezrtsp_rsp_push(rtspc, "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d;ssrc=%x\r\n", 
            rtspc->irtp_port, rtspc->irtcp_port,
            ((session->trackid == 0) ? rtspc->rtp_port_video : rtspc->rtp_port_audio),
            ((session->trackid == 0) ? rtspc->rtcp_port_video : rtspc->rtcp_port_audio),
            session->ssrc
        );
    }
    ezrtsp_rsp_push(rtspc, "Session: %s\r\n\r\n", rtspc->session_str);
    return 0;
}

static int ezrtsp_play(rtsp_con_t *rtspc) {
    rtspc->fplay = 1;

    char rtpinfo[512] = {0};
    int rtpinfon = 0;
    if (rtspc->fvideoenb) {
        rtpinfon += snprintf(rtpinfo + rtpinfon, sizeof(rtpinfo) - rtpinfon, 
            "url=%s/track=0;seq=%d;rtptime=%u",
            (rtspc->ichn == 0 ? EZRTSP_URI_MAIN : EZRTSP_URI_SUB),
            rtspc->session_video.seq,
            rtspc->session_video.ts
        );
    }
    if (rtspc->faudioenb) {
        rtpinfon += snprintf(rtpinfo + rtpinfon, sizeof(rtpinfo) - rtpinfon, 
            "%surl=%s/track=1;seq=%d;rtptime=%u",
            rtspc->fvideoenb ? "," : "",
            (rtspc->ichn == 0 ? EZRTSP_URI_MAIN : EZRTSP_URI_SUB),
            rtspc->session_audio.seq,
            rtspc->session_audio.ts
        );
    }
    ezrtsp_rsp_push(rtspc, "RTSP/1.0 200 OK\r\n");
    ezrtsp_rsp_push(rtspc, "CSeq: %d\r\n", rtspc->icseq);
    ezrtsp_rsp_push(rtspc, "Date: %s\r\n", ezrtsp_gmtstr());
    ezrtsp_rsp_push(rtspc, "Session: %s\r\n", rtspc->session_str);
    ezrtsp_rsp_push(rtspc, "RTP-Info: %s\r\n\r\n", rtpinfo);
    
    dbg("ezrtsp [%p:%d] play. ichn [%d] icseq [%d] chseq [%lld]\n",
        rtspc, rtspc->evt->fd, rtspc->ichn, rtspc->icseq, rtspc->ichseq);
    ezrtp_start(rtspc);
    return 0;
}

static void ezrtsp_rsp(ev_ctx_t *ev_ctx, ev_t *evt) {
    rtsp_con_t *rtspc = (rtsp_con_t *)evt->data;

    meta_clr(rtspc->meta);      ///clr buf
    switch (rtspc->imethod) {
        case METHOD_OPTIONS:
            ezrtsp_options(rtspc);
            break;
        case METHOD_DESCRIBE:
            ezrtsp_describe(rtspc);
            break;
        case METHOD_SETUP:
            ezrtsp_setup(rtspc);
            break;
        case METHOD_PLAY:
            ezrtsp_play(rtspc);
            break;
        default:;
    }
    evt->write_cb = ezrtsp_rsp_send;
    return evt->write_cb(ev_ctx, evt);
}

static void ezrtsp_req(ev_ctx_t *ev_ctx, ev_t *evt) {
    rtsp_con_t *rtspc = (rtsp_con_t*)evt->data;
    char *p = NULL;

    while (!rtspc->fcomplete) {
        int recvd = recv(evt->fd, rtspc->meta->last, meta_getfree(rtspc->meta), 0);
        if (recvd <= 0) {
            if (recvd == 0) {
                err("ezrtsp [%p:%d] peer closed\n", rtspc, evt->fd);
                return ezrtsp_con_free(rtspc);
            } else {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    if (rtspc->fplay) {
                        dbg("ezrtsp [%p:%d] played. recvd [%ld]\n", 
                            rtspc, evt->fd, meta_getlen(rtspc->meta));
                        meta_clr(rtspc->meta);
                        ev_timer_del(ev_ctx, evt);
                    } else {
                        ev_timer_add(ev_ctx, evt, ezrtsp_con_expire, 5000);
                    }
                } else {
                    err("ezrtsp [%p:%d] recv err. [%d]\n", rtspc, evt->fd, errno);
                    return ezrtsp_con_free(rtspc);
                }
                return;
            }
        }
        rtspc->meta->last += recvd;
        rtspc->reqfin = strstr(rtspc->meta->pos, "\r\n\r\n");
        if (rtspc->reqfin) 
            rtspc->fcomplete = 1;

        if (meta_getfree(rtspc->meta) < 1) {
            err("ezrtsp [%p:%d] meta full still not complete.\n", rtspc, evt->fd);
            return ezrtsp_con_free(rtspc);
        }
    }
    ev_timer_del(ev_ctx, evt);
    
    dbg("<========\n");
    dbg("%s\n", rtspc->meta->pos);

    p = strstr(rtspc->meta->pos, "CSeq:");
    if (!p) {
        err("ezrtsp no 'Cseq'\n");
        return ezrtsp_con_free(rtspc);
    }
    rtspc->icseq = strtol(p + strlen("CSeq:"), NULL, 10);

    enum {
        S_METHOD = 0,
        S_URL_S = 1,
        S_URL = 2
    };

    rtspc->method = rtspc->meta->pos;
    for (p = rtspc->meta->pos; p < rtspc->meta->last; p++) {
        if (rtspc->state == S_METHOD) {
            if (*p == ' ') {
                rtspc->methodn = p - rtspc->method;
                rtspc->state = S_URL_S;
                continue;
            }
        }

        if (rtspc->state == S_URL_S) {
            rtspc->url = p;
            rtspc->state = S_URL;
            continue;
        }

        if (rtspc->state == S_URL) {
            if (*p == ' ') {
                rtspc->urln = p - rtspc->url;
                break;
            }
        }
    }

    if (strncasecmp(rtspc->method, "OPTIONS", rtspc->methodn) == 0) {
        rtspc->imethod = METHOD_OPTIONS;
    } else if (strncasecmp(rtspc->method, "DESCRIBE", rtspc->methodn) == 0) {
        rtspc->imethod = METHOD_DESCRIBE;
        if (NULL != strstr(rtspc->meta->pos, EZRTSP_URI_MAIN)) { ///get request chnnel
            rtspc->ichn = 0;
        } else if(NULL != strstr(rtspc->meta->pos, EZRTSP_URI_SUB)) {
            rtspc->ichn = 1;
        } else {
            err("ezrtsp DESCRIBE <url> not support.\n");
            return ezrtsp_con_free(rtspc);
        }
    } else if (strncasecmp(rtspc->method, "SETUP", rtspc->methodn) == 0) {
        rtspc->imethod = METHOD_SETUP;
        if (NULL != strstr(rtspc->url, "/track=0")) {
            rtspc->itrack = 0;
        } else if (NULL != strstr(rtspc->url, "/track=1")) {
            rtspc->itrack = 1;
        } else {
            err("ezrtsp SETUP <track> not support.\n");
            return ezrtsp_con_free(rtspc);
        }
        rtspc->fovertcp = (strstr(rtspc->meta->pos, "TCP")) ? 1 : 0;
        if (!rtspc->fovertcp) {
            char * client_port = strstr(rtspc->meta->pos, "client_port=");
            char str_rtpp[32] = {0};
            char str_rtcpp[32] = {0};
            sscanf(client_port + strlen("client_port="), "%[0-9]-%[0-9]", str_rtpp, str_rtcpp);
            rtspc->irtp_port = atoi(str_rtpp);
            rtspc->irtcp_port = atoi(str_rtcpp);
            dbg("ezrtp SETUP. cli port <%d:%d>\n", rtspc->irtp_port, rtspc->irtcp_port);
        }
    }else if (strncasecmp(rtspc->method, "PLAY", rtspc->methodn) == 0) {
        rtspc->imethod = METHOD_PLAY;
    } else if (strncasecmp(rtspc->method, "TEARDOWN", rtspc->methodn) == 0) {
        dbg("ezrtsp TEARDOWN con free\n");
        return ezrtsp_con_free(rtspc); 
    } else {
        err("ezrtsp method not support.\n");
        return ezrtsp_con_free(rtspc);
    }
    evt->read_cb = NULL;
    evt->write_cb = ezrtsp_rsp;
    ev_opt(ev_ctx, evt, EV_W);
    return evt->write_cb(ev_ctx, evt);
}

static void ezrtsp_accept(ev_ctx_t *ev_ctx, ev_t *evt) {
    rtsp_con_t * rtspc = NULL;

    for (;;) {
        socklen_t addrn = sizeof(struct sockaddr_in);
        struct sockaddr_in addr;
        memset( &addr, 0, addrn );

        int fd = accept(evt->fd, (struct sockaddr*)&addr, &addrn);
        if (fd == -1) {
            if(errno == EWOULDBLOCK ||
                errno == EAGAIN ||
                errno == EINTR ||
                errno == EPROTO ||
                errno == ECONNABORTED
            ) {
                break;
            }
            err("rtsp accept failed. [%d]\n", errno);
            break;
        }
        int nbio = 1;
        ioctl(fd, FIONBIO, &nbio);

        if (0 == ezrtsp_con_alloc(&rtspc)) {
            rtspc->ev_ctx = ev_ctx;

            ev_t *nevt = ev_alloc_evt(ev_ctx);
            if (nevt) {
                rtspc->evt = nevt;
                strcpy(rtspc->client_ip, inet_ntoa(addr.sin_addr));
                dbg("ezrtsp accept [%p:%d] form [%s]\n", rtspc, fd, rtspc->client_ip);
                
                nevt->fd = fd;
                nevt->ev_ctx = ev_ctx;
                nevt->data = rtspc;
                nevt->read_cb = ezrtsp_req;
                ev_opt(ev_ctx, nevt, EV_R);
                return nevt->read_cb(ev_ctx, nevt);
            } else {
                ezrtsp_con_free(rtspc);
            }
        }
    }    
    return;
}

static void *ezrtsp_serv_task(void *para) {
    ev_ctx_t *ev_ctx = NULL;
    ev_t *evt = NULL;

    EZRTSP_THNAME("ezrtsp");
    if (0 != ev_create(&ev_ctx)) {
        err("ev ctx create err\n");
        return NULL;
    }

    evt = ev_alloc_evt(ev_ctx);
    if (evt) {
        evt->ev_ctx = ev_ctx;
        evt->fd = listen_fd;
        evt->read_cb = ezrtsp_accept;
        evt->write_cb = NULL;
        
        ev_opt(ev_ctx, evt, EV_R);
        while (rtsp_stat & EZRTSP_INIT) {
            ev_loop(ev_ctx);
        }
    }

    ev_free(ev_ctx);
    close(listen_fd);
    return NULL;
}

int ezrtsp_serv_start(void) {
    dbg("ezrtsp listen on [%s:%d]\n", "0.0.0.0", EZRTSP_PORT);
    struct sockaddr_in addr;
    memset( &addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(EZRTSP_PORT);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd <= 0) {
        err("ezrtsp listen socket open err. [%d]\n", errno);
        return -1;
    }
    int flag = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    ioctl(listen_fd, FIONBIO, &flag);

    if (0 != bind(listen_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr))) {
        err("ezrtsp listen socket bind err. [%d]\n", errno);
        close(listen_fd);
        return -1;
    }
    if (0 != listen(listen_fd,10)) {
        err("ezrtsp listen socket listen err. [%d]\n", errno);
        close(listen_fd);
        return -1;
    }

    ///init ezrtsp connection manager
    ezrtsp_con_init();
    
    rtsp_stat |= EZRTSP_INIT;
    if (0 != pthread_create(&rtsp_pid, NULL, &ezrtsp_serv_task, NULL)) {
        err("ezrtsp rtsptask create err. [%d]\n", errno);
        close(listen_fd);
        return -1;
    }
    return 0;
}

int ezrtsp_serv_stop(void) {
    if(rtsp_stat == 1) {
        rtsp_stat = 0;
        pthread_join(rtsp_pid, NULL);
    }
    return 0;
}
