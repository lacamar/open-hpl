#ifndef LUX_RUST_FX_A12B_H
#define LUX_RUST_FX_A12B_H

// C ABI of rust/src/luxfx/ffi12.rs and host12.rs: the music and insanity handlers live in Rust,
// the C++ shells hold a handle and forward. OhplA12bInitHost installs the engine side.

#include <cstddef>
#include <cstdint>
#include "RustLuxBase.h"

struct OhplA12bMusicState
{
	int32_t maxPrio, slotCount, curPrio;
	uint32_t closePlaying;
	uint32_t playing[2];
	float volumeMul;
};

struct OhplA12bSlot
{
	uint32_t loop, resume, special;
	float volume;
};

struct OhplA12bEventInfo
{
	float maxSanity;
	uint32_t used, over;
};

// Mirrors MainNums in rust/src/luxbase/ffi.rs; owned by the Rust config handler.
struct OhplA12bMainNums
{
	bool loadDebugMenu, firstStart, forceCache, fullscreen, vsync, adaptiveVsync, fastPhysics, fastStatic, fastEntity;
	bool occlusionTestLights, shadowsActive, worldReflection, refraction, edgeSmooth, ssaoActive, parallaxEnabled, forceSm34Off;
	int32_t screenWidth, screenHeight, display, shadowQuality, shadowRes, guiScale, ssaoSamples, ssaoResolution, parallaxQuality;
	int32_t textureQuality, textureFilter, soundDevId, maxSoundChannels, soundStreamBuffers, soundStreamBufferSize;
	float paperWhite, peakNits, textureAnisotropy;
	bool createCompressed;
};

typedef void (*OhplA12bStrSink)(void*, const unsigned char*, size_t);

struct OhplA12bArgs;

extern "C"
{
	void ohpl_a12b_set_host(size_t (*apHost)(int, OhplA12bArgs*), int alLogging);

	uint64_t ohpl_a12b_music_new();
	void ohpl_a12b_music_free(uint64_t h);
	void ohpl_a12b_music_reset(uint64_t h);
	void ohpl_a12b_music_update(uint64_t h, float dt);
	void ohpl_a12b_music_play(uint64_t h, const char *file, size_t n, int loop, float volume, float fade, int prio, int resume, int special);
	void ohpl_a12b_music_stop(uint64_t h, float fade, int prio);
	void ohpl_a12b_music_on_map(uint64_t h);
	int ohpl_a12b_music_enemy(uint64_t h, int op, int kind, size_t enemy);
	int ohpl_a12b_music_state(uint64_t h, OhplA12bMusicState *out);
	void ohpl_a12b_music_set_state(uint64_t h, int curPrio, int closePlaying, int attack, int search);
	int ohpl_a12b_music_slot(uint64_t h, int idx, OhplA12bSlot *out, OhplA12bStrSink sink, void *ctx);
	void ohpl_a12b_music_set_slot(uint64_t h, int idx, const OhplA12bSlot *rec, const char *file, size_t n);
	size_t ohpl_a12b_music_enemies(uint64_t h, int kind, size_t *out, size_t cap);

	uint64_t ohpl_a12b_cfg_new();
	void ohpl_a12b_cfg_free(uint64_t h);
	OhplA12bMainNums *ohpl_a12b_cfg_nums(uint64_t h);
	void ohpl_a12b_cfg_load(uint64_t h, void *ctx, OhplGa9Call call, const unsigned char *defLang, size_t n);
	void ohpl_a12b_cfg_save(uint64_t h, void *ctx, OhplGa9Call call);
	void ohpl_a12b_cfg_str(uint64_t h, int which, OhplA12bStrSink sink, void *ctx);
	void ohpl_a12b_cfg_set_str(uint64_t h, int which, const char *p, size_t n);
	int ohpl_a12b_cfg_restart(uint64_t h, int op);

	uint64_t ohpl_a12b_ins_new();
	void ohpl_a12b_ins_free(uint64_t h);
	void ohpl_a12b_ins_reset(uint64_t h);
	void ohpl_a12b_ins_update(uint64_t h, float dt);
	void ohpl_a12b_ins_draw(uint64_t h);
	void ohpl_a12b_ins_start(uint64_t h, int mode, int idx, const char *name, size_t n);
	void ohpl_a12b_ins_stop(uint64_t h);
	int ohpl_a12b_ins_possible(uint64_t h);
	void ohpl_a12b_ins_reload(uint64_t h);
	int ohpl_a12b_ins_count(uint64_t h);
	int ohpl_a12b_ins_current(uint64_t h);
	float ohpl_a12b_ins_new_count(uint64_t h);
	void ohpl_a12b_ins_set_new_count(uint64_t h, float v);
	int ohpl_a12b_ins_event(uint64_t h, int idx, OhplA12bEventInfo *out, OhplA12bStrSink sink, void *ctx);
	void ohpl_a12b_ins_set_used(uint64_t h, int idx, int used);
	void ohpl_a12b_ins_set(uint64_t h, int op, const char *name, size_t n);
	void ohpl_a12b_ins_sets(uint64_t h, OhplA12bStrSink sink, void *ctx);
}

void OhplA12bInitHost();

#endif
