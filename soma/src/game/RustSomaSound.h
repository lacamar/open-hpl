#ifndef SOMA_RUST_SOUND_H
#define SOMA_RUST_SOUND_H

// C ABI of rust/src/somasound/ffi.rs.

#include <cstddef>
#include <cstdint>

struct OhplGs5Bank;

struct OhplGs5PlayReq
{
	uint8_t kind; // 0 3D, 1 GUI stream, 2 GUI
	const unsigned char *file;
	size_t file_len;
	bool looped;
	float pos[3];
	int entry_type;
	bool stream;
};

struct OhplGs5Host
{
	void *ctx;
	bool (*valid)(void *, size_t entry, int id);
	bool (*play)(void *, const OhplGs5PlayReq *, size_t *entry, int *id);
	void (*stop)(void *, size_t entry);
	void (*pause)(void *, size_t entry, bool);
	double (*total_time)(void *, size_t entry);
	void (*set_looping)(void *, size_t entry, bool);
	void (*set_filter)(void *, size_t entry, int which, float gain); // 0 HF, 1 LF, 2 send
	void (*set_position)(void *, size_t entry, const float *pos);
	void (*set_default_volume)(void *, size_t entry, float);
	void (*set_default_speed)(void *, size_t entry, float);
	void (*listener)(void *, float *out9); // position, forward, up
	void (*entity)(void *, size_t entity, float *out6); // world position, min, max, volume
};

struct OhplGs5InstState
{
	size_t event;
	size_t entity;
	int id;
	int entry_type;
	float pos[3];
	float rand_gain, rand_pitch;
	float volume;
	float volume_mul, volume_mul_dest, volume_mul_speed;
	float speed_mul, speed_mul_dest, speed_mul_speed;
	float fade, fade_dest, fade_speed;
	int start_fails;
	float time, audibility;
	bool stop_at_fade_end, started, stopped, paused, stop_disabled, play_3d, has_entity;
};
static_assert(sizeof(OhplGs5InstState) == 104 && offsetof(OhplGs5InstState, fade_dest) == 76 && offsetof(OhplGs5InstState, volume_mul_dest) == 52,
			  "OhplGs5InstState layout");

struct OhplGs5EventInfo
{
	bool has_files, has_defs, is_3d, one_shot;
	float min_dist, max_dist;
	size_t data;
	size_t param_count;
};

struct OhplGs5ParamDef
{
	float min, max, velocity, seek, sustain;
	int loop_mode, builtin;
};

struct OhplGs5FilePair
{
	const unsigned char *key;
	size_t key_len;
	const unsigned char *file;
	size_t file_len;
};

