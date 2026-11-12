#ifndef HPL_RUST_L5_H
#define HPL_RUST_L5_H

// C ABI of rust/src/backend/media (stage L5): OpenAL classes, DevIL loaders, BMFont reader and
// Theora stream. Used when OHPL_IMPL=rust.

#include <cstddef>
#include <cstdint>

#include "impl/RustBackends.h"
#include "system/LowLevelSystem.h"

struct OhplL5Data;
struct OhplL5Channel;
struct OhplL5Low;
struct OhplL5Bitmap;
struct OhplL5Font;
struct OhplL5VLoader;
struct OhplL5Video;

// Rust writes these as the engine's Log/Warning/Error/FatalError lines.
enum { kOhplL5LogLog = 0, kOhplL5LogWarning = 1, kOhplL5LogError = 2, kOhplL5LogFatal = 3 };

struct OhplL5EnvParams {
	float density, diffusion, gain, gain_hf, gain_lf, decay_time, decay_hf_ratio, decay_lf_ratio;
	float reflections_gain, reflections_delay, reflections_pan[3];
	float late_reverb_gain, late_reverb_delay, late_reverb_pan[3];
	float echo_time, echo_depth, modulation_time, modulation_depth, air_absorption_gain_hf;
	float hf_reference, lf_reference, room_rolloff_factor;
	int32_t decay_hf_limit;
};

struct OhplL5Device {
	int32_t id;
	const uint8_t* name;
	size_t name_len;
	bool is_default;
};

struct OhplL5InitArgs {
	int32_t device_id;
	bool use_env_audio;
	int32_t max_channels;
	int32_t stream_update_freq;
	bool use_threading;
	bool use_voice_management;
	int32_t max_mono_hint;
	int32_t max_stereo_hint;
	int32_t stream_buffer_size;
	int32_t stream_buffer_count;
	bool enable_log;
};

struct OhplL5InitResult {
	bool initialized;
	int32_t device_id;
};

struct OhplL5BitmapInfo {
	int32_t width, height, depth, bytes_per_pixel;
	uint32_t format;
	bool compressed;
	int32_t images, mips;
	bool setup_data;
};

struct OhplL5FontInfo {
	int32_t line_height, base;
	float height, size_ratio_x;
	int32_t largest_id;
	size_t glyph_count, pages, chars;
};

struct OhplL5FontChar {
	int32_t id, x, y, width, height, x_offset, y_offset, advance, page;
	int32_t status; // 0 usable, 1 invalid id, 2 invalid page
};

static_assert(sizeof(OhplL5EnvParams) == 108 && sizeof(OhplL5Device) == 32 && sizeof(OhplL5InitArgs) == 40 &&
	sizeof(OhplL5InitResult) == 8 && sizeof(OhplL5BitmapInfo) == 36 && sizeof(OhplL5FontInfo) == 48 &&
	sizeof(OhplL5FontChar) == 40, "L5 layouts");
static_assert(offsetof(OhplL5BitmapInfo, compressed) == 20 && offsetof(OhplL5BitmapInfo, setup_data) == 32 &&
	offsetof(OhplL5InitArgs, enable_log) == 36 && offsetof(OhplL5FontInfo, glyph_count) == 24, "L5 offsets");

