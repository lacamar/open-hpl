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
	void* world;
	uint64_t (*now_ms)(void*);
	void (*object_done)(void*);
	void (*warn)(void*, const char*);
	void (*log)(void*, const char*);
	int (*read_file)(void*, const wchar_t*, size_t, void (*)(void*, const char*, size_t), void*);
	void (*dir_light)(void*, const OhplHpmDirLight*);
	void (*env_particles)(void*, const OhplHpmEnvParticles*);
	void (*post_effects)(void*, const OhplHpmPostEffects*);
	void (*sky_box)(void*, const OhplHpmSkyBox*);
	bool (*static_object)(void*, const OhplHpmStaticObject*);
	bool (*plane)(void*, const OhplHpmPlane*);
	bool (*entity)(void*, const OhplHpmEntity*, const OhplHpmEl*);
	bool (*area)(void*, const OhplHpmArea*, const OhplHpmEl*);
	bool (*decal)(void*, const OhplHpmDecal*, const OhplHpmEl*);
	int (*detail_meshes)(void*, const OhplHpmDetailMeshes*);
	void (*exposure_area)(void*, const OhplHpmExposureArea*);
	void (*terrain)(void*, const OhplHpmTerrain*);
};

extern "C" {
	void ohpl_hpm_parse(const OhplHpmVt* vt, const wchar_t* base, size_t n);
	void ohpl_hpm_el_walk(const OhplHpmEl* el, const OhplHpmTreeSink* sink);
	void ohpl_hpm_report(const char* map, size_t n, int totalMs, void (*sink)(void*, const char*, size_t, const char*, size_t), void* ctx);
}

#endif
