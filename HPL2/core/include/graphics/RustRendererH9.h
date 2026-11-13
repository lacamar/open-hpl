#ifndef HPL_RUST_RENDERERH9_H
#define HPL_RUST_RENDERERH9_H

// C ABI of rust/src/render/h9.rs.

#include <cstddef>

#include "graphics/GraphicsTypes.h"

namespace hpl {

	class cProgramComboManager;

	struct OhplH9Define
	{
		const char *name;
		int kind;
		int ival;
		float fval;
	};

	extern "C" {
		size_t ohpl_h9_var_count(int alTable);
		const char *ohpl_h9_var(int alTable, size_t alIdx, int *apId);
		size_t ohpl_h9_feature_count(int alTable);
		const char *ohpl_h9_feature(int alTable, size_t alIdx, unsigned *apStages, unsigned *apNeeded);
		size_t ohpl_h9_light_defaults(int alQuality, bool abGBuffer64, bool abTargets4, int alJitterSize, int alJitterSamples, OhplH9Define *apOut, size_t alCap);
		size_t ohpl_h9_fog_defaults(bool abGBuffer64, OhplH9Define *apOut, size_t alCap);
	}

	// abRust: Rust tables; otherwise the original C++ setup. Exposed for the H9 differential harness.
	void OhplH9SetupFogCombos(cProgramComboManager *apMgr, bool abPackedDepth, bool abRust);
	void OhplH9SetupLightCombos(cProgramComboManager *apMgr, eShadowMapQuality aQuality, bool abGBuffer64, int alGBufferTextures, int alJitterSize, int alJitterSamples, bool abRust);

	static_assert(sizeof(OhplH9Define) == 24, "H9 layout");
}

#endif
