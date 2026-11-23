/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

// Thunks of cMeshEntity (state and control flow live in rust/src/scene/meshent.rs) onto the
// classes that are still C++: cNode3D/cBoneState, iPhysicsBody, cWorld, cSubMeshEntity,
// cAnimationState. Each thunk is deleted with the C++ class it wraps.

#include "scene/MeshEntity.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/AnimationManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Skeleton.h"
#include "graphics/Animation.h"
#include "scene/World.h"
#include "scene/SoundEntity.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"
#include "math/Math.h"
#include "system/LowLevelSystem.h"

namespace hpl {

#define FX(ret, name, ...) static ret Fx_##name(__VA_ARGS__)

	struct OhplMeFxImpl
	{
		struct CollideArgs
		{
			iPhysicsWorld *world; iCollideShape *shape; const cMatrixf *mtx; cCollideData *data;
			tVector3fList *pos; tIntList *num;
		};

		static cMeshEntity* E(void *p){ return static_cast<cMeshEntity*>(p);}
		static cNode3D* N(void *p){ return static_cast<cNode3D*>(p);}
		static cBoneState* B(void *p){ return static_cast<cBoneState*>(N(p));}
		static cSubMeshEntity* S(void *p){ return static_cast<cSubMeshEntity*>(p);}
		static iPhysicsBody* Body(void *p){ return static_cast<iPhysicsBody*>(p);}
		static cAnimationState* A(void *p){ return static_cast<cAnimationState*>(p);}
		static cAnimation* Anim(void *p){ return static_cast<cAnimation*>(p);}
		static cAnimationEvent* Ev(void *p){ return static_cast<cAnimationEvent*>(p);}

		static const char* Str(const tString &s, size_t *n){ *n = s.size(); return s.data();}

	FX(const char*, name, int32_t kind, void *o, size_t *n)
	{
		switch(kind)
		{
		case 0: return Str(static_cast<cSubMesh*>(o)->GetName(), n);
		case 1: return Str(Anim(o)->GetName(), n);
		default: return Str(N(o)->GetName(), n);
		}
	}

	FX(bool, n_active, void *n){ return N(n)->IsActive();}
	FX(void, n_update_matrix, void *n){ N(n)->UpdateMatrix(false);}
	FX(void, n_anim_xform, void *n){ N(n)->ApplyPreAnimTransform(false); N(n)->ApplyPostAnimTransform(false);}
	FX(const cMatrixf*, n_world, void *n){ return &N(n)->GetWorldMatrix();}
	FX(void, n_update_world, void *n){ N(n)->UpdateWorldTransform();}
	FX(void, n_set_matrix, void *n, const cMatrixf *m, bool abChildren){ N(n)->SetMatrix(*m, abChildren);}
	FX(const cMatrixf*, n_local, void *n){ return &N(n)->GetLocalMatrix();}
	FX(void*, n_parent, void *n){ return N(n)->GetParent();}
	FX(size_t, n_child_count, void *n){ return N(n)->GetChildCount();}
	FX(void*, n_child_at, void *n, size_t i){ return N(n)->GetChildAt(i);}
	FX(void, n_set_parent, void *n, void *p){ N(n)->SetParent(N(p));}
	FX(void, n_add_child, void *n, void *c){ N(n)->AddChild(N(c));}
	FX(void, n_ent_children, void *n){ N(n)->UpdateEntityChildren();}

	FX(void*, new_state, void *e, const char *name, size_t len, const cMatrixf *init)
	{
		cBoneState *pState = hplNew(cBoneState,(tString(name, len), false));
		if(init) pState->SetMatrix(*init);
		return static_cast<cNode3D*>(pState);
	}
	FX(void*, new_root, void *e)
	{
		cNode3D *pRoot = hplNew(cNode3D,("BoneStateRoot", false));
		E(e)->AddNodeChild(pRoot);
		return pRoot;
	}
	FX(void, delete_node, void *n){ hplDelete(N(n));}

