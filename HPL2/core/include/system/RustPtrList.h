#ifndef HPL_RUST_PTR_LIST_H
#define HPL_RUST_PTR_LIST_H

// Pointer list stored in a Rust Vec (rust/src/scene/boxtree_dyn_ffi.rs); the std::list subset the engine uses.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hpl {

	struct OhplPtrVec;

	extern "C" {
	OhplPtrVec* ohpl_pv_new();
	void ohpl_pv_free(OhplPtrVec*);
	const uintptr_t* ohpl_pv_data(OhplPtrVec*);
	size_t ohpl_pv_len(OhplPtrVec*);
	void ohpl_pv_push(OhplPtrVec*, uintptr_t);
	void ohpl_pv_clear(OhplPtrVec*);
	void ohpl_pv_erase(OhplPtrVec*, size_t);
	void ohpl_pv_remove_all(OhplPtrVec*, uintptr_t);
	}

	template<class T>
	class cOhplPtrList
	{
	public:
		typedef T* value_type;
		typedef T* const* iterator;
		typedef iterator const_iterator;

		cOhplPtrList() : mpVec(ohpl_pv_new()), mbOwner(true){}
		explicit cOhplPtrList(OhplPtrVec *apVec) : mpVec(apVec), mbOwner(false){}
		cOhplPtrList(const cOhplPtrList&) = delete;
		cOhplPtrList& operator=(const cOhplPtrList&) = delete;
		~cOhplPtrList(){ if(mbOwner) ohpl_pv_free(mpVec);}

		iterator begin() const { return reinterpret_cast<iterator>(ohpl_pv_data(mpVec));}
		iterator end() const { return begin() + size();}
		size_t size() const { return ohpl_pv_len(mpVec);}
		bool empty() const { return size()==0;}
		T* front() const { return *begin();}
		T* back() const { return *(end()-1);}

		void push_back(T *apX){ ohpl_pv_push(mpVec, (uintptr_t)apX);}
		void clear(){ ohpl_pv_clear(mpVec);}
		void remove(T *apX){ ohpl_pv_remove_all(mpVec, (uintptr_t)apX);}
		template<class P> void remove_if(P aPred)
		{
			std::vector<T*> vKeep;
			for(iterator it=begin(); it!=end(); ++it) if(!aPred(*it)) vKeep.push_back(*it);
			clear();
			for(T *pX : vKeep) push_back(pX);
		}
		iterator erase(iterator aIt){ size_t lIdx = aIt - begin(); ohpl_pv_erase(mpVec, lIdx); return begin() + lIdx;}

	private:
		OhplPtrVec *mpVec;
		bool mbOwner;
	};

}

#endif
