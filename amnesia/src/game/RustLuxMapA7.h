#ifndef LUX_RUST_LUX_MAP_A7_H
#define LUX_RUST_LUX_MAP_A7_H

// C ABI of rust/src/luxmap/a7_*.rs: per-map arena for cLuxMap lists/timers/callbacks and area state
// blocks (OHPL_IMPL=rust). The blocks are bound to by reference in the C++ area shells.

#include <cstdlib>

#include "RustLuxMap.h"

struct OhplA7Ctx;
struct OhplA7Area;

struct OhplA7Base
{
	OhplGa5Vec3 size;
	OhplGa5Mtx mtx;
	OhplGa5Vec3 relOffset;
	int bodyId;
	int areaType;
};

struct OhplA7Script { bool itemInteraction; };
struct OhplA7Examine { float insaneLimit, playSoundCount; };
struct OhplA7Sign { unsigned char unused; };
struct OhplA7Ladder { OhplGa5Vec3 forward; float maxY, minY; };
struct OhplA7Rope
{
	OhplGa5Vec3 startPos, endPos;
	float minTotalLength, maxTotalLength, segmentLength, damping, strength, stiffness, radius, lengthTileAmount, lengthTileSize;
	float soundStartSpeed, soundStopSpeed, autoMoveAcc, autoMoveMaxSpeed;
	bool ropeCreated, autoMove;
};

static_assert(sizeof(OhplA7Base) == 96, "OhplA7Base");
static_assert(sizeof(OhplA7Script) == 1 && sizeof(OhplA7Examine) == 8, "pods");
static_assert(sizeof(OhplA7Sign) == 1 && sizeof(OhplA7Ladder) == 20 && sizeof(OhplA7Rope) == 80, "pods");

enum
{
	kOhplA7Script, kOhplA7Examine, kOhplA7Insanity, kOhplA7Flashback, kOhplA7Sign,
	kOhplA7Ladder, kOhplA7Liquid, kOhplA7Sticky, kOhplA7Slime, kOhplA7Rope,
};
enum { kOhplA7Entity, kOhplA7Dissolve, kOhplA7Lamp, kOhplA7Start, kOhplA7Pos, kOhplA7UseItem };
enum { kOhplA7Entities, kOhplA7Enemies, kOhplA7Doomed, kOhplA7StickyList };

// Engine thunks the Rust area code calls while it initialises, loads and saves one area (a7_shell.rs).
struct OhplA7Sv { const char *p; size_t n; };
struct OhplA7Vt
{
	void *ctx;
	float (*var_f)(void *, const char *, size_t, float);
	int (*var_i)(void *, const char *, size_t, int);
	bool (*var_b)(void *, const char *, size_t, bool);
	OhplA7Sv (*var_s)(void *, const char *, size_t, const char *, size_t);
	float (*cfg_f)(void *, const char *, size_t, const char *, size_t, float);
	OhplA7Sv (*cfg_s)(void *, const char *, size_t, const char *, size_t);
	void (*preload_sound)(void *, const char *, size_t);
	void (*preload_ps)(void *, const char *, size_t);
	size_t (*mat_find)(void *, const char *, size_t);
	OhplA7Sv (*mat_name)(void *, size_t);
	void (*body_ids)(void *, size_t, int *);
	size_t (*find_body)(void *, int, int);
	void (*max_focus)(void *, float);
	void (*callback)(void *, int, const char *, size_t, bool);
	bool (*doc_open)(void *, const char *, size_t);
	float (*doc_f)(void *, const char *, size_t);
	OhplA7Sv (*doc_s)(void *, const char *, size_t);
	void (*doc_close)(void *);
};

// Indices of the strings an area keeps (A7Area::strs).
enum { kOhplA7sExDescCat = 0, kOhplA7sExInsaneCat = 2, kOhplA7sExSound = 4, kOhplA7sExInsaneSound = 5, kOhplA7sSignCat = 0, kOhplA7sLadderMaterial = 0,
	kOhplA7sRopeEndPosNode = 0, kOhplA7sRopeStartBody = 1, kOhplA7sRopeEndBody = 2, kOhplA7sRopeMaterial = 3, kOhplA7sRopeSound = 4 };

typedef void (*OhplA7DropFn)(void *);
typedef void (*OhplA7DissolveFn)(void *, void *, float, bool);
typedef void (*OhplA7PtrFn)(void *, void *);