	FX(void*, bs_body, void *n){ return B(n)->GetBody();}
	FX(void*, bs_collider, void *n){ return B(n)->GetColliderBody();}
	FX(const cMatrixf*, bs_inv_body, void *n){ return &B(n)->GetInvBodyMatrix();}
	FX(const cMatrixf*, bs_body_matrix, void *n){ return &B(n)->GetBodyMatrix();}

	FX(const cMatrixf*, body_world, void *b){ return &Body(b)->GetWorldMatrix();}
	FX(void, body_set_active, void *b, bool abX){ Body(b)->SetActive(abX);}
	FX(void, body_enable, void *b){ Body(b)->Enable();}
	FX(void, body_zero_vel, void *b){ Body(b)->SetLinearVelocity(0); Body(b)->SetAngularVelocity(0);}
	FX(void, body_set_matrix, void *b, const cMatrixf *m){ Body(b)->SetMatrix(*m);}
	FX(bool, collide, void *b, size_t args)
	{
		auto *a = reinterpret_cast<CollideArgs*>(args);
		return a->world->CheckShapeCollision(Body(b)->GetShape(), Body(b)->GetLocalMatrix(), a->shape, *a->mtx, *a->data, 1, false);
	}
	FX(void, push_pos, size_t args)
	{
		auto *a = reinterpret_cast<CollideArgs*>(args);
		a->pos->push_back(a->data->mvContactPoints[0].mvPoint);
	}
	FX(void, push_num, size_t args, int32_t i){ reinterpret_cast<CollideArgs*>(args)->num->push_back(i);}

	FX(const cMatrixf*, ent_world, void *e){ return &E(e)->GetWorldMatrix();}
	FX(const cMatrixf*, ent_parent_world, void *e)
	{
		iEntity3D *pParent = E(e)->GetEntityParent();
		return pParent ? &pParent->GetWorldMatrix() : NULL;
	}
	FX(void, ent_set_matrix, void *e, const cMatrixf *m){ E(e)->SetMatrix(*m);}
	FX(void, ent_transform_updated, void *e){ E(e)->SetTransformUpdated(true);}
	FX(int32_t, ent_count, void *e){ return E(e)->GetTransformUpdateCount();}
	FX(void, ent_add_node_child, void *e, void *n){ E(e)->AddNodeChild(N(n));}
	FX(size_t, ent_node_child_count, void *e){ return E(e)->GetNodeChildCount();}
	FX(void*, ent_node_child_at, void *e, size_t i){ return E(e)->GetNodeChildAt(i);}
	FX(void, ent_add_child, void *e, void *s){ E(e)->AddChild(S(s));}
	FX(void, ent_remove_child, void *e, void *s){ E(e)->RemoveChild(S(s));}

	FX(bool, bv_flag, void *e){ return E(e)->mpE->updateBV;}
	FX(void, bv_clear, void *e){ E(e)->mpE->updateBV = false;}
	FX(void, bv_from_vertices, void *p)
	{
		cMeshEntity *e = E(p);
		for(int s=0; s<e->GetSubMeshEntityNum(); s++)
		{
			iVertexBuffer *pVtx = e->GetSubMeshEntity(s)->GetVertexBuffer();
			e->mBoundingVolume.AddArrayPoints(pVtx->GetFloatArray(eVertexBufferElement_Position), pVtx->GetVertexNum());
		}
		if(e->GetSubMeshEntityNum() > 0)
			e->mBoundingVolume.CreateFromPoints(e->GetSubMeshEntity(0)->GetVertexBuffer()->GetElementNum(eVertexBufferElement_Position));
	}
	FX(void, bv_set_local, void *e, const cVector3f *lo, const cVector3f *hi){ E(e)->mBoundingVolume.SetLocalMinMax(*lo, *hi);}
	FX(void, bv_set_identity, void *e){ E(e)->mBoundingVolume.SetTransform(cMatrixf::Identity);}

