#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vorbis/vorbisfile.h>

typedef struct OhplVorbis {
	OggVorbis_File vf;
	const unsigned char* mem;
	size_t mem_len, mem_pos;
} OhplVorbis;

static size_t mem_read(void* dst, size_t size, size_t count, void* src)
{
	OhplVorbis* v = (OhplVorbis*)src;
	size_t left = v->mem_len - v->mem_pos, want = size * count;
	size_t n = want < left ? want : left;
	memcpy(dst, v->mem + v->mem_pos, n);
	v->mem_pos += n;
	return size ? n / size : 0;
}

/* Unseekable in-memory stream; `data` must outlive the handle. */
OhplVorbis* ohpl_vorbis_open_mem(const unsigned char* data, size_t len)
{
	OhplVorbis* v = (OhplVorbis*)calloc(1, sizeof(OhplVorbis));
	if (!v) return NULL;
	v->mem = data;
	v->mem_len = len;
	ov_callbacks cb = {mem_read, NULL, NULL, NULL};
	if (ov_open_callbacks(v, &v->vf, NULL, 0, cb) != 0)
	{
		free(v);
		return NULL;
	}
	return v;
}

long ohpl_vorbis_read_float(OhplVorbis* v, float*** planes, int frames)
{
	int section;
	return ov_read_float(&v->vf, planes, frames, &section);
}

void ohpl_vorbis_clear_mem(OhplVorbis* v)
{
	ov_clear(&v->vf);
	free(v);
}

OhplVorbis* ohpl_vorbis_open(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return NULL;
	OhplVorbis* v = (OhplVorbis*)calloc(1, sizeof(OhplVorbis));
	if (!v || ov_open_callbacks(f, &v->vf, NULL, 0, OV_CALLBACKS_DEFAULT) < 0)
	{
		free(v);
		fclose(f);
		return NULL;
	}
	return v;
}

void ohpl_vorbis_info(OhplVorbis* v, int* channels, long* rate, long* total_samples)
{
	vorbis_info* i = ov_info(&v->vf, -1);
	*channels = i->channels;
	*rate = i->rate;
	*total_samples = (long)ov_pcm_total(&v->vf, -1);
}

/* 16-bit little endian signed PCM; 0 at end, negative on error */
long ohpl_vorbis_next(OhplVorbis* v, char* buf, int len)
{
	int section;
	return ov_read(&v->vf, buf, len, 0, 2, 1, &section);
}

void ohpl_vorbis_close(OhplVorbis* v)
{
	ov_clear(&v->vf);
	free(v);
}