extern "C" {
	OhplA7Ctx *ohpl_a7_new();
	void ohpl_a7_free(OhplA7Ctx *m);
	void ohpl_a7_set_drop(OhplA7Ctx *m, unsigned kind, OhplA7DropFn f);
	void ohpl_a7_teardown(OhplA7Ctx *m);
	void ohpl_a7_destroy_entities(OhplA7Ctx *m);
	void ohpl_a7_destroy_all(OhplA7Ctx *m);
	void ohpl_a7_ent_add(OhplA7Ctx *m, void *p, void *typed, const char *name, size_t name_n, int id, bool is_enemy, bool is_area, bool is_sticky);
	void ohpl_a7_list_add(OhplA7Ctx *m, void *p);
	void ohpl_a7_list_destroy(OhplA7Ctx *m, void *p);
	void *ohpl_a7_ent_find_name(OhplA7Ctx *m, const char *name, size_t n);
	void *ohpl_a7_ent_find_id(OhplA7Ctx *m, int id);
	bool ohpl_a7_ent_exists(OhplA7Ctx *m, const void *p);
	void *ohpl_a7_ent_latest(OhplA7Ctx *m);
	void ohpl_a7_ent_reset_latest(OhplA7Ctx *m);
	int ohpl_a7_ent_free_id(OhplA7Ctx *m);
	bool ohpl_a7_deleting_all(OhplA7Ctx *m);
	void ohpl_a7_doom_push(OhplA7Ctx *m, void *p);
	void ohpl_a7_doom_clear(OhplA7Ctx *m);
	void ohpl_a7_ent_unlink(OhplA7Ctx *m, void *p, void *typed, bool is_enemy, bool is_area, bool is_sticky);
	uint64_t ohpl_a7_cur_begin(OhplA7Ctx *m, unsigned which);
	void *ohpl_a7_cur_get(OhplA7Ctx *m, unsigned which, uint64_t cur);
	uint64_t ohpl_a7_cur_next(OhplA7Ctx *m, unsigned which, uint64_t cur);
	int ohpl_a7_commentary_sync(OhplA7Ctx *m, bool show);

	void ohpl_a7_timer_add(OhplA7Ctx *m, const char *name, size_t name_n, float time, const char *func, size_t func_n);
	void ohpl_a7_timer_push(OhplA7Ctx *m, const char *name, size_t name_n, const char *func, size_t func_n, float count, bool destroy_me);
	void ohpl_a7_timers_clear(OhplA7Ctx *m);
	void ohpl_a7_timer_remove(OhplA7Ctx *m, const char *name, size_t n);
	bool ohpl_a7_timer_get(OhplA7Ctx *m, const char *name, size_t n, float *out);
	void ohpl_a7_timers_begin(OhplA7Ctx *m, float step);
	bool ohpl_a7_timer_poll(OhplA7Ctx *m, size_t *cursor, OhplGa5Sink8 sink, void *name_ctx, void *func_ctx);
	void ohpl_a7_timer_finish(OhplA7Ctx *m, size_t cursor);
	void ohpl_a7_timers_end(OhplA7Ctx *m);
	size_t ohpl_a7_timer_len(OhplA7Ctx *m);
	bool ohpl_a7_timer_at(OhplA7Ctx *m, size_t i, OhplGa5Sink8 sink, void *name_ctx, void *func_ctx, float *count, bool *destroy_me);

	void ohpl_a7_use_add(OhplA7Ctx *m, void *p, const char *name, size_t name_n, const char *item, size_t item_n, const char *entity, size_t entity_n);
	size_t ohpl_a7_use_len(OhplA7Ctx *m);
	void *ohpl_a7_use_at(OhplA7Ctx *m, size_t i);
	bool ohpl_a7_use_remove_name(OhplA7Ctx *m, const char *name, size_t n);
	void ohpl_a7_use_remove_ptr(OhplA7Ctx *m, void *p, const char *name, size_t n);
	void *ohpl_a7_use_find(OhplA7Ctx *m, const char *item, size_t item_n, const char *entity, size_t entity_n);
	void ohpl_a7_use_clear(OhplA7Ctx *m);

	void ohpl_a7_dissolve_add(OhplA7Ctx *m, void *obj, float time);
	void ohpl_a7_dissolve_update(OhplA7Ctx *m, float step, OhplA7DissolveFn cb, void *ctx);
	void ohpl_a7_lamp_add(OhplA7Ctx *m, void *conn, const void *light);
	void ohpl_a7_lamp_each(OhplA7Ctx *m, OhplA7PtrFn cb, void *ctx);
	void ohpl_a7_start_add(OhplA7Ctx *m, void *p, const char *name, size_t n);
	void *ohpl_a7_start_find(OhplA7Ctx *m, const char *name, size_t n);
	void *ohpl_a7_start_first(OhplA7Ctx *m);
	size_t ohpl_a7_start_num(OhplA7Ctx *m);
	void *ohpl_a7_start_at(OhplA7Ctx *m, size_t i);
	void ohpl_a7_pos_add(OhplA7Ctx *m, void *p, const char *name, size_t n);
	void *ohpl_a7_pos_find(OhplA7Ctx *m, const char *name, size_t n);

	OhplA7Area *ohpl_a7_area_new(OhplA7Ctx *m, int kind);
	void ohpl_a7_area_free(OhplA7Ctx *m, OhplA7Area *a);
	size_t ohpl_a7_area_pod_len(const OhplA7Area *a);
	OhplA7Base *ohpl_a7_area_base(OhplA7Area *a);
	void *ohpl_a7_area_pod(OhplA7Area *a);
	void ohpl_a7_area_attach(OhplA7Area *a, const OhplGa5Vec3 *body, const OhplGa5Vec3 *parent);
	void ohpl_a7_area_follow(OhplA7Area *a, const OhplGa5Vec3 *parent, const OhplGa5Vec3 *body, OhplGa5Vec3 *out);
	bool ohpl_a7_area_can_interact(OhplA7Area *a, bool has_callback);
	bool ohpl_a7_area_on_interact(OhplA7Area *a, bool has_callback);
	int ohpl_a7_area_crosshair(OhplA7Area *a, bool has_callback, int custom, const int (*tbl)[4]);
	bool ohpl_a7_area_tick(OhplA7Area *a, float step, float engine_step);
	void ohpl_a7_area_examine(OhplA7Area *a, float sanity, OhplGa5ExamineOut *out);
	const char *ohpl_a7_area_str(const OhplA7Area *a, size_t idx, size_t *len);
	void ohpl_a7_area_ladder_setup(OhplA7Area *a, const OhplGa5Mtx *world, float pos_y, float size_y);
	void ohpl_a7_area_ladder_start(OhplA7Area *a, const OhplGa5Vec3 *area_pos, const OhplGa5Vec3 *char_size, float char_y, OhplGa5Vec3 *out);
	void ohpl_a7_area_liquid_setup(OhplA7Area *a, float pos_y, float size_y);
	void ohpl_a7_area_update(OhplA7Area *a, void *shell, OhplA7Ctx *ctx, float step);
	bool ohpl_a7_area_init(OhplA7Area *a, const char *name, size_t name_n, const OhplA7Vt *vt);
	void ohpl_a7_rope_load(OhplA7Area *a, const OhplGa5Vec3 *start, const OhplA7Vt *vt);
	void ohpl_a7_area_load_vars(OhplA7Area *a, const OhplA7Vt *vt);
	void ohpl_a7_area_save(OhplA7Area *a, void *data, const OhplA7Vt *vt);
	void ohpl_a7_area_load(OhplA7Area *a, const void *data, const OhplA7Vt *vt);
	size_t ohpl_a7_area_sticky_attached(const OhplA7Area *a);
	void ohpl_a7_area_sticky_attach_body(OhplA7Area *a, void *body);
	void ohpl_a7_area_sticky_detach(OhplA7Area *a, void *shell, OhplA7Ctx *ctx);
	bool ohpl_a7_sticky_allow(bool set, bool value);
	bool ohpl_a7_area_sticky_can_detach(OhplA7Area *a);
	void ohpl_a7_area_sticky_attach(OhplA7Area *a, const OhplGa5Mtx *start, bool gravity, float mass, int entity_saved);
	bool ohpl_a7_area_sticky_step(OhplA7Area *a, const OhplGa5Mtx *area_world, const OhplGa5Vec3 *area_pos, const OhplGa5Vec3 *mass_centre,
									const OhplGa5Vec3 *attached_pos, float step, OhplGa5StickyOut *out);
}

