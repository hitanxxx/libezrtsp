#include "ezrtsp.h"
#include "ezcache.h"
#include "ezrtsp_utils.h"


int ezrtp_packet_send(int fd, char *data, int datan) {
    int sendn = 0;
    int tryn = 0;

    while(sendn < datan) {
        int rc = send(fd, data + sendn, datan - sendn, 0);
        if(rc < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                sys_msleep(3);
                tryn++;
                if(tryn >= 3) {
                    return -1;///drop the packet if failure times more than 3
                }
                continue;
            }
            err("ezrtp send err. [%d]\n", errno);
            return -1;
        }
        sendn += rc;
        tryn = 0;
    }
    return 0;
}

static int ezrtp_packet_build(rtsp_session_t * session, int payload, const unsigned char * data, int datan, int marker) {
    char rtp_packet[EZRTSP_MSS] = {0};
    int offset = 0;
    
    /// RTP over tcp header (4 byte)
    if(session->c->fovertcp) {
        rtp_packet[0] = '$';                                    ///rtp over tcp flag. fixed. 0x24
        rtp_packet[1] = session->trackid*2;   				    ///same as SETUP interleaved
        PUT_16(rtp_packet + 2, (12 + datan));                   ///12 is rtp header length
        offset += 4;
    }

    /*
        rtp header format
           0                   1                   2                   3
           0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |V=2|P|X|  CC   |M|     PT      |       sequence number         |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |                           timestamp                           |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |           synchronization source (SSRC) identifier            |
          +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
          |            contributing source (CSRC) identifiers             |
          |                             ....                              |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    ///RTP header (12 byte)
    char * rtp_hdr = rtp_packet + offset;
    rtp_hdr[0] = 0x2<<6;  /// fixed value
    rtp_hdr[1] = ((marker&0x1)<<7) | (payload&0x7f); // fixed value
    if(session->seq > 0xffff) session->seq = 0;
    PUT_16(rtp_hdr + 2, session->seq);  /// sequence
    ///dbg("packet seq [%hd]\n", session->seq);
    PUT_32(rtp_hdr + 4, session->ts);   /// PTS
    PUT_32(rtp_hdr + 8, session->ssrc); /// SSRC
    offset += 12;

    ///RTP payload 
    memcpy((rtp_packet + offset), data, datan);
    ///send
    int fd = (session->c->fovertcp) ? session->c->evt->fd : session->fd_rtp;
    ///dbg("fd [%d]\n", fd);
    if(0 != ezrtp_packet_send(fd, rtp_packet, offset + datan)) {
        return -1;
    }
    session->seq++;
    return 0;
}

static int ezrtp_fu_265(rtsp_session_t *session, int payload, unsigned char *nal, int naln, int mark) {
    unsigned char * fua = NULL;
    unsigned char pkt[EZRTSP_MSS] = {0};
    int pktn = 0;
    int ret = -1;

    /*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |    PayloadHdr (Type=49)       |   FU header   | DONL (cond)   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
       | DONL (cond)   |                                               |
       |-+-+-+-+-+-+-+-+                                               |
       |                         FU payload                            |
       |                                                               |
       |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                               :...OPTIONAL RTP padding        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

       PayloadHr == h265 nalu header
       +---------------+---------------+
       |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |F|   Type    |  LayerId  | TID |
       +-------------+-----------------+

       FU heahder
       +---------------+
       |0|1|2|3|4|5|6|7|
       +-+-+-+-+-+-+-+-+
       |S|E|  FuType   |
       +---------------+
    */

    pkt[0] = 49<<1;
    pkt[1] = 1;
    
    pkt[2] = 0x80|((nal[0]&0b01111110)>>1);

    /// skip 2 byte h265 nalu header 
    fua = nal + 2;
    naln -= 2;

    int mss = EZRTSP_MSS;
    mss -= 12;
    if (session->c->fovertcp) 
        mss -= 4;
    mss -= 3;
    
    while (naln > mss) {
        pktn = mss;
        
        memcpy(&pkt[3], fua, pktn);
        if ((ret = ezrtp_packet_build(session, payload, pkt, pktn+3, 0)) < 0) {
            return ret;
        }
        
        fua += pktn;
        naln -= pktn;

        pkt[2] &= ~0x80;   /// clear start flag
    }
    pkt[2] |= 0x40; /// set end flag
    memcpy(&pkt[3], fua, naln);
    return ezrtp_packet_build(session, payload, pkt, (naln+3), 1);
}

