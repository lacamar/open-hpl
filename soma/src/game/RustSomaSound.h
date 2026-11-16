#ifndef SOMA_RUST_SOUND_H
#define SOMA_RUST_SOUND_H

// C ABI of rust/src/somasound (ffi_game.rs, ffi_fsb.rs, ffi_voice.rs, host.rs).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>


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

struct OhplS7Str
{
	const unsigned char *p;
	size_t n;
};

struct OhplGs5Str
{
	const unsigned char *p;
	size_t n;
};

typedef void (*OhplS7Sink)(void *, const unsigned char *, size_t);

struct OhplS7CallArg
{
	uint8_t kind; // 0 string, 1 bool, 2 int
	int32_t i;
	OhplGs5Str s;
};

struct OhplGs5AreaDef;

struct OhplS7Host
{
	bool (*valid)(size_t, int);
	bool (*play)(const OhplGs5PlayReq *, size_t *, int *);
	void (*stop)(size_t);
	void (*pause)(size_t, bool);
	double (*total_time)(size_t);
	void (*set_looping)(size_t, bool);
	void (*set_filter)(size_t, int, float);
	void (*set_position)(size_t, const float *);
	void (*set_default_volume)(size_t, float);
	void (*set_default_speed)(size_t, float);
	void (*listener)(float *);
	void (*entity)(size_t, float *);
	int (*set_global_volume)(float, unsigned, int);
	float (*global_volume_of)(int);
	int (*fade_global_volume)(float, float, unsigned, int, bool);
	int (*set_global_speed)(float, unsigned, int);
	float (*global_speed_of)(int);
	int (*fade_global_speed)(float, float, unsigned, int, bool);
	void (*fade_out_all)(unsigned, float);
	void (*music_play)(const unsigned char *, size_t, float, float, bool, bool);
	void (*music_stop)(float);
	bool (*music_has_song)();
	void (*music_fade_volume_mul)(float, float);
	float (*music_volume_mul)();
	bool (*entity_active)(uint8_t, int32_t, int32_t);
	bool (*find_file)(const unsigned char *, size_t, OhplS7Sink, void *);
	void (*add_resource_dir)(const unsigned char *, size_t);
	size_t (*data_new)(const unsigned char *, size_t);
	void (*data_add_sound)(size_t, const unsigned char *, size_t);
	void (*data_setup)(size_t, float, float, bool, bool);
	void (*install_updater)();
	void (*install_hooks)();
	void (*install_resolver)();
	const void *(*shell_string)(size_t, unsigned, const unsigned char *, size_t);
	const void *(*shell_position_ref)(size_t);
	size_t (*shell_new)(size_t, size_t, int);
	void (*shell_delete)(size_t);
	size_t (*shell_handle)(size_t);
	void (*shell_set_position)(size_t, const float *);
	size_t (*world_create_sound)(size_t, const unsigned char *, size_t, const unsigned char *, size_t, bool);
	size_t (*world_sound_by_creation)(size_t, int);
	void (*object_id)(size_t, unsigned char *);
	size_t (*se_event)(size_t);
	void (*se_fade)(size_t, bool, float);
	void (*se_main_fade_in)(size_t, float, float);
	int (*se_data_loop)(size_t);
	float (*se_prop)(size_t, unsigned, float);
	void (*se_reset_props)(size_t);
	void (*log)(const unsigned char *, size_t);
	void (*warning)(const unsigned char *, size_t);
	size_t (*scape_world)(size_t);
	void (*scape_areas)(size_t, void (*)(void *, const OhplGs5AreaDef *), void *);
	size_t (*world_sound_by_name)(size_t, const unsigned char *, size_t);
	void (*area_state)(size_t, bool *, float *);
	bool (*camera_pos)(float *);
	bool (*body_pos)(float *);
	void (*apply_reverb)(const float *);
	void (*ent_name)(size_t, OhplS7Sink, void *);
	const void *(*prefix_ref)(const unsigned char *, size_t);
	bool (*voice_present)();
	bool (*voice_play)(const unsigned char *, size_t, uint64_t, void (*)(uint64_t));
	int (*voice_line_count)(const unsigned char *, size_t);
	void (*voice_stop_all)();
	bool (*voice_scene_involving)(const unsigned char *, size_t);
	bool (*voice_subject_involves)(const unsigned char *, size_t, const unsigned char *, size_t);
	int (*script_call)(const unsigned char *, size_t, const OhplS7CallArg *, size_t, bool);
};

