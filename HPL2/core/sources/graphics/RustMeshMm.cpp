#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/Animation.h"
#include "graphics/VertexBuffer.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/MaterialManager.h"
#include "scene/Node3D.h"
#include "physics/PhysicsWorld.h"
#include "math/Math.h"
#include "system/LowLevelSystem.h"

// Thunks over engine classes still in C++ for rust/src/graphics/mm.rs and rust/src/material/mm.rs.
// Each goes away with the class it wraps; the destructors below anchor the vtables.

namespace hpl {

	cMaterial::~cMaterial(){ ohpl_mm_mat_free(mpRs); }
	cMesh::~cMesh(){ ohpl_mm_mesh_free(mpRs); }

	namespace {

	cMaterial *M(void *p){ return static_cast<cMaterial*>(p); }
	iPhysicsWorld *W(void *p){ return static_cast<iPhysicsWorld*>(p); }

	const OhplMmHost gHost = {
		[](void *v){ hplDelete(static_cast<iMaterialVars*>(v)); },
		[](void *r, void *tex){ static_cast<cResources*>(r)->GetTextureManager()->Destroy(static_cast<iTexture*>(tex)); },
		[](void *mgr, void *m){ static_cast<cMaterialManager*>(mgr)->Destroy(M(m)); },
		[](void *vb){ hplDelete(static_cast<iVertexBuffer*>(vb)); },
		[](void *p, OhplMmVbView *o)
		{
			iVertexBuffer *vb = static_cast<iVertexBuffer*>(p);
			o->indexNum = (size_t)vb->GetIndexNum();
			o->indices = vb->GetIndices();
			o->vertexNum = (size_t)vb->GetVertexNum();
			o->positions = vb->GetFloatArray(eVertexBufferElement_Position);
			o->stride = vb->GetElementNum(eVertexBufferElement_Position);
		},
		[](const unsigned char *m, size_t n){ Warning("%.*s", (int)n, (const char*)m); },
		[](void *p, const unsigned char **s, size_t *n){ const tString &name = static_cast<cMesh*>(p)->GetName(); *s = (const unsigned char*)name.data(); *n = name.size(); },
		[](void *s){ hplDelete(static_cast<cSubMesh*>(s)); },
		[](void *s){ return static_cast<cSkeleton*>(s)->GetBoneNum(); },
		[](void *s, int i, cVector3f *o){ *o = cMath::MatrixInverse(static_cast<cSkeleton*>(s)->GetBoneByIndex(i)->GetInvBindTransform()).GetTranslation(); },
		[](void *s){ hplDelete(static_cast<cSkeleton*>(s)); },
		[](void *a){ hplDelete(static_cast<cAnimation*>(a)); },
		[]() -> void* { return hplNew(cNode3D, ()); },
		[](void *n){ hplDelete(static_cast<cNode3D*>(n)); },
		[](void *p, const unsigned char **s, size_t *n){ const tString &name = static_cast<cNode3D*>(p)->GetName(); *s = (const unsigned char*)name.data(); *n = name.size(); },
		[](void *w, const cVector3f *size, cMatrixf *off) -> void* { return W(w)->CreateBoxShape(*size, off); },
		[](void *w, const cVector3f *size, cMatrixf *off) -> void* { return W(w)->CreateSphereShape(*size, off); },
		[](void *w, float r, float h, cMatrixf *off) -> void* { return W(w)->CreateCylinderShape(r, h, off); },
		[](void *w, float r, float h, cMatrixf *off) -> void* { return W(w)->CreateCapsuleShape(r, h, off); },
		[](void *w, void *const *shapes, size_t n) -> void*
		{
			tCollideShapeVec v;
			for(size_t i=0; i<n; ++i) v.push_back(static_cast<iCollideShape*>(shapes[i]));
			return W(w)->CreateCompundShape(v);
		},
	};

	}

	const OhplMmHost *OhplMmHostGet(){ return &gHost; }

}
