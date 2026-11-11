#ifndef SOMA_RUST_SAVE_H
#define SOMA_RUST_SAVE_H

// C ABI of rust/src/somasave/ffi.rs; record layouts mirror rust/src/somasave/rec.rs.

#include <cstddef>
#include <cstdint>

struct OhplGs4Str
{
	const unsigned char *p;
	size_t n;
};

template <class T> struct OhplGs4Slice
{
	const T *p;
	size_t n;
};

struct OhplGs4W
{
	const uint32_t *p;
	size_t n;
};

struct OhplGs4Player
{
	uint8_t present;
	int32_t char_crouching;
	float feet[3];
	float yaw, pitch;
	int32_t state, move_state;
	int32_t gravity_active;
	float gravity[3];
};

struct OhplGs4MapTimer
{
	OhplGs4Str name, func, user_str;
	float time, user_float, length;
	int32_t user_int;
	bool paused;
};

struct OhplGs4Spawned
{
	OhplGs4Str name, file;
	float mtx[16];
	float scale[3];
	unsigned char id[12];
};

struct OhplGs4Collide
{
	OhplGs4Str child, func;
};

struct OhplGs4Body
{
	float mtx[16];
	float mass;
	bool active, gravity;
};

struct OhplGs4Attach
{
	OhplGs4Str parent, body, socket;
	float parent_prev[16], offset[16];
	bool use_rotation, locked;
};

struct OhplGs4Joint
{
	OhplGs4Str name;
	float limits[2];
};

struct OhplGs4Timer
{
	OhplGs4Str func;
	uint64_t id;
	float length, time_left;
	bool repeat, paused;
};

struct OhplGs4Fader
{
	unsigned char b[32];
};
struct OhplGs4ImTimer
{
	unsigned char b[16];
};
struct OhplGs4ImState
{
	unsigned char b[56];
};

struct OhplGs4Timers
{
	OhplGs4Slice<OhplGs4Timer> timers;
	OhplGs4Slice<OhplGs4Fader> faders;
};

struct OhplGs4VarPair
{
	OhplGs4Str key, value;
};

struct OhplGs4ImStatePair
{
	uint64_t id;
	OhplGs4ImState state;
};

struct OhplGs4Anim
{
	OhplGs4Str name;
	float time, weight, speed, fade;
	bool active, looping, paused;
};

struct OhplGs4Conn
{
	OhplGs4Str name, entity;
	int32_t states_used;
	bool invert;
};

struct OhplGs4EntityHead
{
	OhplGs4Str name, parent_name, interact_cb, look_cb;
	OhplGs4Slice<OhplGs4Collide> collide;
	OhplGs4Slice<OhplGs4Body> bodies;
	OhplGs4Slice<float> dyn_mass;
	OhplGs4Slice<OhplGs4Joint> joints;
	OhplGs4Timers timers;
	OhplGs4Attach attach;
	float mtx[16];
	unsigned char parent_id[12];
	int32_t parent_type;
	float look_max, look_delay;
	bool active, interaction_disabled, interacted_with, effects_active, interact_auto_remove, look_auto_remove, look_center, look_ray,
		static_physics, has_attach, has_mtx;
};

struct OhplGs4EntityTail
{
	OhplGs4Slice<OhplGs4VarPair> vars;
	OhplGs4Str on_gui;
	OhplGs4Slice<OhplGs4ImTimer> imgui_timers;
	OhplGs4Slice<OhplGs4ImStatePair> imgui_states;
	OhplGs4Slice<OhplGs4Anim> anims;
	OhplGs4Str conn_cb;
	OhplGs4Slice<OhplGs4Conn> conns;
	bool gui_active;
};

struct OhplGs4Light
{
	OhplGs4Str name;
	float color[4];
	float radius, brightness;
	bool active, visible, flicker;
};

struct OhplGs4Particle
{
	OhplGs4Str name, data;
	float size[3];
	float mtx[16];
	float color[4];
	float brightness;
	float fade[4];
	uint32_t dead;
	bool active, visible, fade_at_distance, owned;
};

struct OhplGs4Sound
{
	OhplGs4Str name, data;
	OhplGs4Slice<float> params;
	float pos[3];
	float min_dist, max_dist, volume, fade, volume_mul;
	bool remove, active, stopped;
};

struct OhplGs4Billboard
{
	OhplGs4Str name;
	float color[4];
	bool visible;
};

struct OhplGs4Agent
{
	OhplGs4Str name, path;
	float mtx[16];
	int32_t state;
	float yaw;
	bool senses, detection;
};

struct OhplGs4Fog
{
	float colors[3][4];
	float floats[9];
	bool flags[5];
};

