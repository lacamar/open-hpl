#ifndef HPL_RUST_POSTEFFECT_H
#define HPL_RUST_POSTEFFECT_H

// C ABI of rust/src/render/posteffect_ffi.rs. Slot constants mirror posteffect.rs.

#include <cstddef>
#include <cstdint>

#include "graphics/RustRender.h"

namespace hpl {

	struct OhplPePlan;

	enum eOhplPeOp
	{
		eOhplPeOp_FlatProjection,
		eOhplPeOp_BlendNone,
		eOhplPeOp_BlendAlpha,
		eOhplPeOp_ChannelRgba,
		eOhplPeOp_TextureRangeNull,
		eOhplPeOp_CompFb,
		eOhplPeOp_CompFbPos,
		eOhplPeOp_FinalFb,
		eOhplPeOp_BaseFb,
		eOhplPeOp_Program,
		eOhplPeOp_ClearColor,
		eOhplPeOp_Tex,
		eOhplPeOp_Float,
		eOhplPeOp_Vec2,
		eOhplPeOp_Vec3,
		eOhplPeOp_Quad1,
		eOhplPeOp_Quad2,
	};

	struct OhplPeOp
	{
		uint32_t kind;
		int32_t a, b, c;
		float f[3];
	};

	struct OhplPeStep
	{
		uint32_t index;
		uint32_t buffer;
	};

	struct OhplPeBloomIn
	{
		float blurSize;
		int iterations;
		float rgbToIntensity[3];
		bool programsOk;
	};

	enum
	{
		kOhplPeBloomTexInput = 0, kOhplPeBloomTexBlur0 = 1, kOhplPeBloomTexBlur1 = 2, kOhplPeBloomTexFinal = 3,
		kOhplPeImageTrailTexInput = 0, kOhplPeImageTrailTexAccum = 1,
		kOhplPeRadialTexInput = 0, kOhplPeRadialTexFinal = 1,
		kOhplPeColorConvTexInput = 0, kOhplPeColorConvTexLut = 1, kOhplPeColorConvTexFinal = 2,
		kOhplPeFinalCopyTexOut = 0,
	};

	extern "C"
	{
		OhplPePlan* ohpl_pe_plan_bloom(OhplPeBloomIn in);
		OhplPePlan* ohpl_pe_plan_image_trail(float amount, float frameTime, bool programOk, bool* clear);
		OhplPePlan* ohpl_pe_plan_radial_blur(float size, float blurStartDist, OhplVec2l targetSize, bool programOk);
		OhplPePlan* ohpl_pe_plan_color_conv(float fadeAlpha, bool plainOk, bool fadeOk);
		OhplPePlan* ohpl_pe_plan_final_copy();
		bool ohpl_pe_plan_next(OhplPePlan*, OhplPeOp* out);
		int ohpl_pe_plan_result(const OhplPePlan*);
		void ohpl_pe_plan_free(OhplPePlan*);

		void ohpl_pe_uv_pos_size(OhplVec2f texSize, OhplVec2f screen, OhplVec2l rtPos, OhplVec2l rtSize, OhplVec2f* outPos,
								 OhplVec2f* outSize);
		void ohpl_pe_round_target(OhplVec2f uvPos, OhplVec2f uvSize, OhplVec2l* outPos, OhplVec2l* outSize);
		void ohpl_pe_quad_uv(OhplVec2f texSize, bool rect, OhplVec2f uvPos, OhplVec2f uvSize, OhplVec2f* outMin,
							 OhplVec2f* outMax);
		size_t ohpl_pe_insert_pos(const int* prios, size_t n, int prio);
		size_t ohpl_pe_chain(const bool* active, size_t n, OhplPeStep* out);
	}
}

#endif
