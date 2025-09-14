#ifndef __EZRTSP_UTILS_H__
#define __EZRTSP_UTILS_H__

#ifndef AT_G711A 
#define AT_G711A 1
#endif
#ifndef AT_AAC
#define AT_AAC 3
#endif

#ifndef VT_H264
#define VT_H264 0
#endif
#ifndef VT_H265
#define VT_H265 1
#endif


#define EZRTSP_CON_MAX	2
#define EZRTSP_PORT     554
#define EZRTSP_MSS      1430

#define EZRTSP_URI_MAIN      "/main_ch"
#define EZRTSP_URI_SUB	     "/sub_ch"

typedef struct {
    int vtype;  ///VT_H264/VT_H265
    int aenb;
    int atype;  ///AT_AAC/AT_G711A
} ezrtsp_ctx_t;

int ezrtsp_start(ezrtsp_ctx_t * ctx);
int ezrtsp_stop(void);
int ezrtsp_push_afrm(unsigned char * data, int datan);
int ezrtsp_push_vfrm(int ch, unsigned char *data, int datan, int typ);   /// 0:audio  1:iframe  2:pframe or bframe 

#endif