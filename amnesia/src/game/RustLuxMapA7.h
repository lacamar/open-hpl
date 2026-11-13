#ifndef LUX_RUST_LUX_MAP_A7_H
#define LUX_RUST_LUX_MAP_A7_H

// C ABI of rust/src/luxmap/a7_*.rs: per-map arena for cLuxMap lists/timers/callbacks and area state
// blocks (OHPL_IMPL=rust). The blocks are bound to by reference in the C++ area shells.

#include <cstdlib>

#include "RustLuxMap.h"
#include "impl/RustBackends.h"

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
struct OhplA7Insanity { float checkTimeMin, checkTimeMax, checkCollisionCount; bool autoDisable; };
struct OhplA7Flashback { float checkCollisionCount; };
struct OhplA7Sign { unsigned char unused; };
struct OhplA7Ladder { OhplGa5Vec3 forward; float maxY, minY; };
struct OhplA7Liquid
{
	float density, linearViscosity, angularViscosity, waveAmp, waveFreq, playerSpeedMul, maxWaveDistanceSqr, timeCount;
	OhplGa5Plane surfacePlane;
	bool hasWaves;
};
struct OhplA7Sticky
{
	float poseTime, attachedBodyMass, setMtxTime;
	OhplGa5Mtx attachedStart;
	bool moveBody, rotateBody, checkCenterInArea, canDetach, attachedBodyGravity, attachedEntityFullGameSaved;
};
struct OhplA7Slime
{
	int slimeType;
	float minCheckAttackTime, maxCheckAttackTime, checkCollisionCount, minAttackDamage, maxAttackDamage, screenShakeAmount;
	bool disableAfterAttack;
};

static_assert(sizeof(OhplA7Base) == 96, "OhplA7Base");
static_assert(sizeof(OhplA7Script) == 1 && sizeof(OhplA7Examine) == 8 && sizeof(OhplA7Insanity) == 16, "pods");
static_assert(sizeof(OhplA7Flashback) == 4 && sizeof(OhplA7Sign) == 1 && sizeof(OhplA7Ladder) == 20, "pods");
static_assert(sizeof(OhplA7Liquid) == 52 && sizeof(OhplA7Sticky) == 84 && sizeof(OhplA7Slime) == 32, "pods");

enum
{
	kOhplA7Script, kOhplA7Examine, kOhplA7Insanity, kOhplA7Flashback, kOhplA7Sign,
	kOhplA7Ladder, kOhplA7Liquid, kOhplA7Sticky, kOhplA7Slime,
};
enum { kOhplA7Entity, kOhplA7Dissolve, kOhplA7Lamp, kOhplA7Start, kOhplA7Pos, kOhplA7UseItem };
enum { kOhplA7Entities, kOhplA7Enemies, kOhplA7Doomed, kOhplA7StickyList };

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
	void *ohpl_a7_lamp_find(OhplA7Ctx *m, const void *light);
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
	size_t ohpl_a7_pod_size(int kind);
	size_t ohpl_a7_base_size();
	OhplA7Base *ohpl_a7_area_base(OhplA7Area *a);
	void *ohpl_a7_area_pod(OhplA7Area *a);
	void ohpl_a7_area_attach(OhplA7Area *a, const OhplGa5Vec3 *body, const OhplGa5Vec3 *parent);
	void ohpl_a7_area_follow(OhplA7Area *a, const OhplGa5Vec3 *parent, const OhplGa5Vec3 *body, OhplGa5Vec3 *out);
	bool ohpl_a7_area_can_interact(OhplA7Area *a, bool has_callback);
	bool ohpl_a7_area_on_interact(OhplA7Area *a, bool has_callback);
	int ohpl_a7_area_crosshair(OhplA7Area *a, bool has_callback, int custom, const int (*tbl)[4]);
	bool ohpl_a7_area_tick(OhplA7Area *a, float step, float engine_step);
	void ohpl_a7_area_examine(OhplA7Area *a, const OhplGa5ExamineIn *i, float sanity, OhplGa5ExamineOut *out);
	void ohpl_a7_area_ladder_setup(OhplA7Area *a, const OhplGa5Mtx *world, float pos_y, float size_y);
	void ohpl_a7_area_ladder_start(OhplA7Area *a, const OhplGa5Vec3 *area_pos, const OhplGa5Vec3 *char_size, float char_y, OhplGa5Vec3 *out);
	void ohpl_a7_area_liquid_setup(OhplA7Area *a, float pos_y, float size_y);
	bool ohpl_a7_area_sticky_can_detach(OhplA7Area *a);
	void ohpl_a7_area_sticky_attach(OhplA7Area *a, const OhplGa5Mtx *start, bool gravity, float mass, int entity_saved);
	bool ohpl_a7_area_sticky_step(OhplA7Area *a, const OhplGa5Mtx *area_world, const OhplGa5Vec3 *area_pos, const OhplGa5Vec3 *mass_centre,
									const OhplGa5Vec3 *attached_pos, float step, OhplGa5StickyOut *out);
}