	FX(void*, new_sub, void *p, int32_t idx, OhplWm2Sub **ppPod)
	{
		cMeshEntity *e = E(p);
		cSubMesh *pSubMesh = e->mpMesh->GetSubMesh(idx);
		cSubMeshEntity *pSub = hplNew(cSubMeshEntity,(e->msName + "_" + pSubMesh->GetName(), e, pSubMesh, e->mpMaterialManager));
		e->InitSubBounds(pSub, pSubMesh->GetVertexBuffer());
		*ppPod = pSub->mpS;
		return pSub;
	}
	FX(void, delete_sub, void *, void *s)
	{
		cSubMeshEntity *pSub = S(s);
		if(iEntity3D *pParent = pSub->GetEntityParent()) pParent->RemoveChild(pSub);
		hplDelete(pSub);
	}
	FX(void, sub_set_matrix, void *s, const cMatrixf *m){ S(s)->SetMatrix(*m);}
	FX(void, sub_visible, void *s, bool b){ S(s)->SetVisible(b);}
	FX(void, sub_static, void *s, bool b){ S(s)->SetStatic(b);}
	FX(void, sub_illum, void *s, float f){ S(s)->SetIlluminationAmount(f);}
	FX(void, sub_coverage, void *s, float f){ S(s)->SetCoverageAmount(f);}
	FX(void, sub_liquid, void *s, float f){ S(s)->SetLiquidAmount(f);}
	FX(void, sub_auto_liquid, void *s){ S(s)->UseAutomaticLiquidAmount();}
	FX(void, sub_render_flag, void *s, int32_t bit, bool b){ S(s)->SetRenderFlagBit((tRenderableFlag)bit, b);}
	FX(void, sub_color_mul, void *s, void *c){ S(s)->SetColorMul(*static_cast<const cColor*>(c));}
	FX(void, sub_illum_color, void *s, void *c){ S(s)->SetIlluminationColor(*static_cast<const cColor*>(c));}
	FX(void, sub_bv, void *s, cVector3f *lo, cVector3f *hi)
	{
		cBoundingVolume *pBv = S(s)->GetBoundingVolume();
		*lo = pBv->GetMin();
		*hi = pBv->GetMax();
	}
	FX(void, sub_update_logic, void *s, float dt){ S(s)->UpdateLogic(dt);}
	FX(void, sub_transform_updated, void *s){ S(s)->SetTransformUpdated(true);}
	FX(void, sub_set_local_node, void *s, void *n){ S(s)->SetLocalNode(N(n));}

	FX(void*, new_anim_state, void *e, void *a){ return hplNew(cAnimationState,(Anim(a), Anim(a)->GetName(), NULL));}
	FX(void*, add_anim_state, void *e, void *a, const char *name, size_t len)
	{
		return hplNew(cAnimationState,(Anim(a), tString(name, len), E(e)->mpAnimationManager));
	}
	FX(OhplAnimCore*, anim_core, void *s){ return A(s)->GetCore();}
	FX(void, delete_state, void *s){ hplDelete(A(s));}
	FX(void*, state_anim, void *s){ return A(s)->GetAnimation();}
	FX(int32_t, anim_track_count, void *a){ return Anim(a)->GetTrackNum();}
	FX(void*, anim_track, void *a, int32_t i){ return Anim(a)->GetTrack(i);}
	FX(const char*, track_name, void *t, size_t *n){ return Str(static_cast<cAnimationTrack*>(t)->GetName(), n);}
	FX(float, track_unit, void *t){ return static_cast<cAnimationTrack*>(t)->GetUnitScale();}
	FX(void, track_apply, void *t, void *n, float time, float weight, bool looping, float scale)
	{
		static_cast<cAnimationTrack*>(t)->ApplyToNode(N(n), time, weight, looping, scale);
	}

