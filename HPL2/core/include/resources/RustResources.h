#ifndef HPL_RUST_RESOURCES_H
#define HPL_RUST_RESOURCES_H

// C ABI of rust/src/resources_ffi.rs.

#include <cstddef>
#include <cstdint>

struct OhplFileSearcher;
struct OhplResMap;
struct OhplExtList;
struct OhplLoaderTable;

struct OhplResMeta
{
	unsigned long handle;
	unsigned long time;
	unsigned long size;
	unsigned int prio;
	unsigned int user_count;
	bool log_destruction;
};

struct OhplResMapEntry
{
	unsigned int first;
	void* second;
};

extern "C" {
	OhplFileSearcher* ohpl_fs_new();
	void ohpl_fs_free(OhplFileSearcher*);
	int64_t ohpl_fs_add(OhplFileSearcher*, const char* key, size_t kn, const uint32_t* path, size_t pn, bool (*same_as)(void*, uint32_t), void* ctx);
	void ohpl_fs_clear(OhplFileSearcher*);
	int64_t ohpl_fs_get(const OhplFileSearcher*, const char* name, size_t n, int* equal_count);

	OhplResMap* ohpl_resmap_new();
	void ohpl_resmap_free(OhplResMap*);
	size_t ohpl_resmap_len(const OhplResMap*);
	OhplResMapEntry* ohpl_resmap_at(OhplResMap*, size_t i);
	void ohpl_resmap_insert(OhplResMap*, unsigned int hash, void* res);
	bool ohpl_resmap_remove(OhplResMap*, unsigned int hash, void* res);
	void* ohpl_resmap_find(const OhplResMap*, unsigned int hash, bool (*matches)(void*, void*), void* ctx);
	void ohpl_resmap_eviction_order(const OhplResMap*, void (*key)(void*, void*, unsigned int*, unsigned long*), void* kctx,
									void (*sink)(void*, void* const*, size_t), void* sctx);

	OhplExtList* ohpl_exts_new();
	void ohpl_exts_free(OhplExtList*);
	void ohpl_exts_add(OhplExtList*, const char* s, size_t n);
	bool ohpl_exts_supports(const OhplExtList*, const char* s, size_t n);
	size_t ohpl_exts_len(const OhplExtList*);
	const char* ohpl_exts_at(const OhplExtList*, size_t i, size_t* n);

	OhplLoaderTable* ohpl_loaders_new();
	void ohpl_loaders_free(OhplLoaderTable*);
	void ohpl_loaders_add(OhplLoaderTable*, void* loader, const OhplExtList* exts);
	size_t ohpl_loaders_len(const OhplLoaderTable*);
	void* ohpl_loaders_at(const OhplLoaderTable*, size_t i);
	void* ohpl_loaders_find(const OhplLoaderTable*, const char* ext, size_t n);

	void ohpl_resmeta_init(OhplResMeta*, unsigned int prio, unsigned long now);
	void ohpl_resmeta_inc_user(OhplResMeta*, unsigned long now);
	void ohpl_resmeta_dec_user(OhplResMeta*);
}

namespace hpl {
	class iResourceBase;

	// Stands in for std::multimap<unsigned, iResourceBase*>; storage lives in Rust.
	class cResourceMap
	{
	public:
		struct value_type
		{
			unsigned int first;
			iResourceBase* second;
		};

		class iterator
		{
		public:
			iterator() : mpMap(nullptr), mlPos(0) {}
			iterator(const cResourceMap* apMap, size_t alPos) : mpMap(apMap), mlPos(alPos) {}
			value_type* operator->() const
			{
				return reinterpret_cast<value_type*>(ohpl_resmap_at(mpMap->mpMap, mlPos));
			}
			iterator& operator++() { ++mlPos; return *this; }
			iterator operator++(int) { iterator it = *this; ++mlPos; return it; }
			bool operator==(const iterator& aOther) const { return mlPos == aOther.mlPos; }
			bool operator!=(const iterator& aOther) const { return mlPos != aOther.mlPos; }
		private:
			const cResourceMap* mpMap;
			size_t mlPos;
		};

		cResourceMap() : mpMap(ohpl_resmap_new()), mbOwned(true) {}
		explicit cResourceMap(OhplResMap* apBorrowed) : mpMap(apBorrowed), mbOwned(false) {}
		~cResourceMap() { if(mbOwned) ohpl_resmap_free(mpMap); }
		cResourceMap(const cResourceMap&) = delete;
		cResourceMap& operator=(const cResourceMap&) = delete;

		iterator begin() const { return iterator(this, 0); }
		iterator end() const { return iterator(this, ohpl_resmap_len(mpMap)); }
		size_t size() const { return ohpl_resmap_len(mpMap); }

		void insert(unsigned int alHash, iResourceBase* apResource) { ohpl_resmap_insert(mpMap, alHash, apResource); }
		bool erase(unsigned int alHash, iResourceBase* apResource) { return ohpl_resmap_remove(mpMap, alHash, apResource); }

		template<class F>
		iResourceBase* find_if(unsigned int alHash, F&& aMatch) const
		{
			return static_cast<iResourceBase*>(ohpl_resmap_find(mpMap, alHash,
				[](void* c, void* r) { return (*static_cast<F*>(c))(static_cast<iResourceBase*>(r)); }, &aMatch));
		}

		OhplResMap* raw() const { return mpMap; }

	private:
		OhplResMap* mpMap;
		bool mbOwned;
	};
}

#endif
