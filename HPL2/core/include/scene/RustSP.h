#ifndef HPL_RUST_SP_H
#define HPL_RUST_SP_H

// C ABI of rust/src/scene/sp_sys.rs. OhplSPPub mirrors its SpPub, the first member of the Rust system.

#include "math/RustTypes.h"
#include "scene/RustSceneH12.h"
#include <cstddef>
#include <cstdint>

struct OhplSPSys;
struct OhplSPData;
struct OhplSPE;

namespace hpl {
	struct OhplSPPub
	{
		cColor mColor, mLightColor;
		float mfBrightness, mfLightBrightness;
		float mfMinFadeDistanceStart, mfMinFadeDistanceEnd, mfMaxFadeDistanceStart, mfMaxFadeDistanceEnd;
		cVector3f mvDataSize;
		void *mpLight, *mpData, *mpManager;
		bool mbRemoveWhenDead, mbIsVisible, mbFadeAtDistance, mbFirstUpdate;
	};

	static_assert(sizeof(OhplSPPub) == 104, "SP pub layout");

	struct OhplSPFrustum
	{
		bool mbInvertsCull;
		cVector3f mvOrigin;
		cMatrixf m_mtxView;
	};

	static_assert(sizeof(OhplSPFrustum) == 80, "SP frustum layout");

	// C++ objects behind an emitter; see rust/src/scene/sp_emit.rs (Host).
	struct OhplSPEHost
	{
		void* (*vb_create)(void *apGfx, uint32_t alMax);
		void (*vb_index)(void *apVb, int32_t alIdx);
		void (*vb_add)(void *apVb, int32_t alKind, float x, float y, float z, float w);
		void (*vb_compile)(void *apVb);
		void (*vb_free)(void *apVb);
		float* (*vb_array)(void *apVb, int32_t alKind);
		int32_t (*vb_stride)(void *apVb);
		void (*vb_set_count)(void *apVb, int32_t alNum);
		void (*vb_update)(void *apVb, bool abWithTex);
		bool (*active)(void *apCtx);
		bool (*same_frame)(void *apCtx);
		int32_t (*matrix_count)(void *apCtx);
		const cMatrixf* (*world_matrix)(void *apCtx);
		const cMatrixf* (*local_matrix)(void *apCtx);
		void (*world_pos)(void *apCtx, cVector3f *apOut);
		void (*transform_updated)(void *apCtx);
		void (*bv_set)(void *apCtx, const cVector3f *apMin, const cVector3f *apMax);
		void (*bv_transform)(void *apCtx, const cMatrixf *a_mtx);
		void (*apply)(void *apCtx, int32_t alOp, bool abArg);
		const cMatrixf* (*ps_world_matrix)(void *apPs);
		void (*ps_center)(void *apPs, cVector3f *apOut);
		void (*frustum)(const void *apFrustum, OhplSPFrustum *apOut);
		void* (*mat_at)(void *apMats, int32_t alIdx);
		bool (*mat_add_blend)(void *apMat);
		bool (*collide)(void *apData, void *apWorld, const cVector3f *apStart, const cVector3f *apEnd,
						cVector3f *apNormal, cVector3f *apPoint);
	};

	extern "C" {
		OhplSPE* ohpl_spe_new(const OhplSPEHost*, void *apCtx, void *apGfx, void *apMats, void *apData,
							  const OhplH12PsDesc*, uint32_t alMaterials, float afWarmTime, float afWarmSteps);
		void ohpl_spe_free(OhplSPE*);
		void ohpl_spe_set_world(OhplSPE*, void*);
		void* ohpl_spe_vertex_buffer(const OhplSPE*);
		uint32_t ohpl_spe_num_particles(const OhplSPE*);
		bool ohpl_spe_is_dead(const OhplSPE*);
		bool ohpl_spe_is_dying(const OhplSPE*);
		void ohpl_spe_kill(OhplSPE*);
		void ohpl_spe_kill_instantly(OhplSPE*);
		void ohpl_spe_ctl(const OhplSPE*, OhplH12PeCtl*);
		void* ohpl_spe_material(const OhplSPE*);
		void ohpl_spe_update_logic(OhplSPE*, float afDt);
		bool ohpl_spe_update_gfx(OhplSPE*, const void *apFrustum);
		void ohpl_spe_update_bv(OhplSPE*);
		const cMatrixf* ohpl_spe_model_matrix(OhplSPE*, const void *apFrustum);
	}
}

extern "C" {
	OhplSPSys* ohpl_sp_new(void *apData, void *apShell);
	void ohpl_sp_free(OhplSPSys*);
	void ohpl_sp_add_emitter(OhplSPSys*, OhplSPE*);
	int32_t ohpl_sp_emitter_count(const OhplSPSys*);
	void* ohpl_sp_emitter(const OhplSPSys*, int32_t);
	void ohpl_sp_set_data_name(OhplSPSys*, const uint8_t*, size_t);
	const uint8_t* ohpl_sp_data_name(const OhplSPSys*, size_t*);
	void ohpl_sp_set_hpl3_color(bool);
	bool ohpl_sp_hpl3_color();
	void ohpl_sp_update_logic(OhplSPSys*, bool abActive, float afStep);
	void ohpl_sp_set_visible(OhplSPSys*, bool);
	void ohpl_sp_apply_all(OhplSPSys*, int32_t alOp, bool abArg);
	bool ohpl_sp_all(OhplSPSys*, int32_t alQuery);

	OhplSPData* ohpl_spd_new();
	void ohpl_spd_free(OhplSPData*);
	void ohpl_spd_add(OhplSPData*, void*);
	uint32_t ohpl_spd_count(const OhplSPData*);
	void* ohpl_spd_get(const OhplSPData*, int32_t);
}

#endif
