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
#include "impl/RustBackends.h"
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

	// Calls the Rust UpdateLogic makes back into the entity (OHPL_IMPL=rust); each repeats one statement of the C++ path.
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

		static void PhysicsFromBodies(void *p)
		{
			cMeshEntity *e = E(p);
			cNode3DIterator BoneIt = e->mpBoneStateRoot->GetChildIterator();
			while(BoneIt.HasNext())
			{
				cBoneState *pBoneState = static_cast<cBoneState*>(BoneIt.Next());
				e->SetBoneMatrixFromBodyRec(e->mpBoneStateRoot->GetWorldMatrix(),pBoneState);
			}
		}

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

		static void BoneNodesUpdate(void *p)
		{
			cMeshEntity *e = E(p);
			cNode3DIterator NodeIt = e->mpBoneStateRoot->GetChildIterator();
			while(NodeIt.HasNext())
			{
				cNode3D *pBoneState = static_cast<cNode3D*>(NodeIt.Next());
				e->UpdateNodeMatrixRec(pBoneState);
			}
		}

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

		static void NodeUpdate(void *p)
		{
			cMeshEntity *e = E(p);
			tNode3DListIt nodeIt = e->mlstNodeChildren.begin();
			for(; nodeIt != e->mlstNodeChildren.end(); ++nodeIt) e->UpdateNodeMatrixRec(*nodeIt);
		}

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

		mpBoneStateRoot = NULL;
		
		mpBody=NULL;

		mbApplyTransformToBV = false;

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

		////////////////////////////////////////////////
		//Create sub entities
		for(int i=0;i<mpMesh->GetSubMeshNum();i++)
		{
			cSubMesh *pSubMesh = mpMesh->GetSubMesh(i);
			cSubMeshEntity* pSub = hplNew( cSubMeshEntity, (asName + "_" + pSubMesh->GetName(),this,pSubMesh,mpMaterialManager) );

			mvSubMeshes.push_back(pSub);
			m_mapSubMeshes.insert(tSubMeshEntityMap::value_type(mpMesh->GetSubMesh(i)->GetName(), pSub));
			
			iVertexBuffer *pVtxBuffer = mpMesh->GetSubMesh(i)->GetVertexBuffer();
			
			pSub->mBoundingVolume.AddArrayPoints(pVtxBuffer->GetFloatArray(eVertexBufferElement_Position), pVtxBuffer->GetVertexNum());
			pSub->mBoundingVolume.CreateFromPoints(pVtxBuffer->GetElementNum(eVertexBufferElement_Position));
		}
				
		////////////////////////////////////////////////
		//Create animation states from mesh.
		mvAnimationStates.reserve( mpMesh->GetAnimationNum());
		for(int i=0; i<  mpMesh->GetAnimationNum(); i++)
		{
			cAnimation* pAnimation = mpMesh->GetAnimation(i);
			
			cAnimationState* pAnimState = hplNew( cAnimationState, (pAnimation,pAnimation->GetName(),NULL) );

			mvAnimationStates.push_back(pAnimState);

			tAnimationStateIndexMap::value_type value(pAnimState->GetName(), (int)mvAnimationStates.size()-1);
			m_mapAnimationStateIndices.insert(value);
		}

		
		////////////////////////////////////////////////
		// Create Skeleton
		if(mpMesh->GetSkeleton())
		{
			//Use bones in skeleton to create nodes
			cSkeleton *pSkeleton = mpMesh->GetSkeleton();

			//Create the root node and attach all node without parents to this.
			mpBoneStateRoot = hplNew( cNode3D, ("BoneStateRoot",false) );
				
			//Create the root callback
			AddNodeChild(mpBoneStateRoot);
			
			//Reserve space for bones to be added
			mvBoneStates.reserve(pSkeleton->GetBoneNum());
            
			//////////////////////////////////
			//Fill the state array with the bones so
			//that each state has the same index as the bones.
			for(int i=0; i< pSkeleton->GetBoneNum(); i++)
			{
				cBone* pBone = pSkeleton->GetBoneByIndex(i);

				cBoneState *pState = hplNew( cBoneState,(pBone->GetName(), false) );
				pState->SetMatrix(pBone->GetLocalTransform());

				//Add bone to array and add it's index to the map.
				mvBoneStates.push_back(pState);
				m_mapBoneStateIndices.insert(tBoneIdxNameMap::value_type(pState->GetName(),i));
			}

			//////////////////////////////////
			//Set parents and children of the nodes in the array
			for(int i=0; i< (int)mvBoneStates.size();i++)
			{
				cNode3D* pState = mvBoneStates[i];
				cBone* pBone = pSkeleton->GetBoneByIndex(i);

				//Set the parent if there is one
				//TODO: Perhaps this should be removed.
				if(pBone->GetParent())
				{
					cNode3D *pParentState = GetBoneStateFromName(pBone->GetParent()->GetName());
					if(pParentState)
						pState->SetParent(pParentState);
					else
						pState->SetParent(mpBoneStateRoot);
				}
				else
				{
					pState->SetParent(mpBoneStateRoot);
				}
			}

			//Create an array to fill with bone matrices
			mvBoneMatrices.resize(pSkeleton->GetBoneNum());

			//////////////////////////////////
			//Reset all bones states
			for(size_t i=0;i < mvBoneStates.size(); i++)
			{
				cNode3D *pState = mvBoneStates[i];
				cBone* pBone = mpMesh->GetSkeleton()->GetBoneByIndex((int)i);

				pState->SetMatrix(pBone->GetLocalTransform());
			}

			//////////////////////////////////
			//Create temp bone nodes
			mvTempBoneStates.resize(mvBoneStates.size());
			for(size_t i=0;i < mvTempBoneStates.size(); i++)
			{
				mvTempBoneStates[i] = hplNew( cBoneState,(mvBoneStates[i]->GetName(),false));
			}

			//////////////////////////////////
			//Iterate and attach the sub meshes
			for(size_t i=0; i< mvSubMeshes.size(); ++i)
			{
				cSubMeshEntity *pSubEnt = mvSubMeshes[i];

				AddChild(pSubEnt);
			}

		}
		////////////////////////////////////////////////
		// No skeleton but animations. Create nodes and attach submeshes to them.
		else if(mvAnimationStates.empty()==false)
		{
			CreateNodes();	
		}
		////////////////////////////////////////////////
		// No skeleton or animation. Attach sub meshes directly!
		else
		{
			//Iterate and attach the sub meshes
			for(size_t i=0; i< mvSubMeshes.size(); ++i)
			{
				cSubMesh *pSubMesh = mpMesh->GetSubMesh((int)i);
				cSubMeshEntity *pSubEnt = mvSubMeshes[i];
				
				//If the mesh has nodes, search for right one and set matrix of submesh to it
				if(mpMesh->GetNodeNum() >0)
				{
					cNode3D* pNode = mpMesh->GetNodeByName(pSubMesh->GetName());
					if(pNode)
						pSubEnt->SetMatrix(pNode->GetWorldMatrix());
					else
						Error("Cannot find node '%s' in mesh '%s'\n",pSubMesh->GetName().c_str(), mpMesh->GetName().c_str());
				}
				//Attach the submesh
				AddChild(pSubEnt);
			}
		}
	}

	//-----------------------------------------------------------------------

	cMeshEntity::~cMeshEntity()
	{
		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			cSubMeshEntity *pSub = mvSubMeshes[i];

			//Remove from parent
			iEntity3D *pParent = pSub->GetEntityParent();
			if(pParent) pParent->RemoveChild(pSub);

			hplDelete(pSub);
		}

		if(mpBoneStateRoot)
		{
			//RemoveNodeChild(mpBoneStateRoot);
			hplDelete(mpBoneStateRoot);
		}
		
		mpMeshManager->Destroy(mpMesh);
		
		STLDeleteAll(mvNodeStates);
		STLDeleteAll(mvBoneStates);
		
		STLDeleteAll(mvTempBoneStates);

		STLDeleteAll(mvAnimationStates);
	}

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	
	//-----------------------------------------------------------------------
	
	//This functions sets the matrices of the bones according the matrices of
	//the body that is attached to each bone.
	//To get max speed all this is done recursivly from the root and then down.
	//To use function, iterate notes of root bone and call this function for each.
	void cMeshEntity::SetBoneMatrixFromBodyRec(const cMatrixf& a_mtxParentWorld,cBoneState *apBoneState)
	{
		iPhysicsBody *pBody = apBoneState->GetBody();
		
		if(pBody)
		{
			cMatrixf mtxBoneWorld = cMath::MatrixMul(pBody->GetWorldMatrix(),apBoneState->GetInvBodyMatrix());
			cMatrixf mtxParentInv = cMath::MatrixInverse(a_mtxParentWorld);

			apBoneState->SetMatrix(cMath::MatrixMul(mtxParentInv,mtxBoneWorld),false);

			cNode3DIterator BoneIt = apBoneState->GetChildIterator();
			while(BoneIt.HasNext())
			{
				cBoneState *pBoneState = static_cast<cBoneState*>(BoneIt.Next());
				SetBoneMatrixFromBodyRec(mtxBoneWorld, pBoneState);
			}
		}
		else
		{
			apBoneState->UpdateWorldTransform();
			const cMatrixf &mtxBoneWorld = apBoneState->GetWorldMatrix();
			
			cNode3DIterator BoneIt = apBoneState->GetChildIterator();
			while(BoneIt.HasNext())
			{
				cBoneState *pBoneState = static_cast<cBoneState*>(BoneIt.Next());

				SetBoneMatrixFromBodyRec(mtxBoneWorld, pBoneState);
			}
		}
	}

	//-----------------------------------------------------------------------
	

	/**
	 * This function will make less work be made in UpdateMatrix(...), else that might demand alot!
	 */
	void cMeshEntity::UpdateNodeMatrixRec(cNode3D *apNode)
	{
		if(apNode->IsActive())
		{
			apNode->UpdateMatrix(false);
		}

		apNode->ApplyPreAnimTransform(false);
		apNode->ApplyPostAnimTransform(false);
		

		//Should not be needed at all:
		//apNode->UpdateWorldTransform();
		//const cMatrixf& mtxWorld = apNode->GetWorldMatrix();

		cNode3DIterator NodeIt = apNode->GetChildIterator();
		while(NodeIt.HasNext())
		{
			cNode3D *pChildNode = static_cast<cNode3D*>(NodeIt.Next());

			UpdateNodeMatrixRec(pChildNode);
		}
	}
	
	//-----------------------------------------------------------------------
	
	void cMeshEntity::UpdateLogic(float afTimeStep)
	{	
		if(OhplBackendRustRequested())
		{
			OhplH11MeshHost host = OhplH11MeshHostImpl::Make(this);
			ohpl_h11_mesh_update_logic(&host, &mCtl, mbStatic, afTimeStep);
			return;
		}

		if(mbStatic) return; //No update on static models

		/////////////////////////////////////////////
		//Update the skeleton physics fade
		if(mCtl.fading && mCtl.physics)
		{
			mCtl.physics_weight -= afTimeStep * mCtl.physics_fade_speed;
			if(mCtl.physics_weight <= 0)
			{
				mCtl.fading = false;
				mCtl.physics_weight = 1.0f;
				SetSkeletonPhysicsActive(false);
			}
		}
		
		/////////////////////////////////////////////
		//Check if all bodies connected to the skeleton is at rest,
		//If so we can skip skinning the body and simply just use the mesh as is.
		//(has some problems so turned off at the moment)
		mCtl.sleeping = false;
		if(mCtl.physics && mCtl.physics_weight==1.0f && mCtl.can_sleep)
		{
			bool bEnabled = false;
			for(int bone =0; bone< GetBoneStateNum(); ++bone)
			{
				cBoneState *pState = GetBoneState(bone);
				iPhysicsBody *pBody = pState->GetBody();
				
				if(pBody && pBody->GetEnabled()){
					bEnabled = true;
					break;
				}
			}
			if(bEnabled == false){
				//mCtl.sleeping = true;
			}
		}
		/////////////////////////////////////////////
		//Update animations and skeleton physics
		if(mvAnimationStates.empty()==false || mCtl.physics)
		{
			////////////////////////
			//Check if it is animated
			bool bAnimationActive = false;
			bool bBaseAnimationActive = false;
			for(size_t i=0; i< mvAnimationStates.size(); i++)
			{
				if(mvAnimationStates[i]->IsActive()){
					bAnimationActive = true;
					if(!mvAnimationStates[i]->IsLayer()) bBaseAnimationActive = true;
				}
			}
			// Tracks are deltas from the skeleton rest, not from a map pose
			bool bUseMapPose = !mvBoneRestPose.empty() && !bBaseAnimationActive;

			//////////////////////////////////////
			// SKELETON
			if(mpMesh->GetSkeleton())
			{
				//If transform needs to be updated.
				bool bUpdateTransform = false;

				//////////
				//Reset all bones states
				if(	bAnimationActive || mCtl.updated_bones == false ||
					(mCtl.physics && !mCtl.sleeping))
				{
					for(size_t i=0;i < mvBoneStates.size(); i++)
					{
						cNode3D *pState = mvBoneStates[i];
						cBone* pBone = mpMesh->GetSkeleton()->GetBoneByIndex((int)i);

						if(pState->IsActive())
						{
							pState->SetMatrix(bUseMapPose ? mvBoneRestPose[i] : pBone->GetLocalTransform(),false);
						}
						
						//can optimize this by doing it in the order of the tree
						//and using recursive. (should be enough as is...)
						if(mCtl.physics && mCtl.physics_weight!=1.0f)
						{
							mvTempBoneStates[i]->SetMatrix(bUseMapPose ? mvBoneRestPose[i] : pBone->GetLocalTransform(),false);
						}
					}

					bUpdateTransform = true;
				}

				///////////////////////////
				// Update skeleton physics
				if(	mCtl.physics && (!mCtl.sleeping || mCtl.updated_bones==false))
				{
					mCtl.updated_bones = true;
					cNode3DIterator BoneIt = mpBoneStateRoot->GetChildIterator();
					while(BoneIt.HasNext())
					{
						cBoneState *pBoneState = static_cast<cBoneState*>(BoneIt.Next());
	                    
						SetBoneMatrixFromBodyRec(mpBoneStateRoot->GetWorldMatrix(),pBoneState);
					}
					
					//Interpolate matrices
					if(mCtl.physics_weight!=1.0f)
					{	
						for(size_t i=0;i < mvBoneStates.size(); i++)
						{
							cMatrixf mtxMixLocal = cMath::MatrixSlerp(	mCtl.physics_weight,
																		mvTempBoneStates[i]->GetLocalMatrix(),
																		mvBoneStates[i]->GetLocalMatrix(),
																		true);
							
							mvBoneStates[i]->SetMatrix(mtxMixLocal, false);
						}
					}
				}

				//////////////////////////////////
				//Go the weight mul (in case weights are normalized!)
				float fAnimationWeightMul = GetAnimationWeightMul();

				//////////////////////////////////
				//Go through all animations states and update the bones 
				for(size_t i=0; i< mvAnimationStates.size(); i++)
				{
					cAnimationState *pAnimState = mvAnimationStates[i];

					if(pAnimState->IsActive())
					{
						cAnimation *pAnim = pAnimState->GetAnimation();
						std::vector<int> &vIdx = pAnimState->mvTrackNodeIndices;
						if((int)vIdx.size() != pAnim->GetTrackNum())
						{
							vIdx.resize(pAnim->GetTrackNum());
							for(int i=0; i<pAnim->GetTrackNum(); i++)
								vIdx[i] = mpMesh->GetSkeleton()->GetBoneIndexByName(pAnim->GetTrack(i)->GetName());
						}

						/////////////////////////////////////
						//Go through all tracks in animation and apply to nodes
						for(int i=0; i<pAnim->GetTrackNum(); i++)
						{
							cAnimationTrack *pTrack = pAnim->GetTrack(i);
							cNode3D* pState = GetBoneState(vIdx[i]);
							
							///////////////////////////////////
							//Apply the animation track to node.
							if(pState && pState->IsActive())
							{
								float fTransScale = mpMesh->GetSkeleton()->mbFileUnits ? 1.0f : pTrack->GetUnitScale();
								pTrack->ApplyToNode(pState,pAnimState->GetTimePosition(),pAnimState->GetWeight() * (pAnimState->IsLayer() ? 1.0f : fAnimationWeightMul), pAnimState->IsLooping(), fTransScale);
							}
						}
					}
				}
				
				ohpl_meshentity_update_anims(mvAnimCores.data(), (int)mvAnimCores.size(), afTimeStep);

				//////////////////////////////////
				//Go through all states and update the matrices (and thereby adding the animations together).
				if(bAnimationActive)
				{
					cNode3DIterator NodeIt = mpBoneStateRoot->GetChildIterator();
					while(NodeIt.HasNext())
					{
						cNode3D *pBoneState = static_cast<cNode3D*>(NodeIt.Next());
						UpdateNodeMatrixRec(pBoneState);
					}

					//Entities are updated after BV is calculated, as the entity has the rootnode attached to it.
				}

				////////////////////////////
				//Update attached entities
				if(bAnimationActive || mCtl.physics)
				{
					for(size_t i=0;i < mvBoneStates.size(); i++)
					{
						mvBoneStates[i]->UpdateEntityChildren();
					}
				}
				
				//////////////////////////////////
				//Update the colliders if they are active
				//Note this must be done after all bone states are updated.
				if(mCtl.colliders && mCtl.physics==false)
				{
					for(size_t i=0;i < mvBoneStates.size(); i++)
					{
						cBoneState *pState = mvBoneStates[i];
						iPhysicsBody *pColliderBody = pState->GetColliderBody();

						if(pColliderBody)
						{
							cMatrixf mtxBody = cMath::MatrixMul(pState->GetWorldMatrix(), pState->GetBodyMatrix());
							pColliderBody->SetMatrix(mtxBody);
						}
					}
				}

				/////////////////////////////////////
				//Update the sub entity transform, so that they are updated in the renderable container.
				if(bUpdateTransform)
				{
					for(size_t i=0; i<mvSubMeshes.size(); ++i)
					{
						mvSubMeshes[i]->SetTransformUpdated(true);
					}
				}
				
			}
			//////////////////////////
			// NODES
			else
			{
				//////////////////////////////
				//Animation is being played
				if(bAnimationActive)
				{
					//Reset all state matrices
					for(size_t i=0;i < mvNodeStates.size(); i++)
					{
						cNode3D *pState = mvNodeStates[i];
						if(pState->IsActive()) pState->SetMatrix(cMatrixf::Identity);
					}

					//////////////////////////////////
					//Go the weight mul (in case weights are normalized!)
					float fAnimationWeightMul = GetAnimationWeightMul();

					/////////////////////////
					//Go through all animations states and set the node's
					for(size_t i=0; i< mvAnimationStates.size(); i++)
					{
						cAnimationState *pAnimState = mvAnimationStates[i];
						if(pAnimState->IsActive())
						{
							cAnimation *pAnim = pAnimState->GetAnimation();
							std::vector<int> &vIdx = pAnimState->mvTrackNodeIndices;
							if((int)vIdx.size() != pAnim->GetTrackNum())
							{
								vIdx.resize(pAnim->GetTrackNum());
								for(int i=0; i<pAnim->GetTrackNum(); i++)
									vIdx[i] = GetNodeStateIndex(pAnim->GetTrack(i)->GetName());
							}

							for(int i=0; i<pAnim->GetTrackNum(); i++)
							{
								cAnimationTrack *pTrack = pAnim->GetTrack(i);
								if(vIdx[i] < 0) continue;
								cNode3D* pNodeState = GetNodeState(vIdx[i]);

								if(pNodeState->IsActive()) 
									pTrack->ApplyToNode(pNodeState,pAnimState->GetTimePosition(),pAnimState->GetWeight() * fAnimationWeightMul, true, pTrack->GetUnitScale());
							}

						}
					}

					ohpl_meshentity_update_anims(mvAnimCores.data(), (int)mvAnimCores.size(), afTimeStep);

					//////////////////////
					//Go through all states and update the matrices (and thereby adding the animations together).
				
					tNode3DListIt nodeIt = mlstNodeChildren.begin();
					for(; nodeIt != mlstNodeChildren.end(); ++nodeIt)
					{
						cNode3D *pNodeState = *nodeIt;

						UpdateNodeMatrixRec(pNodeState);
					}

					mCtl.has_updated_animation = true;
				}
				//////////////////////////////
				//No animation is played, only do this if an animation has been played.
				else if(mCtl.has_updated_animation)
				{
					//Reset all state matrices
					for(size_t i=0;i < mvNodeStates.size(); i++)
					{
						cNode3D *pState = mvNodeStates[i];
						cNode3D* pMeshNode = mpMesh->GetNode((int)i);
						if(pState->IsActive()) pState->SetMatrix(pMeshNode->GetLocalMatrix());
					}
					mCtl.has_updated_animation = false;
				}
			}
			
			//////////////////
			//Call callback
			if(mpCallback )mpCallback->AfterAnimationUpdate(this,afTimeStep);
		}
				
		/////////////////////////////////////////
		/// Update Sub Entities
		for(size_t i=0; i<mvSubMeshes.size(); ++i)
		{
			cSubMeshEntity *pSub = mvSubMeshes[i];

			pSub->UpdateLogic(afTimeStep);
		}
		
		/////////////////////////////////////////
		/// Update animation events
		if(mvAnimationStates.empty()==false)
		{
			for(size_t i=0; i< mvAnimationStates.size(); ++i)
			{
				cAnimationState *pState = mvAnimationStates[i];

				if(pState->IsActive()==false || pState->IsPaused()) continue;

				for(int j=0; j < pState->GetEventNum(); ++j)
				{
					cAnimationEvent *pEvent = pState->GetEvent(j);

					if(	pEvent->mfTime >= pState->GetPreviousTimePosition() &&
						pEvent->mfTime < pState->GetTimePosition())
					{
						HandleAnimationEvent(pEvent);
					}
				}
			}
		}

		/////////////////////////////////////////
		/// Final things
		if(mpMesh->GetSkeleton()) mCtl.matrices_need_update = true;
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

		tAnimationStateIndexMap::value_type value(pAnimState->GetName(), (int)mvAnimationStates.size()-1);
		m_mapAnimationStateIndices.insert(value);

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
		tAnimationStateIndexMapIt it = m_mapAnimationStateIndices.find(asName);
		if(it != m_mapAnimationStateIndices.end())
		{
			return it->second;
		}
		else
		{
			return -1;
		}
	}
	cAnimationState* cMeshEntity::GetAnimationStateFromName(const tString &asName)
	{
		int lIdx = GetAnimationStateIndex(asName);
		if(lIdx >= 0)
		{
			return mvAnimationStates[lIdx];
		}
		else
		{
			return NULL;
		}
	}
	int cMeshEntity::GetAnimationStateNum()
	{
		return (int)mvAnimationStates.size();
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::Play(int alIndex,bool abLoop, bool bStopPrev)
	{
		if(OhplBackendRustRequested())
		{
			SyncAnimCores();
			ohpl_h11_mesh_play(mvAnimCores.data(), (int32_t)mvAnimCores.size(), alIndex, abLoop, bStopPrev);
			return;
		}
		if(bStopPrev) Stop();
		if(alIndex >= (int)mvAnimationStates.size()) return;
		
		mvAnimationStates[alIndex]->SetActive(true);
		mvAnimationStates[alIndex]->SetTimePosition(0);
		mvAnimationStates[alIndex]->SetLoop(abLoop);
		mvAnimationStates[alIndex]->SetWeight(1);
	}

	void cMeshEntity::PlayName(const tString &asName,bool abLoop, bool bStopPrev)
	{
		int lIdx = GetAnimationStateIndex(asName);
		if(lIdx>=0)
		{
			Play(lIdx,abLoop,bStopPrev);
		}
		else
		{
			Warning("Can not find animation '%s' in meshentity '%s'\n",asName.c_str(),
																	msName.c_str());
		}
	}
	
	//-----------------------------------------------------------------------

	void cMeshEntity::PlayFadeTo(int alIndex,bool abLoop, float afTime)
	{
		if(OhplBackendRustRequested())
		{
			SyncAnimCores();
			ohpl_h11_mesh_play_fade_to(mvAnimCores.data(), (int32_t)mvAnimCores.size(), alIndex, abLoop, afTime);
			return;
		}
		///////////////////////
		// Fade out previous
		for(size_t i=0; i< mvAnimationStates.size(); i++)
		{
			cAnimationState *pAnim = mvAnimationStates[i];
			if(pAnim->IsActive()) pAnim->FadeOut(afTime);
		}

		///////////////////////
		// Fade in new
		cAnimationState *pAnim = mvAnimationStates[alIndex];
		if(pAnim->IsActive()==false)
		{
			pAnim->SetWeight(0);
		}

		pAnim->SetActive(true);
		pAnim->SetTimePosition(0);
		pAnim->SetLoop(abLoop);
		pAnim->FadeIn(afTime);
	}
	
	void cMeshEntity::PlayFadeToName(const tString &asName,bool abLoop, float afTime)
	{
		int lIdx = GetAnimationStateIndex(asName);
		if(lIdx>=0)
		{
			PlayFadeTo(lIdx, abLoop, afTime);
		}
		else
		{
			Warning("Can not find animation '%s' in mesh entity '%s'\n",asName.c_str(), msName.c_str());
		}
	}

	//-----------------------------------------------------------------------

	
	void cMeshEntity::Stop()
	{
		if(OhplBackendRustRequested())
		{
			SyncAnimCores();
			ohpl_h11_mesh_stop(mvAnimCores.data(), (int32_t)mvAnimCores.size());
			return;
		}
		for(size_t i=0; i< mvAnimationStates.size(); i++)
		{
			mvAnimationStates[i]->SetActive(false);
			mvAnimationStates[i]->SetTimePosition(0);
		}
	}

	//-----------------------------------------------------------------------

	bool cMeshEntity::AnimationIsOver(const tString &asName)
	{
		int lIdx = GetAnimationStateIndex(asName);
		if(lIdx>=0)
		{
			return mvAnimationStates[lIdx]->IsOver();
		}
		else
		{
			Warning("Can not find animation '%s' in mesh entity '%s'\n",asName.c_str(), msName.c_str());
			return false;
		}
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
		tNodeStateIndexMapIt it = m_mapBoneStateIndices.find(asName);
		if(it != m_mapBoneStateIndices.end())
		{
			return it->second;
		}
		else
		{
			return -1;
		}
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
		int lIdx = GetBoneStateIndex(asName);
		if(lIdx >= 0)
		{
			return mvBoneStates[lIdx];
		}
		else
		{
			return NULL;
		}
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
		if(OhplBackendRustRequested())
		{
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
		for(int i=0; i<pSkeleton->GetBoneNum(); ++i)
		{
			const float *v = &avVals[i * 7];
			cBone *pBone = pSkeleton->GetBoneByIndex(i);
			const cMatrixf &m = pBone->GetLocalTransform();
			float fS = cMath::Min(cMath::Min(cVector3f(m.m[0][0], m.m[0][1], m.m[0][2]).Length(), cVector3f(m.m[1][0], m.m[1][1], m.m[1][2]).Length()),
								  cVector3f(m.m[2][0], m.m[2][1], m.m[2][2]).Length());
			vLocal[i] = cMath::MatrixMul(cMath::MatrixQuaternion(cQuaternion(v[0], v[1], v[2], v[3])), cMath::MatrixScale(cVector3f(fS)));
			vLocal[i].SetTranslation(cVector3f(v[4], v[5], v[6]) * fS * pBone->GetLocalUnitScale());
		}
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
		if(OhplBackendRustRequested())
		{
			ohpl_h11_mesh_physics_set(&mCtl, abX);
			ResetGraphicsUpdated();
		}
		else
		{
			mCtl.physics = abX;

			mCtl.updated_bones = false;
			ResetGraphicsUpdated();

			mCtl.fading = false;
			mCtl.physics_weight = 1.0f;
		}

		for(int bone =0; bone< GetBoneStateNum(); ++bone)
		{
			cBoneState *pState = GetBoneState(bone);
			iPhysicsBody *pBody = pState->GetBody();
			iPhysicsBody *pColliderBody = pState->GetColliderBody();

			if(pBody)
			{
				pBody->SetActive(abX);
				pBody->Enable();

				if(abX==false)
				{
					pBody->SetLinearVelocity(0);
					pBody->SetAngularVelocity(0);
				}

				if(mCtl.colliders)
				{
					pColliderBody->SetActive(!abX);
				}
			}
		}
	}

	bool cMeshEntity::GetSkeletonPhysicsActive()
	{
		return mCtl.physics;
	}

	//----------------------------------------------------------------------

	void cMeshEntity::FadeSkeletonPhysicsWeight(float afTime)
	{
		if(OhplBackendRustRequested() ? ohpl_h11_mesh_physics_fade(&mCtl, afTime) : mCtl.physics)
		{
			if(!OhplBackendRustRequested())
			{
				mCtl.fading = true;
				mCtl.physics_fade_speed = 1.0f / afTime;
			}

			for(int bone =0; bone< GetBoneStateNum(); ++bone)
			{
				cBoneState *pState = GetBoneState(bone);
				iPhysicsBody *pBody = pState->GetBody();
				iPhysicsBody *pColliderBody = pState->GetColliderBody();

				if(pBody) pBody->SetActive(false);
			}
		}
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
		mCtl.colliders = abX;

		//Set active to the correct state.
		for(int bone =0; bone< GetBoneStateNum(); ++bone)
		{
			cBoneState *pState = GetBoneState(bone);
			iPhysicsBody *pColliderBody = pState->GetColliderBody();

			if(pColliderBody)
			{
				if(abX && !mCtl.physics) pColliderBody->SetActive(true);
				else if(!abX) pColliderBody->SetActive(false);
			}
		}
	}
	bool cMeshEntity::GetSkeletonCollidersActive()
	{
		return mCtl.colliders;
	}

	//----------------------------------------------------------------------

	void cMeshEntity::AlignBodiesToSkeleton(bool abCalculateSpeed)
	{
		// The entity may be attached to one of the bodies: moving it would move the remaining bones
		std::vector<cMatrixf> vMatrices(GetBoneStateNum());
		for(int bone =0; bone< GetBoneStateNum(); ++bone)
		{
			cBoneState *pState = GetBoneState(bone);
			if(pState->GetBody()) vMatrices[bone] = cMath::MatrixMul(pState->GetWorldMatrix(), pState->GetBodyMatrix());
		}
		const cMatrixf mtxEntityWorld = GetWorldMatrix();
		// A body carrying several bones follows the first
		std::set<iPhysicsBody*> setDone;
		for(int bone =0; bone< GetBoneStateNum(); ++bone)
		{
			iPhysicsBody *pBody = GetBoneState(bone)->GetBody();
			if(pBody && setDone.insert(pBody).second) pBody->SetMatrix(vMatrices[bone]);
		}
		// Keep the mesh where it was when it hangs off a body that just moved
		if(GetEntityParent()) SetMatrix(cMath::MatrixMul(cMath::MatrixInverse(GetEntityParent()->GetWorldMatrix()), mtxEntityWorld));
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
		if(OhplBackendRustRequested())
		{
			cMatrixf mtxOut;
			cVector3f vPos;
			float fYAngle;
			ohpl_h11_transform_from_skeleton(&pBoneState->GetWorldMatrix(), &pBone->GetInvWorldTransform(), &mtxOut, &vPos, &fYAngle);
			if(apPostion) *apPostion = vPos;
			if(apAngles) *apAngles = cVector3f(0,fYAngle,0);
			return mtxOut;
		}

		cMatrixf mtxInvBind = pBone->GetInvWorldTransform();
		cMatrixf mtxInvBone = cMath::MatrixInverse(pBoneState->GetWorldMatrix());
		cVector3f vStateForward = mtxInvBone.GetForward();
		cVector3f vBindForward = mtxInvBind.GetForward();

		float fBindYAngle = -cMath::GetAngleFromPoints2D(0,cVector2f(-vBindForward.x,-vBindForward.z));
		float fStateYAngle = -cMath::GetAngleFromPoints2D(0,cVector2f(-vStateForward.x,-vStateForward.z));
		float fYAngle = fStateYAngle - fBindYAngle;

		cMatrixf mtxTransform = cMath::MatrixRotateY(fYAngle);

		cVector3f vRootBoneOffset = pBone->GetLocalTransform().GetTranslation();
		vRootBoneOffset.y =0;
		vRootBoneOffset = cMath::MatrixMul(mtxTransform,vRootBoneOffset);

		mtxTransform.SetTranslation(pBoneState->GetWorldPosition());// - vRootBoneOffset);
		
		if(apPostion) *apPostion = pBoneState->GetWorldPosition();// - vRootBoneOffset;
		if(apAngles) *apAngles = cVector3f(0,fYAngle,0);

		return mtxTransform;
	}

	//----------------------------------------------------------------------

	bool cMeshEntity::CheckColliderShapeCollision(iPhysicsWorld *apWorld,
						iCollideShape *apShape, const cMatrixf &a_mtxShape,
						tVector3fList *apPosList, tIntList *apNumList)
	{
		bool bCollision = false;
		cCollideData collideData;
		collideData.SetMaxSize(1);

		for(size_t i=0; i< mvBoneStates.size(); ++i)
		{
			cBoneState *pState = mvBoneStates[i];
			iPhysicsBody *pBody = pState->GetColliderBody();
			if(pBody==NULL) continue;

			cMatrixf mtxBody = cMath::MatrixMul(pState->GetWorldMatrix(), pState->GetBodyMatrix());
			pBody->SetMatrix(mtxBody);
			
			bool bRet = apWorld->CheckShapeCollision(pBody->GetShape(),
												pBody->GetLocalMatrix(),
												apShape,a_mtxShape,
												collideData,1, false);

			if(bRet)
			{
				bCollision = true;
				if(!apPosList && !apNumList) break;

				if(apPosList) apPosList->push_back(collideData.mvContactPoints[0].mvPoint);
				if(apNumList) apNumList->push_back((int)i);
			}
		}
		
		return bCollision;
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
		tNodeStateIndexMapIt it = m_mapNodeStateIndices.find(asName);
		if(it != m_mapNodeStateIndices.end())
		{
			return it->second;
		}
		else
		{
			return -1;
		}
	}

	cNode3D* cMeshEntity::GetNodeStateFromName(const tString &asName)
	{
		int lIdx = GetNodeStateIndex(asName);
		if(lIdx >= 0)
		{
			return mvNodeStates[lIdx];
		}
		else
		{
			return NULL;
		}
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
		tSubMeshEntityMapIt it = m_mapSubMeshes.find(asName);
		if(it == m_mapSubMeshes.end())return NULL;

		return it->second;
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
		bool bInvStale;
		if(OhplBackendRustRequested())
		{
			uint32_t lGate = ohpl_h11_mesh_graphics_gate(&mCtl, pSkeleton != NULL, GetTransformUpdateCount());
			if(lGate == 0) return;
			bInvStale = (lGate & 2) != 0;
		}
		else
		{
			if(	mCtl.matrices_need_update == false &&
				mCtl.bone_matrices_transform_count == GetTransformUpdateCount())
			{
				return;
			}

			mCtl.bone_matrices_transform_count = GetTransformUpdateCount();
			mCtl.matrices_need_update = false;

			bInvStale = pSkeleton && mCtl.inv_world_transform_count != GetTransformUpdateCount();
			if(bInvStale) mCtl.inv_world_transform_count = GetTransformUpdateCount();
		}

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
		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			mvSubMeshes[i]->SetVisible(abX);
		}
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* cMeshEntity::GetBoundingVolume()
	{
		///////////////////////////////////////////////
		// Update the BV from bones
		if(mpMesh->GetSkeleton())
		{
			if(mbUpdateBoundingVolume)
			{
				UpdateBVFromSkeleton();
				mbUpdateBoundingVolume = false;
			}
		}
		///////////////////////////////////////////////
		// Update the BV from the sub meshes
		else
		{
			if(mbUpdateBoundingVolume)
			{
				mbUpdateBoundingVolume = false;

				if(OhplBackendRustRequested())
				{
					std::vector<cVector3f> vMins, vMaxs;
					for(size_t i=0; i< mvSubMeshes.size(); i++)
					{
						cBoundingVolume *pSubBV = mvSubMeshes[i]->GetBoundingVolume();
						vMins.push_back(pSubBV->GetMin());
						vMaxs.push_back(pSubBV->GetMax());
					}
					cVector3f vRustMin, vRustMax;
					if(ohpl_h11_bounds_union(vMins.data(), vMaxs.data(), (int32_t)vMins.size(), &vRustMin, &vRustMax))
						mBoundingVolume.SetLocalMinMax(vRustMin, vRustMax);
					return &mBoundingVolume;
				}

				cBoundingVolume *pBV = mvSubMeshes[0]->GetBoundingVolume(); 
				cVector3f vFinalMin = pBV->GetMin();
				cVector3f vFinalMax = pBV->GetMax();

				for(int i=1; i< (int)mvSubMeshes.size(); i++)
				{
					cBoundingVolume *pBV = mvSubMeshes[i]->GetBoundingVolume(); 

					cVector3f vMin = pBV->GetMin();
					cVector3f vMax = pBV->GetMax();

					if(vFinalMin.x > vMin.x)vFinalMin.x = vMin.x;
					if(vFinalMax.x < vMax.x)vFinalMax.x = vMax.x;

					if(vFinalMin.y > vMin.y)vFinalMin.y = vMin.y;
					if(vFinalMax.y < vMax.y)vFinalMax.y = vMax.y;

					if(vFinalMin.z > vMin.z)vFinalMin.z = vMin.z;
					if(vFinalMax.z < vMax.z)vFinalMax.z = vMax.z;
				}

				mBoundingVolume.SetLocalMinMax(vFinalMin,vFinalMax);
			}
		}
	
		//Return the bounding volume.
		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::SetStatic(bool abX)
	{
		if(abX == mbStatic) return;

		mbStatic = abX;
		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			mvSubMeshes[i]->SetStatic(abX);
		}
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::SetRenderFlagBit(tRenderableFlag alFlagBit, bool abSet)
	{
		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			mvSubMeshes[i]->SetRenderFlagBit(alFlagBit, abSet);
		}
	}

	void cMeshEntity::SetColorMul(const cColor& aX)
	{
		for(size_t i=0; i<mvSubMeshes.size(); ++i) mvSubMeshes[i]->SetColorMul(aX);
	}

	void cMeshEntity::SetIlluminationAmount(float afX)
	{
		if(mfIlluminationAmount == afX) return;

		mfIlluminationAmount = afX;

		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			mvSubMeshes[i]->SetIlluminationAmount(mfIlluminationAmount);
		}
	}

	void cMeshEntity::SetIlluminationColor(const cColor& aX)
	{
		for(int i=0;i<(int)mvSubMeshes.size();i++)
			mvSubMeshes[i]->SetIlluminationColor(aX);
	}

	void cMeshEntity::SetCoverageAmount(float afX)
	{
		if(mfCoverageAmount == afX) return;

		mfCoverageAmount = afX;

		for(int i=0;i<(int)mvSubMeshes.size();i++)
		{
			mvSubMeshes[i]->SetCoverageAmount(mfCoverageAmount);
		}
	}

	void cMeshEntity::SetLiquidAmount(float afX)
	{
		mfMeshLiquidAmount = afX > 0 ? afX : 0;
		for(size_t i=0; i<mvSubMeshes.size(); ++i) mvSubMeshes[i]->SetLiquidAmount(mfMeshLiquidAmount);
	}

	void cMeshEntity::UseAutomaticLiquidAmount()
	{
		mfMeshLiquidAmount = -1;
		for(size_t i=0; i<mvSubMeshes.size(); ++i) mvSubMeshes[i]->UseAutomaticLiquidAmount();
	}

	
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIAVTE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	float cMeshEntity::GetAnimationWeightMul()
	{
		SyncAnimCores();
		return ohpl_meshentity_weight_mul(mvAnimCores.data(), (int)mvAnimCores.size(), mCtl.normalize);
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::CreateNodes()
	{
		/////////////////////////////////
		//Check so it is okay to add nodes
		if(mvNodeStates.empty() ==false) return;
		if(mpMesh->GetSkeleton()) return;
		if(mpMesh->GetNodeNum()==0) return;
		
		////////////////////////
		//Make sure that the submeshes are not added to t
		for(size_t i=0; i<mvSubMeshes.size(); ++i)
		{
			cSubMeshEntity *pSubEnt = mvSubMeshes[i];

            RemoveChild(pSubEnt);
			pSubEnt->SetMatrix(cMatrixf::Identity);
		}
		

		////////////////////////////////
		// Set up data

		//Reserve size for nodes to be added
		mvNodeStates.reserve(mpMesh->GetNodeNum());
		
		////////////////////////////////
		//Create the nodes and attach sub meshes
		for(int i=0; i< mpMesh->GetNodeNum(); i++)
		{
			cNode3D* pMeshNode = mpMesh->GetNode(i);

			cBoneState *pNode = hplNew( cBoneState, (pMeshNode->GetName(), false) );
			pNode->SetMatrix(pMeshNode->GetLocalMatrix());

			//Add node to array and add it's index to the map.
			mvNodeStates.push_back(pNode);
			m_mapNodeStateIndices.insert(tNodeStateIndexMap::value_type(pNode->GetName(),i));

			//Connect with sub mesh entity
			cSubMeshEntity *pSubEntity = GetSubMeshEntityName(pMeshNode->GetName());
			if(pSubEntity)
			{
				pSubEntity->SetLocalNode(pNode);
			}
		}

		////////////////////////////////
		//Set up hierarchy
		for(int i=0; i< (int)mvNodeStates.size();i++)
		{
			cNode3D* pStateNode = mvNodeStates[i];
			cNode3D* pMeshNode = mpMesh->GetNode(i);

			//Set the parent if there is one
			if(pMeshNode->GetParent())
			{
				cNode3D* pParentNode = GetNodeStateFromName(pMeshNode->GetParent()->GetName());
				if(pParentNode)
				{
					pStateNode->SetParent(pParentNode);
				}
				else 
				{
					AddNodeChild(pStateNode);
				}
			}
			//If not set root node as parent.
			else
			{
				AddNodeChild(pStateNode);
			}

			//Add children if there are any.
			cNode3DIterator it = pMeshNode->GetChildIterator();
			while(it.HasNext())
			{
				cNode3D* pChildNode = static_cast<cNode3D*>(it.Next());
				pStateNode->AddChild(GetNodeStateFromName(pChildNode->GetName()));
			}
		}

		//Make sure everything gets updated!
		SetTransformUpdated(true);
	}
	
	//-----------------------------------------------------------------------

	void cMeshEntity::HandleAnimationEvent(cAnimationEvent *apEvent)
	{
		if(mpCallback && mpCallback->OnAnimationEvent(this, apEvent)) return;
		if(apEvent->msValue == "") return;

		switch(apEvent->mType)
		{
		case eAnimationEventType_PlaySound:
			{
				cSoundEntity *pSound = mpWorld->CreateSoundEntity(msName+"_AnimEvent",apEvent->msValue,true);
				if(pSound)
				{
					pSound->SetIsSaved(false);
					cNode3DIterator nodeIt = mpBoneStateRoot->GetChildIterator();
					if(nodeIt.HasNext())
					{
						cNode3D *pNode = nodeIt.Next();
						pNode->AddEntity(pSound);
					}
					else
					{
						pSound->SetPosition(mBoundingVolume.GetWorldCenter());
					}
				}
				break;
			}
		}
	}
	
	//-----------------------------------------------------------------------
	
	void cMeshEntity::UpdateBVFromSkeleton()
	{
		if(mpMesh->GetSkeleton()==NULL) return;
		
		if(mvBoneStates.empty())
		{
			////////////////////////////////
			//Using vertices

			//Go through all the sub meshes and build BV from vertices.
			for(int i=0;i<GetSubMeshEntityNum();i++)
			{
				cSubMeshEntity* pSub = GetSubMeshEntity(i);

				iVertexBuffer *pVtxBuffer = pSub->GetVertexBuffer();

				mBoundingVolume.AddArrayPoints(pVtxBuffer->GetFloatArray(eVertexBufferElement_Position), 
																pVtxBuffer->GetVertexNum());
			}
			if(GetSubMeshEntityNum()>0)
				mBoundingVolume.CreateFromPoints(GetSubMeshEntity(0)->GetVertexBuffer()->GetElementNum(eVertexBufferElement_Position));
		}
		else
		{
			////////////////////////////////
			//Using bones
			cVector3f vMin,vMax;
			GetAABBFromBones(vMin, vMax);
			
			mBoundingVolume.SetTransform(cMatrixf::Identity);
			mBoundingVolume.SetLocalMinMax(vMin, vMax);
		}
	}

	//-----------------------------------------------------------------------

	void cMeshEntity::GetAABBFromBones(cVector3f &avMin, cVector3f &avMax)
	{
		// HPL3 0xf862e0: unweighted bones don't count
		const int lNum = (int)mvBoneStates.size();
		mvScratchWorld.resize(lNum);
		mvScratchRadii.resize(lNum);
		for(int i=0; i< lNum; ++i)
		{
			mvScratchWorld[i] = &mvBoneStates[i]->GetWorldMatrix();
			mvScratchRadii[i] = mpMesh->GetBoneBoundingRadius(i);
		}
		ohpl_meshentity_aabb_from_bones(mvScratchWorld.data(), mvScratchRadii.data(), lNum, &avMin, &avMax);
	}
	
	//-----------------------------------------------------------------------
}