extern "C" {
	size_t ohpl_s7_inst_new(size_t shell, size_t event, const unsigned char *name, size_t n, size_t entity, bool has_entity, int type);
	OhplGs5InstState *ohpl_s7_inst_state(size_t h);
	void ohpl_s7_inst_destroy(size_t h);
	void ohpl_s7_inst_play(size_t h);
	void ohpl_s7_inst_stop(size_t h);
	void ohpl_s7_inst_stop_now(size_t h);
	void ohpl_s7_inst_fade_out(size_t h, float speed);
	void ohpl_s7_inst_fade_in_to(size_t h, float mul, float speed);
	void ohpl_s7_inst_fade_volume_mul_to(size_t h, float dest, float speed);
	void ohpl_s7_inst_fade_speed_mul_to(size_t h, float dest, float speed);
	void ohpl_s7_inst_update(size_t h, float dt);
	void ohpl_s7_inst_set_paused(size_t h, bool on);
	void ohpl_s7_inst_set_param(size_t h, int idx, float v);
	float ohpl_s7_inst_param_value(size_t h, int idx);
	int ohpl_s7_inst_param_count(size_t h);
	int ohpl_s7_inst_param_index(size_t h, const unsigned char *name, size_t n);
	float ohpl_s7_inst_total_time(size_t h);
	bool ohpl_s7_inst_is_stopped(size_t h);
	void ohpl_s7_inst_set_position(size_t shell, size_t h, const float *pos);
	void ohpl_s7_inst_describe(size_t h, OhplS7Sink sink, void *ctx);
	void ohpl_s7_event_name(size_t event, OhplS7Sink sink, void *ctx);
	int64_t ohpl_s7_event_of_data(size_t data);
	size_t ohpl_s7_resolve(const unsigned char *name, size_t n);
	void ohpl_s7_preload(const unsigned char *name, size_t n);
	size_t ohpl_s7_play_gui(const unsigned char *name, size_t n, float volume, int type, bool looped, bool stream);
	size_t ohpl_s7_play_3d(const unsigned char *name, size_t n, float volume, const float *pos, int type);
	size_t ohpl_s7_find_instance(const unsigned char *name, size_t n);
	bool ohpl_s7_is_live(size_t shell, int id);
	void ohpl_s7_fade_out_all(unsigned types, float speed);
	void ohpl_s7_update(float dt, bool paused);
	void ohpl_s7_pause_update(float dt, bool paused);
	void ohpl_s7_register_natives(void *engine);
	void ohpl_s7_music_serialize(OhplS7Sink sink, void *ctx);
	void ohpl_s7_music_deserialize(const unsigned char *data, size_t n);
	void ohpl_s7_install_host(const void *ops);
	void ohpl_s7_dialog_update(float dt);
	void ohpl_s7_dialog_stop_all();
	void ohpl_s7_register_dialog(void *engine);
	void ohpl_s7_scape_update(size_t map, float dt);
	void ohpl_s7_scape_forget(size_t map);
	void ohpl_s7_scape_set_paused(bool on);
	void ohpl_s7_scape_describe(OhplS7Sink sink, void *ctx);
	void ohpl_s7_register_soundscape(void *engine);
	void ohpl_s7_cache_dir(const unsigned char *game, size_t gn, const unsigned char *sub, size_t sn, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_s7_extract_samples(const unsigned char *bank, size_t bn, const unsigned char *cache, size_t cn, const unsigned char *prefix, size_t pn,
								 const OhplS7Str *samples, size_t count, void (*sink)(void *, const unsigned char *, size_t, const unsigned char *, size_t), void *ctx);
	void ohpl_s7_dump_text(const unsigned char *tag, size_t tn, const unsigned char *text, size_t n);
}

inline void OhplS7Text(const char *tag, const std::string &text)
{
	ohpl_s7_dump_text((const unsigned char *)tag, strlen(tag), (const unsigned char *)text.data(), text.size());
}

struct OhplGs5Voices;


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


	bool ohpl_gs5_fsb_mode_supported(unsigned int mode);
	unsigned char ohpl_gs5_fsb_cached_state(bool has_ogg, bool has_wav);
	void ohpl_gs5_fsb_output_name(const unsigned char *prefix, size_t pn, const unsigned char *sample, size_t sn, unsigned int mode, int channels,
								  void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	int64_t ohpl_gs5_fsb_wanted_index(const unsigned char *sample, size_t sn, const OhplGs5Str *wanted, size_t count);
	void ohpl_gs5_fsb_downmix_block(const float *const *planes, size_t ch, size_t n, const float *left, const float *right, short *out);
}

#endif