	FX(int32_t, ev_count, void *s){ return A(s)->GetEventNum();}
	FX(void*, ev_at, void *s, int32_t i){ return A(s)->GetEvent(i);}
	FX(float, ev_time, void *v){ return Ev(v)->mfTime;}
	FX(bool, ev_is_sound, void *v){ return Ev(v)->mType == eAnimationEventType_PlaySound;}
	FX(bool, ev_value_empty, void *v){ return Ev(v)->msValue == "";}
	FX(bool, ev_callback, void *p, void *v)
	{
		cMeshEntity *e = E(p);
		return e->mpCallback && e->mpCallback->OnAnimationEvent(e, Ev(v));
	}
	FX(void, ev_play_sound, void *p, void *v)
	{
		cMeshEntity *e = E(p);
		cSoundEntity *pSound = e->mpWorld->CreateSoundEntity(e->msName + "_AnimEvent", Ev(v)->msValue, true);
		if(pSound == NULL) return;
		pSound->SetIsSaved(false);
		cNode3DIterator nodeIt = e->GetBoneStateRoot()->GetChildIterator();
		if(nodeIt.HasNext()) nodeIt.Next()->AddEntity(pSound);
		else pSound->SetPosition(e->mBoundingVolume.GetWorldCenter());
	}
	FX(void, after_anim, void *p, float dt)
	{
		cMeshEntity *e = E(p);
		if(e->mpCallback) e->mpCallback->AfterAnimationUpdate(e, dt);
	}
	FX(void, mesh_destroy, void *p){ E(p)->mpMeshManager->Destroy(E(p)->mpMesh);}
	FX(void, report, void *p, int32_t kind, const char *name, size_t len)
	{
		cMeshEntity *e = E(p);
		tString sName(name, len);
		if(kind == 0) Warning("Can not find animation '%s' in meshentity '%s'\n", sName.c_str(), e->msName.c_str());
		else if(kind == 3) Error("Cannot find node '%s' in mesh '%s'\n", sName.c_str(), e->mpMesh->GetName().c_str());
		else Warning("Can not find animation '%s' in mesh entity '%s'\n", sName.c_str(), e->msName.c_str());
	}

	};