namespace ohpl_a7
{
// Walks a list of the Rust-owned map (cursor); same HasNext/Next/PeekNext as cSTLIterator.
template<class T>
class Iterator
{
public:
	Iterator(OhplA7Ctx *apCtx, unsigned alWhich) : mpCtx(apCtx), mWhich(alWhich), mCur(ohpl_a7_cur_begin(apCtx, alWhich)) {}

	bool HasNext() { return ohpl_a7_cur_get(mpCtx, mWhich, mCur) != NULL; }

	T Next()
	{
		T val = static_cast<T>(ohpl_a7_cur_get(mpCtx, mWhich, mCur));
		mCur = ohpl_a7_cur_next(mpCtx, mWhich, mCur);
		return val;
	}

	T PeekNext() { return static_cast<T>(ohpl_a7_cur_get(mpCtx, mWhich, mCur)); }

private:
	OhplA7Ctx *mpCtx;
	unsigned mWhich;
	uint64_t mCur;
};

// The position advances after the body, so an element appended during the body is still visited.
template<class CONT>
class Walk
{
public:
	typedef typename CONT::value_type V;

	Walk(OhplA7Ctx *apCtx, unsigned alWhich) : mpCtx(apCtx), mWhich(alWhich), mCur(ohpl_a7_cur_begin(apCtx, alWhich)) {}

	bool Has() const { return ohpl_a7_cur_get(mpCtx, mWhich, mCur) != NULL; }

	V Get() const { return static_cast<V>(ohpl_a7_cur_get(mpCtx, mWhich, mCur)); }

	void Advance() { mCur = ohpl_a7_cur_next(mpCtx, mWhich, mCur); }

private:
	OhplA7Ctx *mpCtx;
	unsigned mWhich;
	uint64_t mCur;
};

inline const char *Data(const std::string &s) { return s.data(); }
inline size_t Size(const std::string &s) { return s.size(); }
}

#define OHPL_A7_S(s) (s).data(), (s).size()

#endif
