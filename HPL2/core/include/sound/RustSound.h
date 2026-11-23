#ifndef HPL_RUST_SOUND_H
#define HPL_RUST_SOUND_H

// C ABI of rust/src/sound/own_ffi.rs. The handler, entries, entity data, sound entities and the
// music handler are Rust objects; the C++ classes are inline facades over these calls.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "system/RustXml.h"

struct OhplSndEventOps
{
	void (*play)(void*);
	void (*stop)(void*, bool);
	void (*fade_in)(void*, float);
	void (*fade_out)(void*, float);
	bool (*is_stopped)(void*);
	void (*update)(void*, float);
	void (*destroy)(void*);
};

struct OhplSndHost
{
	const OhplSndEventOps* event;
	int (*create_channel)(void* res, const unsigned char* name, size_t len, int priority, bool stream, bool* notEnough, void** data);
	void (*release_data)(void* data);
	bool (*is_blocked)(void* world, void* lowLevel, const hpl::cVector3f*);
	void (*listener_position)(void* lowLevel, hpl::cVector3f*);
	void (*listener_matrix)(void* lowLevel, hpl::cMatrixf*);
	void (*listener_axes)(void* lowLevel, float* forwardUp);
	uint64_t (*app_time)();
	void (*log)(int level, const unsigned char*, size_t);
	void (*preload)(void* res, const unsigned char*, size_t);
	void (*entity_pos)(void* entity, hpl::cVector3f*);
	const unsigned char* (*entity_name)(void* entity, size_t* len);
	bool (*world_is_emitter)(void* world);
	void (*data_release)(void* dataShell);
	void (*on_start)(void* entity);
	int (*create_stream)(void* res, const unsigned char*, size_t, bool loop, void** data);
	void (*set_lock)(void* lock, const unsigned char*, size_t, float, bool);
};

struct OhplSndHandler;
struct OhplSndRec;
struct OhplSndData;
struct OhplSndEnt;
struct OhplMusic;

struct OhplMusicSong
{
	void* mpStream;
	float mfMaxVolume;
	float mfVolume;
	float mfVolumeAdd;
	bool mbLoop;
};

struct OhplSndGsEntry;

static_assert(sizeof(OhplMusicSong) == 24 && offsetof(OhplMusicSong, mfMaxVolume) == 8, "OhplMusicSong layout");

namespace hpl { const OhplSndHost* OhplSndHostTable(); }

enum OhplSndCh
{
	OhplSndCh_Volume, OhplSndCh_Playing, OhplSndCh_Priority, OhplSndCh_Elapsed, OhplSndCh_Total, OhplSndCh_Underrun,
	OhplSndCh_Paused, OhplSndCh_Looping, OhplSndCh_Is3D, OhplSndCh_Relative, OhplSndCh_Pos, OhplSndCh_RelPos = 13,
	OhplSndCh_MinDist = 16, OhplSndCh_MaxDist
};

