#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogg/ogg.h>
#include <theora/theora.h>

#include "shims.h"

typedef struct OhplTheora {
	ogg_sync_state sync;
	ogg_stream_state stream;
	ogg_stream_state test;
	theora_info info;
	theora_comment comment;
	theora_state state;
	ogg_page page;
	ogg_packet packet;
	ogg_packet test_packet;
} OhplTheora;


OhplTheora* ohpl_th_new(void)
{
	OhplTheora* t = (OhplTheora*)calloc(1, sizeof(OhplTheora));
	if (!t) return NULL;
	theora_comment_init(&t->comment);
	theora_info_init(&t->info);
	ogg_sync_init(&t->sync);
	return t;
}

void ohpl_th_free(OhplTheora* t, int video_loaded)
{
	ogg_sync_clear(&t->sync);
	theora_comment_clear(&t->comment);
	theora_info_clear(&t->info);
	ogg_stream_clear(&t->stream);
	if (video_loaded) theora_clear(&t->state);
	free(t);
}

int ohpl_th_pageout(OhplTheora* t) { return ogg_sync_pageout(&t->sync, &t->page); }
int ohpl_th_page_bos(OhplTheora* t) { return ogg_page_bos(&t->page); }
int ohpl_th_queue_page(OhplTheora* t) { return ogg_stream_pagein(&t->stream, &t->page); }
int ohpl_th_packetout(OhplTheora* t) { return ogg_stream_packetout(&t->stream, &t->packet); }
int ohpl_th_decode_header_packet(OhplTheora* t) { return theora_decode_header(&t->info, &t->comment, &t->packet); }

/* -1: a page was still buffered and nothing was read */
int ohpl_th_fill(OhplTheora* t, FILE* f, int size)
{
	ogg_page temp;
	if (ogg_sync_pageout(&t->sync, &temp) == 1) return -1;
	char* buf = ogg_sync_buffer(&t->sync, size);
	if (!buf) return 0;
	int n = (int)fread(buf, 1, size, f);
	ogg_sync_wrote(&t->sync, n);
	return n;
}

void ohpl_th_test_begin(OhplTheora* t)
{
	ogg_stream_init(&t->test, ogg_page_serialno(&t->page));
	ogg_stream_pagein(&t->test, &t->page);
	ogg_stream_packetout(&t->test, &t->test_packet);
}
int ohpl_th_test_theora_header(OhplTheora* t) { return theora_decode_header(&t->info, &t->comment, &t->test_packet); }
int ohpl_th_test_header_bit(OhplTheora* t) { return t->test_packet.packet[0] & 0x80; }
void ohpl_th_test_adopt(OhplTheora* t) { memcpy(&t->stream, &t->test, sizeof(t->test)); }
void ohpl_th_test_clear(OhplTheora* t) { ogg_stream_clear(&t->test); }

/* 0: decoded, time written */
int ohpl_th_decode_packet(OhplTheora* t, double* time)
{
	if (theora_decode_packetin(&t->state, &t->packet) != 0) return -1;
	*time = theora_granule_time(&t->state, t->state.granulepos);
	return 0;
}

void ohpl_th_decode_init(OhplTheora* t) { theora_decode_init(&t->state, &t->info); }
void ohpl_th_state_clear(OhplTheora* t) { theora_clear(&t->state); }
void ohpl_th_info_clear(OhplTheora* t) { theora_info_clear(&t->info); }
void ohpl_th_comment_clear(OhplTheora* t) { theora_comment_clear(&t->comment); }
void ohpl_th_stream_clear(OhplTheora* t) { ogg_stream_clear(&t->stream); }
void ohpl_th_sync_reset(OhplTheora* t) { ogg_sync_reset(&t->sync); }
void ohpl_th_drain_packets(OhplTheora* t) { while (ogg_stream_packetout(&t->stream, &t->packet) > 0); }
void ohpl_th_drain_pages(OhplTheora* t) { while (ogg_sync_pageout(&t->sync, &t->page)); }

void ohpl_th_dims(OhplTheora* t, int* w, int* h, unsigned* ox, unsigned* oy)
{
	*w = (int)t->info.frame_width;
	*h = (int)t->info.frame_height;
	*ox = (unsigned)t->info.offset_x;
	*oy = (unsigned)t->info.offset_y;
}

void ohpl_th_yuv(OhplTheora* t, OhplThYuv* out)
{
	yuv_buffer b;
	theora_decode_YUVout(&t->state, &b);
	out->y = b.y;
	out->u = b.u;
	out->v = b.v;
	out->y_len = (size_t)b.y_stride * b.y_height;
	out->u_len = (size_t)b.uv_stride * b.uv_height;
	out->v_len = out->u_len;
	out->y_stride = b.y_stride;
	out->uv_stride = b.uv_stride;
}
