#ifndef HPL_RUST_RENDER_H8_H
#define HPL_RUST_RENDER_H8_H

// C ABI of rust/src/render/h8.rs.

#include <cstdint>

#include "graphics/RustRender.h"

namespace hpl {

	enum
	{
		eOhplH8Gate_MaterialUpdate = 1, eOhplH8Gate_ObjectUpdate = 2, eOhplH8Gate_ViewportUpdate = 4,
		eOhplH8List_Occlusion = 1, eOhplH8List_Light = 2, eOhplH8List_Fog = 4, eOhplH8List_Solid = 8,
		eOhplH8List_Trans = 16, eOhplH8List_Decal = 32, eOhplH8List_Illum = 64
	};

	struct OhplH8GateIn
	{
		bool hasMaterial, hasType, translucent, decal;
		int frame, materialFrame, objectFrame;
	};

	struct OhplH8PostIn
	{
		int renderType;
		bool hasMaterial, hasType, translucent, decal, hasIllumTexture, usesOcclusionQuery;
		float illumination;
		OhplVec3f bvMin, bvMax, bvCenter, origin;
		OhplMat4 view;
	};

	struct OhplH8PostOut { float viewZ; unsigned lists; };
	struct OhplH8Rect { int x, y, w, h; };

	extern "C" {
		unsigned ohpl_h8_rl_gates(const OhplH8GateIn *apIn);
		void ohpl_h8_rl_post(const OhplH8PostIn *apIn, OhplH8PostOut *apOut);
		void ohpl_h8_clip_pad(OhplH8Rect *apRect, int alScreenW, int alScreenH, float afHalfFovTan);
	}

	static_assert(sizeof(OhplH8GateIn) == 16 && sizeof(OhplH8PostIn) == 128 && sizeof(OhplH8PostOut) == 8, "H8 layout");
}

#endif
