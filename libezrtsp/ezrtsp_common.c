#include "ezrtsp_common.h"

#if defined(FAC)
void *__stack_chk_guard = (void *)0xdeadbeef;
#endif

static const char b64enc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/* This assumes that an unsigned char is exactly 8 bits. Not portable code! :-) */
static const char b64dec[128] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 62, 0xff, 0xff, 0xff, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0xff, 0xff, 0xff, 0xff, 0xff};
#define char64(c) ((c > 127) ? 0xff : b64dec[(c)])

static pthread_mutex_t g_alloc_lock = PTHREAD_MUTEX_INITIALIZER;

int ezrtsp_base64_encode(const char *in, int inlen, char *out, int outlen)
{
	int pad = 0;
	int i = 0, j = 0;
	const unsigned char *data = (const unsigned char *)in;

	if ((data == NULL) || (out == NULL))
	{
		return -1;
	}

	if (outlen < (inlen * 4 / 3))
	{
		return -1;
	}

	while (i < inlen)
	{
		pad = 3 - (inlen - i);
		if (pad == 2)
		{
			out[j] = b64enc[data[i] >> 2];
			out[j + 1] = b64enc[(data[i] & 0x03) << 4];
			out[j + 2] = '=';
			out[j + 3] = '=';
		}
		else if (pad == 1)
		{
			out[j] = b64enc[data[i] >> 2];
			out[j + 1] = b64enc[((data[i] & 0x03) << 4) | ((data[i + 1] & 0xf0) >> 4)];
			out[j + 2] = b64enc[(data[i + 1] & 0x0f) << 2];
			out[j + 3] = '=';
		}
		else
		{
			out[j] = b64enc[data[i] >> 2];
			out[j + 1] = b64enc[((data[i] & 0x03) << 4) | ((data[i + 1] & 0xf0) >> 4)];
			out[j + 2] = b64enc[((data[i + 1] & 0x0f) << 2) | ((data[i + 2] & 0xc0) >> 6)];
			out[j + 3] = b64enc[data[i + 2] & 0x3f];
		}
		i += 3;
		j += 4;
	}
	out[j] = '\0';
	return j;
}

int ezrtsp_base64_decode(const char *in, int inlen, char *out, int outlen)
{
	int i = 0, j = 0, pad = 0;
	unsigned char c[4] = {0};
	if ((in == NULL) || (out == NULL))
	{
		return -1;
	}

	if ((outlen < (inlen * 3 / 4)) || ((inlen % 4) != 0))
	{
		return -1;
	}

	while ((i + 3) < inlen)
	{
		pad = 0;
		c[0] = char64((unsigned char)in[i]);
		pad += (c[0] == 0xff);
		c[1] = char64((unsigned char)in[i + 1]);
		pad += (c[1] == 0xff);
		c[2] = char64((unsigned char)in[i + 2]);
		pad += (c[2] == 0xff);
		c[3] = char64((unsigned char)in[i + 3]);
		pad += (c[3] == 0xff);
		if (pad == 2)
		{
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j] = (c[1] & 0x0f) << 4;
		}
		else if (pad == 1)
		{
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
			out[j] = (c[2] & 0x03) << 6;
		}
		else
		{
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
			out[j++] = ((c[2] & 0x03) << 6) | (c[3] & 0x3f);
		}
		i += 4;
	}
	out[j] = '\0';
	return j;
}


void ezrtsp_queue_init(ezrtsp_queue_t * q)
{
	q->prev = q;
	q->next = q;
}

void ezrtsp_queue_insert(ezrtsp_queue_t * h, ezrtsp_queue_t * q)
{
	q->next = h->next;
	q->prev = h;

	h->next->prev = q;
	h->next = q;
}

void ezrtsp_queue_insert_tail(ezrtsp_queue_t * h, ezrtsp_queue_t * q)
{
	ezrtsp_queue_t * last;

	last = h->prev;

	q->next = last->next;
	q->prev = last;

	last->next->prev = q;
	last->next = q;
}

void ezrtsp_queue_remove(ezrtsp_queue_t * q)
{
	if( q->prev ) {
		q->prev->next = q->next;
	}
	if( q->next ) {
		q->next->prev = q->prev;
	}
	q->prev = NULL;
	q->next = NULL;
}

int ezrtsp_queue_empty(ezrtsp_queue_t * h)
{
	return ( (h == h->prev) ? 1 : 0 );
}

ezrtsp_queue_t * ezrtsp_queue_head(ezrtsp_queue_t * h)
{
	return h->next;
}

ezrtsp_queue_t * ezrtsp_queue_next(ezrtsp_queue_t * q)
{
	return q->next;
}

ezrtsp_queue_t * ezrtsp_queue_prev(ezrtsp_queue_t * q)
{
	return q->prev;
}

ezrtsp_queue_t * ezrtsp_queue_tail(ezrtsp_queue_t * h)
{
	return h;
}



void ezrtsp_free(void * addr)
{
    assert(addr);
    pthread_mutex_lock(&g_alloc_lock);
    free(addr);
    pthread_mutex_unlock(&g_alloc_lock);
}

void * ezrtsp_alloc( int size )
{
    assert( size > 0 );
    pthread_mutex_lock(&g_alloc_lock); /// lock for thread safe 
    void * addr = malloc( size );
    pthread_mutex_unlock(&g_alloc_lock);
    if( !addr ) {
        err("alloc mem failed. [%d]\n", errno );
        return NULL;
    }
    memset( addr, 0x0, size );
    return addr;
}

/// @brief absolutely msecond number since device startup
/// @param
/// @return
unsigned long long ezrtsp_ts_msec(void)
{
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return ((time.tv_sec * 1000) + (time.tv_nsec/1000000));
}

int ezrtsp_meta_alloc(ezrtsp_meta_t ** meta, int size)
{
	ezrtsp_meta_t * nmeta = ezrtsp_alloc(sizeof(ezrtsp_meta_t) + size);
	if(!nmeta) {
		err("meta alloc err\n");
		return -1;
	}
	nmeta->start = nmeta->pos = nmeta->last = nmeta->data;
	nmeta->end = nmeta->start + size;
	nmeta->next = NULL;
	*meta = nmeta;
	return 0;
}

void ezrtsp_meta_free(ezrtsp_meta_t * meta)
{
	if(meta) {
		ezrtsp_free(meta);
	}
	return;
}

