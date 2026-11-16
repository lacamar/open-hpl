#ifndef LUX_RUST_LUX_MAP_H
#define LUX_RUST_LUX_MAP_H

// C ABI of rust/src/luxmap/. Texts are (pointer, length) in; results leave through sinks.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct OhplGa5Plane
{
	float a, b, c, d;
};

struct OhplGa5Vec3
{
	float x, y, z;
};

struct OhplGa5Mtx
{
	float m[4][4];
};

struct OhplGa5BuoyIn
{
	bool inside, allowed, active, hasWaves;
};

struct OhplGa5BuoyOut
{
	bool deactivate, setup, waves;
};

struct OhplGa5StickyIn
{
	OhplGa5Mtx start, areaWorld;
	OhplGa5Vec3 areaPos, massCentre, attachedPos;
	float poseTime, step, setTime;
	bool moveBody, rotateBody;
};

struct OhplGa5StickyOut
{
	OhplGa5Mtx mtx;
	OhplGa5Vec3 pos;
	float setTime;
	int mode;
	bool done;
};

struct OhplGa5ExamineIn
{
	bool hasCat, hasInsaneCat, hasInsaneEntry, hasSound, hasInsaneSound;
};

struct OhplGa5ExamineOut
{
	bool showText, insaneText, playSound, insaneSound;
};

struct OhplGa5LadderSetup
{
	OhplGa5Vec3 forward;
	float maxY, minY;
};

struct OhplGa5PlayerStart
{
	OhplGa5Vec3 pos;
	float angle;
};

struct OhplGa5SubMesh
{
	bool parentIsBody, hasMaterial, translucent, shadowCaster;
};

enum { kOhplGa5AttachNone, kOhplGa5AttachSetMatrix, kOhplGa5AttachSetPosition };
enum { kOhplGa5CharNone, kOhplGa5CharEnter, kOhplGa5CharLeave };
enum { kOhplGa5InsideProceed, kOhplGa5InsideSkip, kOhplGa5InsideForgetLast };
enum { kOhplGa5RopeReturn, kOhplGa5RopeRelink, kOhplGa5RopeCreate };
enum { kOhplGa5ScriptNone, kOhplGa5ScriptPlain, kOhplGa5ScriptCompressed };
enum { kOhplGa5EnterStart = 1, kOhplGa5EnterCompletion = 2, kOhplGa5EnterEnter = 4 };
enum { kOhplGa5ListEnemy = 1, kOhplGa5ListSticky = 2 };
enum
{
	kOhplGa5CompleteNone, kOhplGa5CompletePuzzle, kOhplGa5CompleteNote, kOhplGa5CompleteDiary,
	kOhplGa5CompleteChest, kOhplGa5CompleteFlashback,
};
enum { kOhplGa5EventEscapeArtist = 1, kOhplGa5EventDescendant = 2, kOhplGa5EventSanity = 4 };
enum
{
	kOhplGa5SetLeave = 1, kOhplGa5SetDestroyWorldEntities, kOhplGa5SetMapLeave, kOhplGa5SetAssign,
	kOhplGa5SetCreateWorldEntities, kOhplGa5SetMapEnter, kOhplGa5SetPlacePlayer, kOhplGa5SetAutoCheckpoint,
	kOhplGa5SetEnter, kOhplGa5SetViewWorld, kOhplGa5SetViewNoWorld,
};

typedef void (*OhplGa5Sink8)(void *, const unsigned char *, size_t);
typedef bool (*OhplGa5FitsFn)(void *, const OhplGa5Vec3 *);

namespace ohpl_ga5
{
inline void Collect(void *ctx, const unsigned char *p, size_t n)
{
	static_cast<std::string *>(ctx)->assign(reinterpret_cast<const char *>(p), n);
}
}

extern "C" {
	void ohpl_ga5_call1(const char *func, size_t func_n, const char *name, size_t name_n, OhplGa5Sink8 sink, void *ctx);
	void ohpl_ga5_call2(const char *func, size_t func_n, const char *a, size_t a_n, const char *b, size_t b_n, OhplGa5Sink8 sink, void *ctx);
}

namespace ohpl_ga5
{
inline std::string Call1(const std::string &asFunc, const std::string &asName)
{
	std::string s;
	ohpl_ga5_call1(asFunc.data(), asFunc.size(), asName.data(), asName.size(), Collect, &s);
	return s;
}
inline std::string Call2(const std::string &asFunc, const std::string &asA, const std::string &asB)
{
	std::string s;
	ohpl_ga5_call2(asFunc.data(), asFunc.size(), asA.data(), asA.size(), asB.data(), asB.size(), Collect, &s);
	return s;
}
}

extern "C" {
	// area
	bool ohpl_ga5_body_skipped(bool collide, bool active, float mass, bool is_char);
	float ohpl_ga5_liquid_surface_y(float pos_y, float size_y);
	float ohpl_ga5_wave_surface_y(float time, float freq, float px, float pz, float amp, float surface_y);
	int ohpl_ga5_liquid_char(bool is_player, bool has_enemy, bool inside, bool in_water);
	int ohpl_ga5_sticky_inside(bool inside, bool is_last);
	bool ohpl_ga5_sticky_after_callback(bool has_callback, bool allow);
	int ohpl_ga5_rope_after_world(bool created, bool nothing_linked);
	int ohpl_ga5_rope_max_segments(float max_len, float segment);
	void ohpl_ga5_wave_plane(float surface_y, OhplGa5Plane *out);
	void ohpl_ga5_liquid_body(const OhplGa5BuoyIn *i, float dist_sqr, float max_dist_sqr, OhplGa5BuoyOut *out);
	bool ohpl_ga5_sticky_name(const char *attach, size_t attach_n, const char *body, size_t body_n, bool *allow);
	bool ohpl_ga5_ladder_find_start(OhplGa5Vec3 *start, float max_y, OhplGa5FitsFn fits, void *ctx);
	void ohpl_ga5_player_start(const OhplGa5Mtx *m, OhplGa5PlayerStart *out);
	void ohpl_ga5_node_up(const OhplGa5Vec3 *t, OhplGa5Vec3 *out);

	// map

	// handler
	void ohpl_ga5_map_name(const char *file, size_t file_n, OhplGa5Sink8 sink, void *ctx);

}

#endif
