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

#include "scene/MeshEntity.h"
#include <cstring>

#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/AnimationManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"

#include "resources/MeshLoaderHandler.h"
#include "resources/FileSearcher.h"

#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/BoneState.h"

#include "scene/AnimationState.h"
#include "scene/NodeState.h"
#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/SoundEntity.h"

#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"

#include "math/Math.h"

#include "engine/Engine.h"

namespace hpl {

	// Calls the Rust UpdateLogic makes back into the entity.
	struct OhplH11MeshHostImpl
	{
		static cMeshEntity* E(void *apCtx){ return static_cast<cMeshEntity*>(apCtx); }

		static OhplAnimCore *const* Cores(void *p, int32_t *apNum)
		{
			cMeshEntity *e = E(p);
			e->SyncAnimCores();
			*apNum = (int32_t)e->mvAnimCores.size();
			return e->mvAnimCores.data();
		}
		static int32_t StateCount(void *p){ return (int32_t)E(p)->mvAnimationStates.size(); }
		static int32_t BoneCount(void *p){ return (int32_t)E(p)->mvBoneStates.size(); }
		static bool HasSkeleton(void *p){ return E(p)->mpMesh->GetSkeleton() != NULL; }
		static bool HasRestPose(void *p){ return E(p)->mvBoneRestPose.empty() == false; }

		static void BoneReset(void *p, int32_t alBone, bool abMapPose, bool abTemp)
		{
			cMeshEntity *e = E(p);
			cNode3D *pState = e->mvBoneStates[alBone];
			cBone *pBone = e->mpMesh->GetSkeleton()->GetBoneByIndex(alBone);
			if(pState->IsActive())
			{
				pState->SetMatrix(abMapPose ? e->mvBoneRestPose[alBone] : pBone->GetLocalTransform(), false);
			}
			if(abTemp)
			{
				e->mvTempBoneStates[alBone]->SetMatrix(abMapPose ? e->mvBoneRestPose[alBone] : pBone->GetLocalTransform(), false);
			}
		}

		static void PhysicsFromBodies(void *p){ OhplH11Call c = MakeCall(E(p)); ohpl_h11b_physics_from_bodies(&c); }

		static void PhysicsMix(void *p, int32_t alBone, float afWeight)
		{
			cMeshEntity *e = E(p);
			cMatrixf mtxMixLocal = cMath::MatrixSlerp(afWeight, e->mvTempBoneStates[alBone]->GetLocalMatrix(),
													  e->mvBoneStates[alBone]->GetLocalMatrix(), true);
			e->mvBoneStates[alBone]->SetMatrix(mtxMixLocal, false);
		}

		static void PhysicsDeactivate(void *p){ E(p)->SetSkeletonPhysicsActive(false); }

		static void SkeletonApply(void *p, int32_t alState, float afWeight)
		{
			cMeshEntity *e = E(p);
			cAnimationState *pAnimState = e->mvAnimationStates[alState];
			cAnimation *pAnim = pAnimState->GetAnimation();
			std::vector<int> &vIdx = pAnimState->mvTrackNodeIndices;
			if((int)vIdx.size() != pAnim->GetTrackNum())
			{
				vIdx.resize(pAnim->GetTrackNum());
				for(int i=0; i<pAnim->GetTrackNum(); i++)
					vIdx[i] = e->mpMesh->GetSkeleton()->GetBoneIndexByName(pAnim->GetTrack(i)->GetName());
			}
			for(int i=0; i<pAnim->GetTrackNum(); i++)
			{
				cAnimationTrack *pTrack = pAnim->GetTrack(i);
				cNode3D* pState = e->GetBoneState(vIdx[i]);
				if(pState && pState->IsActive())
				{
					float fTransScale = e->mpMesh->GetSkeleton()->mbFileUnits ? 1.0f : pTrack->GetUnitScale();
					pTrack->ApplyToNode(pState,pAnimState->GetTimePosition(),afWeight, pAnimState->IsLooping(), fTransScale);
				}
			}
		}

		static void NodeApply(void *p, int32_t alState, float afWeight)
		{
			cMeshEntity *e = E(p);
			cAnimationState *pAnimState = e->mvAnimationStates[alState];
			cAnimation *pAnim = pAnimState->GetAnimation();
			std::vector<int> &vIdx = pAnimState->mvTrackNodeIndices;
			if((int)vIdx.size() != pAnim->GetTrackNum())
			{
				vIdx.resize(pAnim->GetTrackNum());
				for(int i=0; i<pAnim->GetTrackNum(); i++)
					vIdx[i] = e->GetNodeStateIndex(pAnim->GetTrack(i)->GetName());
			}
			for(int i=0; i<pAnim->GetTrackNum(); i++)
			{
				cAnimationTrack *pTrack = pAnim->GetTrack(i);
				if(vIdx[i] < 0) continue;
				cNode3D* pNodeState = e->GetNodeState(vIdx[i]);
				if(pNodeState->IsActive())
					pTrack->ApplyToNode(pNodeState,pAnimState->GetTimePosition(),afWeight, true, pTrack->GetUnitScale());
			}
		}

