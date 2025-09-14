#include "ezrtsp.h"
#include "ezcache.h"
#include "ezrtsp_utils.h"

static char faudio_cfg = 0;
static unsigned char audio_adts_hdr[4] = {0};

typedef struct {
    int cfg;
    ezrtsp_data_t * vps;
    ezrtsp_data_t * sps;
    ezrtsp_data_t * pps; 
} video_cfg_t;
static video_cfg_t video_cfg[2] = {0};
static ezrtsp_ctx_t g_ctx = {0};

int ezrtsp_audio_codec_typ(void) { return g_ctx.atype; }
int ezrtsp_video_codec_typ(void) { return g_ctx.vtype; }

void ezrtsp_video_sequence_parament_set_clr(void) {
    int i = 0;
    for (i = 0; i < 2; i++) {
        if (video_cfg[i].vps) 
            ez_free(video_cfg[i].vps);
        if (video_cfg[i].sps) 
            ez_free(video_cfg[i].sps);
        if (video_cfg[i].pps) 
            ez_free(video_cfg[i].pps);
    }
}

int ezrtsp_video_sequence_parament_set_get(int ch, ezrtsp_data_t **vps, ezrtsp_data_t **sps, ezrtsp_data_t **pps) {

    if (vps) {
        if (video_cfg[ch].vps)
            *vps = video_cfg[ch].vps;
        else
            *vps = NULL;
    }

    if (sps) {
        if (video_cfg[ch].sps)
            *sps = video_cfg[ch].sps;
        else
            *sps = NULL;
    }

    if (pps) {
        if (video_cfg[ch].vps)
            *pps = video_cfg[ch].pps;
        else
            *pps = NULL;
    }
    return 0;
}

static int ezrtsp_video_sequence_parament_set_save(unsigned char *data, int datan, ezrtsp_data_t **sys) {
    ezrtsp_data_t *t = ez_alloc(sizeof(ezrtsp_data_t) + datan);
    if (!t) {
        err("sys data alloc err\n");
        return -1;
    }
    t->datan = datan;
    memcpy(t->data, data, datan);
    *sys = t;
    return 0;
}

static int ezrtsp_video_sequence_parament_set_proc(int ch, unsigned char *nalu, int nalun) {
    if (g_ctx.vtype == VT_H264) {
        int nalu_type = nalu[0] & 0b00011111;
        if (nalu_type == 7) {
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
        if (video_cfg[ch].sps && video_cfg[ch].pps) {
            video_cfg[ch].cfg = 1;
        }
    } else {
        int nalu_type = (nalu[0] & 0b01111110) >> 1;
        if (nalu_type == 32) {  ///vps
            if (!video_cfg[ch].vps) {
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
        if (video_cfg[ch].vps && video_cfg[ch].sps && video_cfg[ch].pps) {
            video_cfg[ch].cfg = 1;
        }
    }
    return 0;
}

int ezrtsp_video_sequence_parament_set_ready(int ch) {
    if(video_cfg[ch].cfg) return 1;
    return 0;
}

int ezrtsp_audio_enb(void) { return g_ctx.aenb; }
int ezrtsp_audio_aacadts_ready(void) { return faudio_cfg; }

static int ezrtsp_audio_aacadts_proc(unsigned char *data, int datan) {
    if(!faudio_cfg) {
        memcpy(audio_adts_hdr, data, datan < 4 ? datan : 4);
        faudio_cfg = 1;
    }
    return 0;
}
unsigned char * ezrtsp_audio_aacadts_get(void) { return audio_adts_hdr; }

int ezrtsp_push_afrm(unsigned char *data, int datan) {
    if(!ezrtsp_audio_aacadts_ready()) {
        ezrtsp_audio_aacadts_proc(data, datan);
        faudio_cfg = 1;
    }
    unsigned long long ts_now = ezrtsp_ts_msec();
    ezcache_frm_add(0, data, datan, 0, ts_now);  ///push audio data into channel 0
    ezcache_frm_add(1, data, datan, 0, ts_now);  ///push audio data into channel 1
    return 0;
}

int ezrtsp_push_vfrm(int ch, unsigned char *data, int datan, int typ) {

    unsigned char * nalus = NULL;
    char find = 0;

    int i = 0;
    for (; i < datan; i++) {
        unsigned char * p = data + i;
        if (datan - i > 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
            find = 1;
            i += 3;
        }
        if (datan - i > 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
            find = 1;
            i += 4;
        }

        if (find > 0) {
            if (!nalus) {
                nalus = data + i;
            } else {
                ezrtsp_video_sequence_parament_set_proc(ch, nalus, p - nalus);
                nalus = data + i;
            }
            find = 0;
        }
    }

    if (nalus) {
        ezrtsp_video_sequence_parament_set_proc(ch, nalus, data + datan - nalus);
    }

    ezcache_frm_add(ch, data, datan, typ, ezrtsp_ts_msec());
    return 0;
}

int ezrtsp_start(ezrtsp_ctx_t *ctx) {
    memcpy(&g_ctx, ctx, sizeof(ezrtsp_ctx_t));
    ezrtsp_serv_start();
    return 0;
}

int ezrtsp_stop(void) {
    ezrtsp_serv_stop();
    ezrtsp_video_sequence_parament_set_clr();
    ezcache_exit(0);
    ezcache_exit(1);
    return 0;
}