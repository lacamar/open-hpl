#ifndef HPL_RUST_WORLD_H4_H
#define HPL_RUST_WORLD_H4_H

// C ABI of rust/src/world_hpm/h4_ffi.rs: the plans cHpmWorldBuilder executes.

#include "impl/RustBackends.h"
#include "math/MathTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplH4Xform
{
	hpl::cMatrixf world, body;
};

struct OhplH4LightMask
{
	float center[3], extent[3];
};

struct OhplH4Exposure
{
	hpl::cMatrixf invTransform;
	float halfSize[3];
};

struct OhplH4SubMesh
{
	bool isColliderMesh, isCharColliderMesh, hide;
};

struct OhplH4Cand
{
	uint32_t ent;
	int32_t cell[3];
	uint64_t mat;
	bool shadow, oneSided, culledDist, culledFog;
	uint32_t layoutOff, layoutLen;
	float v[9];
};

struct OhplH4CombineOut
{
	uint32_t* order;
	uint32_t* groupSizes;
	uint32_t numGroups;
	bool* destroy;
	uint32_t combinedSubs;
};

enum OhplH4FinishStep : uint32_t
{
	OhplH4Finish_FlushStaticBatches,
	OhplH4Finish_BuildShapeBodies,
	OhplH4Finish_CombineStaticMeshes,
	OhplH4Finish_ObjectDone,
	OhplH4Finish_CompileWorld,
};

extern "C"
{
	void ohpl_h4_static_xform(const float* rot, const float* scale, const float* pos, OhplH4Xform* out);
	void ohpl_h4_rot_trans(const float* rot, const float* pos, hpl::cMatrixf* out);
	bool ohpl_h4_is_combine_candidate(bool hasPose, bool poseApplied);
	void ohpl_h4_light_mask(const float* rot, const float* size, const float* scale, const float* worldPos, OhplH4LightMask* out);
	void ohpl_h4_exposure_area(const float* rot, const float* worldPos, const float* scale, OhplH4Exposure* out);
	float ohpl_h4_detail_mesh_radius(const float* mins, const float* maxs, size_t n);
	float ohpl_h4_detail_scale(bool hasRadius, float radius, float meshRadius);
	void ohpl_h4_detail_matrix(const float* quatWxyz, const float* pos, float scale, hpl::cMatrixf* out);
	void ohpl_h4_sub_mesh_plan(const char* name, OhplH4SubMesh* out);
	bool ohpl_h4_is_collider_name(const char* name);
	bool ohpl_h4_sub_mesh_batches(bool hasColliderMesh, bool isColliderMesh, const bool* colliderIsChar, size_t n);
	bool ohpl_h4_batch_must_flush(size_t batchIndices, size_t adding);
	void ohpl_h4_mesh_cell(const float* center, int32_t* out);
	void ohpl_h4_body_cell(const float* pos, int32_t* out);
	void ohpl_h4_combine_plan(const OhplH4Cand* cands, size_t n, const int32_t* layouts, size_t numLayout, const uint32_t* entSubNum,
							  size_t numEnts, OhplH4CombineOut* out);
	size_t ohpl_h4_finish_steps(bool noMeshCombine, OhplH4FinishStep* out);
}

namespace hpl
{
	inline bool OhplH4UseRust()
	{
		return OhplBackendRustRequested();
	}
}

#endif
