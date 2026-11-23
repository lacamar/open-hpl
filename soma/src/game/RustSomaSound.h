#ifndef SOMA_RUST_SOUND_H
#define SOMA_RUST_SOUND_H

// C ABI of rust/src/somasound (ffi_game.rs, ffi_fsb.rs, ffi_voice.rs, host.rs).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>



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
	bool (*entity_active)(uint8_t, int32_t, int32_t);
	bool (*find_file)(const unsigned char *, size_t, OhplS7Sink, void *);
	void (*add_resource_dir)(const unsigned char *, size_t);
	size_t (*data_new)(const unsigned char *, size_t);
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


}

#endif
