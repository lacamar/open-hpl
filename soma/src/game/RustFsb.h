#ifndef SOMA_RUST_FSB_H
#define SOMA_RUST_FSB_H

// C ABI of rust/src/fsb5_ffi.rs. Inputs are (pointer, length); outputs go through the sink.

#include <cstddef>

struct OhplFsbSample
{
	const unsigned char *name;
	size_t name_len;
	unsigned int frequency;
	int channels;
	size_t data_offset;
	size_t data_size;
	unsigned int num_pcm_samples;
	bool has_vorbis_crc;
	unsigned int vorbis_crc;
};

extern "C" {
	bool ohpl_fsb_parse(const unsigned char *file, size_t n, unsigned int *mode, void (*sink)(void *, const OhplFsbSample *), void *ctx);
	bool ohpl_fsb_encode_pcm(unsigned int mode, const OhplFsbSample *sample, const unsigned char *file, size_t n,
							 void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	bool ohpl_fsb_mux_vorbis(const OhplFsbSample *sample, const unsigned char *file, size_t n, const unsigned char *setup, size_t setup_len,
							 void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_fsb_wav_pcm16(const OhplFsbSample *sample, const unsigned char *pcm, size_t n, void (*sink)(void *, const unsigned char *, size_t),
							void *ctx);
	void ohpl_fsb_downmix_matrix(size_t channels, float *left, float *right);
}

#endif
