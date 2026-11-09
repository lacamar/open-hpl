#ifndef HPL_RUST_HPM_H
#define HPL_RUST_HPM_H

// C ABI of rust/src/world_hpm/ffi.rs.

#include <cstddef>
#include <cstdint>

#include "resources/HpmBuilder.h"

struct OhplHpmEl;

struct OhplHpmTreeSink
{
	void* ctx;
	void (*begin)(void*, const char*, size_t);
	void (*attr)(void*, const char*, size_t, const char*, size_t);
	void (*end)(void*);
};

struct OhplHpmVt
{
	void* ctx;
	uint64_t (*now_ms)(void*);
	void (*object_done)(void*);
	void (*warn)(void*, const char*);
	void (*log)(void*, const char*);
	int (*read_file)(void*, const wchar_t*, size_t, void (*)(void*, const char*, size_t), void*);
	void (*global_fog)(void*, const OhplHpmFog*);
	void (*distance_cull)(void*, const OhplHpmDistanceCull*);
	void (*dir_light)(void*, const OhplHpmDirLight*);
	void (*env_particles)(void*, const OhplHpmEnvParticles*);
	void (*post_effects)(void*, const OhplHpmPostEffects*);
	void (*sky_box)(void*, const OhplHpmSkyBox*);
	bool (*static_object)(void*, const OhplHpmStaticObject*);
	bool (*plane)(void*, const OhplHpmPlane*);
	bool (*entity)(void*, const OhplHpmEntity*, const OhplHpmEl*);
	bool (*light)(void*, const OhplHpmEl*);
	bool (*area)(void*, const OhplHpmArea*, const OhplHpmEl*);
	bool (*sound)(void*, const OhplHpmEl*, bool);
	bool (*decal)(void*, const OhplHpmDecal*, const OhplHpmEl*);
	bool (*billboard)(void*, const OhplHpmEl*);
	bool (*particle_system)(void*, const OhplHpmEl*, const char*);
	bool (*fog_area)(void*, const OhplHpmEl*);
	bool (*lens_flare)(void*, const OhplHpmEl*);
	void (*light_mask_box)(void*, const OhplHpmLightMask*);
	void (*connect_lights)(void*, int*, int*);
	void (*connect_light_masks)(void*);
	int (*detail_meshes)(void*, const OhplHpmDetailMeshes*);
	void (*exposure_area)(void*, const OhplHpmExposureArea*);
	void (*terrain)(void*, const OhplHpmTerrain*);
	void (*track_report)(void*, const char*, int, int, int, bool, const OhplHpmSkip*, size_t);
};

extern "C" {
	void ohpl_hpm_parse(const OhplHpmVt* vt, const wchar_t* base, size_t n);
	void ohpl_hpm_el_walk(const OhplHpmEl* el, const OhplHpmTreeSink* sink);
}

#endif
