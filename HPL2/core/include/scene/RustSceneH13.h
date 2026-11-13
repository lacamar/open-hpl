#ifndef HPL_RUST_SCENE_H13_H
#define HPL_RUST_SCENE_H13_H

// C ABI of rust/src/scene/ffi_h13.rs: cWorld entity registry and .map static-combine planner.

#include "scene/RustWorld.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "system/MemoryManager.h"

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
		kOhplH13EnvParticles,
		kOhplH13Area	// set_foreign only: the area map
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
typedef void (*OhplH13DropFn)(void*);

void ohpl_h13_reg_set_foreign(OhplH13Reg*, uint32_t kind, OhplH13NameFn, OhplH13IdFn, OhplH13DropFn);
void* ohpl_h13_reg_find_name_f(const OhplH13Reg*, uint32_t kind, OhplName key, bool last);
void* ohpl_h13_reg_find_id_f(const OhplH13Reg*, uint32_t kind, int32_t id);
void* ohpl_h13_reg_first_by_name_f(const OhplH13Reg*, uint32_t kind);
bool ohpl_h13_reg_destroy(OhplH13Reg*, uint32_t kind, void *p, bool onlyIfFound);
void ohpl_h13_reg_destroy_all(OhplH13Reg*, uint32_t kind);
void ohpl_h13_area_destroy_all(OhplH13Reg*);
size_t ohpl_h13_area_len(const OhplH13Reg*);
void* ohpl_h13_area_at(const OhplH13Reg*, size_t i, OhplName *key);
uint64_t ohpl_h13_cur_erase(OhplH13Reg*, uint32_t kind, uint64_t cur);
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

	template<class T>
	void OhplH13DeleteOf(void *apItem)
	{
		hplDelete(static_cast<T*>(apItem));
	}

	// Thunks only for the virtuals a kind has.
	template<class T>
	auto OhplH13NameThunk(int) -> decltype(std::declval<T&>().GetName(), OhplH13NameFn())
	{
		return OhplH13NameOf<T>;
	}
	template<class T> OhplH13NameFn OhplH13NameThunk(...) { return NULL; }

	template<class T>
	auto OhplH13IdThunk(int) -> decltype(std::declval<T&>().GetUniqueID(), OhplH13IdFn())
	{
		return OhplH13IdOf<T>;
	}
	template<class T> OhplH13IdFn OhplH13IdThunk(...) { return NULL; }

	// T is the pointee of the list's value_type: the address the registry holds.
	template<class T>
	void OhplH13Foreign(OhplH13Reg *apReg, uint32_t alKind)
	{
		if(apReg) ohpl_h13_reg_set_foreign(apReg, alKind, OhplH13NameThunk<T>(0), OhplH13IdThunk<T>(0), OhplH13DeleteOf<T>);
	}

	template<class CONT>
	void OhplH13Push(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, typename CONT::value_type apItem)
	{
		if(apReg) ohpl_h13_reg_add(apReg, alKind, (void*)apItem);
		else aCont.push_back(apItem);
	}

	// STLFindAndDelete: unlink the first match, delete regardless. With abOnlyIfFound delete only when linked.
	template<class CONT, class T>
	void OhplH13Destroy(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, T *apItem, bool abOnlyIfFound = false)
	{
		typename CONT::value_type pItem = apItem;
		if(apReg) { ohpl_h13_reg_destroy(apReg, alKind, (void*)pItem, abOnlyIfFound); return; }
		bool bFound = false;
		for(typename CONT::iterator it = aCont.begin(); it != aCont.end(); ++it)
		{
			if(*it == pItem) { aCont.erase(it); bFound = true; break; }
		}
		if(bFound || abOnlyIfFound == false) hplDelete(pItem);
	}

	// STLFindAndRemove
	template<class CONT, class T>
	void OhplH13Erase(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, T *apItem)
	{
		typename CONT::value_type pItem = apItem;
		if(apReg) ohpl_h13_reg_remove(apReg, alKind, (void*)pItem);
		else STLFindAndRemove(aCont, pItem);
	}

	template<class CONT>
	void OhplH13DeleteAll(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont)
	{
		if(apReg) ohpl_h13_reg_destroy_all(apReg, alKind);
		else STLDeleteAll(aCont);
	}

	template<class CONT>
	void OhplH13Clear(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont)
	{
		if(apReg) ohpl_h13_reg_clear(apReg, alKind);
		else aCont.clear();
	}

	template<class CONT>
	size_t OhplH13Size(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont)
	{
		return apReg ? ohpl_h13_reg_len(apReg, alKind) : aCont.size();
	}

	template<class CONT>
	typename CONT::value_type OhplH13Name(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, const tString &asName, bool abLast = false)
	{
		typedef typename CONT::value_type V;
		if(apReg == NULL) return OhplFindByName(aCont, asName, abLast);
		OhplName key{asName.data(), asName.size()};
		return static_cast<V>(ohpl_h13_reg_find_name_f(apReg, alKind, key, abLast));
	}

	template<class CONT>
	typename CONT::value_type OhplH13Id(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, int alID)
	{
		typedef typename CONT::value_type V;
		if(apReg == NULL) return OhplFindByUniqueID(aCont, alID);
		return static_cast<V>(ohpl_h13_reg_find_id_f(apReg, alKind, alID));
	}

	template<class CONT, class T>
	int OhplH13Index(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont, T *apItem)
	{
		if(apReg == NULL) return OhplPtrIndex(aCont, apItem);
		return ohpl_h13_reg_index(apReg, alKind, (const void*)apItem);
	}

	// One loop body for both paths: cursor on the Rust-owned list, or the std container's iterator.
	// Get() is the element at the position; Erase() unlinks it (the caller deletes) and moves to the next.
	template<class CONT>
	class OhplH13Walk
	{
	public:
		typedef typename CONT::value_type V;

		OhplH13Walk(OhplH13Reg *apReg, uint32_t alKind, CONT &aCont) : mpReg(apReg), mlKind(alKind), mpCont(&aCont)
		{
			if(mpReg) mlCur = ohpl_h13_cur_begin(mpReg, mlKind);
			else mIt = mpCont->begin();
		}

		bool Has() const
		{
			if(mpReg) return ohpl_h13_cur_get(mpReg, mlKind, mlCur) != NULL;
			return mIt != mpCont->end();
		}

		V Get() const
		{
			if(mpReg) return static_cast<V>(ohpl_h13_cur_get(mpReg, mlKind, mlCur));
			return *mIt;
		}

		void Advance()
		{
			if(mpReg) mlCur = ohpl_h13_cur_next(mpReg, mlKind, mlCur);
			else ++mIt;
		}

		void Erase()
		{
			if(mpReg) mlCur = ohpl_h13_cur_erase(mpReg, mlKind, mlCur);
			else mIt = mpCont->erase(mIt);
		}

	private:
		OhplH13Reg *mpReg;
		uint32_t mlKind;
		CONT *mpCont;
		uint64_t mlCur = 0;
		typename CONT::iterator mIt;
	};
}

#endif