static int ezrtp_fu_264(rtsp_session_t *session, int payload, unsigned char *nal, int naln, int mark) {
    /*  FU-A type
         0                   1                   2                   3
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | FU indicator  |   FU header   |                               |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
        |                                                               |
        |                         FU payload                            |
        |                                                               |
        |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                               :...OPTIONAL RTP padding        |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        FU indicator
       +---------------+
       |0|1|2|3|4|5|6|7|
       +-+-+-+-+-+-+-+-+
       |F|NRI|  Type   |
       +---------------+

       FU header
       +---------------+
       |0|1|2|3|4|5|6|7|
       +-+-+-+-+-+-+-+-+
       |S|E|R|  Type   |
       +---------------+
    */

    int ret = 0;
    unsigned char * fua = NULL;
    unsigned char pkt[EZRTSP_MSS] = {0};
    int pktn = 0;

    pkt[0] = (nal[0]&0b11100000) | 28;    /// FU indicator
    pkt[1] = 0x80 | (nal[0]&0b00011111);   /// FU header

    /// skip nalu header (h264 1 byte)
    fua = nal+1;
    naln -= 1;

    int mss = EZRTSP_MSS;
    mss -= 12;
    if (session->c->fovertcp)
        mss -= 4;
    mss -= 2;

    while (naln > mss) {
        pktn = mss;
        memcpy(&pkt[2], fua, pktn);
        if ((ret = ezrtp_packet_build(session, payload, pkt, pktn+2, 0)) < 0) {
            return ret;
        }

        fua += pktn;
        naln -= pktn;
        pkt[1] &= ~0x80; /// clear start flag
    }
    pkt[1]  |= 0x40;  /// set end flag
    memcpy(&pkt[2], fua, naln);
    return ezrtp_packet_build(session, payload, pkt, (naln+2), 1);
}

static int ezrtp_send_vnalu(rtsp_session_t *session, unsigned char *data, int datan, char nalu_fin) {
    int ret = 0;
    int mss = EZRTSP_MSS;
    mss -= 12;
    if (session->c->fovertcp)
        mss -= 4;
    
    if (datan <= mss) {
        ret = ezrtp_packet_build(session, ezrtsp_video_codec_typ() == VT_H264 ? 96 : 97, data, datan, nalu_fin);
    } else {
        if (ezrtsp_video_codec_typ() == VT_H264) {
            ret = ezrtp_fu_264(session, 96, data, datan, nalu_fin);
        } else {
            ret = ezrtp_fu_265(session, 97, data, datan, nalu_fin);
        }
    }  
    return ret;
}

static int ezrtp_send_analu(rtsp_session_t *session, unsigned char *data, int datan) {
    const unsigned char *aac = NULL;
    unsigned char pkt[EZRTSP_MSS] = {0};
    int pktn = 0;
    int ret = -1;

    if (ezrtsp_audio_codec_typ() == AT_AAC) {
        ///skip ADTS header
        datan -= 7;
        pkt[0] = 0x0;
        pkt[1] = 0x10;
        pkt[2] = (datan & 0x1fe0) >> 5;
        pkt[3] = (datan & 0x1f) << 3;
        ///skip nalu header
        aac = data + 7;

        int mss = EZRTSP_MSS;
        mss -= 12;
        if (session->c->fovertcp)
            mss -= 4;
        mss -= 4;

        while (datan > mss) {
            pktn = mss;
            memcpy(&pkt[4], aac, pktn);
            ret = ezrtp_packet_build(session, 98, pkt, pktn+4, 0);
            if (ret < 0) {
                break;
            }
            aac += pktn;
            datan -= pktn;
        }
        memcpy(&pkt[4], aac, datan);
        return ezrtp_packet_build(session, 98, pkt, datan+4, 1);
    } else if(ezrtsp_audio_codec_typ() == AT_G711A) {
   
        int mss = EZRTSP_MSS;
        mss -= 12;
        if (session->c->fovertcp) 
            mss -= 4;
        mss -= 4;

        while (datan > mss) {
            pktn = mss;

            ret = ezrtp_packet_build(session, 8, data, pktn, 0);
            if (ret < 0) 
                break;

            data += pktn;
            datan -= pktn;
        }
        return ezrtp_packet_build(session, 8, data, datan, 1);
    }
    err("ezrtsp not support audio codec typ [%d]\n", ezrtsp_audio_codec_typ());
    return -1;
}

