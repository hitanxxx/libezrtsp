#include "ezrtsp.h"
#include "ezcache.h"
#include "ezrtsp_utils.h"

static char faudio_cfg = 0;
static unsigned char audio_adts_hdr[4] = {0};

typedef struct {
    int cfg;
    sys_data_t * vps;
    sys_data_t * sps;
    sys_data_t * pps; 
} video_cfg_t;
static video_cfg_t video_cfg[2] = {0};
static ezrtsp_ctx_t g_ctx = {0};

int ezrtsp_audio_codec_typ()
{
    return g_ctx.atype;
}

int ezrtsp_video_codec_typ()
{
    return g_ctx.vtype;
}

int ezrtsp_video_sequence_parament_set_get(int ch, sys_data_t ** vps, sys_data_t ** sps, sys_data_t ** pps)
{
    if(vps) *vps = video_cfg[ch].vps;
    if(sps) *sps = video_cfg[ch].sps;
    if(pps) *pps = video_cfg[ch].pps;
    return 0;
}

static int ezrtsp_video_sequence_parament_set_save(unsigned char * data, int datan, sys_data_t ** sys)
{
    sys_data_t * t = sys_alloc(sizeof(sys_data_t) + datan);
    if(!t) {
        err("sys data alloc err\n");
        return -1;
    }
    t->datan = datan;
    memcpy(t->data, data, datan);
    *sys = t;
    return 0;
}


static int ezrtsp_video_sequence_parament_set_proc(int ch, unsigned char * nalu, int nalun, char fin)
{
    if(g_ctx.vtype == VT_H264) {
        int nalu_type = nalu[0] & 0b00011111;
        if(nalu_type == 7) {
            if(!video_cfg[ch].sps) {
                ezrtsp_video_sequence_parament_set_save(nalu, nalun, &video_cfg[ch].sps);
                dbg("save sps <%p:%d>\n", nalu, nalun);
            }
        } else if (nalu_type == 8) {
            if(!video_cfg[ch].pps) {
                ezrtsp_video_sequence_parament_set_save(nalu, nalun, &video_cfg[ch].pps);
                dbg("save pps <%p:%d>\n", nalu, nalun);
            }
        }
        if(video_cfg[ch].sps && video_cfg[ch].pps) {
            video_cfg[ch].cfg = 1;
        }
        ezcache_frm_add(ch, nalu, nalun, nalu_type == 5 ? 1 : 2, sys_ts_msec(), fin);

    } else {
        int nalu_type = (nalu[0] & 0b01111110) >> 1;
        if(nalu_type == 32) {  ///vps
            if(!video_cfg[ch].vps) {
                ezrtsp_video_sequence_parament_set_save(nalu, nalun, &video_cfg[ch].vps);
            }
        } else if (nalu_type == 33) {   ///sps
            if(!video_cfg[ch].sps) {
                ezrtsp_video_sequence_parament_set_save(nalu, nalun, &video_cfg[ch].sps);
            }
        } else if (nalu_type == 34) {   ///pps
            if(!video_cfg[ch].pps) {
                ezrtsp_video_sequence_parament_set_save(nalu, nalun, &video_cfg[ch].pps);
            }
        }
        if(video_cfg[ch].vps && video_cfg[ch].sps && video_cfg[ch].pps) {
            video_cfg[ch].cfg = 1;
        }
        ezcache_frm_add(ch, nalu, nalun, (nalu_type == 19 || nalu_type == 20) ? 1 : 2, sys_ts_msec(), fin);
    }
    return 0;
}


int ezrtsp_video_sequence_parament_set_ready(int ch)
{
    if(video_cfg[ch].cfg) return 1;
    return 0;
}

int ezrtsp_audio_enb()
{
    return g_ctx.aenb;
}

int ezrtsp_audio_aacadts_ready()
{
    return faudio_cfg;
}

static int ezrtsp_audio_aacadts_proc(unsigned char * data, int datan)
{
    if(!faudio_cfg) {
        memcpy(audio_adts_hdr, data, datan < 4 ? datan : 4);
        faudio_cfg = 1;
    }
    return 0;
}

unsigned char * ezrtso_audio_aadadts_get()
{
    return audio_adts_hdr;
}

int ezrtsp_push_afrm(unsigned char *data, int datan)
{
    if(!ezrtsp_audio_aacadts_ready()) {
        ezrtsp_audio_aacadts_proc(data, datan);
        faudio_cfg = 1;
    }
    unsigned long long ts_now = sys_ts_msec();
    ezcache_frm_add(0, data, datan, 0, ts_now ,0);
    return 0;
}

int ezrtsp_push_vfrm(int ch, unsigned char * data, int datan)
{   
    unsigned char * nalus = NULL;
    char find = 0;

    int i = 0;
    for(; i < datan; i++) {
        unsigned char * p = data + i;
        if(datan - i > 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
            find = 1;
            i += 3;
        }
        if(datan - i > 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
            find = 1;
            i += 4;
        }

        if(find > 0) {
            if(!nalus) {
                nalus = data + i;
            } else {
                ezrtsp_video_sequence_parament_set_proc(ch, nalus, p - nalus, 0);
                nalus = data + i;
            }
            find = 0;
        }
    }

    if(nalus) {
        ezrtsp_video_sequence_parament_set_proc(ch, nalus, data + datan - nalus, 1);
    }
    return 0;
}

int ezrtsp_start(ezrtsp_ctx_t * ctx)
{
    memcpy(&g_ctx, ctx, sizeof(ezrtsp_ctx_t));
    ezrtsp_serv_start();
    return 0;
}

int ezrtsp_stop()
{
    return ezrtsp_serv_stop();
}