extern "C" {
	OhplSndHandler* ohpl_sndh_new(const OhplSndHost* host, void* lowLevel, void* resources);
	void ohpl_sndh_free(OhplSndHandler*);
	void ohpl_sndh_set_world(OhplSndHandler*, void* world);
	void* ohpl_sndh_lowlevel(OhplSndHandler*);
	void ohpl_sndh_set_silent(OhplSndHandler*, bool);
	bool ohpl_sndh_silent(OhplSndHandler*);
	float ohpl_sndh_global_volume(OhplSndHandler*, uint32_t type);
	float ohpl_sndh_global_speed(OhplSndHandler*, uint32_t type);
	void ohpl_sndh_update(OhplSndHandler*, float dt);
	OhplSndRec* ohpl_sndh_play(OhplSndHandler*, const unsigned char* name, size_t len, bool looping, float volume, const hpl::cVector3f* pos,
							   float minDist, float maxDist, uint32_t entryType, bool relative, bool is3d, int priorityModifier, bool stream, bool* notEnough);
	bool ohpl_sndh_stop(OhplSndHandler*, const unsigned char* name, size_t len);
	bool ohpl_sndh_is_playing(OhplSndHandler*, const unsigned char* name, size_t len);
	void ohpl_sndh_each(OhplSndHandler*, uint32_t mask, int op, float speed, bool disableStop);
	bool ohpl_sndh_is_valid(OhplSndHandler*, const OhplSndRec*, int id);
	size_t ohpl_sndh_count(OhplSndHandler*);
	OhplSndRec* ohpl_sndh_entry_at(OhplSndHandler*, size_t i);
	const unsigned char* ohpl_sndh_recent(OhplSndHandler*, size_t i, size_t* len);
	size_t ohpl_sndh_recent_count(OhplSndHandler*);
	OhplSndGsEntry* ohpl_sndh_gs_entry(OhplSndHandler*, size_t kind, int id, bool force);
	int ohpl_sndh_gs_create(OhplSndHandler*, size_t kind);
	bool ohpl_sndh_gs_exists(OhplSndHandler*, size_t kind, int id);
	void ohpl_sndh_gs_destroy(OhplSndHandler*, size_t kind, int id);
	float ohpl_sndh_gs_calc(OhplSndHandler*, size_t kind, uint32_t types, int calc, float def);
	int ohpl_sndh_gs_set(OhplSndHandler*, size_t kind, float val, uint32_t types, int id);
	int ohpl_sndh_gs_fade(OhplSndHandler*, size_t kind, float dest, float speed, uint32_t types, int id, bool destroy);

	const unsigned char* ohpl_sndent_name(OhplSndRec*, size_t* len);
	int ohpl_sndent_id(OhplSndRec*);
	void* ohpl_sndent_channel(OhplSndRec*);
	void* ohpl_sndch_data(void* channel);
	double ohpl_sndch_read(void* channel, int what);
	void ohpl_sndch_set_elapsed(void* channel, double time);
	void ohpl_sndch_stop(void* channel);
	void ohpl_sndent_set(OhplSndRec*, int what, float v);
	float ohpl_sndent_get(OhplSndRec*, int what);
	uint32_t ohpl_sndent_type(OhplSndRec*);
	void ohpl_sndent_stop(OhplSndRec*);
	void ohpl_sndent_set_paused(OhplSndRec*, bool);
	void ohpl_sndent_fade(OhplSndRec*, int what, float dest, float speed);

	OhplSndData* ohpl_sndd_new(void* shell);
	void ohpl_sndd_free(OhplSndData*);
	void ohpl_sndd_set_lazy(bool);
	float ohpl_sndd_get(OhplSndData*, int what);
	void ohpl_sndd_set(OhplSndData*, int what, float v);
	size_t ohpl_sndd_num(OhplSndData*, size_t type);
	const unsigned char* ohpl_sndd_name(OhplSndData*, size_t type, int idx, bool random, bool skipPrevious, size_t* len);
	void ohpl_sndd_put_name(OhplSndData*, size_t type, int idx, const unsigned char* name, size_t len);
	int ohpl_sndd_load(OhplSndData*, const OhplXmlDoc* doc, bool loaded);
	void ohpl_sndd_preload(OhplSndData*, const OhplSndHost* host, void* resources);

	OhplSndEnt* ohpl_sndse_new(OhplSndHandler*, OhplSndData*, void* dataShell, void* shell, void* world, bool removeWhenOver, int creationId);
	void ohpl_sndse_free(OhplSndEnt*);
	void ohpl_sndse_play(OhplSndEnt*, bool playStart);
	void ohpl_sndse_stop(OhplSndEnt*, bool playEnd);
	void ohpl_sndse_fade_in(OhplSndEnt*, float speed);
	void ohpl_sndse_fade_out(OhplSndEnt*, float speed);
	void ohpl_sndse_update(OhplSndEnt*, float dt);
	bool ohpl_sndse_is_stopped(OhplSndEnt*);
	int ohpl_sndse_flag(OhplSndEnt*, int what);
	void ohpl_sndse_set_force_gui(OhplSndEnt*, bool);
	float ohpl_sndse_get(OhplSndEnt*, int what);
	void ohpl_sndse_set(OhplSndEnt*, int what, float v);
	OhplSndRec* ohpl_sndse_entry(OhplSndEnt*, size_t type, bool check);
	void ohpl_sndse_set_event(OhplSndEnt*, void* event);
	void* ohpl_sndse_event(OhplSndEnt*);

	OhplMusic* ohpl_music_new(const OhplSndHost* host, void* resources);
	void ohpl_music_free(OhplMusic*);
	bool ohpl_music_play(OhplMusic*, const unsigned char* name, size_t len, float volume, float fadeStep, bool loop, bool resume);
	void ohpl_music_stop(OhplMusic*, float fadeStep);
	void ohpl_music_set_paused(OhplMusic*, bool);
	void ohpl_music_lock(OhplMusic*, void* lock);
	void ohpl_music_fade_volume_mul(OhplMusic*, float dest, float speed);
	void ohpl_music_set_volume_mul(OhplMusic*, float mul);
	float ohpl_music_volume_mul(OhplMusic*);
	OhplMusicSong* ohpl_music_main_song(OhplMusic*);
	const unsigned char* ohpl_music_song_name(OhplMusicSong*, size_t* len);
	void ohpl_music_reset_resume(OhplMusic*);
	void ohpl_music_update(OhplMusic*, float dt);
}

#endif