namespace ohpl_a7
{
// Walks a list of the Rust-owned map (cursor) or the std container; same HasNext/Next/PeekNext as cSTLIterator.
template<class T, class CONT, class IT>
class Iterator
{
public:
	Iterator(CONT *apCont) : mpCont(apCont), mIt(apCont->begin()), mpCtx(NULL), mWhich(0), mCur(0) {}
	Iterator(OhplA7Ctx *apCtx, unsigned alWhich) : mpCont(NULL), mpCtx(apCtx), mWhich(alWhich), mCur(ohpl_a7_cur_begin(apCtx, alWhich)) {}

	bool HasNext()
	{
		if(mpCtx) return ohpl_a7_cur_get(mpCtx, mWhich, mCur) != NULL;
		return mIt != mpCont->end();
	}

	T Next()
	{
		if(mpCtx)
		{
			T val = static_cast<T>(ohpl_a7_cur_get(mpCtx, mWhich, mCur));
			mCur = ohpl_a7_cur_next(mpCtx, mWhich, mCur);
			return val;
		}
		if(mIt == mpCont->end()) return NULL;
		T &temp = const_cast<T &>(*mIt);
		++mIt;
		return temp;
	}

	T PeekNext()
	{
		if(mpCtx) return static_cast<T>(ohpl_a7_cur_get(mpCtx, mWhich, mCur));
		if(mIt == mpCont->end()) return NULL;
		return *mIt;
	}

private:
	CONT *mpCont;
	IT mIt;
	OhplA7Ctx *mpCtx;
	unsigned mWhich;
	uint64_t mCur;
};

// One loop body for both paths: cursor on the Rust-owned list, or the std list's iterator. The
// position advances after the body, so an element appended during the body is still visited.
template<class CONT>
class Walk
{
public:
	typedef typename CONT::value_type V;

	Walk(OhplA7Ctx *apCtx, unsigned alWhich, CONT &aCont) : mpCtx(apCtx), mWhich(alWhich), mIt(aCont.begin()), mpCont(&aCont), mCur(0)
	{
		if(mpCtx) mCur = ohpl_a7_cur_begin(mpCtx, mWhich);
	}

	bool Has() const
	{
		if(mpCtx) return ohpl_a7_cur_get(mpCtx, mWhich, mCur) != NULL;
		return mIt != mpCont->end();
	}

	V Get() const
	{
		if(mpCtx) return static_cast<V>(ohpl_a7_cur_get(mpCtx, mWhich, mCur));
		return *mIt;
	}

	void Advance()
	{
		if(mpCtx) mCur = ohpl_a7_cur_next(mpCtx, mWhich, mCur);
		else ++mIt;
	}

private:
	OhplA7Ctx *mpCtx;
	unsigned mWhich;
	typename CONT::iterator mIt;
	CONT *mpCont;
	uint64_t mCur;
};

inline const char *Data(const std::string &s) { return s.data(); }
inline size_t Size(const std::string &s) { return s.size(); }
}

#define OHPL_A7_S(s) (s).data(), (s).size()

#endif
