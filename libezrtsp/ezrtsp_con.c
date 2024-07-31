#include "ezrtsp.h"
#include "ezrtsp_utils.h"

static rtsp_con_t cons[EZRTSP_CON_MAX] = {0};
static pthread_mutex_t con_lock = PTHREAD_MUTEX_INITIALIZER;

int ezrtsp_con_init()
{
	int i = 0;
	for(i = 0; i < EZRTSP_CON_MAX; i++) {
		cons[i].rtp_port_video = 30000 + (2*i);
		cons[i].rtp_port_audio = 30000 + 1 + (2*i);
		
		cons[i].rtcp_port_video = 30100 + (2*i);
		cons[i].rtcp_port_audio = 30100 + 1 + (2*i);
	}
	return 0;
}

int ezrtsp_con_alloc(rtsp_con_t ** out)
{
	int i = 0;
	pthread_mutex_lock(&con_lock);
	for(i = 0; i < EZRTSP_CON_MAX; i++) {
		rtsp_con_t * c = &cons[i];
		if(c->fuse) continue;
		
		if(!c->meta) {
			if(0 != ezrtsp_meta_alloc(&c->meta, 4096)) {
				err("meta alloc\n");
				pthread_mutex_unlock(&con_lock);
				return -1;
			}
		}
		c->fd = 0;
		c->ctx = NULL;
		meta_clr(c->meta);
		memset(c->client_ip, 0x0, sizeof(c->client_ip));
		c->state = 0;
		c->method = NULL;
		c->methodn = 0;
		c->url = NULL;
		c->urln = 0;
		
		c->fuse = 1;
		c->fplay = 0;
		c->fcomplete = 0;
		c->fovertcp = 0;
		c->faudioenb = 0;
		c->fvideoenb = 0;
		
		c->ichseq = -1;
		c->ichn = 0;
		c->icseq = 0;
		c->imethod = 0;
		c->itrack = 0;
		c->irtp_port = 0;
		c->irtcp_port = 0;
		
		memset(&c->session_audio, 0x0, sizeof(c->session_audio));
		memset(&c->session_video, 0x0, sizeof(c->session_video));
		memset(c->session_str, 0x0, sizeof(c->session_str));
		
		c->rtp_stat = 0;
		c->rtp_pid = 0;
		*out = c;
		pthread_mutex_unlock(&con_lock);
		return 0;
	}
	pthread_mutex_unlock(&con_lock);
	return -1;
}

void ezrtsp_con_free(rtsp_con_t * c)
{
	pthread_mutex_lock(&con_lock);
    if(c) {
		if(c->rtp_stat & EZRTP_INIT) {
			c->rtp_stat &= ~EZRTP_INIT;
			pthread_join(c->rtp_pid, NULL);
		}
		if(c->fd) {
			ev_timer_del(c->ctx, c->fd);
			ev_opt(c->ctx, c->fd, NULL, NULL, EV_NONE);
			close(c->fd);
			c->fd = 0;
		}
		if(c->meta) {
			meta_clr(c->meta);
		}

		if(c->session_video.fd_rtp) close(c->session_video.fd_rtp);
        if(c->session_video.fd_rtcp) close(c->session_video.fd_rtcp);

        if(c->session_audio.fd_rtp) close(c->session_audio.fd_rtp);
        if(c->session_audio.fd_rtcp) close(c->session_audio.fd_rtcp);

		c->ctx = NULL;
		c->state = 0;
		c->method = NULL;
		c->methodn = 0;
		c->url = NULL;
		c->urln = 0;
		
		c->fuse = 0;
		c->fplay = 0;
		c->fcomplete = 0;
		c->fovertcp = 0;
		c->faudioenb = 0;
		c->fvideoenb = 0;
		
		c->ichseq = -1;
		c->ichn = 0;
		c->icseq = 0;
		c->imethod = 0;
		c->itrack = 0;
		c->irtp_port = 0;
		c->irtcp_port = 0;
		
		memset(&c->session_audio, 0x0, sizeof(c->session_audio));
		memset(&c->session_video, 0x0, sizeof(c->session_video));
		memset(c->session_str, 0x0, sizeof(c->session_str));
		
		c->rtp_stat = 0;
		c->rtp_pid = 0;
	}
	pthread_mutex_unlock(&con_lock);
}

void ezrtsp_con_expire(ev_ctx_t * ctx, int fd, void * user_data)
{
    rtsp_con_t * c = (rtsp_con_t*)user_data;
	ezrtsp_con_free(c);
}