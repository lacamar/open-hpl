#ifndef HPL_RUST_L6_H
#define HPL_RUST_L6_H

// C ABI of rust/src/backend/meshio (stage L6): mesh loaders driven from Rust.
// Rust reads, parses and decides; the OhplL6Builder callbacks create the engine objects.
// Used when OHPL_IMPL=rust.

#include <cstddef>
#include <cstdint>

#include "impl/RustBackends.h"
#include "impl/RustMeshLoadMsh.h"
#include "system/SystemTypes.h"

namespace hpl {

	class cMesh;
	class cAnimation;

	struct OhplL6Str { const uint8_t* p; size_t n; };

	struct OhplL6Cfg
	{
		uint8_t boneUnitScale, keepHpl3Anims;
		uint8_t convertUnit, convertUnitAny, unscaledSkeleton;
		uint8_t loadAndSaveMsh, forceCacheSkipSaving;
		uint8_t useFastload;
		uint32_t meshFlags;
		OhplL6Str meshCacheDir;
		OhplL6Str workingDir;
		OhplL6Str fastloadMaterial;
	};

	enum { eOhplL6Log = 0, eOhplL6Warning = 1, eOhplL6Error = 2 };

	struct OhplL6Bone
	{
		OhplL6Str name, sid;
		float m[16];
		float invBind[16];
		float unitScale;
		int32_t parent;
		int32_t value;
		int32_t hasUnitScale, hasInvBind;
	};

	struct OhplL6Collider
	{
		float m[16];
		float size[3];
		int32_t kind, charCollider;
	};

	struct OhplL6Pair { int32_t vtx, bone; float weight; };

	struct OhplL6VbCfg
	{
		int32_t drawType, usageType, reserveVtx, reserveIdx;
	};

	static_assert(sizeof(OhplL6Str) == 16 && sizeof(OhplL6Cfg) == 64, "OhplL6Cfg layout");
	static_assert(sizeof(OhplL6Bone) == 184 && sizeof(OhplL6Collider) == 84 && sizeof(OhplL6Pair) == 12 && sizeof(OhplL6VbCfg) == 16, "L6 record layout");

	struct OhplL6Builder
	{
		void* ctx;

		void (*log)(void*, int level, OhplL6Str msg);

		bool (*file_exists)(void*, OhplL6Str path);
		int64_t (*file_date)(void*, OhplL6Str path);
		OhplL6Str (*cache_file)(void*, OhplL6Str path, OhplL6Str ext);
		OhplL6Str (*relative_dir)(void*, OhplL6Str path);
		bool (*save_mesh)(void*, OhplL6Str cacheFile);
		bool (*save_anim)(void*, OhplL6Str cacheFile);

		void (*mesh_new)(void*, OhplL6Str name, OhplL6Str path);
		void (*mesh_set_path)(void*, OhplL6Str path);
		void (*mesh_drop)(void*);
		void (*mesh_compile)(void*);

		void (*skeleton_begin)(void*);
		void (*bone)(void*, const OhplL6Bone*);
		void (*skeleton_end)(void*, int fileUnits);

		void (*node)(void*, int parent, OhplL6Str name, const float* m, int flags);

		void (*sub_select)(void*, int index);
		void (*sub_new)(void*, OhplL6Str name, const float* m, const float* scale, int collide);
		uintptr_t (*material_create)(void*, OhplL6Str name);
		void (*sub_material)(void*, OhplL6Str name, uintptr_t material);
		void (*sub_collider)(void*, const OhplL6Collider*);
		void (*sub_pairs)(void*, const OhplL6Pair*, size_t n);
		void (*sub_vb_new)(void*, const OhplL6VbCfg*);
		void (*sub_vb_array)(void*, const OhplMshArray*);
		void (*sub_vb_indices)(void*, const uint32_t*, size_t n);
		void (*sub_vb_transform)(void*, const float* m);
		void (*sub_vb_compile)(void*);
		void (*sub_vb_attach)(void*);
		void (*sub_compile)(void*);

		void (*anim_new)(void*, OhplL6Str name, OhplL6Str fileName, OhplL6Str animName, float length, size_t tracks);
		void (*anim_track)(void*, OhplL6Str name, int flags, const OhplMshKey* keys, size_t n);
		void (*anim_unit_scale)(void*, float scale);
		void (*anim_set_path)(void*, OhplL6Str path);
		void (*anim_end)(void*, int addToMesh);
	};

extern "C" {

// 0 ok, nonzero: nothing was built (an error was logged through the builder)
int ohpl_l6_msh_mesh(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);
int ohpl_l6_msh_anim(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);
int ohpl_l6_collada_mesh(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);
int ohpl_l6_collada_anim(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);
int ohpl_l6_assimp_mesh(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);
int ohpl_l6_assimp_anim(const OhplL6Cfg*, const OhplL6Builder*, OhplL6Str path);

}

	class iLowLevelGraphics;
	class cMaterialManager;
	class cAnimationManager;
	class cMeshLoaderMSH;

	enum { eOhplL6KindMsh = 0, eOhplL6KindCollada = 1, eOhplL6KindAssimp = 2 };

	struct OhplL6Env
	{
		iLowLevelGraphics* lowLevel;
		cMaterialManager* materials;
		cAnimationManager* animations;
		cMeshLoaderMSH* msh;
	};

	class cMeshManager;

	struct OhplL6CfgBuf
	{
		OhplL6Cfg cfg;
		tString cacheDir, workDir, fastload;
	};

	void OhplL6FillCfg(OhplL6CfgBuf& aBuf, cMeshManager* apMeshManager, bool abLoadAndSaveMsh, uint32_t alMeshFlags);

	// Shared by the three loaders; implemented in MeshLoaderRustBuilder.cpp.
	cMesh* OhplL6LoadMesh(int aKind, const OhplL6Env& aEnv, const OhplL6Cfg& aCfg, const tWString& asFile);
	cAnimation* OhplL6LoadAnim(int aKind, const OhplL6Env& aEnv, const OhplL6Cfg& aCfg, const tWString& asFile);

}

#endif
