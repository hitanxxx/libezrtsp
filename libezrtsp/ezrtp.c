#include "ezrtsp.h"
#include "ezcache.h"
#include "ezrtsp_utils.h"

int ezrtp_packet_send(int fd, char * data, int datan)
{
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

static int ezrtp_packet_build(rtsp_session_t * session, int payload, const unsigned char * data, int datan, int marker)
{
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
    PUT_32(rtp_hdr + 4, session->ts);   /// PTS
    PUT_32(rtp_hdr + 8, session->ssrc); /// SSRC
    offset += 12;

    ///RTP payload 
    memcpy((rtp_packet + offset), data, datan);
    ///send
    int fd = (session->c->fovertcp) ? session->c->fd : session->fd_rtp;
    if(0 != ezrtp_packet_send(fd, rtp_packet, offset + datan)) {
        return -1;
    }
    session->seq++;
    return 0;
}

static int ezrtp_fu_265(rtsp_session_t * session, int payload, unsigned char * nal, int naln, int mark)
{
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
    if(session->c->fovertcp) mss -= 4;
    mss -= 3;
    
    while(naln > mss) {
        pktn = mss;
        
        memcpy(&pkt[3], fua, pktn);
        if((ret = ezrtp_packet_build(session, payload, pkt, pktn+3, 0)) < 0) {
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

static int ezrtp_fu_264(rtsp_session_t * session, int payload, unsigned char * nal, int naln, int mark)
{
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
    if(session->c->fovertcp) mss -= 4;
    mss -= 2;

    while(naln > mss) {
        
        pktn = mss;
        memcpy(&pkt[2], fua, pktn);
        if((ret = ezrtp_packet_build(session, payload, pkt, pktn+2, 0)) < 0) {
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

static int ezrtp_send_vnalu(rtsp_session_t * session, unsigned char * data, int datan, char nalu_fin)
{
    int ret = 0;
   
    int mss = EZRTSP_MSS;
    mss -= 12;
    if(session->c->fovertcp) mss -= 4;
    
    if(datan <= mss) {
        ret = ezrtp_packet_build(session, ezrtsp_video_codec_typ() == VT_H264 ? 96 : 97, data, datan, nalu_fin);
    } else {
        if(ezrtsp_video_codec_typ() == VT_H264) {
            ret = ezrtp_fu_264(session, 96, data, datan, nalu_fin);
        } else {
            ret = ezrtp_fu_265(session, 97, data, datan, nalu_fin);
        }
    }  
    return ret;
}

static int ezrtp_send_analu(rtsp_session_t * session, unsigned char * data, int datan)
{
    const unsigned char * aac = NULL;
    unsigned char pkt[EZRTSP_MSS] = {0};
    int pktn = 0;
    int ret = -1;

    if(ezrtsp_audio_codec_typ() == AT_AAC) {
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
        if(session->c->fovertcp) mss -= 4;
        mss -= 4;

        while(datan > mss) {
            pktn = mss;
            memcpy(&pkt[4], aac, pktn);
            ret = ezrtp_packet_build(session, 98, pkt, pktn+4, 0);
            if(ret < 0) {
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
        if(session->c->fovertcp) mss -= 4;
        mss -= 4;

        while(datan > mss) {
            pktn = mss;

            ret = ezrtp_packet_build(session, 8, data, pktn, 0);
            if(ret < 0) 
                break;

            data += pktn;
            datan -= pktn;
        }
        return ezrtp_packet_build(session, 8, data, datan, 1);
    }
    err("ezrtsp not support audio codec typ [%d]\n", ezrtsp_audio_codec_typ());
    return -1;
}

static int ezrtp_send_audio(rtsp_con_t * c, unsigned char * data, int datan)
{
    if(!c->faudioenb) return 0;
    rtsp_session_t * ses = &c->session_audio;

    unsigned long long ts_now = ezrtsp_ts_msec();
    if(ses->ts_prev > 0) ses->ts += (ts_now - ses->ts_prev) * 8;
    ses->ts_prev = ts_now;

    int ret = ezrtp_send_analu(ses, data, datan);
    if(ret != 0) {
        ses->send_err++;
        if(ses->send_err >= 7) {
            err("ezrtsp [%p:%d] rtp afrm send errn [%d]\n", c, c->fd, ses->send_err);
            return -1;
        }
    }
    ses->send_err = 0;
    return 0;
}

static int ezrtp_send_video(rtsp_con_t * c, unsigned char * data, int datan, int idr, char nalu_fin)
{
    if(!c->fvideoenb) return 0;
    rtsp_session_t * ses = &c->session_video;

    unsigned long long ts_now = ezrtsp_ts_msec();
    if(ses->ts_prev > 0) ses->ts += (ts_now - ses->ts_prev) * 90;
    ses->ts_prev = ts_now;

    int ret = ezrtp_send_vnalu(ses, data, datan, nalu_fin);
    if(ret != 0) {
        ses->send_err ++;
        if(ses->send_err >= 7) {
            err("ezrtsp [%p:%d] rtp vfrm send errn [%d]\n", c, c->fd, ses->send_err);
            return -1;
        }
    }
    ses->send_err = 0;
    return 0;
}

static void * ezrtp_task(void * param)
{
    EZRTSP_THNAME("hm2p_ezrtp");
    rtsp_con_t * c = (rtsp_con_t *) param;
    unsigned long long ts_rtcp_sr = 0;
    int nfrmn = 0;

    if(c->ichseq == -1) {
        c->ichseq = ezcache_seq_last(c->ichn);
    }

    while(c->rtp_stat & EZRTP_INIT) {
        ezcache_frm_t * frm = ezcache_frm_get(c->ichn, c->ichseq);
        if(frm) {
            nfrmn = 0;
            c->ichseq ++;
            
            if(frm->typ == 0) {
                if(0 != ezrtp_send_audio(c, frm->data, frm->datan)) {
                    err("ezrtp audio send err\n");
                    ezrtsp_con_free(c);
                    break;
                } 
            } else {
                if(0 != ezrtp_send_video(c, frm->data, frm->datan, (frm->typ == 1) ? 1 : 0, frm->nalu_fin)) {
                    err("ezrtp video send err\n");
                    ezrtsp_con_free(c);
                    break;
                }
            }
            ezrtsp_free(frm);
        } else {
            nfrmn++;
            if(nfrmn > 5) {
                nfrmn = 0;
                sys_msleep(5);
            }
        }

        unsigned long long ts_now = ezrtsp_ts_msec();
        if(ts_now - ts_rtcp_sr >= 5000) {
            ezrtcp_sr_send(c);
            ts_rtcp_sr = ts_now;
        }
    }
    return NULL;
}

int ezrtp_start(rtsp_con_t * c)
{
    int flag = 1;
    if(c->fovertcp) setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &flag, sizeof(flag));
    
    if(!(c->rtp_stat & EZRTP_INIT)) {
        c->rtp_stat |= EZRTP_INIT;
        if(0 != pthread_create(&c->rtp_pid, NULL, &ezrtp_task, c)) {
            err("ezrtp task create err. [%d]\n", errno);
            return -1;
        }
    } else {
        err("ezrtsp con already in playing?\n");
    }
    return 0;
}