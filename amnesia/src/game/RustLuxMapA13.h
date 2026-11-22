#ifndef LUX_RUST_LUX_MAP_A13_H
#define LUX_RUST_LUX_MAP_A13_H

// C ABI of rust/src/luxmap/ (mhelper*.rs, mhost.rs): map helper and the host call table.

#include <cstddef>
#include <cstdint>

struct OhplA13S
{
	const unsigned char *p;
	size_t n;
};

struct OhplA13A
{
	uintptr_t p[6];
	int32_t i[4];
	float f[12];
	float m[16];
	OhplA13S s[2];
	uintptr_t r;
	OhplA13S rs[2];
};

struct OhplA13Damage
{
	void *shape;
	float transform[16];
	float origin[3];
	float minDamage, maxDamage, force, maxImpulse;
	int32_t strength;
	float hitSpeed;
	int32_t damageType, weaponHitType;
	bool checkEnemies, checkPlayer, checkProps, lethalForPlayer;
};

struct OhplA13MapState
{
	int32_t quests, total, current, cpCount, cpPrio;
	bool cpResume;
	float cpVolume;
	OhplA13S cpName, cpStart, cpCallback, cpMusic;
};

struct OhplA7Ctx;
struct OhplA13H;

extern "C" {
	void ohpl_a13_set_host(void (*op)(int32_t, OhplA13A *));
	bool ohpl_a13_ray_before(void *ctx, void *body);
	bool ohpl_a13_ray_hit(void *ctx, void *body, float dist, const float *normal);
	bool ohpl_a13_check_los(const float *start, const float *end, bool shadows);
	bool ohpl_a13_closest_entity(const float *start, const float *dir, float len, float *dist, void **body, void **entity);
	bool ohpl_a13_closest_char(const float *start, const float *dir, float len, float *dist, float *normal, void **body);
	float ohpl_a13_light_level(const float *pos, const void *const *skip, size_t n, bool hasSkip, float radiusAdd);
	bool ohpl_a13_shape_damage(const OhplA13Damage *args, bool *hitPlayer);

	void ohpl_a13_map_init(OhplA7Ctx *c, uintptr_t cmap);
	void ohpl_a13_map_load(OhplA7Ctx *c, const char *file, size_t n, bool entities);
	void ohpl_a13_map_after_world_load(OhplA7Ctx *c);
	void ohpl_a13_map_update_doomed(OhplA7Ctx *c, bool useCallbacks);
	void ohpl_a13_map_on_enter(OhplA7Ctx *c, bool runScript, bool firstTime);
	void ohpl_a13_map_on_leave(OhplA7Ctx *c, bool runScript);
	void ohpl_a13_map_update(OhplA7Ctx *c, float step);
	void ohpl_a13_map_render_solid(OhplA7Ctx *c, void *functions);
	void ohpl_a13_map_run_script(OhplA7Ctx *c, const char *cmd, size_t n);
	bool ohpl_a13_map_recompile(OhplA7Ctx *c, void (*sink)(void *, const unsigned char *, size_t), void *ctx);
	void ohpl_a13_map_place_player(OhplA7Ctx *c, const char *name, size_t n);
	void ohpl_a13_map_set_checkpoint(OhplA7Ctx *c, const char *name, size_t nameN, const char *start, size_t startN, const char *callback, size_t callbackN);
	void ohpl_a13_map_load_checkpoint(OhplA7Ctx *c);
	void ohpl_a13_map_create_entity(OhplA7Ctx *c, const char *name, size_t nameN, const char *file, size_t fileN, const float *mtx, const float *scale);
	void ohpl_a13_map_add_entity(OhplA7Ctx *c, void *e);
	void ohpl_a13_map_destroy_entity(OhplA7Ctx *c, void *e);
	void ohpl_a13_map_broadcast(OhplA7Ctx *c, int32_t kind, bool hasPos, const float *pos, float radius, float time, bool local, const float *vx, float fx, int32_t lx);
	void ohpl_a13_map_broadcast_sound(OhplA7Ctx *c, const float *pos, float volume, float min, float max);
	bool ohpl_a13_map_node_is_goal(OhplA7Ctx *c, const void *node);
	bool ohpl_a13_map_door(OhplA7Ctx *c, int32_t id, int32_t *state);
	bool ohpl_a13_map_body_in_sticky(OhplA7Ctx *c, const void *body);
	bool ohpl_a13_map_detach_sticky(OhplA7Ctx *c, const void *body);
	void *ohpl_a13_map_body_from_pair(OhplA7Ctx *c, int32_t parent, int32_t child);
	bool ohpl_a13_map_check_collision(OhplA7Ctx *c, void *c1, void *c2);
	void ohpl_a13_map_add_completion(OhplA7Ctx *c, int32_t amount, float delay);
	void ohpl_a13_map_add_dissolve(OhplA7Ctx *c, void *mesh, float time);
	void *ohpl_a13_map_add_lamp(OhplA7Ctx *c, void *lamp, void *light, float amount, bool onColor, bool spec);
	void ohpl_a13_map_teardown(OhplA7Ctx *c);
	void *ohpl_a13_map_world(OhplA7Ctx *c);
	void *ohpl_a13_map_phys(OhplA7Ctx *c);
	void ohpl_a13_map_get(OhplA7Ctx *c, OhplA13MapState *out);
	void ohpl_a13_map_set(OhplA7Ctx *c, const OhplA13MapState *s);
	void ohpl_a13_map_set_quests(OhplA7Ctx *c, int32_t n);

	OhplA13H *ohpl_a13_handler_new(uintptr_t cpp);
	void ohpl_a13_handler_free(OhplA13H *h);
	OhplA7Ctx *ohpl_a13_handler_maps(OhplA13H *h);
	void ohpl_a13_handler_teardown(OhplA13H *h);
	void ohpl_a13_handler_update(OhplA13H *h, float step);
	void ohpl_a13_handler_render_solid(OhplA13H *h, void *functions);
	void ohpl_a13_handler_reset(OhplA13H *h);
	void ohpl_a13_handler_on_quit(OhplA13H *h);
	void ohpl_a13_handler_set_update_active(OhplA13H *h, bool active);
	void ohpl_a13_handler_container(OhplA13H *h, bool enter);
	bool ohpl_a13_handler_show_commentary(OhplA13H *h);
	void ohpl_a13_handler_set_show_commentary(OhplA13H *h, bool show, bool stop);
	void ohpl_a13_handler_pause(OhplA13H *h, bool pause);
	void *ohpl_a13_handler_load_map(OhplA13H *h, const char *file, size_t n, bool entities);
	void ohpl_a13_handler_destroy_map(OhplA13H *h, void *map, bool loadingSave);
	void ohpl_a13_handler_set_current(OhplA13H *h, void *map, bool runScript, bool firstTime, const char *pos, size_t n);
	void ohpl_a13_handler_change_map(OhplA13H *h, const char *map, size_t mapN, const char *pos, size_t posN,
										const char *startSound, size_t startN, const char *endSound, size_t endN);
	void ohpl_a13_handler_add_hearable(OhplA13H *h, const char *name, size_t n);
	void ohpl_a13_handler_sound_started(OhplA13H *h, const char *typeName, size_t n, float maxDist, float volume, const float *pos);
}

#endif
