#ifndef HPL_RUST_SP_H
#define HPL_RUST_SP_H

// C ABI of rust/src/scene/sp_sys.rs. OhplSPPub mirrors its SpPub, the first member of the Rust system.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplSPSys;
struct OhplSPData;

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
}

extern "C" {
	OhplSPSys* ohpl_sp_new(void *apData);
	void ohpl_sp_free(OhplSPSys*);
	void ohpl_sp_add_emitter(OhplSPSys*, void*);
	int32_t ohpl_sp_emitter_count(const OhplSPSys*);
	void* ohpl_sp_emitter(const OhplSPSys*, int32_t);
	void ohpl_sp_set_data_name(OhplSPSys*, const uint8_t*, size_t);
	const uint8_t* ohpl_sp_data_name(const OhplSPSys*, size_t*);
	void ohpl_sp_set_hpl3_color(bool);
	bool ohpl_sp_hpl3_color();

	OhplSPData* ohpl_spd_new();
	void ohpl_spd_free(OhplSPData*);
	void ohpl_spd_add(OhplSPData*, void*);
	uint32_t ohpl_spd_count(const OhplSPData*);
	void* ohpl_spd_get(const OhplSPData*, int32_t);
}

#endif
