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

struct OhplRmEnv
{
	void* ctx;
	const uint32_t* (*file_path)(void*, const uint8_t*, size_t, int32_t*, size_t*);
	void* (*get_resource)(void*, const uint32_t*, size_t);
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
	void ohpl_rm_find_with_formats(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* exts, size_t ne, OhplRmOut*);
	void ohpl_rm_find_sample(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* formats, size_t nf, OhplRmOut*);
	void ohpl_rm_find_stream_path(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* formats, size_t nf, OhplRmOut*);
	int32_t ohpl_rm_resolve_model(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* exts, size_t ne, OhplRmOut*);
	int32_t ohpl_rm_resolve_simple(const OhplRmEnv*, int32_t kind, const uint8_t* name, size_t n, OhplRmOut*);
	int32_t ohpl_rm_font_kind(const uint8_t* name, size_t n);

	OhplRmCustom* ohpl_rm_custom_new();
	void ohpl_rm_custom_free(OhplRmCustom*);
	void* ohpl_rm_custom_get(const OhplRmCustom*, const uint8_t* name, size_t n, bool* found);
	void ohpl_rm_custom_set(OhplRmCustom*, const uint8_t* name, size_t n, void* data);
	bool ohpl_rm_snd_needs_resolver(const OhplRmEnv*, const uint8_t* name, size_t n, bool has_resolver);

	void ohpl_rm_particle_by_file(const OhplRmEnv*, const uint8_t* type, size_t n, OhplRmOut*);
	void* ohpl_rm_particle_by_data(const OhplRmEnv*, const uint8_t* name, size_t n);

	bool ohpl_rm_destroy_standard(OhplResMeta*);
	void ohpl_rm_destroy_particle(OhplResMeta*);
	bool ohpl_rm_destroy_sound(OhplResMeta*, bool is_stream);
	uint32_t ohpl_rm_destroy_image_frame(bool frame_empty, bool frame_custom, bool has_bitmap_frame);
	void ohpl_rm_image_frame_size(int32_t fw, int32_t fh, int32_t bw, int32_t bh, int32_t* w, int32_t* h);

	void ohpl_rm_anim_first_frame(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* exts, size_t ne, OhplRmOut*);
	int32_t ohpl_rm_anim_plan(const OhplRmEnv*, const uint8_t* first, size_t n, OhplRmOut*);
	int32_t ohpl_rm_cube_pre(const OhplRmEnv*, const uint8_t* path, size_t n, OhplRmOut*);
	int32_t ohpl_rm_cube_faces(const OhplRmEnv*, const uint8_t* name, size_t n, const OhplRmBytes* exts, size_t ne, OhplRmOut*);

	int32_t ohpl_rm_mat_texture_type(const uint8_t*, size_t);
	int32_t ohpl_rm_mat_wrap(const uint8_t*, size_t);
	int32_t ohpl_rm_mat_anim_mode(const uint8_t*, size_t);
	int32_t ohpl_rm_mat_blend_mode(const uint8_t*, size_t, bool* warn);
	int32_t ohpl_rm_mat_uv_anim_type(const uint8_t*, size_t, bool* err);
	int32_t ohpl_rm_mat_anim_axis(const uint8_t*, size_t, bool* err);
	void ohpl_rm_mat_texture_string(int32_t type, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
	bool ohpl_rm_mat_texture_is_srgb(int32_t type);
	int32_t ohpl_rm_mat_create_kind(int32_t anim_mode, int32_t type);
	void ohpl_rm_mat_texture_file(const uint8_t* file, size_t n, const uint32_t* mat_path, size_t pn, void (*sink)(void*, const uint8_t*, size_t), void* ctx);
	bool ohpl_rm_mat_anisotropy_accepted(float x, int32_t caps, int32_t max, float current);

	int32_t ohpl_rm_world_billboard_type(const uint8_t*, size_t);
	int32_t ohpl_rm_world_shadow_resolution(const uint8_t*, size_t);
	int32_t ohpl_rm_world_light_kind(const uint8_t*, size_t);
	int32_t ohpl_rm_world_gobo_kind(bool spot, int32_t anim_mode);
	uint32_t ohpl_rm_world_shadow_casters(bool dynamic, bool stat);
	bool ohpl_rm_world_sound_use_default(bool use_default, int32_t custom);
	size_t ohpl_rm_world_band_count(size_t values);
	uint32_t ohpl_rm_world_light_mask_id(const uint8_t*, size_t);
	bool ohpl_rm_world_decal_vertices(const float* const* arrays, const size_t* lens, size_t verts, float* out);

	OhplRmLoaders* ohpl_rm_loaders_new();
	void ohpl_rm_loaders_free(OhplRmLoaders*);
	void ohpl_rm_loaders_add(OhplRmLoaders*, const uint8_t* name, size_t n, void* loader, bool set_default);
	void* ohpl_rm_loaders_get(const OhplRmLoaders*, const uint8_t* name, size_t n, bool* missing);
	void ohpl_rm_loaders_each(const OhplRmLoaders*, void (*f)(void*, void*), void* ctx);

	void ohpl_rm_cache_file(const uint32_t* home, size_t hn, const uint32_t* file, size_t fn, const uint32_t* ext, size_t en, OhplRmOut*);
	void ohpl_rm_resource_dirs(const uint8_t* path, size_t n, const uint32_t* alt, size_t an, OhplRmOut*);
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

	// Lets Rust ask the file searcher and the manager's resource map.
	class cRmEnv
	{
	public:
		cRmEnv(cFileSearcher* apFileSearcher, iResourceManager* apManager) : mpFileSearcher(apFileSearcher), mpManager(apManager)
		{
			mRaw.ctx = this;
			mRaw.file_path = [](void* c, const uint8_t* p, size_t n, int32_t* apCount, size_t* apLen) -> const uint32_t*
			{
				cRmEnv* pThis = static_cast<cRmEnv*>(c);
				int lCount = 0;
				const tWString& sPath = pThis->mpFileSearcher->GetFilePath(tString(reinterpret_cast<const char*>(p), n), apCount ? &lCount : NULL);
				if(apCount) *apCount = lCount;
				*apLen = sPath.size();
				return reinterpret_cast<const uint32_t*>(sPath.data());
			};
			mRaw.get_resource = [](void* c, const uint32_t* p, size_t n) -> void*
			{
				return static_cast<cRmEnv*>(c)->mpManager->GetResource(tWString(reinterpret_cast<const wchar_t*>(p), n));
			};
		}
		cRmEnv(const cRmEnv&) = delete;
		cRmEnv& operator=(const cRmEnv&) = delete;

		const OhplRmEnv* raw() const { return &mRaw; }

	private:
		cFileSearcher* mpFileSearcher;
		iResourceManager* mpManager;
		OhplRmEnv mRaw;
	};
}

#endif
