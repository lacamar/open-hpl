#ifndef HPL_RUST_WORLD_H
#define HPL_RUST_WORLD_H

// C ABI of rust/src/scene/ffi_world.rs. Layouts mirror soundentity.rs / world.rs / scenemgr.rs.

#include "math/RustTypes.h"
#include "system/SystemTypes.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hpl {
	struct OhplSoundCfg
	{
		float interval, random;
		bool looping, use_3d, has_start;
	};

	struct OhplSoundState
	{
		float interval_count, sleep_count;
		bool stopped, started, skip_start_end, prio_remove, fading_out, out_of_range, remove_when_over;
	};

	struct OhplSoundVt
	{
		void* ctx;
		bool (*silent)(void*);
		bool (*has_entry)(void*, int32_t);
		bool (*is_valid)(void*, int32_t);
		void (*clear_entry)(void*, int32_t);
		void (*stop_entry)(void*, int32_t);
		void (*fade_in)(void*, int32_t, float);
		void (*fade_out)(void*, int32_t, float);
		void (*update_position)(void*, int32_t);
		int32_t (*play_sound)(void*, int32_t, bool);
		void (*on_start)(void*);
		bool (*emitter)(void*);
		float (*listener_sqr_dist)(void*);
	};

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

	static_assert(sizeof(OhplSoundCfg) == 12 && offsetof(OhplSoundCfg, looping) == 8, "OhplSoundCfg layout");
	static_assert(sizeof(OhplSoundState) == 16 && offsetof(OhplSoundState, stopped) == 8, "OhplSoundState layout");
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
	void ohpl_snd_init(const OhplSoundCfg*, bool, bool, OhplSoundState*);
	void ohpl_snd_priority_release(OhplSoundState*);
	bool ohpl_snd_is_stopped(const OhplSoundState*, bool);
	void ohpl_snd_play(OhplSoundState*, OhplSoundVt*, const OhplSoundCfg*, bool);
	void ohpl_snd_stop(OhplSoundState*, OhplSoundVt*, const OhplSoundCfg*, bool);
	void ohpl_snd_fade_in(OhplSoundState*, OhplSoundVt*, const OhplSoundCfg*, float);
	void ohpl_snd_fade_out(OhplSoundState*, OhplSoundVt*, float);
	void ohpl_snd_update(OhplSoundState*, OhplSoundVt*, const OhplSoundCfg*, float, float);

	int32_t ohpl_world_find_name(const OhplName*, int32_t, OhplName);
	int32_t ohpl_world_find_last_name(const OhplName*, int32_t, OhplName);
	int32_t ohpl_world_first_by_name(const OhplName*, int32_t);
	int32_t ohpl_world_find_id(const int32_t*, int32_t, int32_t);
	int32_t ohpl_world_find_ptr(const void* const*, int32_t, const void*);
	int32_t ohpl_world_force_fields(const OhplFieldIn*, int32_t, const cVector3f*, const cVector3f*, uint32_t*);
	void ohpl_world_skybox_color(const cColor*, bool, float, cColor*);
	void ohpl_world_physics_bounds(const cVector3f*, const cVector3f*, cVector3f*, cVector3f*);
	uint32_t ohpl_world_pre_update_steps(float, float);
	uint32_t ohpl_world_destroy_plan(uint32_t);

	bool ohpl_scene_wants_post_effects(uint32_t, bool);
	uint32_t ohpl_scene_plan_viewport(uint32_t, const OhplViewportIn*);
	int32_t ohpl_scene_gui_order(const int32_t*, int32_t, uint32_t*);
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
