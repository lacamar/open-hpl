#ifndef OHPL_CSHIM_SHIMS_H
#define OHPL_CSHIM_SHIMS_H

#include <stddef.h>
#include <stdint.h>

/* Structs shared with rust/src/backend/media (oal.rs RawInit, video.rs RawYuv). */

typedef struct OhplOalInit {
	const char* device_name;
	int32_t output_freq;
	int32_t update_freq;
	int32_t num_sources_hint;
	int32_t min_mono_hint;
	int32_t min_stereo_hint;
	int32_t stream_buffer_size;
	int32_t stream_buffer_count;
	uint8_t use_thread, voice_management, use_efx;
} OhplOalInit;

typedef struct OhplThYuv {
	const unsigned char *y, *u, *v;
	size_t y_len, u_len, v_len;
	int y_stride, uv_stride;
} OhplThYuv;

#endif
