#ifndef SOMA_RUST_SOMA_LUX_H
#define SOMA_RUST_SOMA_LUX_H

// C ABI of rust/src/somalux/. Plans and decisions come back as values or ordered ops; C++ replays them.

#include "hpl.h"

#include <cstddef>
#include <cstdint>

struct OhplGs3Fader
{
	uint64_t mlId;
	float mfValue, mfStart, mfGoal, mfSpeed;
	bool mbReverseAtEnd, mbPaused;
};

struct OhplGs3Pause
{
	bool mbMusic, mbSounds, mbAmbience, mbSavingEnabled;
	float mfSavingDisabledTime;
};

struct OhplGs3Preload
{
	int mlPrio;
	unsigned int mlLastFrame;
	double mfDebtMs;
};

struct OhplGs3Begin
{
	bool mbUseTransfer, mbPreloaded, mbLookupArea;
};

struct OhplGs3Rel
{
	hpl::cMatrixf mtxRel, mtxInv;
	float mfYawRel, mfPitch;
	int mlActiveSize;
};

struct OhplGs3Arrival
{
	hpl::cVector3f mvFeet;
	float mfYaw;
	bool mbCrouching;
};

struct OhplGs3Tick
{
	bool mbDropOld, mbStart, mbStep;
	uint64_t mlBudgetMs;
};

struct OhplGs3Op
{
	int mlCode;
	uint32_t mlArg;
};

// OhplGs3Op codes
enum
{
	kOhplGs3OpSetScriptPaused, kOhplGs3OpClearUpdateWhenPaused, kOhplGs3OpWorldActive, kOhplGs3OpMusicResume, kOhplGs3OpMusicPause,
	kOhplGs3OpSoundsResumeAll, kOhplGs3OpSoundsPause, kOhplGs3OpSoundscapePaused, kOhplGs3OpReapplyVoicePauses,
};
enum { kOhplGs3OpCapacity = 12 };

extern "C" {
	// scriptable
	uint64_t ohpl_gs3_hash64(const unsigned char *s, size_t n);
	float ohpl_gs3_easing(int kind, float t);

	// map change and preload
	bool ohpl_gs3_same_map(const unsigned char *a, size_t an, const unsigned char *b, size_t bn);
	void ohpl_gs3_map_begin(bool start_empty, bool pending_transfer_empty, bool has_map, const unsigned char *preload, size_t preload_n,
							const unsigned char *map, size_t map_n, OhplGs3Begin *out);
	void ohpl_gs3_capture(const hpl::cMatrixf *area, const hpl::cVector3f *feet, float yaw, int active_size, bool has_camera, float pitch,
						  OhplGs3Rel *out);
	bool ohpl_gs3_carries(const hpl::cMatrixf *inv, const hpl::cVector3f *pos, const hpl::cVector3f *size);
	void ohpl_gs3_arrive(const hpl::cMatrixf *new_area, const OhplGs3Rel *rel, OhplGs3Arrival *out);
	int ohpl_gs3_after_load(bool loaded, bool captured, bool has_current_map);
	int ohpl_gs3_restore_prop(bool found, bool bodies_match, bool is_static);
	void ohpl_gs3_preload_tick(OhplGs3Preload *st, const unsigned char *want, size_t want_n, const unsigned char *loaded, size_t loaded_n,
							   bool running, uint32_t frame, OhplGs3Tick *out);
	void ohpl_gs3_preload_stepped(OhplGs3Preload *st, uint64_t elapsed_ms, uint64_t budget_ms);
	int ohpl_gs3_take_plan(const unsigned char *loaded, size_t loaded_n, const unsigned char *map, size_t map_n);
	int ohpl_gs3_taken(bool have_world, bool same);
	bool ohpl_gs3_preload_ready(const unsigned char *loaded, size_t loaded_n, bool running, const unsigned char *map, size_t map_n);

	// pause
	size_t ohpl_gs3_set_paused(OhplGs3Pause *st, bool script_paused, bool pause, bool music, bool sounds, bool ambience, uint32_t types,
							   OhplGs3Op *ops);
	void ohpl_gs3_tick_saving(OhplGs3Pause *st, float dt);
	uint32_t ohpl_gs3_frame_plan(bool has_game, bool has_map, bool map_active, bool uses_real_player, bool paused);
	uint32_t ohpl_gs3_post_plan(bool updated, bool scripts_held, bool paused, bool has_game, bool has_map, bool map_active);

	// game
	float ohpl_gs3_string_duration(float min, float start, float per_char, size_t chars);
}

static_assert(sizeof(OhplGs3Fader) == 32);
static_assert(sizeof(OhplGs3Pause) == 8);
static_assert(sizeof(OhplGs3Preload) == 16 && sizeof(hpl::cMatrixf) == 64 && sizeof(hpl::cVector3f) == 12);

#endif // SOMA_RUST_SOMA_LUX_H