struct OhplGs4CamTex
{
	OhplGs4Str name, attached;
	float rot[16];
	float pos[3];
	int32_t size[2];
	uint32_t fps;
	float fov, near_plane, far_plane;
};

struct OhplGs4FogArea
{
	OhplGs4Str name;
	bool visible;
};

struct OhplGs4Colliding
{
	OhplGs4Str parent, child, func;
};

struct OhplGs4FadeValue
{
	unsigned char b[20];
};

struct OhplGs4Camera
{
	float pos[3];
	float limits[4];
	OhplGs4FadeValue fov_mul, aspect_mul;
	uint8_t detached;
};

struct OhplGs4OwnedLight
{
	OhplGs4Str name;
	float color[4];
	float radius;
	bool visible;
};

struct OhplGs4Tone
{
	float exposure, white_cut;
};

struct OhplGs4World
{
	OhplGs4Player player;
	OhplGs4Slice<OhplGs4MapTimer> map_timers;
	OhplGs4Slice<OhplGs4Spawned> spawned;
	OhplGs4Slice<OhplGs4Light> lights;
	OhplGs4Slice<OhplGs4Particle> particles;
	OhplGs4Slice<OhplGs4Sound> sounds;
	OhplGs4Slice<OhplGs4Billboard> billboards;
	OhplGs4Slice<OhplGs4Agent> agents;
	OhplGs4Slice<OhplGs4CamTex> cam_textures;
	OhplGs4Slice<OhplGs4FogArea> fog_areas;
	OhplGs4Slice<OhplGs4Colliding> colliding;
	OhplGs4Slice<OhplGs4OwnedLight> owned_lights;
	OhplGs4Str display_name;
	OhplGs4Fog fog;
	OhplGs4Camera camera;
	OhplGs4Tone tone;
	float health;
	size_t entity_count;
	size_t other_count;
	bool underwater, has_camera, has_tone;
};

static_assert(sizeof(OhplGs4Player) == 52, "OhplGs4Player layout");
static_assert(sizeof(OhplGs4MapTimer) == 72, "OhplGs4MapTimer layout");
static_assert(sizeof(OhplGs4Spawned) == 120, "OhplGs4Spawned layout");
static_assert(sizeof(OhplGs4Body) == 72, "OhplGs4Body layout");
static_assert(sizeof(OhplGs4Attach) == 184, "OhplGs4Attach layout");
static_assert(sizeof(OhplGs4Timer) == 40, "OhplGs4Timer layout");
static_assert(sizeof(OhplGs4Anim) == 40, "OhplGs4Anim layout");
static_assert(sizeof(OhplGs4Conn) == 40, "OhplGs4Conn layout");
static_assert(sizeof(OhplGs4EntityHead) == 448, "OhplGs4EntityHead layout");
static_assert(sizeof(OhplGs4EntityTail) == 120, "OhplGs4EntityTail layout");
static_assert(sizeof(OhplGs4Light) == 48, "OhplGs4Light layout");
static_assert(sizeof(OhplGs4Particle) == 152, "OhplGs4Particle layout");
static_assert(sizeof(OhplGs4Sound) == 88, "OhplGs4Sound layout");
static_assert(sizeof(OhplGs4Billboard) == 40, "OhplGs4Billboard layout");
static_assert(sizeof(OhplGs4Agent) == 112, "OhplGs4Agent layout");
static_assert(sizeof(OhplGs4Fog) == 92, "OhplGs4Fog layout");
static_assert(sizeof(OhplGs4CamTex) == 136, "OhplGs4CamTex layout");
static_assert(sizeof(OhplGs4Camera) == 72, "OhplGs4Camera layout");
static_assert(sizeof(OhplGs4OwnedLight) == 40, "OhplGs4OwnedLight layout");
static_assert(sizeof(OhplGs4World) == 448, "OhplGs4World layout");
static_assert(sizeof(OhplGs4ImStatePair) == 64, "OhplGs4ImStatePair layout");

struct OhplGs4Out;
struct OhplGs4Header;
struct OhplGs4In;

struct OhplGs4Config
{
	float master_volume, gamma;
	int32_t screen_width, screen_height;
	bool vsync, fullscreen, anti_aliasing, show_subtitles;
};

struct OhplGs4CfgHost
{
	void *ctx;
	float (*get_float)(void *, const char *, size_t, const char *, size_t, float);
	bool (*get_bool)(void *, const char *, size_t, const char *, size_t, bool);
	int32_t (*get_int)(void *, const char *, size_t, const char *, size_t, int32_t);
	void (*set_float)(void *, const char *, size_t, const char *, size_t, float);
	void (*set_bool)(void *, const char *, size_t, const char *, size_t, bool);
	void (*set_int)(void *, const char *, size_t, const char *, size_t, int32_t);
};