extern "C" {

void ohpl_l5_set_log(void (*fn)(int level, const char* msg));

OhplL5Data* ohpl_l5_data_new(bool stream, bool loop_stream);
bool ohpl_l5_data_create(OhplL5Data*, const uint32_t* path, size_t len);
int ohpl_l5_data_channel(OhplL5Data*, int priority);
bool ohpl_l5_data_is_stereo(OhplL5Data*);
void ohpl_l5_data_free(OhplL5Data*);

OhplL5Channel* ohpl_l5_chan_new(int handle);
void ohpl_l5_chan_free(OhplL5Channel*);
void ohpl_l5_chan_stop(OhplL5Channel*);
void ohpl_l5_chan_set_paused(OhplL5Channel*, bool);
void ohpl_l5_chan_set_pitch(OhplL5Channel*, float);
void ohpl_l5_chan_set_volume(OhplL5Channel*, float);
void ohpl_l5_chan_set_loop(OhplL5Channel*, bool);
void ohpl_l5_chan_set_spatialize(OhplL5Channel*, bool);
void ohpl_l5_chan_set_elapsed(OhplL5Channel*, double);
void ohpl_l5_chan_set_priority(OhplL5Channel*, int x, int modifier);
void ohpl_l5_chan_set_aux_slot(OhplL5Channel*, bool affected);
void ohpl_l5_chan_set_filtering(OhplL5Channel*, bool on, int flags);
void ohpl_l5_chan_set_filter_gain(OhplL5Channel*, float);
void ohpl_l5_chan_set_filter_gain_hf(OhplL5Channel*, float);
void ohpl_l5_chan_set_filter_gain_lf(OhplL5Channel*, float);
void ohpl_l5_chan_set_env_send_gain(OhplL5Channel*, float);
void ohpl_l5_chan_set_attributes(OhplL5Channel*, const float* pos, const float* vel);
bool ohpl_l5_chan_is_playing(OhplL5Channel*);
bool ohpl_l5_chan_is_underrun(OhplL5Channel*);
double ohpl_l5_chan_elapsed(OhplL5Channel*);
double ohpl_l5_chan_total(OhplL5Channel*);
int ohpl_l5_chan_priority(OhplL5Channel*);

OhplL5Low* ohpl_l5_low_new();
void ohpl_l5_low_free(OhplL5Low*);
void ohpl_l5_low_update(OhplL5Low*);
void ohpl_l5_low_set_listener(OhplL5Low*, const float* pos, const float* vel, const float* fwd, const float* up);
void ohpl_l5_low_set_rolloff(OhplL5Low*, float);
void ohpl_l5_low_set_volume(OhplL5Low*, float);
bool ohpl_l5_low_set_env_volume(OhplL5Low*, float v, float* out);
bool ohpl_l5_low_env_enabled(OhplL5Low*);
void ohpl_l5_devices(void (*sink)(void*, int32_t id, const uint8_t* name, size_t len, bool is_default), void* ctx);
int32_t ohpl_l5_first_default(const OhplL5Device* all, size_t n_all, const OhplL5Device* valid, size_t n_valid);
void ohpl_l5_low_prelude(bool enable_log);
OhplL5InitResult ohpl_l5_low_init(OhplL5Low*, const OhplL5InitArgs*, const OhplL5Device* all, size_t n_all,
								  const OhplL5Device* valid, size_t n_valid, const char* env_device);
void ohpl_l5_low_set_env(OhplL5Low*, const OhplL5EnvParams* env);
void ohpl_l5_low_fade_env(OhplL5Low*, const OhplL5EnvParams* src, const OhplL5EnvParams* dst, float t);
bool ohpl_l5_env_load(const char* path, OhplL5EnvParams* out, void (*name_sink)(void*, const uint8_t*, size_t), void* ctx);

OhplL5Bitmap* ohpl_l5_bitmap_load(const uint32_t* path, size_t len, uint32_t kind, bool no_compression);
void ohpl_l5_bitmap_info(const OhplL5Bitmap*, OhplL5BitmapInfo* out);
const uint8_t* ohpl_l5_bitmap_plane(const OhplL5Bitmap*, int image, int mip, int* size);
void ohpl_l5_bitmap_free(OhplL5Bitmap*);
bool ohpl_l5_bitmap_save(const uint32_t* path, size_t len, uint32_t width, uint32_t height, uint32_t depth,
						 uint8_t bytes_per_pixel, uint32_t format, const uint8_t* data);

OhplL5Font* ohpl_l5_font_load(const char* path, int32_t* status, int32_t* largest_id);
void ohpl_l5_font_info(const OhplL5Font*, OhplL5FontInfo* out);
bool ohpl_l5_font_page(const OhplL5Font*, size_t i, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
bool ohpl_l5_font_char(const OhplL5Font*, size_t i, OhplL5FontChar* out);
void ohpl_l5_font_free(OhplL5Font*);

OhplL5VLoader* ohpl_l5_vloader_new();
void ohpl_l5_vloader_free(OhplL5VLoader*);
OhplL5Video* ohpl_l5_video_new(const OhplL5VLoader*);
bool ohpl_l5_video_load(OhplL5Video*, const uint32_t* path, size_t len);
void ohpl_l5_video_update(OhplL5Video*, float dt);
void ohpl_l5_video_play(OhplL5Video*);
void ohpl_l5_video_stop(OhplL5Video*);
void ohpl_l5_video_pause(OhplL5Video*, bool);
void ohpl_l5_video_set_loop(OhplL5Video*, bool);
void ohpl_l5_video_size(const OhplL5Video*, int32_t* w, int32_t* h);
const uint8_t* ohpl_l5_video_take_frame(OhplL5Video*);
uint32_t ohpl_l5_video_flags(const OhplL5Video*);
void ohpl_l5_video_free(OhplL5Video*);

bool ohpl_l5_audio_hash(const char* path, uint64_t* out);
bool ohpl_l5_wav_hash(const uint8_t* data, size_t len, uint64_t* out);

}

namespace hpl {

inline void OhplL5LogLine(int level, const char* msg)
{
	switch (level)
	{
	case kOhplL5LogWarning: Warning("%s", msg); break;
	case kOhplL5LogError: Error("%s", msg); break;
	case kOhplL5LogFatal: FatalError("%s", msg);
	default: Log("%s", msg);
	}
}

inline void OhplL5InstallLog()
{
	ohpl_l5_set_log(OhplL5LogLine);
}

}

#endif