static int ezrtp_send_audio(rtsp_con_t *rtspc, unsigned char *data, int datan) {
    if (!rtspc->faudioenb) 
        return 0;
    
    rtsp_session_t *ses = &rtspc->session_audio;
    unsigned long long ts_now = ezrtsp_ts_msec();
    if (ses->ts_prev > 0) 
        ses->ts += (ts_now - ses->ts_prev) * 8;
    ses->ts_prev = ts_now;

    int ret = ezrtp_send_analu(ses, data, datan);
    if (ret != 0) {
        ses->send_err++;
        if (ses->send_err >= 7) {
            err("ezrtsp [%p:%d] rtp afrm send errn [%d]\n", rtspc, rtspc->evt->fd, ses->send_err);
            return -1;
        }
    }
    ses->send_err = 0;
    return 0;
}

static const char *AvcFindInternalStartCode(const char *p, const char *end)
{
    const char *a = p + 4 - ((int)p & 3);

    for (end -= 3; p < a && p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4)
    {
        int x = *(const int *)p;
        if ((x - 0x01010101) & (~x) & 0x80808080)   // generic
        {
            if (p[1] == 0)
            {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p + 1;
            }
            if (p[3] == 0)
            {
                if (p[2] == 0 && p[4] == 1)
                    return p + 2;
                if (p[4] == 0 && p[5] == 1)
                    return p + 3;
            }
        }
    }

    for (end += 3; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

const char *AvcFindStartCode(const char *p, const char *end)
{
    const char *out = AvcFindInternalStartCode(p, end);
    if (p < out && out < end && !out[-1])
    {
        out--;
    }
    return out;
}

static int ezrtp_send_video(rtsp_con_t *rtspc, unsigned char *data, int datan) {
    if (!rtspc->fvideoenb) 
        return 0;
    
    rtsp_session_t *ses = &rtspc->session_video;
    unsigned long long ts_now = ezrtsp_ts_msec();
    if (ses->ts_prev > 0) 
        ses->ts += (ts_now - ses->ts_prev) * 90;
    ses->ts_prev = ts_now;

    const char *p = (char*)data;
    const char *end = (char*)data + datan;
    const char *nal_start = NULL;
    const char *nal_end = NULL;

    nal_start = AvcFindStartCode(p, end);
    for (;;) {
        int nlen = 0;
        while (nal_start < end && !*(nal_start++)) {
            ;
        }
        if (nal_start == end) {
            break;
        }

        nal_end = AvcFindStartCode(nal_start, end);
        nlen = nal_end - nal_start;

        if (0) {
            int typ = nal_start[0] & 0x1f;
            dbg("naluint typ [%d] len [%d]\n", typ, nlen + 4);
        }
        
        int ret = ezrtp_send_vnalu(ses, (unsigned char*)nal_start, nlen, nal_start + nlen >= nal_end ? 1 : 0);
        if (ret != 0) {
            ses->send_err ++;
            if (ses->send_err >= 7) {
                err("ezrtsp [%p:%d] rtp vfrm send errn [%d]\n", rtspc, rtspc->evt->fd, ses->send_err);
                return -1;
            }
        }
        
        nal_start = nal_end;
    }
    
    ses->send_err = 0;
    return 0;
}

#if 0
void *ezrtp_ff_task(void *param) {
    EZRTSP_THNAME("lib_ezrtp-ff");
    rtsp_con_t *rtspc = (rtsp_con_t *)param;
    
    while (rtspc->rtp_stat & EZRTP_INIT) {
        /// cut h264 file frame by frame 
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
#endif
        
        AVFormatContext *ff_format_ctx = NULL;
        AVPacket *pkt = NULL;

        if (avformat_open_input(&ff_format_ctx, "./1718880308_test.h264", NULL, NULL) != 0) {
            printf("ff input file open err\n");
            return NULL;
        }
        if (avformat_find_stream_info(ff_format_ctx, NULL) < 0) {
            printf("ff input file get stream info err\n");
            avformat_close_input(&ff_format_ctx);
            return NULL;
        }
        pkt = av_packet_alloc();
        if (!pkt) {
            printf("ff pkt alloc err\n");
            avformat_close_input(&ff_format_ctx);
            return NULL;
        }
        while (1) {
            int ret = av_read_frame(ff_format_ctx, pkt);
            if (ret < 0) {
                printf("reset stream\n");
                ret = av_seek_frame(ff_format_ctx, 0, 0, AVSEEK_FLAG_BACKWARD);
                if (ret < 0) {
                    printf("stream seek to start err\n");
                    break;
                }
            } else {
                printf("frame data len [%d] stream idx [%d] key frame [%s]\n",
                    pkt->size,
                    pkt->stream_index,
                    pkt->flags == AV_PKT_FLAG_KEY ? "yes" : "no"
                );
                ret = ezrtp_send_video(rtspc, pkt->data, pkt->size);

                usleep((1000/30)*1000);
            }
           av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        avformat_close_input(&ff_format_ctx);
    }
    
    return NULL;
}
#endif

static void * ezrtp_task(void *param) {
    EZRTSP_THNAME("lib_ezrtp");
    
    rtsp_con_t *rtspc = (rtsp_con_t *)param;
    unsigned long long ts_rtcp_sr = 0;
    unsigned long long ts_catch = 0;
    int frm_fail = 0;
    int ret = 0;

    dbg("rtp send task in working\n");
    while (rtspc->rtp_stat & EZRTP_INIT) {
    
        if (rtspc->ichseq == -1) {
            rtspc->ichseq = ezcache_idr_last(rtspc->ichn);
            dbg("rtspc chseq [%lld]\n", rtspc->ichseq);
        }
        
        unsigned long long ts_now = ezrtsp_ts_msec();
        if (ts_catch == 0) 
            ts_catch = ts_now;

        if (ts_now - ts_catch > 500) {
	    ts_catch = ts_now;

            long long last_seq = ezcache_seq_last(rtspc->ichn);
            long long limit = 25;
            long long delta = last_seq - rtspc->ichseq;
            if (delta >= limit) {
                err("rtp delay frame [%lld] >= [%lld]. skip it\n", delta, limit);
                rtspc->ichseq = -1;
                continue;
            }
        }

        if(ts_now - ts_rtcp_sr >= 5000) {
            ezrtcp_sr_send(rtspc);
            ts_rtcp_sr = ts_now;
        }

        /// get frame data form cache manager 
        ezcache_frm_t * frm = ezcache_frm_get(rtspc->ichn, rtspc->ichseq);
        if (frm) {
            frm_fail = 0;
            rtspc->ichseq ++;
            ///dbg("get frm form cache ok. type [%s] iframe [%d]\n", frm->typ == 0 ? "audio" : "video", (frm->typ == 1) ? 1 : 0);
            if(frm->typ == 0) {
                ret = ezrtp_send_audio(rtspc, frm->data, frm->datan);
            } else {
                ret = ezrtp_send_video(rtspc, frm->data, frm->datan);
            }
            ez_free(frm);
            if (ret != 0) 
                err("ezrtp send [%s] frame err\n", frm->typ == 0 ? "audio" : "video");
        } else {
            frm_fail ++;
            if(frm_fail > 5) {
                frm_fail = 0;
                sys_msleep(5);
            }
        }
    }
    return NULL;
}

int ezrtp_start(rtsp_con_t *rtspc) {
    int flag = 1;
    if (rtspc->fovertcp) 
        setsockopt(rtspc->evt->fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &flag, sizeof(flag));
    
    if (!(rtspc->rtp_stat & EZRTP_INIT)) {
        rtspc->rtp_stat |= EZRTP_INIT;
        if(0 != pthread_create(&rtspc->rtp_pid, NULL, &ezrtp_task, rtspc)) {
            err("ezrtp task create err. [%d]\n", errno);
            return -1;
        }
    } else {
        err("ezrtsp con already in playing???\n");
    }
    return 0;
}