	const OhplMeFx* OhplMeFxGet()
	{
		static const OhplMeFx kFx = {
		OhplMeFxImpl::Fx_name,
		OhplMeFxImpl::Fx_n_active,
		OhplMeFxImpl::Fx_n_update_matrix,
		OhplMeFxImpl::Fx_n_anim_xform,
		OhplMeFxImpl::Fx_n_world,
		OhplMeFxImpl::Fx_n_update_world,
		OhplMeFxImpl::Fx_n_set_matrix,
		OhplMeFxImpl::Fx_n_local,
		OhplMeFxImpl::Fx_n_parent,
		OhplMeFxImpl::Fx_n_child_count,
		OhplMeFxImpl::Fx_n_child_at,
		OhplMeFxImpl::Fx_n_set_parent,
		OhplMeFxImpl::Fx_n_add_child,
		OhplMeFxImpl::Fx_n_ent_children,
		OhplMeFxImpl::Fx_new_state,
		OhplMeFxImpl::Fx_new_root,
		OhplMeFxImpl::Fx_delete_node,
		OhplMeFxImpl::Fx_bs_body,
		OhplMeFxImpl::Fx_bs_collider,
		OhplMeFxImpl::Fx_bs_inv_body,
		OhplMeFxImpl::Fx_bs_body_matrix,
		OhplMeFxImpl::Fx_body_world,
		OhplMeFxImpl::Fx_body_set_active,
		OhplMeFxImpl::Fx_body_enable,
		OhplMeFxImpl::Fx_body_zero_vel,
		OhplMeFxImpl::Fx_body_set_matrix,
		OhplMeFxImpl::Fx_collide,
		OhplMeFxImpl::Fx_push_pos,
		OhplMeFxImpl::Fx_push_num,
		OhplMeFxImpl::Fx_ent_world,
		OhplMeFxImpl::Fx_ent_parent_world,
		OhplMeFxImpl::Fx_ent_set_matrix,
		OhplMeFxImpl::Fx_ent_transform_updated,
		OhplMeFxImpl::Fx_ent_count,
		OhplMeFxImpl::Fx_ent_add_node_child,
		OhplMeFxImpl::Fx_ent_node_child_count,
		OhplMeFxImpl::Fx_ent_node_child_at,
		OhplMeFxImpl::Fx_ent_add_child,
		OhplMeFxImpl::Fx_ent_remove_child,
		OhplMeFxImpl::Fx_bv_flag,
		OhplMeFxImpl::Fx_bv_clear,
		OhplMeFxImpl::Fx_bv_from_vertices,
		OhplMeFxImpl::Fx_bv_set_local,
		OhplMeFxImpl::Fx_bv_set_identity,
		OhplMeFxImpl::Fx_new_sub,
		OhplMeFxImpl::Fx_delete_sub,
		OhplMeFxImpl::Fx_sub_set_matrix,
		OhplMeFxImpl::Fx_sub_visible,
		OhplMeFxImpl::Fx_sub_static,
		OhplMeFxImpl::Fx_sub_illum,
		OhplMeFxImpl::Fx_sub_coverage,
		OhplMeFxImpl::Fx_sub_liquid,
		OhplMeFxImpl::Fx_sub_auto_liquid,
		OhplMeFxImpl::Fx_sub_render_flag,
		OhplMeFxImpl::Fx_sub_color_mul,
		OhplMeFxImpl::Fx_sub_illum_color,
		OhplMeFxImpl::Fx_sub_bv,
		OhplMeFxImpl::Fx_sub_update_logic,
		OhplMeFxImpl::Fx_sub_transform_updated,
		OhplMeFxImpl::Fx_sub_set_local_node,
		OhplMeFxImpl::Fx_new_anim_state,
		OhplMeFxImpl::Fx_add_anim_state,
		OhplMeFxImpl::Fx_anim_core,
		OhplMeFxImpl::Fx_delete_state,
		OhplMeFxImpl::Fx_state_anim,
		OhplMeFxImpl::Fx_anim_track_count,
		OhplMeFxImpl::Fx_anim_track,
		OhplMeFxImpl::Fx_track_name,
		OhplMeFxImpl::Fx_track_unit,
		OhplMeFxImpl::Fx_track_apply,
		OhplMeFxImpl::Fx_ev_count,
		OhplMeFxImpl::Fx_ev_at,
		OhplMeFxImpl::Fx_ev_time,
		OhplMeFxImpl::Fx_ev_is_sound,
		OhplMeFxImpl::Fx_ev_value_empty,
		OhplMeFxImpl::Fx_ev_callback,
		OhplMeFxImpl::Fx_ev_play_sound,
		OhplMeFxImpl::Fx_after_anim,
		OhplMeFxImpl::Fx_mesh_destroy,
		OhplMeFxImpl::Fx_report,
		};
		return &kFx;
	}

	//-----------------------------------------------------------------------

	cMeshEntity::cMeshEntity(const tString asName,cMesh* apMesh, cMaterialManager* apMaterialManager,
							cMeshManager* apMeshManager, cAnimationManager *apAnimationManager) :
							iEntity3D(asName), mpMaterialManager(apMaterialManager), mpMeshManager(apMeshManager),
							mpAnimationManager(apAnimationManager), mpMesh(apMesh)
	{
		mpE->applyBV = false;

		cSkeleton *pSkeleton = apMesh->GetSkeleton();
		mpM = ohpl_me_new(this, OhplMeFxGet(), &mCtl, apMesh->mpRs, pSkeleton ? pSkeleton->Rs() : NULL,
						  pSkeleton ? pSkeleton->FileUnitsPtr() : NULL);
	}

	cMeshEntity::~cMeshEntity()
	{
		ohpl_me_free(mpM);
	}

