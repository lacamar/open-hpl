#ifndef RUST_SOMA_LUX_VOICE_H
#define RUST_SOMA_LUX_VOICE_H

#include "RustSomaSound.h"

#include <stddef.h>
#include <stdint.h>

// C ABI of rust/src/somasound/voice_handler.rs.
struct OhplS5bVoiceHost
{
	bool (*entry_valid)(size_t, int);
	void (*entry_set_volume_mul)(size_t, float);
	void (*entry_fade_out)(size_t, float);
	void (*entry_set_paused)(size_t, bool);
	void (*channel_set_position)(size_t, const float *);
	void (*channel_filter)(size_t, bool high, float);
	double (*channel_total)(size_t);
	double (*channel_elapsed)(size_t);
	void (*listener_pos)(float *);
	bool (*map_entity_pos)(const unsigned char *, size_t, float *);
	bool (*map_current)();
	bool (*file_exists)(const unsigned char *, size_t);
	bool (*read_file)(const unsigned char *, size_t, void (*)(void *, const unsigned char *, size_t), void *);
	bool (*play_3d)(const unsigned char *, size_t, float vol, const float *pos, float min, float max, int type, size_t *entry, int *id);
	bool (*play_gui_stream)(const unsigned char *, size_t, float vol, int type, size_t *entry, int *id);
	bool (*effect_play)(const unsigned char *, size_t, float vol, int type, size_t *effect, int *id);
	bool (*effect_live)(size_t, int);
	void (*effect_set_volume_mul)(size_t, float);
	void (*effect_fade_out)(size_t, float);
	void (*effect_set_paused)(size_t, bool);
	bool (*effect_is_playing)(size_t);
	float (*effect_total)(size_t);
	float (*effect_elapsed)(size_t);
	void (*error)(const unsigned char *, size_t);
	void (*wrap_rows)(const unsigned char *, size_t, float width, float line_height, const float *size, void (*)(void *, const unsigned int *, size_t), void *);
	void (*draw_fade)(float);
	bool (*subtitles_on)();
	void (*begin_subtitles)(float *screen);
	void (*draw_font)(const unsigned int *, size_t, const float *pos, const float *size, const float *color);
	bool (*game_paused)();
	bool (*lip_apply)(const unsigned char *, size_t, const float *weights);
	int (*map_call)(const unsigned char *, size_t, const OhplS7CallArg *, size_t, int);
	unsigned (*arr_size)(const void *);
	void *(*arr_at)(const void *, unsigned);
	void (*arr_resize)(void *, unsigned);
	const void *(*str_ref)(const unsigned char *, size_t);
};

int OhplMapScriptCall(const unsigned char *d, size_t dn, const OhplS7CallArg *a, size_t n, bool abRet, int lRetDefault);

extern "C" {
	void ohpl_s5b_voice_install_host(const OhplS5bVoiceHost *);
	void ohpl_s5b_voice_create();
	void ohpl_s5b_voice_destroy();
	bool ohpl_s5b_voice_present();
	void ohpl_s5b_voice_update(float dt, bool game_paused);
	void ohpl_s5b_voice_draw();
	void ohpl_s5b_voice_load_map_file(const unsigned char *, size_t);
	void ohpl_s5b_voice_reapply_pauses();
	void ohpl_s5b_voice_fade_to(float goal, float time);
	void ohpl_s5b_voice_clear_played();
	size_t ohpl_s5b_voice_played_count();
	void ohpl_s5b_voice_played_at(size_t, void (*)(void *, const unsigned char *, size_t), void *);
	void ohpl_s5b_voice_played_insert(const unsigned char *, size_t);
	void ohpl_s5b_register_voice_natives(void *engine);
}

#endif
