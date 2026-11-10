#ifndef HPL_RUST_MESH_LOAD_MSH_H
#define HPL_RUST_MESH_LOAD_MSH_H

// C ABI of rust/src/meshload/msh_ffi.rs.

#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplMshStr { const uint8_t* p; size_t n; };

	struct OhplMshNode { OhplMshStr name; float m[16]; int32_t flags; int32_t parent; };
	struct OhplMshBone { OhplMshStr name; OhplMshStr sid; float m[16]; float unitScale; int32_t parent; };
	struct OhplMshCollider { float m[16]; float size[3]; int32_t kind; int32_t charCollider; };
	struct OhplMshPair { int32_t vtx; int32_t bone; float weight; };
	struct OhplMshArray { int32_t kind; int32_t format; int32_t progVar; int32_t elemNum; int32_t repeats; size_t count; const uint8_t* data; size_t bytes; };
	struct OhplMshKey { float time; float trans[3]; float rot[4]; };
	struct OhplMshTrack { OhplMshStr name; int32_t flags; const OhplMshKey* keys; size_t keyNum; };
	struct OhplMshAnim { float length; const OhplMshTrack* tracks; size_t trackNum; };

	struct OhplMshSub
	{
		OhplMshStr name;
		OhplMshStr material;
		float m[16];
		float scale[3];
		int32_t collideShape;
		const OhplMshCollider* colliders; size_t colliderNum;
		const OhplMshPair* pairs; size_t pairNum;
		const OhplMshArray* arrays; size_t arrayNum;
		const int32_t* skipped; size_t skippedNum;
		const uint32_t* indices; size_t indexNum;
	};

	struct OhplMshMesh
	{
		int32_t hpl3;
		int32_t hasSkeleton;
		const OhplMshBone* bones; size_t boneNum;
		const OhplMshNode* nodes; size_t nodeNum;
		const OhplMshSub* subs; size_t subNum;
		const OhplMshAnim* anims; size_t animNum;
	};

	static_assert(sizeof(OhplMshKey) == 32 && offsetof(OhplMshKey, rot) == 16, "OhplMshKey layout");
	static_assert(sizeof(OhplMshPair) == 12, "OhplMshPair layout");
	static_assert(sizeof(OhplMshNode) == 88 && sizeof(OhplMshBone) == 104 && sizeof(OhplMshCollider) == 84, "msh record layout");
	static_assert(sizeof(OhplMshArray) == 48 && sizeof(OhplMshTrack) == 40 && sizeof(OhplMshAnim) == 24 && sizeof(OhplMshSub) == 192, "msh record layout");

extern "C" {

// 0 ok, 1 bad magic, 2 bad version, 3+ input the original loader crashes or throws on
const char* ohpl_msh_reason(int);
int ohpl_msh_parse(const uint8_t* data, size_t len, bool boneUnitScale, bool keepHpl3Anims, OhplMshMesh** out);
void ohpl_msh_free(OhplMshMesh*);
int ohpl_msh_parse_anim(const uint8_t* data, size_t len, OhplMshAnim** out);
void ohpl_msh_anim_free(OhplMshAnim*);

}

}

#endif
