#ifndef HPL_RUST_SCENE_H13_H
#define HPL_RUST_SCENE_H13_H

// C ABI of rust/src/scene/ffi_h13.rs: cWorld entity registry and .map static-combine planner.

#include "scene/RustWorld.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hpl {
	struct OhplH13Reg;

	enum
	{
		kOhplH13Light = 0,
		kOhplH13DynMesh,
		kOhplH13StaticMesh,
		kOhplH13Billboard,
		kOhplH13LensFlare,
		kOhplH13Beam,
		kOhplH13ParticleSystem,
		kOhplH13GuiSet,
		kOhplH13Rope,
		kOhplH13Sound,
		kOhplH13StartPos,
		kOhplH13FogArea,
		kOhplH13ForceField,
		kOhplH13Dummy,
		kOhplH13AiContainer,
		kOhplH13AStar,
		kOhplH13EnvParticles
	};

	struct OhplH13MeshItem
	{
		size_t mMaterial;
		bool mbShadow, mbCombine;
	};

	struct OhplH13BodyItem
	{
		size_t mPhysMaterial;
		bool mbShadow, mbCharCollider, mbCollides;
		int32_t mlIndexNum;
	};

	static_assert(sizeof(OhplH13MeshItem) == 16 && sizeof(OhplH13BodyItem) == 16, "H13 plan item layout");

extern "C" {

typedef const uint8_t* (*OhplH13NameFn)(const void*, size_t*);
typedef int32_t (*OhplH13IdFn)(const void*);

OhplH13Reg* ohpl_h13_reg_new();
void ohpl_h13_reg_free(OhplH13Reg*);
void ohpl_h13_reg_add(OhplH13Reg*, uint32_t kind, void *p);
bool ohpl_h13_reg_remove(OhplH13Reg*, uint32_t kind, void *p);
void ohpl_h13_reg_remove_set(OhplH13Reg*, uint32_t kind, const size_t *set, size_t n);
void ohpl_h13_reg_make_dynamic(OhplH13Reg*, void *p);
void ohpl_h13_reg_clear(OhplH13Reg*, uint32_t kind);
size_t ohpl_h13_reg_len(const OhplH13Reg*, uint32_t kind);
void* ohpl_h13_reg_get(const OhplH13Reg*, uint32_t kind, size_t i);
int32_t ohpl_h13_reg_index(const OhplH13Reg*, uint32_t kind, const void *p);
void* ohpl_h13_reg_find_name(const OhplH13Reg*, uint32_t kind, OhplName key, bool last, OhplH13NameFn);
void* ohpl_h13_reg_find_id(const OhplH13Reg*, uint32_t kind, int32_t id, OhplH13IdFn);
void* ohpl_h13_reg_first_by_name(const OhplH13Reg*, uint32_t kind, OhplH13NameFn);
bool ohpl_h13_area_insert(OhplH13Reg*, OhplName name, void *p);
void* ohpl_h13_area_get(const OhplH13Reg*, OhplName name);
void ohpl_h13_area_clear(OhplH13Reg*);
size_t ohpl_h13_plan_meshes(const OhplH13MeshItem*, size_t n, int32_t *out, size_t cap);
size_t ohpl_h13_plan_bodies(const OhplH13BodyItem*, size_t n, size_t meshCount, int32_t *out, size_t cap);

}

	template<class T>
	const uint8_t* OhplH13NameOf(const void *apItem, size_t *apLen)
	{
		const tString &sName = static_cast<T*>(const_cast<void*>(apItem))->GetName();
		*apLen = sName.size();
		return (const uint8_t*)sName.data();
	}

	template<class T>
	int32_t OhplH13IdOf(const void *apItem)
	{
		return static_cast<T*>(const_cast<void*>(apItem))->GetUniqueID();
	}

	// The registry holds the pointer value the std::list holds (the list's value_type).
	template<class CONT>
	void OhplH13Added(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont)
	{
		if(apReg) ohpl_h13_reg_add(apReg, alKind, (void*)aCont.back());
	}

	template<class CONT, class T>
	void OhplH13Removed(OhplH13Reg *apReg, uint32_t alKind, CONT&, T *apItem)
	{
		typename CONT::value_type pItem = apItem;
		if(apReg) ohpl_h13_reg_remove(apReg, alKind, (void*)pItem);
	}

	inline void OhplH13Cleared(OhplH13Reg *apReg, uint32_t alKind)
	{
		if(apReg) ohpl_h13_reg_clear(apReg, alKind);
	}

	template<class CONT>
	typename CONT::value_type OhplH13Name(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, const tString &asName, bool abLast = false)
	{
		typedef typename CONT::value_type V;
		if(apReg == NULL) return OhplFindByName(aCont, asName, abLast);
		OhplName key{asName.data(), asName.size()};
		return static_cast<V>(ohpl_h13_reg_find_name(apReg, alKind, key, abLast, OhplH13NameOf<typename std::remove_pointer<V>::type>));
	}

	template<class CONT>
	typename CONT::value_type OhplH13Id(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, int alID)
	{
		typedef typename CONT::value_type V;
		if(apReg == NULL) return OhplFindByUniqueID(aCont, alID);
		return static_cast<V>(ohpl_h13_reg_find_id(apReg, alKind, alID, OhplH13IdOf<typename std::remove_pointer<V>::type>));
	}

	template<class CONT, class T>
	int OhplH13Index(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, T *apItem)
	{
		if(apReg == NULL) return OhplPtrIndex(aCont, apItem);
		return ohpl_h13_reg_index(apReg, alKind, (const void*)apItem);
	}
}

#endif