struct OhplGs4EnginePlan
{
	int32_t linear_depth_texture_idx, gbuffer_textures;
	bool shadow_map_compare_off, depth_in_normal_alpha, keep_hpl3_animations, hardware_srgb;
};

struct OhplGs4Date
{
	int32_t seconds, minutes, hours, month_day, month, year;
};

struct OhplGs4NameState
{
	int32_t count;
	OhplGs4Date latest;
};

struct OhplGs4NamesHost
{
	void *ctx;
	void (*translate_level)(void *, const unsigned char *, size_t, const uint32_t **, size_t *);
	void (*local_mtime)(void *, const uint32_t *, size_t, int32_t *);
};

struct OhplGs4SaveFile
{
	const uint32_t *file;
	size_t file_n;
	OhplGs4Date date;
};

struct OhplGs4ScriptHost
{
	void *ctx;
	bool (*handle_obj)(void *, size_t, int, size_t *);
	bool (*resolve_handle)(void *, size_t, int, const unsigned char *, size_t, size_t *);
	void (*obj_class)(void *, size_t, const unsigned char **, size_t *);
	size_t (*prim_size)(void *, int);
	int (*kind)(void *, int, size_t *);
	void (*string_get)(void *, size_t, const unsigned char **, size_t *);
	void (*string_set)(void *, size_t, const unsigned char *, size_t);
	void (*wstring_get)(void *, size_t, const uint32_t **, size_t *);
	void (*wstring_set)(void *, size_t, const uint32_t *, size_t);
	uint32_t (*array_len)(void *, size_t);
	int (*array_elem_type)(void *, size_t);
	size_t (*array_at)(void *, size_t, uint32_t);
	void (*array_resize)(void *, size_t, uint32_t);
	size_t (*set_items)(void *, size_t);
	void (*set_item)(void *, size_t, const unsigned char **, size_t *);
	void (*set_clear)(void *, size_t);
	void (*set_insert)(void *, size_t, const unsigned char *, size_t);
	uint32_t (*prop_count)(void *, size_t);
	bool (*prop_saved)(void *, size_t, uint32_t);
	void (*prop_name)(void *, size_t, uint32_t, const unsigned char **, size_t *);
	int (*prop_type_id)(void *, size_t, uint32_t);
	size_t (*prop_addr)(void *, size_t, uint32_t);
	void (*type_decl)(void *, size_t, int, const unsigned char **, size_t *);
};

struct OhplGs4WriteHost
{
	void *ctx;
	void (*map_script)(void *, OhplGs4Out *);
	void (*entity_head)(void *, size_t, OhplGs4EntityHead *);
	void (*entity_script)(void *, size_t, OhplGs4Out *);
	void (*entity_tail)(void *, size_t, OhplGs4EntityTail *);
	void (*other_key)(void *, size_t, OhplGs4Str *);
	void (*other_timers)(void *, size_t, OhplGs4Timers *);
	void (*other_script)(void *, size_t, OhplGs4Out *);
};

struct OhplGs4HeaderOut
{
	OhplGs4Str map, start_pos;
	OhplGs4Slice<OhplGs4Str> visited;
	OhplGs4Str vars, preload;
	OhplGs4Slice<OhplGs4Str> played;
	OhplGs4Str music;
	bool exploration;
};

struct OhplGs4HeaderInfo
{
	int32_t version;
	size_t body, visited, played;
	bool exploration;
};

struct OhplGs4ReadHost
{
	void *ctx;
	const OhplGs4ScriptHost *script;
	bool (*has_player)(void *);
	bool (*has_camera)(void *);
	void (*map_timers_begin)(void *);
	void (*map_timer)(void *, const OhplGs4MapTimer *);
	size_t (*map_script)(void *);
	void (*spawned)(void *, const OhplGs4Spawned *);
	size_t (*entity_begin)(void *, OhplGs4Str);
	void (*entity_head)(void *, size_t, const OhplGs4EntityHead *);
	size_t (*entity_script)(void *, size_t);
	void (*entity_tail)(void *, size_t, const OhplGs4EntityHead *, const OhplGs4EntityTail *);
	void (*entities_done)(void *, const OhplGs4Str *, size_t);
	size_t (*other_begin)(void *, OhplGs4Str);
	void (*other_timers)(void *, size_t, const OhplGs4Timers *);
	void (*other_reset)(void *, size_t);
	size_t (*other_script)(void *, size_t);
	void (*light)(void *, const OhplGs4Light *);
	void (*particles_begin)(void *);
	void (*particle)(void *, const OhplGs4Particle *);
	void (*particles_end)(void *, bool);
	void (*sounds_begin)(void *);
	void (*sound)(void *, const OhplGs4Sound *);
	void (*sounds_end)(void *, bool);
	void (*billboard)(void *, const OhplGs4Billboard *);
	void (*player_apply)(void *, const OhplGs4Player *);
	void (*health)(void *, float);
	void (*agent)(void *, const OhplGs4Agent *);
	void (*underwater)(void *, bool);
	void (*fog)(void *, const OhplGs4Fog *);
	void (*cam_texture)(void *, const OhplGs4CamTex *);
	void (*fog_area)(void *, const OhplGs4FogArea *);
	void (*colliding)(void *, const OhplGs4Colliding *);
	void (*display_name)(void *, OhplGs4Str);
	void (*camera)(void *, const OhplGs4Camera *);
	void (*owned_light)(void *, const OhplGs4OwnedLight *);
	void (*tone)(void *, const OhplGs4Tone *);
};