		static void BoneNodesUpdate(void *p){ OhplH11Call c = MakeCall(E(p)); ohpl_h11b_bone_nodes_update(&c); }

		static void BoneEntityChildren(void *p)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0;i < e->mvBoneStates.size(); i++) e->mvBoneStates[i]->UpdateEntityChildren();
		}

		static void CollidersUpdate(void *p)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0;i < e->mvBoneStates.size(); i++)
			{
				cBoneState *pState = e->mvBoneStates[i];
				iPhysicsBody *pColliderBody = pState->GetColliderBody();
				if(pColliderBody)
				{
					cMatrixf mtxBody = cMath::MatrixMul(pState->GetWorldMatrix(), pState->GetBodyMatrix());
					pColliderBody->SetMatrix(mtxBody);
				}
			}
		}

		static void SubTransformUpdated(void *p)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0; i<e->mvSubMeshes.size(); ++i) e->mvSubMeshes[i]->SetTransformUpdated(true);
		}

		static void NodeReset(void *p)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0;i < e->mvNodeStates.size(); i++)
			{
				cNode3D *pState = e->mvNodeStates[i];
				if(pState->IsActive()) pState->SetMatrix(cMatrixf::Identity);
			}
		}

		static void NodeUpdate(void *p){ OhplH11Call c = MakeCall(E(p)); ohpl_h11b_node_update(&c); }

		static void NodeRestore(void *p)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0;i < e->mvNodeStates.size(); i++)
			{
				cNode3D *pState = e->mvNodeStates[i];
				cNode3D* pMeshNode = e->mpMesh->GetNode((int)i);
				if(pState->IsActive()) pState->SetMatrix(pMeshNode->GetLocalMatrix());
			}
		}

		static void AfterAnim(void *p, float afTimeStep)
		{
			cMeshEntity *e = E(p);
			if(e->mpCallback) e->mpCallback->AfterAnimationUpdate(e, afTimeStep);
		}

		static void SubUpdateLogic(void *p, float afTimeStep)
		{
			cMeshEntity *e = E(p);
			for(size_t i=0; i<e->mvSubMeshes.size(); ++i) e->mvSubMeshes[i]->UpdateLogic(afTimeStep);
		}

		static int32_t EventCount(void *p, int32_t alState){ return E(p)->mvAnimationStates[alState]->GetEventNum(); }
		static float EventTime(void *p, int32_t alState, int32_t alEvent){ return E(p)->mvAnimationStates[alState]->GetEvent(alEvent)->mfTime; }
		static void HandleEvent(void *p, int32_t alState, int32_t alEvent)
		{
			cMeshEntity *e = E(p);
			e->HandleAnimationEvent(e->mvAnimationStates[alState]->GetEvent(alEvent));
		}
		static void PhysicsActive(cMeshEntity *e, bool abX){ OhplH11Call c = MakeCall(e); ohpl_h11b_physics_active(&c, &e->mCtl, abX); }

		struct CollideArgs
		{
			iPhysicsWorld *world; iCollideShape *shape; const cMatrixf *mtx; cCollideData *data;
			tVector3fList *pos; tIntList *num;
		};

		static cBoneState* B(size_t a){ return reinterpret_cast<cBoneState*>(a); }
		static cNode3D* N(size_t a){ return reinterpret_cast<cNode3D*>(a); }
		static iPhysicsBody* Body(size_t a){ return reinterpret_cast<iPhysicsBody*>(a); }
		static size_t P(const void *p){ return reinterpret_cast<size_t>(p); }

		static const tString* Str(cMeshEntity *e, size_t kind, size_t j, int32_t i)
		{
			switch(kind)
			{
			case 0: return &e->mpMesh->GetSubMesh(i)->GetName();
			case 1: return &e->mpMesh->GetAnimation(i)->GetName();
			case 2: return &e->mpMesh->GetSkeleton()->GetBoneByIndex(i)->GetName();
			case 3: { cBone *pParent = e->mpMesh->GetSkeleton()->GetBoneByIndex(i)->GetParent(); return pParent ? &pParent->GetName() : NULL; }
			case 4: return &e->mpMesh->GetNode(i)->GetName();
			case 5: { cNode3D *pParent = e->mpMesh->GetNode(i)->GetParent(); return pParent ? &pParent->GetName() : NULL; }
			case 6: return &e->mpMesh->GetNode(i)->GetChildAt(j)->GetName();
			}
			return NULL;
		}

		static size_t Call(void *p, int32_t op, size_t a, size_t b, int32_t i, float f)
		{
			cMeshEntity *e = E(p);
			switch(op)
			{
			case OhplH11Op_NodeActive: return N(a)->IsActive();
			case OhplH11Op_NodeUpdateMatrix: N(a)->UpdateMatrix(false); return 0;
			case OhplH11Op_NodeAnimXform: N(a)->ApplyPreAnimTransform(false); N(a)->ApplyPostAnimTransform(false); return 0;
			case OhplH11Op_NodeChildCount: return N(a)->GetChildCount();
			case OhplH11Op_NodeChild: return P(N(a)->GetChildAt(i));
			case OhplH11Op_NodeWorld: return P(&N(a)->GetWorldMatrix());
			case OhplH11Op_NodeUpdateWorld: N(a)->UpdateWorldTransform(); return 0;
			case OhplH11Op_NodeSetMatrix: N(a)->SetMatrix(*reinterpret_cast<const cMatrixf*>(b), false); return 0;
			case OhplH11Op_BoneBody: return P(B(a)->GetBody());
			case OhplH11Op_BoneCollider: return P(B(a)->GetColliderBody());
			case OhplH11Op_BoneInvBody: return P(&B(a)->GetInvBodyMatrix());
			case OhplH11Op_BoneBodyMatrix: return P(&B(a)->GetBodyMatrix());
			case OhplH11Op_BodyWorld: return P(&Body(a)->GetWorldMatrix());
			case OhplH11Op_BodySetActive: if(Body(a)) Body(a)->SetActive(i != 0); return 0;
			case OhplH11Op_BodyEnable: Body(a)->Enable(); return 0;
			case OhplH11Op_BodyZeroVel: Body(a)->SetLinearVelocity(0); Body(a)->SetAngularVelocity(0); return 0;
			case OhplH11Op_BodySetMatrix: Body(a)->SetMatrix(*reinterpret_cast<const cMatrixf*>(b)); return 0;
			case OhplH11Op_BoneState: return P(e->GetBoneState(i));
			case OhplH11Op_BoneCount: return e->mvBoneStates.size();
			case OhplH11Op_Root: return P(e->mpBoneStateRoot);
			case OhplH11Op_ResetGfx: e->ResetGraphicsUpdated(); return 0;
			case OhplH11Op_EntWorld: return P(&e->GetWorldMatrix());
			case OhplH11Op_EntParentWorld: return e->GetEntityParent() ? P(&e->GetEntityParent()->GetWorldMatrix()) : 0;
			case OhplH11Op_EntSetMatrix: e->SetMatrix(*reinterpret_cast<const cMatrixf*>(b)); return 0;
			case OhplH11Op_Collide:
			{
				CollideArgs *args = reinterpret_cast<CollideArgs*>(b);
				return args->world->CheckShapeCollision(Body(a)->GetShape(), Body(a)->GetLocalMatrix(),
										args->shape, *args->mtx, *args->data, 1, false);
			}
			case OhplH11Op_PushPos:
			{
				CollideArgs *args = reinterpret_cast<CollideArgs*>(a);
				args->pos->push_back(args->data->mvContactPoints[0].mvPoint);
				return 0;
			}
			case OhplH11Op_PushNum: reinterpret_cast<CollideArgs*>(a)->num->push_back(i); return 0;
			case OhplH11Op_SubCount: return e->mvSubMeshes.size();
			case OhplH11Op_SubVisible: e->mvSubMeshes[i]->SetVisible(a != 0); return 0;
			case OhplH11Op_SubStatic: e->mvSubMeshes[i]->SetStatic(a != 0); return 0;
			case OhplH11Op_SubIllum: e->mvSubMeshes[i]->SetIlluminationAmount(f); return 0;
			case OhplH11Op_SubCoverage: e->mvSubMeshes[i]->SetCoverageAmount(f); return 0;
			case OhplH11Op_SubLiquid: e->mvSubMeshes[i]->SetLiquidAmount(f); return 0;
			case OhplH11Op_SubAutoLiquid: e->mvSubMeshes[i]->UseAutomaticLiquidAmount(); return 0;
			case OhplH11Op_SubRenderFlag: e->mvSubMeshes[i]->SetRenderFlagBit((tRenderableFlag)a, b != 0); return 0;
			case OhplH11Op_SubColorMul: e->mvSubMeshes[i]->SetColorMul(*reinterpret_cast<const cColor*>(a)); return 0;
			case OhplH11Op_SubIllumColor: e->mvSubMeshes[i]->SetIlluminationColor(*reinterpret_cast<const cColor*>(a)); return 0;
			case OhplH11Op_SubBvMin: return P(&e->mvSubMeshes[i]->GetBoundingVolume()->GetMin());
			case OhplH11Op_SubBvMax: return P(&e->mvSubMeshes[i]->GetBoundingVolume()->GetMax());
			case OhplH11Op_BvFlag: return e->mpE->updateBV;
			case OhplH11Op_BvClearFlag: e->mpE->updateBV = false; return 0;
			case OhplH11Op_BvFromVertices:
			{
				for(int s=0;s<e->GetSubMeshEntityNum();s++)
				{
					iVertexBuffer *pVtxBuffer = e->GetSubMeshEntity(s)->GetVertexBuffer();
					e->mBoundingVolume.AddArrayPoints(pVtxBuffer->GetFloatArray(eVertexBufferElement_Position), pVtxBuffer->GetVertexNum());
				}
				if(e->GetSubMeshEntityNum()>0)
					e->mBoundingVolume.CreateFromPoints(e->GetSubMeshEntity(0)->GetVertexBuffer()->GetElementNum(eVertexBufferElement_Position));
				return 0;
			}
			case OhplH11Op_BvSetLocal:
				e->mBoundingVolume.SetLocalMinMax(*reinterpret_cast<const cVector3f*>(a), *reinterpret_cast<const cVector3f*>(b));
				return 0;
			case OhplH11Op_BvSetIdentity: e->mBoundingVolume.SetTransform(cMatrixf::Identity); return 0;
			case OhplH11Op_BoneRadius: { float r = e->mpMesh->GetBoneBoundingRadius(i); uint32_t u; memcpy(&u, &r, 4); return u; }
			case OhplH11Op_HasSkeleton: return e->mpMesh->GetSkeleton() != NULL;
			case OhplH11Op_Play: e->Play(i, a != 0, b != 0); return 0;
			case OhplH11Op_PlayFade: e->PlayFadeTo(i, a != 0, f); return 0;
			case OhplH11Op_Warn:
			{
				tString sName(reinterpret_cast<const char*>(a), b);
				if(i == 0) Warning("Can not find animation '%s' in meshentity '%s'\n", sName.c_str(), e->msName.c_str());
				else Warning("Can not find animation '%s' in mesh entity '%s'\n", sName.c_str(), e->msName.c_str());
				return 0;
			}
			case OhplH11Op_AnimIsOver: return e->mvAnimationStates[i]->IsOver();
			case OhplH11Op_EvCallback:
				return e->mpCallback && e->mpCallback->OnAnimationEvent(e, reinterpret_cast<cAnimationEvent*>(a));
			case OhplH11Op_EvValueEmpty: return reinterpret_cast<cAnimationEvent*>(a)->msValue == "";
			case OhplH11Op_EvType: return reinterpret_cast<cAnimationEvent*>(a)->mType == eAnimationEventType_PlaySound;
			case OhplH11Op_EvPlaySound:
			{
				cAnimationEvent *apEvent = reinterpret_cast<cAnimationEvent*>(a);
				cSoundEntity *pSound = e->mpWorld->CreateSoundEntity(e->msName+"_AnimEvent",apEvent->msValue,true);
				if(pSound)
				{
					pSound->SetIsSaved(false);
					cNode3DIterator nodeIt = e->mpBoneStateRoot->GetChildIterator();
					if(nodeIt.HasNext()) nodeIt.Next()->AddEntity(pSound);
					else pSound->SetPosition(e->mBoundingVolume.GetWorldCenter());
				}
				return 0;
			}
			case OhplH11Op_SubMeshNum: return e->mpMesh->GetSubMeshNum();
			case OhplH11Op_CreateSub:
			{
				cSubMesh *pSubMesh = e->mpMesh->GetSubMesh(i);
				cSubMeshEntity* pSub = hplNew( cSubMeshEntity, (e->msName + "_" + pSubMesh->GetName(),e,pSubMesh,e->mpMaterialManager) );
				e->mvSubMeshes.push_back(pSub);
				iVertexBuffer *pVtxBuffer = pSubMesh->GetVertexBuffer();
				e->InitSubBounds(pSub, pVtxBuffer);
				return 0;
			}
			case OhplH11Op_AnimNum: return e->mpMesh->GetAnimationNum();
			case OhplH11Op_CreateAnimState:
			{
				cAnimation* pAnimation = e->mpMesh->GetAnimation(i);
				e->mvAnimationStates.push_back(hplNew( cAnimationState, (pAnimation,pAnimation->GetName(),NULL) ));
				return 0;
			}
			case OhplH11Op_CreateBoneRoot:
				e->mpBoneStateRoot = hplNew( cNode3D, ("BoneStateRoot",false) );
				e->AddNodeChild(e->mpBoneStateRoot);
				return 0;
			case OhplH11Op_SkelBoneNum: return e->mpMesh->GetSkeleton()->GetBoneNum();
			case OhplH11Op_CreateBone:
			{
				cBone* pBone = e->mpMesh->GetSkeleton()->GetBoneByIndex(i);
				cBoneState *pState = hplNew( cBoneState,(pBone->GetName(), false) );
				pState->SetMatrix(pBone->GetLocalTransform());
				e->mvBoneStates.push_back(pState);
				return 0;
			}
			case OhplH11Op_BoneSetParent: e->mvBoneStates[a]->SetParent(b ? static_cast<cNode3D*>(e->mvBoneStates[b-1]) : e->mpBoneStateRoot); return 0;
			case OhplH11Op_BoneInitMatrices: e->mvBoneMatrices.resize(e->mpMesh->GetSkeleton()->GetBoneNum()); return 0;
			case OhplH11Op_BoneResetMatrix: e->mvBoneStates[i]->SetMatrix(e->mpMesh->GetSkeleton()->GetBoneByIndex(i)->GetLocalTransform()); return 0;
			case OhplH11Op_TempBoneInit: e->mvTempBoneStates.resize(e->mvBoneStates.size()); return 0;
			case OhplH11Op_TempBoneCreate: e->mvTempBoneStates[i] = hplNew( cBoneState,(e->mvBoneStates[i]->GetName(),false)); return 0;
			case OhplH11Op_SubAttach: e->AddChild(e->mvSubMeshes[i]); return 0;
			case OhplH11Op_SubDetach: e->RemoveChild(e->mvSubMeshes[i]); e->mvSubMeshes[i]->SetMatrix(cMatrixf::Identity); return 0;
			case OhplH11Op_MeshNodeNum: return e->mpMesh->GetNodeNum();
			case OhplH11Op_SubFromMeshNode:
			{
				cSubMesh *pSubMesh = e->mpMesh->GetSubMesh(i);
				cNode3D* pNode = e->mpMesh->GetNodeByName(pSubMesh->GetName());
				if(pNode) e->mvSubMeshes[i]->SetMatrix(pNode->GetWorldMatrix());
				else Error("Cannot find node '%s' in mesh '%s'\n",pSubMesh->GetName().c_str(), e->mpMesh->GetName().c_str());
				return 0;
			}
			case OhplH11Op_NodeStateCount: return e->mvNodeStates.size();
			case OhplH11Op_AnimStateCount: return e->mvAnimationStates.size();
			case OhplH11Op_CreateNodeState:
			{
				cNode3D* pMeshNode = e->mpMesh->GetNode(i);
				cBoneState *pNode = hplNew( cBoneState, (pMeshNode->GetName(), false) );
				pNode->SetMatrix(pMeshNode->GetLocalMatrix());
				e->mvNodeStates.push_back(pNode);
				return 0;
			}
			case OhplH11Op_NodeSetParent: e->mvNodeStates[a]->SetParent(e->mvNodeStates[b]); return 0;
			case OhplH11Op_NodeAddRoot: e->AddNodeChild(e->mvNodeStates[a]); return 0;
			case OhplH11Op_NodeAddChild: e->mvNodeStates[a]->AddChild(b < e->mvNodeStates.size() ? static_cast<cNode3D*>(e->mvNodeStates[b]) : NULL); return 0;
			case OhplH11Op_TransformUpdated: e->SetTransformUpdated(true); return 0;
			case OhplH11Op_StrPtr: { const tString *s = Str(e, a, b, i); return s ? P(s->data()) : 0; }
			case OhplH11Op_StrLen: { const tString *s = Str(e, a, b, i); return s ? s->size() : 0; }
			case OhplH11Op_SubSetLocalNode: e->mvSubMeshes[i]->SetLocalNode(e->mvNodeStates[a]); return 0;
			case OhplH11Op_MeshNodeChildCount: return e->mpMesh->GetNode(a)->GetChildCount();
			case OhplH11Op_SubDestroy:
			{
				cSubMeshEntity *pSub = e->mvSubMeshes[i];
				iEntity3D *pParent = pSub->GetEntityParent();
				if(pParent) pParent->RemoveChild(pSub);
				hplDelete(pSub);
				return 0;
			}
			case OhplH11Op_RootDelete: if(e->mpBoneStateRoot) hplDelete(e->mpBoneStateRoot); return 0;
			case OhplH11Op_MeshDestroy: e->mpMeshManager->Destroy(e->mpMesh); return 0;
			case OhplH11Op_DeleteAll:
				if(a == 0) STLDeleteAll(e->mvNodeStates);
				else if(a == 1) STLDeleteAll(e->mvBoneStates);
				else if(a == 2) STLDeleteAll(e->mvTempBoneStates);
				else STLDeleteAll(e->mvAnimationStates);
				return 0;
			case OhplH11Op_ListCount: return e->GetNodeChildCount();
			case OhplH11Op_ListAt: return P(e->GetNodeChildAt(i));
			}
			return 0;
		}

		static OhplH11Call MakeCall(cMeshEntity *apEntity){ OhplH11Call c = { apEntity, Call }; return c; }

		static OhplH11MeshHost Make(cMeshEntity *apEntity)
		{
			OhplH11MeshHost host;
			host.ctx = apEntity;
			host.cores = Cores;
			host.state_count = StateCount;
			host.bone_count = BoneCount;
			host.has_skeleton = HasSkeleton;
			host.has_rest_pose = HasRestPose;
			host.bone_reset = BoneReset;
			host.physics_from_bodies = PhysicsFromBodies;
			host.physics_mix = PhysicsMix;
			host.physics_deactivate = PhysicsDeactivate;
			host.skeleton_apply = SkeletonApply;
			host.node_apply = NodeApply;
			host.bone_nodes_update = BoneNodesUpdate;
			host.bone_entity_children = BoneEntityChildren;
			host.colliders_update = CollidersUpdate;
			host.sub_transform_updated = SubTransformUpdated;
			host.node_reset = NodeReset;
			host.node_update = NodeUpdate;
			host.node_restore = NodeRestore;
			host.after_anim = AfterAnim;
			host.sub_update_logic = SubUpdateLogic;
			host.event_count = EventCount;
			host.event_time = EventTime;
			host.handle_event = HandleEvent;
			return host;
		}
	};


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMeshEntity::cMeshEntity(const tString asName,cMesh* apMesh, cMaterialManager* apMaterialManager,
							cMeshManager* apMeshManager, cAnimationManager *apAnimationManager) : 
							iEntity3D(asName)
	{
		mpMaterialManager = apMaterialManager;
		mpMeshManager = apMeshManager;
		mpAnimationManager = apAnimationManager;

		mpWorld = NULL;

		mpCallback = NULL;

		mpMesh = apMesh;
		mpNames = ohpl_h11b_names_new();

		mpBoneStateRoot = NULL;
		
		mpBody=NULL;

		mpE->applyBV = false;

		mbIsVisible = true;
		mfIlluminationAmount = 1.0f;
		mfCoverageAmount = 1.0f;
		mlRenderFlags =0;

		mCtl.inv_world_transform_count = -1;
		mCtl.bone_matrices_transform_count = -1;
		mCtl.bone_matrices_version = 0;

		mCtl.matrices_need_update = true;

		mbStatic = false;

		mCtl.physics = false;
		mCtl.physics_weight = 1.0f;
		mCtl.fading = false;
		mCtl.physics_fade_speed = 1.0f;
		
		mCtl.sleeping = false;
		mCtl.can_sleep = true;
		
		mCtl.colliders = false;

		mCtl.normalize = true;

		mCtl.updated_bones = false;
		mCtl.has_updated_animation = true;

		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_build(&c, mpNames);
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::InitSubBounds(cSubMeshEntity *apSub, iVertexBuffer *apVtxBuffer)
	{
		apSub->mBoundingVolume.AddArrayPoints(apVtxBuffer->GetFloatArray(eVertexBufferElement_Position), apVtxBuffer->GetVertexNum());
		apSub->mBoundingVolume.CreateFromPoints(apVtxBuffer->GetElementNum(eVertexBufferElement_Position));
	}

	cMeshEntity::~cMeshEntity()
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_destroy(&c);
		ohpl_h11b_names_free(mpNames);
	}

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	
	//-----------------------------------------------------------------------
	
	void cMeshEntity::UpdateLogic(float afTimeStep)
	{	
		OhplH11MeshHost host = OhplH11MeshHostImpl::Make(this);
		ohpl_h11_mesh_update_logic(&host, &mCtl, mbStatic, afTimeStep);
	}

	//-----------------------------------------------------------------------

	cAnimationState* cMeshEntity::AddAnimation(cAnimation *apAnimation,const tString &asName, float afBaseSpeed)
	{
		/////////////////////////////
		// Create nodes. Will only be done once
		CreateNodes();

		/////////////////////////////
		// Create and set up animation state
		cAnimationState* pAnimState = hplNew( cAnimationState, (apAnimation,asName,mpAnimationManager) );
		
		pAnimState->SetBaseSpeed(afBaseSpeed);
		
		mvAnimationStates.push_back(pAnimState);

		NameAdd(0, pAnimState->GetName(), (int)mvAnimationStates.size()-1);

		return pAnimState;
	}

	void cMeshEntity::SyncAnimCores()
	{
		if(mvAnimCores.size() == mvAnimationStates.size()) return;
		mvAnimCores.clear();
		for(cAnimationState *pState : mvAnimationStates) mvAnimCores.push_back(pState->GetCore());
	}

	void cMeshEntity::ClearAnimations()
	{
		mvAnimCores.clear();
		STLDeleteAll(mvAnimationStates);
	}

	//-----------------------------------------------------------------------

	cAnimationState* cMeshEntity::GetAnimationState(int alIndex)
	{
		return mvAnimationStates[alIndex];
	}
	int cMeshEntity::GetAnimationStateIndex(const tString &asName)
	{
		return NameFind(0, asName);
	}
	cAnimationState* cMeshEntity::GetAnimationStateFromName(const tString &asName)
	{
		int lIdx = NameFind(0, asName);
		return lIdx < 0 ? NULL : mvAnimationStates[lIdx];
	}
	int cMeshEntity::GetAnimationStateNum()
	{
		return (int)mvAnimationStates.size();
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::Play(int alIndex,bool abLoop, bool bStopPrev)
	{
		SyncAnimCores();
		ohpl_h11_mesh_play(mvAnimCores.data(), (int32_t)mvAnimCores.size(), alIndex, abLoop, bStopPrev);
	}

	void cMeshEntity::PlayName(const tString &asName,bool abLoop, bool bStopPrev)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_play_name(&c, mpNames, asName.data(), asName.size(), abLoop, 0, 0, bStopPrev);
	}
	
	//-----------------------------------------------------------------------

	void cMeshEntity::PlayFadeTo(int alIndex,bool abLoop, float afTime)
	{
		SyncAnimCores();
		ohpl_h11_mesh_play_fade_to(mvAnimCores.data(), (int32_t)mvAnimCores.size(), alIndex, abLoop, afTime);
	}
	
	void cMeshEntity::PlayFadeToName(const tString &asName,bool abLoop, float afTime)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_play_name(&c, mpNames, asName.data(), asName.size(), abLoop, afTime, 1, false);
	}

	//-----------------------------------------------------------------------

	
	void cMeshEntity::Stop()
	{
		SyncAnimCores();
		ohpl_h11_mesh_stop(mvAnimCores.data(), (int32_t)mvAnimCores.size());
	}

	//-----------------------------------------------------------------------

	bool cMeshEntity::AnimationIsOver(const tString &asName)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		return ohpl_h11b_anim_is_over(&c, mpNames, asName.data(), asName.size());
	}
	
	//-----------------------------------------------------------------------

	cBoneState* cMeshEntity::GetBoneState(int alIndex)
	{
		if ( alIndex < 0 || alIndex >= mvBoneStates.size() )
		{
			return NULL;
		}
		return mvBoneStates[alIndex];
	}

	int cMeshEntity::GetBoneStateIndex(const tString &asName)
	{
		return NameFind(1, asName);
	}

	int cMeshEntity::GetBoneStateIndexFromPtr(cBoneState* apBoneState)
	{
		for(size_t i=0; i<mvBoneStates.size(); ++i)
		{
			if(apBoneState == mvBoneStates[i]) return (int)i;
		}

		return -1;
	}

	cBoneState* cMeshEntity::GetBoneStateFromName(const tString &asName)
	{
		int lIdx = NameFind(1, asName);
		return lIdx < 0 ? NULL : mvBoneStates[lIdx];
	}
	int cMeshEntity::GetBoneStateNum()
	{
		return (int)mvBoneStates.size();
	}

	bool cMeshEntity::SetBoneRestPoseFromMapValues(const tFloatVec &avVals)
	{
		cSkeleton *pSkeleton = mpMesh->GetSkeleton();
		if(pSkeleton == NULL || (int)avVals.size() != pSkeleton->GetBoneNum() * 7) return false;

		std::vector<cMatrixf> vLocal(pSkeleton->GetBoneNum());
		std::vector<cMatrixf> vBind(vLocal.size());
		std::vector<float> vUnit(vLocal.size());
		for(int i=0; i<pSkeleton->GetBoneNum(); ++i)
		{
			vBind[i] = pSkeleton->GetBoneByIndex(i)->GetLocalTransform();
			vUnit[i] = pSkeleton->GetBoneByIndex(i)->GetLocalUnitScale();
		}
		if(!ohpl_h11_rest_pose_from_map(avVals.data(), (int32_t)avVals.size(), vBind.data(), vUnit.data(), vLocal.data(), (int32_t)vLocal.size())) return false;
		SetBoneRestPose(vLocal);
		return true;
	}

	void cMeshEntity::SetBoneRestPose(const std::vector<cMatrixf> &avLocal)
	{
		if(avLocal.size() != mvBoneStates.size()) return;
		mvBoneRestPose = avLocal;
		for(size_t i=0; i<mvBoneStates.size(); ++i) mvBoneStates[i]->SetMatrix(avLocal[i]);
		mCtl.updated_bones = false;
	}

	//----------------------------------------------------------------------
	
	void cMeshEntity::SetSkeletonPhysicsActive(bool abX)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_physics_active(&c, &mCtl, abX);
	}

	bool cMeshEntity::GetSkeletonPhysicsActive()
	{
		return mCtl.physics;
	}

	//----------------------------------------------------------------------

	void cMeshEntity::FadeSkeletonPhysicsWeight(float afTime)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_physics_fade(&c, &mCtl, afTime);
	}

	//----------------------------------------------------------------------


	float cMeshEntity::GetSkeletonPhysicsWeight()
	{
		return mCtl.physics_weight;
	}
	void cMeshEntity::SetSkeletonPhysicsWeight(float afX)
	{
		mCtl.physics_weight = afX;
	}

	//----------------------------------------------------------------------

	void cMeshEntity::SetSkeletonCollidersActive(bool abX)
	{		
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_colliders_active(&c, &mCtl, abX);
	}
	bool cMeshEntity::GetSkeletonCollidersActive()
	{
		return mCtl.colliders;
	}

	//----------------------------------------------------------------------

	void cMeshEntity::AlignBodiesToSkeleton(bool abCalculateSpeed)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_align_bodies(&c);
	}

	//----------------------------------------------------------------------

	cMatrixf cMeshEntity::CalculateTransformFromSkeleton(cVector3f *apPostion,cVector3f *apAngles)
	{
		//Root bone
		cNode3DIterator StateIt = mpBoneStateRoot->GetChildIterator();
		cBoneState *pBoneState = static_cast<cBoneState*>(StateIt.Next());

		//Get the root bone (should only be one)
		cBoneIterator BoneIt = GetMesh()->GetSkeleton()->GetRootBone()->GetChildIterator();
		cBone* pBone = BoneIt.Next();

		//Rotation and position
		cMatrixf mtxOut;
		cVector3f vPos;
		float fYAngle;
		ohpl_h11_transform_from_skeleton(&pBoneState->GetWorldMatrix(), &pBone->GetInvWorldTransform(), &mtxOut, &vPos, &fYAngle);
		if(apPostion) *apPostion = vPos;
		if(apAngles) *apAngles = cVector3f(0,fYAngle,0);
		return mtxOut;
	}

	//----------------------------------------------------------------------

	bool cMeshEntity::CheckColliderShapeCollision(iPhysicsWorld *apWorld,
						iCollideShape *apShape, const cMatrixf &a_mtxShape,
						tVector3fList *apPosList, tIntList *apNumList)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		cCollideData collideData;
		collideData.SetMaxSize(1);
		OhplH11MeshHostImpl::CollideArgs args = { apWorld, apShape, &a_mtxShape, &collideData, apPosList, apNumList };
		return ohpl_h11b_check_colliders(&c, (size_t)&args, apPosList != NULL, apNumList != NULL);
	}

	//----------------------------------------------------------------------

	void cMeshEntity::ResetGraphicsUpdated()
	{
		for(size_t i=0; i< mvSubMeshes.size(); ++i)
		{
			mvSubMeshes[i]->mbGraphicsUpdated = false;
		}
		mCtl.updated_bones = false;
	}

	//----------------------------------------------------------------------

	cNode3D* cMeshEntity::GetNodeState(int alIndex)
	{
		return mvNodeStates[alIndex];
	}

	int cMeshEntity::GetNodeStateIndex(const tString &asName)
	{
		return NameFind(2, asName);
	}

	cNode3D* cMeshEntity::GetNodeStateFromName(const tString &asName)
	{
		int lIdx = NameFind(2, asName);
		return lIdx < 0 ? NULL : mvNodeStates[lIdx];
	}
	int cMeshEntity::GetNodeStateNum()
	{
		return (int)mvNodeStates.size();
	}

	//-----------------------------------------------------------------------
	
	cSubMeshEntity* cMeshEntity::GetSubMeshEntity(unsigned int alIdx)
	{
		if(alIdx >= mvSubMeshes.size()) return NULL;

		return mvSubMeshes[alIdx];
	}

	cSubMeshEntity* cMeshEntity::GetSubMeshEntityName(const tString &asName)
	{
		int lIdx = NameFind(3, asName);
		return lIdx < 0 ? NULL : mvSubMeshes[lIdx];
	}

	int cMeshEntity::GetSubMeshEntityNum()
	{
		return (int)mvSubMeshes.size();
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::UpdateGraphicsForFrame(float afFrameTime)
	{
		//////////////////////////////////////////
		cSkeleton *pSkeleton = mpMesh->GetSkeleton();
		uint32_t lGate = ohpl_h11_mesh_graphics_gate(&mCtl, pSkeleton != NULL, GetTransformUpdateCount());
		if(lGate == 0) return;
		bool bInvStale = (lGate & 2) != 0;

		///////////////////////////////////
		//Update the bone matrices
		if(pSkeleton)
		{
			if(bInvStale)
			{
				m_mtxInvWorldMatrix = cMath::MatrixInverse(GetWorldMatrix());
			}
			
			const int lBoneNum = pSkeleton->GetBoneNum();
			mvScratchWorld.resize(lBoneNum);
			mvScratchInvBind.resize(lBoneNum);
			for(int i=0; i< lBoneNum; i++)
			{
				mvScratchWorld[i] = &mvBoneStates[i]->GetWorldMatrix();
				mvScratchInvBind[i] = &pSkeleton->GetBoneByIndex(i)->GetInvBindTransform();
			}
			mCtl.bone_matrices_version += (int)ohpl_meshentity_bone_matrices(&m_mtxInvWorldMatrix, mvScratchWorld.data(),
									mvScratchInvBind.data(), mvBoneMatrices.data(), lBoneNum);
		}
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::SetVisible(bool abX)
	{
		if(abX == mbIsVisible) return;
		mbIsVisible = abX;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubVisible, abX, 0, 0);
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* cMeshEntity::GetBoundingVolume()
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_bounding_volume(&c);
		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::SetStatic(bool abX)
	{
		if(abX == mbStatic) return;
		mbStatic = abX;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubStatic, abX, 0, 0);
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::SetRenderFlagBit(tRenderableFlag alFlagBit, bool abSet)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubRenderFlag, alFlagBit, abSet, 0);
	}

	void cMeshEntity::SetColorMul(const cColor& aX)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubColorMul, (size_t)&aX, 0, 0);
	}

	void cMeshEntity::SetIlluminationAmount(float afX)
	{
		if(mfIlluminationAmount == afX) return;
		mfIlluminationAmount = afX;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubIllum, 0, 0, afX);
	}

	void cMeshEntity::SetIlluminationColor(const cColor& aX)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubIllumColor, (size_t)&aX, 0, 0);
	}

	void cMeshEntity::SetCoverageAmount(float afX)
	{
		if(mfCoverageAmount == afX) return;
		mfCoverageAmount = afX;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubCoverage, 0, 0, afX);
	}

	void cMeshEntity::SetLiquidAmount(float afX)
	{
		mfMeshLiquidAmount = afX > 0 ? afX : 0;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubLiquid, 0, 0, mfMeshLiquidAmount);
	}

	void cMeshEntity::UseAutomaticLiquidAmount()
	{
		mfMeshLiquidAmount = -1;
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_fan_out(&c, OhplH11Op_SubAutoLiquid, 0, 0, 0);
	}

	
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIAVTE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cMeshEntity::CreateNodes()
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_create_nodes(&c, mpNames);
	}
	
	//-----------------------------------------------------------------------

	void cMeshEntity::HandleAnimationEvent(cAnimationEvent *apEvent)
	{
		OhplH11Call c = OhplH11MeshHostImpl::MakeCall(this);
		ohpl_h11b_animation_event(&c, (size_t)apEvent);
	}
	
	//-----------------------------------------------------------------------
}
