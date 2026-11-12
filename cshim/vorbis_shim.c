#include <stdio.h>
#include <stdlib.h>

#include <vorbis/vorbisfile.h>

typedef struct OhplVorbis {
	OggVorbis_File vf;
} OhplVorbis;

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
