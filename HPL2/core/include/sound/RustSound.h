#ifndef HPL_RUST_SOUND_H
#define HPL_RUST_SOUND_H

// C ABI of rust/src/sound/ffi.rs (layouts: rust/src/sound/{channel,entry,music,play}.rs).

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "system/RustXml.h"

namespace hpl { class iSoundChannel; }

struct OhplSoundChannelOps
{
	void (*play)(void*);
	void (*stop)(void*);
	void (*set_paused)(void*, bool);
	bool (*paused)(void*);
	bool (*is_playing)(void*);
	bool (*stop_used)(void*);
	float (*speed)(void*);
	void (*set_speed)(void*, float);
	float (*volume)(void*);
	void (*set_volume)(void*, float);
	bool (*looping)(void*);
	void (*set_looping)(void*, bool);
	void (*set_3d)(void*, bool);
	void (*set_priority)(void*, int);
	void (*set_priority_modifier)(void*, int);
	void (*set_affected_by_env)(void*, bool);
	bool (*position_is_relative)(void*);
	void (*set_position_is_relative)(void*, bool);
	void (*position)(void*, hpl::cVector3f*);
	void (*set_position)(void*, const hpl::cVector3f*);
	void (*rel_position)(void*, hpl::cVector3f*);
	void (*set_rel_position)(void*, const hpl::cVector3f*);
	float (*min_distance)(void*);
	void (*set_min_distance)(void*, float);
	float (*max_distance)(void*);
	void (*set_max_distance)(void*, float);
	float (*block_volume_mul)(void*);
	double (*elapsed_time)(void*);
	double (*total_time)(void*);
	void (*set_elapsed_time)(void*, double);
};

struct OhplSoundHostOps
{
	float (*global_speed)(void*, uint32_t);
	float (*global_volume)(void*, uint32_t);
	bool (*silent)(void*);
	bool (*is_blocked)(void*, const hpl::cVector3f*);
	void (*listener_position)(void*, hpl::cVector3f*);
	void (*listener_matrix)(void*, hpl::cMatrixf*);
};

struct OhplSoundEntryState
{
	float normal_volume;
	float volume_mul;
	float volume_fade_dest;
	float volume_fade_speed;
	float normal_speed;
	float speed_mul;
	float speed_fade_dest;
	float speed_fade_speed;
	float block_mul;
	float block_fade_dest;
	float block_fade_speed;
	uint32_t entry_type;
	bool stop_after_fade_out;
	bool first_time;
	bool stop_disabled;
	bool is_3d;
	bool stream;
};

struct OhplSoundPlayParams
{
	hpl::cVector3f pos;
	float min_dist;
	float max_dist;
	int priority_modifier;
	uint32_t entry_type;
	bool looping;
	bool relative;
	bool is_3d;
	bool silent;
};

struct OhplSoundProps
{
	float volume;
	float max_distance;
	float min_distance;
	float block_volume_mul;
	float random;
	float interval;
	int priority;
	bool use_3d;
	bool looping;
	bool stream;
	bool blockable;
	bool fade_start;
	bool fade_stop;
};

struct OhplSntSink
{
	void* ctx;
	void (*add_name)(void*, uint32_t, const unsigned char*, size_t);
};

struct OhplMusicSong
{
	hpl::iSoundChannel* mpStream;
	float mfMaxVolume;
	float mfVolume;
	float mfVolumeAdd;
	bool mbLoop;
};

struct OhplMusicHostOps
{
	const OhplSoundChannelOps* channel;
	OhplMusicSong* (*new_song)(void*);
	void (*delete_song)(void*, OhplMusicSong*);
	void* (*create_stream)(void*, const unsigned char*, size_t, bool);
	void (*set_song_name)(void*, OhplMusicSong*, const unsigned char*, size_t);
	void (*set_lock)(void*, void*, const unsigned char*, size_t, float, bool);
};

struct OhplMusic;

static_assert(sizeof(OhplSoundEntryState) == 56 && offsetof(OhplSoundEntryState, entry_type) == 44, "OhplSoundEntryState layout");
static_assert(sizeof(OhplSoundPlayParams) == 32 && offsetof(OhplSoundPlayParams, entry_type) == 24, "OhplSoundPlayParams layout");
static_assert(sizeof(OhplSoundProps) == 36 && offsetof(OhplSoundProps, priority) == 24, "OhplSoundProps layout");
static_assert(sizeof(OhplMusicSong) == 24 && offsetof(OhplMusicSong, mfMaxVolume) == 8, "OhplMusicSong layout");

extern "C" {
	int ohpl_snt_load(const OhplXmlDoc* doc, bool loaded, const OhplSntSink* sink, OhplSoundProps* props);
	int ohpl_snd_random_index(int size, int* previous, bool skipPrevious);
	int ohpl_snd_name_index(int size, int idx);

	void ohpl_snd_entry_init(OhplSoundEntryState* state, float volume, uint32_t type, bool is3d, bool stream);
	int ohpl_snd_entry_update(OhplSoundEntryState* state, float dt, const OhplSoundChannelOps* chOps, void* ch, const OhplSoundHostOps* hostOps, void* handler);
	void ohpl_snd_entry_stop(OhplSoundEntryState* state, const OhplSoundChannelOps* chOps, void* ch);
	void ohpl_snd_entry_fade_volume(OhplSoundEntryState* state, float dest, float speed);
	void ohpl_snd_entry_fade_speed(OhplSoundEntryState* state, float dest, float speed);
	void ohpl_snd_entry_fade_out(OhplSoundEntryState* state, float speed);
	void ohpl_snd_entry_fade_in(OhplSoundEntryState* state, float volumeMul, float speed);

	int ohpl_snd_play_priority(const hpl::cVector3f* pos, const hpl::cVector3f* listener, float minDist, float maxDist, bool is3d, bool relative);
	void ohpl_snd_play_setup(const OhplSoundPlayParams* params, int distPriority, const hpl::cMatrixf* listener, const OhplSoundChannelOps* chOps, void* ch);

	OhplMusic* ohpl_music_new();
	void ohpl_music_free(OhplMusic* m, const OhplMusicHostOps* ops, void* ctx);
	bool ohpl_music_play(OhplMusic* m, const OhplMusicHostOps* ops, void* ctx, const unsigned char* name, size_t len, float volume, float fadeStep, bool loop, bool resume);
	void ohpl_music_stop(OhplMusic* m, const OhplMusicHostOps* ops, void* ctx, float fadeStep);
	void ohpl_music_set_paused(OhplMusic* m, const OhplMusicHostOps* ops, void* ctx, bool paused);
	void ohpl_music_lock(OhplMusic* m, void* lock);
	void ohpl_music_fade_volume_mul(OhplMusic* m, float dest, float speed);
	void ohpl_music_set_volume_mul(OhplMusic* m, float mul);
	float ohpl_music_volume_mul(const OhplMusic* m);
	OhplMusicSong* ohpl_music_main_song(const OhplMusic* m);
	void ohpl_music_reset_resume(OhplMusic* m);
	void ohpl_music_update(OhplMusic* m, const OhplMusicHostOps* ops, void* ctx, float dt);
}

#endif
