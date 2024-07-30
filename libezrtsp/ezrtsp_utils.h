#ifndef __EZRTSP_UTILS_H__
#define __EZRTSP_UTILS_H__

#include "common.h"

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
int ezrtsp_stop();
int ezrtsp_push_afrm(char * data, int datan);
int ezrtsp_push_vfrm(int ch, char * data, int datan);

#endif