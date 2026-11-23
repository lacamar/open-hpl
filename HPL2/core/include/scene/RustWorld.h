#ifndef HPL_RUST_WORLD_H
#define HPL_RUST_WORLD_H

// C ABI of rust/src/scene/ffi_world.rs. Layouts mirror soundentity.rs / world.rs / scenemgr.rs.

#include "math/RustTypes.h"
#include "system/SystemTypes.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hpl {
	struct OhplName
	{
		const char* ptr;
		size_t len;
	};

	struct OhplFieldIn
	{
		cVector3f min, max;
		float radius;
	};

	struct OhplViewportIn
	{
		bool visible, has_composite, has_renderer, has_world, has_frustum, post_effects_active;
	};

	static_assert(sizeof(OhplName) == 16 && sizeof(OhplFieldIn) == 28 && sizeof(OhplViewportIn) == 6, "world layouts");

	enum
	{
		kOhplDestroyStatic = 1,
		kOhplDestroyPhysics = 2,
		kOhplStepSkip = 1,
		kOhplStepPreCallback = 2,
		kOhplStepRenderWorld = 4,
		kOhplStepFallbackTarget = 8,
		kOhplStepPostCallback = 16,
		kOhplStepGui3D = 32,
		kOhplStepPostEffects = 64,
		kOhplStepGuiScreen = 128,
		kOhplStepPostFlag = 256
	};

	extern "C" {
	int32_t ohpl_world_find_name(const OhplName*, int32_t, OhplName);
	int32_t ohpl_world_find_last_name(const OhplName*, int32_t, OhplName);
	int32_t ohpl_world_find_id(const int32_t*, int32_t, int32_t);
	int32_t ohpl_world_find_ptr(const void* const*, int32_t, const void*);

	}

	// Index of the first (or, with abLast, last) element whose GetName() equals asName, or -1.
	template<class CONT>
	int OhplNameIndex(CONT& aCont, const tString& asName, bool abLast = false)
	{
		std::vector<OhplName> vNames;
		for(auto *pItem : aCont)
		{
			const tString& sName = pItem->GetName();
			vNames.push_back(OhplName{sName.data(), sName.size()});
		}
		OhplName key{asName.data(), asName.size()};
		const int lNum = (int)vNames.size();
		return abLast ? ohpl_world_find_last_name(vNames.data(), lNum, key)
					  : ohpl_world_find_name(vNames.data(), lNum, key);
	}

	template<class CONT>
	typename CONT::value_type OhplFindByName(CONT& aCont, const tString& asName, bool abLast = false)
	{
		const int lIdx = OhplNameIndex(aCont, asName, abLast);
		if(lIdx < 0) return NULL;
		auto it = aCont.begin();
		std::advance(it, lIdx);
		return *it;
	}

	template<class CONT>
	typename CONT::value_type OhplFindByUniqueID(CONT& aCont, int alID)
	{
		std::vector<int32_t> vIDs;
		for(auto *pItem : aCont) vIDs.push_back(pItem->GetUniqueID());
		const int lIdx = ohpl_world_find_id(vIDs.data(), (int32_t)vIDs.size(), alID);
		if(lIdx < 0) return NULL;
		auto it = aCont.begin();
		std::advance(it, lIdx);
		return *it;
	}

	template<class CONT, class T>
	int OhplPtrIndex(CONT& aCont, T *apItem)
	{
		std::vector<const void*> vPtrs;
		for(auto *pItem : aCont) vPtrs.push_back((const void*)pItem);
		return ohpl_world_find_ptr(vPtrs.data(), (int32_t)vPtrs.size(), (const void*)apItem);
	}
}

#endif
