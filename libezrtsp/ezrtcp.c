
#include "ezrtsp.h"

int ezrtcp_sr_send(rtsp_con_t *c)
{
    dbg("\n");
    struct timeval tm;
	gettimeofday(&tm, NULL);
    double fractionalPart = (tm.tv_usec/15625.0)*0x04000000; // 2^32/10^6
    unsigned int ntp_sec 	= tm.tv_sec + 0x83AA7E80;
    unsigned int ntp_frac 	= (unsigned int)(fractionalPart+0.5);

    char buf[512] = {0};
    int bufn = 0;
    int fd = 0;

    memset(buf, 0x0, sizeof(buf));
    bufn = 0;
    if(c->faudioenb) {
        ezrtcp_sr_t sr;
        memset(&sr, 0x0, sizeof(sr));
    
        sr.ver = 2;
        sr.pad = 0;
        sr.pt = 200;
        sr.count = 0;
        sr.len = htons(sizeof(sr)/4-1);

        sr.ssrc = htonl(c->session_audio.ssrc);
        sr.ntp_sec = htonl(ntp_sec);
        sr.ntp_frac = htonl(ntp_frac);
        sr.rtp_ts = htonl(c->session_audio.ts);
        sr.psent = 0;
        sr.osent = 0;

        
        if(c->fovertcp) {
            buf[0] = '$';
            buf[1] = (c->session_audio.trackid*2)+1;
            PUT_16(buf + 2, sizeof(sr));
            bufn += 4;
        }
        memcpy(buf + bufn, &sr, sizeof(sr));
        bufn += sizeof(sr);

        fd = (c->fovertcp) ? c->fd : c->session_audio.fd_rtcp;
        if(0 != ezrtp_packet_send(fd, (void*)buf, bufn)) {
            err("ezrtcp sr audio session send err\n");
        }
    }

    memset(buf, 0x0, sizeof(buf));
    bufn = 0;
    if(c->fvideoenb) {
        ezrtcp_sr_t sr;
        memset(&sr, 0x0, sizeof(sr));
    
        sr.ver = 2;
        sr.pad = 0;
        sr.pt = 200;
        sr.count = 0;
        sr.len = htons(sizeof(sr)/4-1);

        sr.ssrc = htonl(c->session_video.ssrc);
        sr.ntp_sec = htonl(ntp_sec);
        sr.ntp_frac = htonl(ntp_frac);
        sr.rtp_ts = htonl(c->session_video.ts);
        sr.psent = 0;
        sr.osent = 0;

        memset(buf, 0x0, sizeof(buf));
        if(c->fovertcp) {
            buf[0] = '$';
            buf[1] = (c->session_video.trackid*2)+1;
            PUT_16(buf + 2, sizeof(sr));
            bufn += 4;
        }
        memcpy(buf + bufn, &sr, sizeof(sr));
        bufn += sizeof(sr);

        fd = (c->fovertcp) ? c->fd : c->session_video.fd_rtcp;
        if(0 != ezrtp_packet_send(fd, (void*)buf, bufn)) {
            err("ezrtcp sr video session send err\n");
        }
    }
    return 0;
}