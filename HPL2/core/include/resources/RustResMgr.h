#ifndef HPL_RUST_RES_MGR_H
#define HPL_RUST_RES_MGR_H

// C ABI of rust/src/resmgr_ffi.rs.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "system/SystemTypes.h"
#include "resources/FileSearcher.h"
#include "resources/ResourceManager.h"
#include "resources/RustResources.h"

struct OhplRmCustom;
struct OhplRmLoaders;

struct OhplRmBytes
{
	const uint8_t* p;
	size_t n;
};

struct OhplRmOut
{
	void* ctx;
	void (*name)(void*, const uint8_t*, size_t);
	void (*path)(void*, const uint32_t*, size_t);
	void (*item)(void*, const uint32_t*, size_t);
	void* res;
};

extern "C" {

	OhplRmCustom* ohpl_rm_custom_new();
	void ohpl_rm_custom_free(OhplRmCustom*);

	int32_t ohpl_rm_mat_anim_mode(const uint8_t*, size_t);
	bool ohpl_rm_mat_texture_is_srgb(int32_t type);
	bool ohpl_rm_mat_anisotropy_accepted(float x, int32_t caps, int32_t max, float current);

	bool ohpl_rm_world_decal_vertices(const float* const* arrays, const size_t* lens, size_t verts, float* out);

	void ohpl_rm_cache_file(const uint32_t* home, size_t hn, const uint32_t* file, size_t fn, const uint32_t* ext, size_t en, OhplRmOut*);
}

#ifndef RM_STR
#define RM_STR(s) reinterpret_cast<const uint8_t*>((s).data()), (s).size()
#endif

namespace hpl {
	class iResourceBase;

	inline OhplRmBytes RmBytes(const tString& asStr) { return OhplRmBytes{reinterpret_cast<const uint8_t*>(asStr.data()), asStr.size()}; }

	// Names and extension lists handed to Rust.
	class cRmExts
	{
	public:
		template<class C>
		explicit cRmExts(const C& aList) { for(const tString& s : aList) mvBytes.push_back(RmBytes(s)); }
		const OhplRmBytes* data() const { return mvBytes.data(); }
		const OhplH3Str* h3() const { return reinterpret_cast<const OhplH3Str*>(mvBytes.data()); }
		size_t size() const { return mvBytes.size(); }
	private:
		std::vector<OhplRmBytes> mvBytes;
	};

	// Receives the strings, items and resource Rust sends back.
	class cRmOut
	{
	public:
		cRmOut()
		{
			mRaw.ctx = this;
			mRaw.name = [](void* c, const uint8_t* p, size_t n){ static_cast<cRmOut*>(c)->msName.assign(reinterpret_cast<const char*>(p), n); };
			mRaw.path = [](void* c, const uint32_t* p, size_t n){ static_cast<cRmOut*>(c)->msPath.assign(reinterpret_cast<const wchar_t*>(p), n); };
			mRaw.item = [](void* c, const uint32_t* p, size_t n){ static_cast<cRmOut*>(c)->mvItems.emplace_back(reinterpret_cast<const wchar_t*>(p), n); };
			mRaw.res = NULL;
		}
		cRmOut(const cRmOut&) = delete;
		cRmOut& operator=(const cRmOut&) = delete;

		OhplRmOut* raw() { return &mRaw; }
		iResourceBase* Res() const { return static_cast<iResourceBase*>(mRaw.res); }

		tString msName;
		tWString msPath;
		std::vector<tWString> mvItems;
	private:
		OhplRmOut mRaw;
	};
}

#endif
