#include "scene/SubMeshEntity.h"

#include "scene/MeshEntity.h"

#include "resources/MaterialManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Skeleton.h"
#include "scene/Node3D.h"

namespace hpl {

	OhplWm2SubHost cSubMeshEntity::Wm2Host()
	{
		OhplWm2SubHost h;
		h.ctx = this;
		h.has_skeleton = [](void *p){ return static_cast<cSubMeshEntity*>(p)->mpMeshEntity->GetMesh()->GetSkeleton() != NULL; };
		h.copy_vb = [](void *p) -> void* {
			return static_cast<cSubMeshEntity*>(p)->mpSubMesh->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware,eVertexBufferUsageType_Dynamic,eFlagBit_All);
		};
		h.delete_vb = [](void *vb){ hplDelete(static_cast<iVertexBuffer*>(vb)); };
		h.destroy_material = [](void *p, void *m){ static_cast<cSubMeshEntity*>(p)->mpMaterialManager->Destroy(static_cast<cMaterial*>(m)); };
		h.update_mesh = [](void *p, float f){ static_cast<cSubMeshEntity*>(p)->mpMeshEntity->UpdateGraphicsForFrame(f); };
		h.skin_gate = [](void *p, OhplWm2GateIn *g){
			cSubMeshEntity *e = static_cast<cSubMeshEntity*>(p);
			g->mesh_version = e->mpMeshEntity->mCtl.bone_matrices_version;
			g->sleeping = e->mpMeshEntity->mCtl.sleeping;
			return e->mpSubMesh->mpVertexWeights != NULL;
		};
		h.skin_in = [](void *p, OhplWm2SkinIn *o){
			cSubMeshEntity *e = static_cast<cSubMeshEntity*>(p);
			iVertexBuffer *pBind = e->mpSubMesh->GetVertexBuffer();
			iVertexBuffer *pSkin = static_cast<iVertexBuffer*>(e->mpS->dyn_vb);
			o->bind_pos = pBind->GetFloatArray(eVertexBufferElement_Position);
			o->bind_normal = pBind->GetFloatArray(eVertexBufferElement_Normal);
			o->bind_tangent = pBind->GetFloatArray(eVertexBufferElement_Texture1Tangent);
			o->skin_pos = pSkin->GetFloatArray(eVertexBufferElement_Position);
			o->skin_normal = pSkin->GetFloatArray(eVertexBufferElement_Normal);
			o->skin_tangent = pSkin->GetFloatArray(eVertexBufferElement_Texture1Tangent);
			o->weights = e->mpSubMesh->mpVertexWeights;
			o->bones = e->mpSubMesh->mpVertexBones;
			o->matrices = e->mpMeshEntity->mvBoneMatrices.data();
			o->pos_stride = pSkin->GetElementNum(eVertexBufferElement_Position);
			o->vertex_count = pSkin->GetVertexNum();
			o->matrix_count = (int)e->mpMeshEntity->mvBoneMatrices.size();
		};
		h.upload_vb = [](void *p){
			static_cast<iVertexBuffer*>(static_cast<cSubMeshEntity*>(p)->mpS->dyn_vb)->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Normal | eVertexElementFlag_Texture1,false);
		};
		return h;
	}

	cSubMeshEntity::cSubMeshEntity(const tString &asName, cMeshEntity *apMeshEntity, cSubMesh * apSubMesh,
								cMaterialManager* apMaterialManager) : iRenderable(asName)
	{
		mpMeshEntity = apMeshEntity;
		mpSubMesh = apSubMesh;

		mbIsOneSided = mpSubMesh->GetIsOneSided();
		mvOneSidedNormal = mpSubMesh->GetOneSidedNormal();

		mpMaterialManager = apMaterialManager;

		mpS = ohpl_wm2_sub_new();
		OhplWm2SubHost h = Wm2Host();
		ohpl_wm2_sub_init(mpS, &h);
	}

	cSubMeshEntity::~cSubMeshEntity()
	{
		OhplWm2SubHost h = Wm2Host();
		ohpl_wm2_sub_free(mpS, &h);
	}

	cMaterial* cSubMeshEntity::GetMaterial()
	{
		return mpS->material ? static_cast<cMaterial*>(mpS->material) : mpSubMesh->GetMaterial();
	}

	void cSubMeshEntity::UpdateGraphicsForFrame(float afFrameTime)
	{
		OhplWm2SubHost h = Wm2Host();
		ohpl_wm2_sub_update_gfx(mpS, &h, afFrameTime);
	}

	iVertexBuffer* cSubMeshEntity::GetVertexBuffer()
	{
		return mpS->dyn_vb ? static_cast<iVertexBuffer*>(mpS->dyn_vb) : mpSubMesh->GetVertexBuffer();
	}

	cBoundingVolume* cSubMeshEntity::GetBoundingVolume()
	{
		if(mpMeshEntity->GetMesh()->GetSkeleton())
		{
			return mpMeshEntity->GetBoundingVolume();
		}
		if(mpE->updateBV)
		{
			mBoundingVolume.SetTransform(GetWorldMatrix());
			mpE->updateBV = false;
		}
		return &mBoundingVolume;
	}

	int cSubMeshEntity::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	cMatrixf* cSubMeshEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		return ohpl_wm2_sub_model_null(mpS, IsStatic(), &GetWorldMatrix()) ? NULL : &GetWorldMatrix();
	}

	void cSubMeshEntity::SetLocalNode(cNode3D *apNode)
	{
		mpS->local_node = apNode;
		apNode->AddEntity(this);
	}

	void cSubMeshEntity::SetCustomMaterial(cMaterial *apMaterial, bool abDestroyOldCustom)
	{
		OhplWm2SubHost h = Wm2Host();
		ohpl_wm2_sub_set_material(mpS, &h, apMaterial, abDestroyOldCustom);
	}

	void cSubMeshEntity::OnTransformUpdated()
	{
		mpMeshEntity->SetUpdateBoundingVolume(true);
	}

}