extern "C" {
	OhplGs5Bank *ohpl_gs5_bank_new();
	void ohpl_gs5_bank_free(OhplGs5Bank *);

	bool ohpl_gs5_ev_mark_project(OhplGs5Bank *, const unsigned char *key, size_t n);
	bool ohpl_gs5_ev_load_fdp(OhplGs5Bank *, const unsigned char *data, size_t n, bool read_ok, const unsigned char *project, size_t pn,
							  void (*name)(void *, const unsigned char *, size_t), void *ctx, int *count);
	void ohpl_gs5_ev_wanted(OhplGs5Bank *, const unsigned char *key, size_t n,
							void (*sink)(void *, const unsigned char *, size_t, const unsigned char *, size_t), void *ctx);
	void ohpl_gs5_ev_assign(OhplGs5Bank *, const unsigned char *key, size_t n, const OhplGs5FilePair *pairs, size_t count);
	int64_t ohpl_gs5_ev_project_len(const unsigned char *name, size_t n);
	int64_t ohpl_gs5_ev_find(OhplGs5Bank *, const unsigned char *name, size_t n);
	size_t ohpl_gs5_ev_file_event(OhplGs5Bank *, const unsigned char *file, size_t n, bool looped, bool stream);
	bool ohpl_gs5_ev_info(OhplGs5Bank *, size_t idx, OhplGs5EventInfo *out);
	void ohpl_gs5_ev_files(OhplGs5Bank *, size_t idx, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_gs5_ev_set_data(OhplGs5Bank *, size_t idx, size_t data);
	int64_t ohpl_gs5_ev_of_data(OhplGs5Bank *, size_t data);
	void ohpl_gs5_ev_name(OhplGs5Bank *, size_t idx, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_gs5_ev_dump(OhplGs5Bank *, void (*sink)(void *, const unsigned char *, size_t), void *ctx);

	size_t ohpl_gs5_inst_new(OhplGs5Bank *, size_t event, size_t entity, bool has_entity, int entry_type);
	OhplGs5InstState *ohpl_gs5_inst_state(OhplGs5Bank *, size_t h);
	void ohpl_gs5_inst_destroy(OhplGs5Bank *, const OhplGs5Host *, size_t h);
	void ohpl_gs5_inst_play(OhplGs5Bank *, const OhplGs5Host *, size_t h);
	void ohpl_gs5_inst_stop(OhplGs5Bank *, const OhplGs5Host *, size_t h);
	void ohpl_gs5_inst_stop_now(OhplGs5Bank *, const OhplGs5Host *, size_t h);
	void ohpl_gs5_inst_fade_out(OhplGs5Bank *, const OhplGs5Host *, size_t h, float speed);
	void ohpl_gs5_inst_fade_in_to(OhplGs5Bank *, const OhplGs5Host *, size_t h, float mul, float speed);
	void ohpl_gs5_inst_set_paused(OhplGs5Bank *, const OhplGs5Host *, size_t h, bool on);
	void ohpl_gs5_inst_update(OhplGs5Bank *, const OhplGs5Host *, size_t h, float dt);
	void ohpl_gs5_inst_fade_volume_mul_to(OhplGs5Bank *, size_t h, float dest, float speed);
	void ohpl_gs5_inst_fade_speed_mul_to(OhplGs5Bank *, size_t h, float dest, float speed);
	void ohpl_gs5_inst_set_param(OhplGs5Bank *, size_t h, int idx, float value);
	int ohpl_gs5_inst_param_index(OhplGs5Bank *, size_t h, const unsigned char *name, size_t n);
	float ohpl_gs5_inst_param_value(OhplGs5Bank *, size_t h, int idx);
	int ohpl_gs5_inst_param_count(OhplGs5Bank *, size_t h);
	bool ohpl_gs5_inst_param_def(OhplGs5Bank *, size_t h, int idx, OhplGs5ParamDef *out, void (*name)(void *, const unsigned char *, size_t), void *ctx);
	float ohpl_gs5_inst_total_time(OhplGs5Bank *, const OhplGs5Host *, size_t h);
	int ohpl_gs5_inst_voice(OhplGs5Bank *, size_t h, size_t i, float *out3);
}

struct OhplGs5Voices;
struct OhplGs5Scape;

struct OhplGs5Str
{
	const unsigned char *p;
	size_t n;
};

struct OhplGs5VoiceHost
{
	void *ctx;
	bool (*entry_valid)(void *, size_t, int);
	void (*entry_set_volume_mul)(void *, size_t, float);
	void (*entry_fade_out)(void *, size_t, float);
	void (*entry_set_paused)(void *, size_t, bool);
	void (*channel_set_position)(void *, size_t, const float *);
	void (*channel_filter)(void *, size_t, bool high, float);
	double (*channel_total)(void *, size_t);
	double (*channel_elapsed)(void *, size_t);
	void (*listener_pos)(void *, float *);
	bool (*map_entity_pos)(void *, const unsigned char *, size_t, float *);
	bool (*map_current)(void *);
	bool (*file_exists)(void *, const unsigned char *, size_t);
	bool (*read_file)(void *, const unsigned char *, size_t, void (*)(void *, const unsigned char *, size_t), void *);
	bool (*play_3d)(void *, const unsigned char *, size_t, float vol, const float *pos, float min, float max, int type, size_t *entry, int *id);
	bool (*play_gui_stream)(void *, const unsigned char *, size_t, float vol, int type, size_t *entry, int *id);
	bool (*effect_play)(void *, const unsigned char *, size_t, float vol, int type, size_t *effect, int *id);
	bool (*effect_live)(void *, size_t, int);
	void (*effect_set_volume_mul)(void *, size_t, float);
	void (*effect_fade_out)(void *, size_t, float);
	void (*effect_set_paused)(void *, size_t, bool);
	bool (*effect_is_playing)(void *, size_t);
	float (*effect_total)(void *, size_t);
	float (*effect_elapsed)(void *, size_t);
	bool (*played_has)(void *, const unsigned char *, size_t);
	bool (*played_insert)(void *, const unsigned char *, size_t);
	void (*log)(void *, const unsigned char *, size_t);
	void (*error)(void *, const unsigned char *, size_t);
	size_t (*wrap_rows)(void *, const unsigned char *, size_t, float width, float line_height, const float *size);
};

struct OhplGs5VoiceEvent
{
	int kind; // 0 finished, 1 line callback
	const unsigned char *func;
	size_t func_len;
	const unsigned char *scene;
	size_t scene_len;
	const unsigned char *name;
	size_t name_len;
	uint64_t on_done;
	int line;
	bool start;
};

struct OhplGs5AreaDef
{
	size_t ent;
	OhplGs5Str level;
	int prio;
	OhplGs5Str bg;
	float bg_volume, bg_fade_in, bg_trans_speed, parent_vol_mul;
	OhplGs5Str in_names, out_names;
	float ent_fade_in, ent_fade_out;
	OhplGs5Str prefix;
	int prefix_prio;
	bool use_reverb;
	int reverb_prio;
	OhplGs5Str reverb_type;
	float reverb_amount, reverb_fade;
};

struct OhplGs5ScapeHost
{
	void *ctx;
	bool (*bg_live)(void *, size_t, int);
	void (*bg_fade_out)(void *, size_t, float);
	void (*bg_fade_volume_mul_to)(void *, size_t, float, float);
	bool (*bg_play)(void *, const unsigned char *, size_t, size_t *, int *);
	void (*bg_set_volume_mul)(void *, size_t, float);
	void (*bg_fade_in)(void *, size_t, float);
	void (*bg_set_paused)(void *, size_t, bool);
	bool (*world_present)(void *);
	void (*entity_fade)(void *, const unsigned char *, size_t, bool fade_in, float speed);
	void (*area)(void *, size_t ent, bool *active, float *matrix16_size3);
	bool (*camera_pos)(void *, float *);
	bool (*body_pos)(void *, float *);
	void (*apply_reverb)(void *, const float *env16);
};

struct OhplGs5ScapeInfo
{
	int rev_type;
	float rev_amount, rev_room;
	size_t rev_area_ent;
	bool rev_has_area;
};

extern "C" {
	OhplGs5Voices *ohpl_gs5_voice_new();
	void ohpl_gs5_voice_free(OhplGs5Voices *);
	void ohpl_gs5_decode_text(const unsigned char *data, size_t n, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	bool ohpl_gs5_voice_mark_set(OhplGs5Voices *, const unsigned char *set, size_t n);
	void ohpl_gs5_voice_load_lang(OhplGs5Voices *, const unsigned char *text, size_t n);
	int64_t ohpl_gs5_voice_load_voice(OhplGs5Voices *, const unsigned char *text, size_t n, const unsigned char *set, size_t sn);
	bool ohpl_gs5_voice_subject_exists(OhplGs5Voices *, const unsigned char *name, size_t n);
	bool ohpl_gs5_voice_any_active(OhplGs5Voices *);
	int ohpl_gs5_voice_play_check(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *subject, size_t n, int prio);
	void ohpl_gs5_voice_scene_of(OhplGs5Voices *, const unsigned char *subject, size_t n, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	bool ohpl_gs5_voice_play_start(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *subject, size_t n, int line, const unsigned char *callback,
								   size_t cn, int prio, uint64_t on_done, bool play_while_paused);
	int64_t ohpl_gs5_voice_stop_next(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *scene, size_t n);
	void ohpl_gs5_voice_stop_all(OhplGs5Voices *, const OhplGs5VoiceHost *);
	void ohpl_gs5_voice_reset_state(OhplGs5Voices *);
	void ohpl_gs5_voice_skip_line(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *scene, size_t n);
	bool ohpl_gs5_voice_character_is_speaking(OhplGs5Voices *, const unsigned char *name, size_t n);
	void ohpl_gs5_voice_set_source(OhplGs5Voices *, const unsigned char *character, size_t cn, const unsigned char *entity, size_t en, float min, float max,
								   bool use_3d, float listen_range, float min_freq, float max_freq, int freq_flags);
	void ohpl_gs5_voice_set_focus_scene(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *scene, size_t n);
	void ohpl_gs5_voice_fade_scene_volume(OhplGs5Voices *, const unsigned char *scene, size_t n, float volume, float time);
	void ohpl_gs5_voice_fade_to(OhplGs5Voices *, float goal, float time);
	float ohpl_gs5_voice_fade_alpha(OhplGs5Voices *);
	bool ohpl_gs5_voice_is_fading(OhplGs5Voices *);
	void ohpl_gs5_voice_update_begin(OhplGs5Voices *, const OhplGs5VoiceHost *, float dt, bool game_paused);
	bool ohpl_gs5_voice_update_step(OhplGs5Voices *, const OhplGs5VoiceHost *, void (*sink)(void *, const OhplGs5VoiceEvent *), void *ctx);
	void ohpl_gs5_voice_speaking_changes(OhplGs5Voices *, void (*sink)(void *, const unsigned char *, size_t, bool), void *ctx);
	bool ohpl_gs5_voice_speaking_func(OhplGs5Voices *, const unsigned char *name, size_t n, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_gs5_voice_speaking_done(OhplGs5Voices *, const unsigned char *name, size_t n, const unsigned char *func, size_t fn, bool keep);
	void ohpl_gs5_voice_set_speaking_callback(OhplGs5Voices *, const unsigned char *name, size_t n, const unsigned char *func, size_t fn);
	void ohpl_gs5_voice_add_subtitle(OhplGs5Voices *, const OhplGs5Str *rows, size_t count, const unsigned char *name, size_t nn, const float *color, int drawn_chars);
	void ohpl_gs5_voice_subtitles(OhplGs5Voices *, const OhplGs5VoiceHost *, float screen_h, bool game_paused,
								  void (*sink)(void *, const unsigned char *, size_t, float alpha, bool has_color, const float *color, float y, size_t rows), void *ctx);
	void ohpl_gs5_voice_drop_immediate(OhplGs5Voices *);
	bool ohpl_gs5_voice_subject_is_playing(OhplGs5Voices *, const unsigned char *, size_t);
	bool ohpl_gs5_voice_scene_is_active(OhplGs5Voices *, const unsigned char *, size_t);
	bool ohpl_gs5_voice_scene_involving_character_is_active(OhplGs5Voices *, const unsigned char *, size_t);
	bool ohpl_gs5_voice_subject_involves(OhplGs5Voices *, const unsigned char *s, size_t n, const unsigned char *c, size_t cn);
	int ohpl_gs5_voice_subject_line_count(OhplGs5Voices *, const unsigned char *, size_t);
	void ohpl_gs5_voice_subject_scene(OhplGs5Voices *, const unsigned char *, size_t, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	float ohpl_gs5_voice_time_left(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *, size_t);
	bool ohpl_gs5_voice_spectrum_target(OhplGs5Voices *, bool by_scene, const unsigned char *s, size_t n, size_t *entry, int *id,
										void (*file)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_gs5_voice_set_paused_all(OhplGs5Voices *, const OhplGs5VoiceHost *, bool on);
	void ohpl_gs5_voice_set_paused(OhplGs5Voices *, const OhplGs5VoiceHost *, const unsigned char *scene, size_t n, bool on);
	void ohpl_gs5_voice_reapply_pauses(OhplGs5Voices *, const OhplGs5VoiceHost *);
	void ohpl_gs5_voice_lip_weights(OhplGs5Voices *, const OhplGs5VoiceHost *, void (*sink)(void *, const unsigned char *, size_t, const float *), void *ctx);
	void ohpl_gs5_spectrum(const float *pcm, size_t n, int channels, int rate, double elapsed, int num, float *out);

	OhplGs5Scape *ohpl_gs5_scape_new();
	void ohpl_gs5_scape_free(OhplGs5Scape *);
	void ohpl_gs5_scape_load(OhplGs5Scape *, const OhplGs5AreaDef *defs, size_t count);
	void ohpl_gs5_scape_update(OhplGs5Scape *, const OhplGs5ScapeHost *, float dt);
	void ohpl_gs5_scape_set_default_reverb(OhplGs5Scape *, int preset, float fade);
	void ohpl_gs5_scape_set_paused(OhplGs5Scape *, const OhplGs5ScapeHost *, bool on);
	const unsigned char *ohpl_gs5_scape_prefix(OhplGs5Scape *, size_t *len);
	void ohpl_gs5_scape_info(OhplGs5Scape *, OhplGs5ScapeInfo *out);
	void ohpl_gs5_scape_level(OhplGs5Scape *, size_t i, size_t *ent, bool *has_area, bool *has_bg, void (*name)(void *, const unsigned char *, size_t), void *ctx);
	const unsigned char *ohpl_gs5_reverb_name(int i, size_t *len);

	bool ohpl_gs5_fsb_mode_supported(unsigned int mode);
	unsigned char ohpl_gs5_fsb_cached_state(bool has_ogg, bool has_wav);
	void ohpl_gs5_fsb_output_name(const unsigned char *prefix, size_t pn, const unsigned char *sample, size_t sn, unsigned int mode, int channels,
								  void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	int64_t ohpl_gs5_fsb_wanted_index(const unsigned char *sample, size_t sn, const OhplGs5Str *wanted, size_t count);
	void ohpl_gs5_fsb_downmix_block(const float *const *planes, size_t ch, size_t n, const float *left, const float *right, short *out);
}

#endif
