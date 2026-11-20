#ifndef HPL_RUST_LD_TERRAIN_H
#define HPL_RUST_LD_TERRAIN_H

// C ABI of rust/src/world_hpm/ld_ffi.rs: SOMA terrain geometry and undergrowth baking.

#include <cstddef>
#include <cstdint>

struct OhplLdTerrain;
struct OhplLdUg;

struct OhplLdPatch
{
	uint32_t numVerts;
	const float *pos, *normal, *uv0, *blendUv;
	uint32_t numIdx;
	const uint32_t* idx;
};

struct OhplLdUgMat
{
	const float *pos, *uv;
	uint32_t numVerts;
	const uint32_t* idx;
	uint32_t numIdx;
	float subDiv[2], minColor[4], maxColor[4], density, align, baseSize[3], minSizeMul[3], maxSizeMul[3];
};

struct OhplLdUgArea
{
	int32_t mat;
	bool sub;
	float maxInfluence, fadeBorder;
	bool circle;
	float center[2], radius;
	const float* points;
	size_t numPoints;
};

struct OhplLdUgGroup
{
	int32_t mat, cell;
	uint32_t numVerts;
	const float *pos, *normal, *color, *uv0, *uv1;
	uint32_t numIdx;
	const uint32_t* idx;
};

extern "C"
{
	OhplLdTerrain* ohpl_ld_terrain_new(const uint8_t* data, size_t len, int32_t size, float unit, float maxHeight);
	bool ohpl_ld_terrain_set_hole(OhplLdTerrain* t, const uint8_t* data, size_t len, int32_t channel);
	bool ohpl_ld_terrain_has_hole(const OhplLdTerrain* t);
	void ohpl_ld_terrain_patch(OhplLdTerrain* t, int32_t x0, int32_t z0, int32_t w, int32_t h, float tile, OhplLdPatch* out);
	void ohpl_ld_terrain_sink_holes(OhplLdTerrain* t);
	const uint16_t* ohpl_ld_terrain_elev(const OhplLdTerrain* t);
	const float* ohpl_ld_terrain_heights(const OhplLdTerrain* t);
	void ohpl_ld_terrain_free(OhplLdTerrain* t);
	OhplLdUg* ohpl_ld_ug_build(const OhplLdTerrain* t, float grid, size_t numMats, const OhplLdUgMat* mats, size_t numAreas, const OhplLdUgArea* areas);
	size_t ohpl_ld_ug_num(const OhplLdUg* g);
	bool ohpl_ld_ug_group(const OhplLdUg* g, size_t i, OhplLdUgGroup* out);
	void ohpl_ld_ug_free(OhplLdUg* g);
}

#endif