	void cMeshEntity::InitSubBounds(cSubMeshEntity *apSub, iVertexBuffer *apVtxBuffer)
	{
		apSub->mBoundingVolume.AddArrayPoints(apVtxBuffer->GetFloatArray(eVertexBufferElement_Position), apVtxBuffer->GetVertexNum());
		apSub->mBoundingVolume.CreateFromPoints(apVtxBuffer->GetElementNum(eVertexBufferElement_Position));
	}

	bool cMeshEntity::SetBoneRestPoseFromMapValues(const tFloatVec &avVals)
	{
		cSkeleton *pSkeleton = mpMesh->GetSkeleton();
		if(pSkeleton == NULL || (int)avVals.size() != pSkeleton->GetBoneNum() * 7) return false;

		const int lNum = pSkeleton->GetBoneNum();
		std::vector<cMatrixf> vLocal(lNum), vBind(lNum);
		std::vector<float> vUnit(lNum);
		for(int i=0; i<lNum; ++i)
		{
			cBone *pBone = pSkeleton->GetBoneByIndex(i);
			vBind[i] = pBone->GetLocalTransform();
			vUnit[i] = pBone->GetLocalUnitScale();
		}
		if(!ohpl_h11_rest_pose_from_map(avVals.data(), (int32_t)avVals.size(), vBind.data(), vUnit.data(), vLocal.data(), lNum)) return false;
		SetBoneRestPose(vLocal);
		return true;
	}

	bool cMeshEntity::CheckColliderShapeCollision(iPhysicsWorld *apWorld,
						iCollideShape *apShape, const cMatrixf &a_mtxShape,
						tVector3fList *apPosList, tIntList *apNumList)
	{
		cCollideData collideData;
		collideData.SetMaxSize(1);
		OhplMeFxImpl::CollideArgs args = { apWorld, apShape, &a_mtxShape, &collideData, apPosList, apNumList };
		return ohpl_me_check_colliders(mpM, (size_t)&args, apPosList != NULL, apNumList != NULL);
	}

	//-----------------------------------------------------------------------

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
			o->matrices = ohpl_me_bone_matrices(e->mpMeshEntity->mpM, &o->matrix_count);
			o->pos_stride = pSkin->GetElementNum(eVertexBufferElement_Position);
			o->vertex_count = pSkin->GetVertexNum();
		};
		h.upload_vb = [](void *p){
			static_cast<iVertexBuffer*>(static_cast<cSubMeshEntity*>(p)->mpS->dyn_vb)->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Normal | eVertexElementFlag_Texture1,false);
		};
		return h;
	}

	cSubMeshEntity::cSubMeshEntity(const tString &asName, cMeshEntity *apMeshEntity, cSubMesh * apSubMesh,
								cMaterialManager* apMaterialManager) :
								iRenderable(asName), mpSubMesh(apSubMesh), mpMeshEntity(apMeshEntity),
								mpMaterialManager(apMaterialManager), mpS(ohpl_wm2_sub_new())
	{
		mbIsOneSided = mpSubMesh->GetIsOneSided();
		mvOneSidedNormal = mpSubMesh->GetOneSidedNormal();

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

	iVertexBuffer* cSubMeshEntity::GetVertexBuffer()
	{
		return mpS->dyn_vb ? static_cast<iVertexBuffer*>(mpS->dyn_vb) : mpSubMesh->GetVertexBuffer();
	}

	cBoundingVolume* cSubMeshEntity::GetBoundingVolume()
	{
		if(mpMeshEntity->GetMesh()->GetSkeleton()) return mpMeshEntity->GetBoundingVolume();
		if(mpE->updateBV)
		{
			mBoundingVolume.SetTransform(GetWorldMatrix());
			mpE->updateBV = false;
		}
		return &mBoundingVolume;
	}

	cMatrixf* cSubMeshEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		return ohpl_wm2_sub_model_null(mpS, IsStatic(), &GetWorldMatrix()) ? NULL : &GetWorldMatrix();
	}

	void cSubMeshEntity::OnTransformUpdated()
	{
		mpMeshEntity->SetUpdateBoundingVolume(true);
	}

}