extern "C" {
	int32_t ohpl_gs4_game_id(const unsigned char *data, size_t n);
	OhplGs4Config ohpl_gs4_cfg_default();
	void ohpl_gs4_cfg_load(const OhplGs4CfgHost *, OhplGs4Config *);
	void ohpl_gs4_cfg_save(const OhplGs4CfgHost *, const OhplGs4Config *);
	void ohpl_gs4_paths(int32_t kind, const uint32_t *root, size_t root_n, const unsigned char *id, size_t id_n, bool headless, int32_t pid,
						void (*sink)(void *, const uint32_t *, size_t, bool), void *ctx);
	void ohpl_gs4_init_config_file(const unsigned char *cmd, size_t n, void (*sink)(void *, const uint32_t *, size_t), void *ctx);
	void ohpl_gs4_init_config_error(const uint32_t *s, size_t n, void (*sink)(void *, const uint32_t *, size_t), void *ctx);
	void ohpl_gs4_error_message(const uint32_t *s, size_t n, void (*sink)(void *, const uint32_t *, size_t), void *ctx);
	OhplGs4EnginePlan ohpl_gs4_engine_plan(int32_t id);
	bool ohpl_gs4_use_real_player(const char *freecam);
	bool ohpl_gs4_boot_map(const char *map, const char *pos, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_gs4_menu_map(const unsigned char *s, size_t n, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	int32_t ohpl_gs4_start_area(const unsigned char *s, size_t n);
	bool ohpl_gs4_main_plan(const char *report, const char *game_dir, const char *api, void (*sink)(void *, const unsigned char *, size_t),
							void *ctx);
	void ohpl_gs4_save_name(OhplGs4NameState *, const uint32_t *prefix, size_t prefix_n, const unsigned char *map, size_t map_n,
							const OhplGs4Date *, const uint32_t *suffix, size_t suffix_n, void (*sink)(void *, const uint32_t *, size_t),
							void *ctx);
	void ohpl_gs4_list_saves(const OhplGs4NamesHost *, const OhplGs4SaveFile *files, size_t n,
							 void (*sink)(void *, size_t, const uint32_t *, size_t, const unsigned char *, size_t, const uint32_t *, size_t),
							 void *ctx);
	void ohpl_gs4_rotation(const OhplGs4W *suffixes, size_t n, const uint32_t *suffix, size_t suffix_n, int32_t max, unsigned char *out);
	OhplGs4Out *ohpl_gs4_out_new();
	void ohpl_gs4_out_bytes(const OhplGs4Out *, const unsigned char **p, size_t *n);
	void ohpl_gs4_out_free(OhplGs4Out *);
	void ohpl_gs4_write_script(OhplGs4Out *, const OhplGs4ScriptHost *, size_t obj);
	void ohpl_gs4_encode_save(const OhplGs4HeaderOut *, const OhplGs4World *, const OhplGs4WriteHost *,
							  void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	int32_t ohpl_gs4_header_read(const unsigned char *data, size_t n, bool file_open, OhplGs4Header **out);
	bool ohpl_gs4_header_str(const OhplGs4Header *, size_t which, const unsigned char **p, size_t *n);
	OhplGs4HeaderInfo ohpl_gs4_header_info(const OhplGs4Header *);
	void ohpl_gs4_header_free(OhplGs4Header *);
	OhplGs4In *ohpl_gs4_in_new(const unsigned char *data, size_t n);
	bool ohpl_gs4_in_ok(const OhplGs4In *);
	void ohpl_gs4_in_str(OhplGs4In *, const unsigned char **p, size_t *n);
	void ohpl_gs4_in_free(OhplGs4In *);
	void ohpl_gs4_read_world(OhplGs4In *, int32_t version, const OhplGs4ReadHost *);
	void ohpl_gs4_read_script(OhplGs4In *, int32_t version, const OhplGs4ScriptHost *, size_t obj);
}

#endif // SOMA_RUST_SAVE_H
