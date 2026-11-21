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

#ifndef HPL_MESH_ENTITY_H
#define HPL_MESH_ENTITY_H

#include <vector>
#include <map>

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/Entity3D.h"
#include "math/MeshTypes.h"
#include "scene/SubMeshEntity.h"
#include "scene/AnimationState.h"
#include "scene/RustMeshEnt.h"
#include "scene/Node3D.h"
#include "graphics/BoneState.h"

namespace hpl {

	class cMaterialManager;
	class cMeshManager;
	class cAnimationManager;
	class cMesh;
	class cMeshEntity;
	class cAnimation;
	class iCollideShape;
	class iPhysicsBody;
	class iPhysicsWorld;
	class cWorld;

	//-----------------------------------------------------------------------

	class cMeshEntityRootNodeUpdate : public iEntityCallback
	{
	public:
		void OnTransformUpdate(iEntity3D * apEntity);
	};

	//------------------------------------------

	class cMeshEntityCallback
	{
	public:
		virtual void AfterAnimationUpdate(cMeshEntity *apMeshEntity, float afTimeStep)=0;
		virtual bool OnAnimationEvent(cMeshEntity *apMeshEntity, cAnimationEvent *apEvent){ return false; }
	};

	//------------------------------------------

	class cMeshEntity : public iEntity3D
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
	friend class cSubMeshEntity;
	friend class cMeshEntityRootNodeUpdate;
	friend class cMesh;
	friend struct OhplMeFxImpl;
	public:
		cMeshEntity(const tString asName,cMesh* apMesh, cMaterialManager* apMaterialManager,
					cMeshManager* apMeshManager, cAnimationManager *apAnimationManager);
		~cMeshEntity();

		void SetWorld(cWorld *apWorld){ mpWorld = apWorld;}
		cWorld* GetWorld(){ return mpWorld;}

		cMesh* GetMesh(){ return mpMesh;}

		void SetCallback(cMeshEntityCallback *apCallback){mpCallback = apCallback;}

		//Sub mesh entities
		cSubMeshEntity* GetSubMeshEntity(unsigned int alIdx){ return static_cast<cSubMeshEntity*>(ohpl_me_sub(mpM, alIdx));}
		cSubMeshEntity* GetSubMeshEntityName(const tString &asName){ return GetSubMeshEntity(ohpl_me_sub_index(mpM, asName.data(), asName.size()));}
		int GetSubMeshEntityNum(){ return ohpl_me_sub_num(mpM);}

		//Animation states
		cAnimationState* AddAnimation(cAnimation *apAnimation,const tString &asName, float afBaseSpeed)
		{
			return static_cast<cAnimationState*>(ohpl_me_add_animation(mpM, apAnimation, asName.data(), asName.size(), afBaseSpeed));
		}
		void ClearAnimations(){ ohpl_me_clear_animations(mpM);}
		cAnimationState* GetAnimationState(int alIndex){ return static_cast<cAnimationState*>(ohpl_me_anim_state(mpM, alIndex));}
		int GetAnimationStateIndex(const tString &asName){ return ohpl_me_anim_index(mpM, asName.data(), asName.size());}
		cAnimationState* GetAnimationStateFromName(const tString &asName){ return GetAnimationState(GetAnimationStateIndex(asName));}
		int GetAnimationStateNum(){ return ohpl_me_anim_num(mpM);}

		//Animation controller
		void Play(int alIndex,bool abLoop, bool bStopPrev){ ohpl_me_play(mpM, alIndex, abLoop, bStopPrev);}
		void PlayName(const tString &asName,bool abLoop, bool bStopPrev){ ohpl_me_play_name(mpM, asName.data(), asName.size(), abLoop, 0, false, bStopPrev);}
		void Stop(){ ohpl_me_stop(mpM);}
		void PlayFadeTo(int alIndex,bool abLoop, float afTime){ ohpl_me_play_fade_to(mpM, alIndex, abLoop, afTime);}
		void PlayFadeToName(const tString &asName,bool abLoop, float afTime){ ohpl_me_play_name(mpM, asName.data(), asName.size(), abLoop, afTime, true, false);}
		bool AnimationIsOver(const tString &asName){ return ohpl_me_anim_is_over(mpM, asName.data(), asName.size());}

		void SetNormalizeAnimationWeights(bool abX){ mCtl.normalize = abX;}
		bool GetNormalizeAnimationWeights(){  return mCtl.normalize;}

		//Bone states
		cNode3D* GetBoneStateRoot(){ return static_cast<cNode3D*>(ohpl_me_root(mpM));}
		cBoneState* GetBoneState(int alIndex){ return static_cast<cBoneState*>(static_cast<cNode3D*>(ohpl_me_bone(mpM, alIndex)));}
		int GetBoneStateIndex(const tString &asName){ return ohpl_me_bone_index(mpM, asName.data(), asName.size());}
		int GetBoneStateIndexFromPtr(cBoneState* apBoneState){ return ohpl_me_bone_index_of(mpM, static_cast<cNode3D*>(apBoneState));}
		cBoneState* GetBoneStateFromName(const tString &asName){ return GetBoneState(GetBoneStateIndex(asName));}
		int GetBoneStateNum(){ return ohpl_me_bone_num(mpM);}

		void SetBoneRestPose(const std::vector<cMatrixf> &avLocal){ ohpl_me_set_rest_pose(mpM, avLocal.data(), avLocal.size());}
		bool SetBoneRestPoseFromMapValues(const tFloatVec &avVals);

		//Skeleton physics
		void SetSkeletonPhysicsActive(bool abX){ ohpl_me_physics_active(mpM, abX);}
		bool GetSkeletonPhysicsActive(){ return mCtl.physics;}
		void SetSkeletonPhysicsCanSleep(bool abX){mCtl.can_sleep = abX;}
		bool GetSkeletonPhysicsCanSleep(){ return mCtl.can_sleep;}
		float GetSkeletonPhysicsWeight(){ return mCtl.physics_weight;}
		void SetSkeletonPhysicsWeight(float afX){ mCtl.physics_weight = afX;}
		void FadeSkeletonPhysicsWeight(float afTime){ ohpl_me_physics_fade(mpM, afTime);}

		void SetSkeletonCollidersActive(bool abX){ ohpl_me_colliders_active(mpM, abX);}
		bool GetSkeletonCollidersActive(){ return mCtl.colliders;}

		void AlignBodiesToSkeleton(bool abCalculateSpeed){ ohpl_me_align_bodies(mpM);}

		/**
		 * Calculates the transform (and angles and postion if wanted) of a mesh based on the postion of the root bone.
		 * This is useful when going from rag doll to mesh.
		 * \param *apPostion Can be NULL, the postion
		 * \param *apAngles Can be NULL, the angles.
		 */
		cMatrixf CalculateTransformFromSkeleton(cVector3f *apPostion,cVector3f *apAngles)
		{
			cMatrixf mtxOut = cMatrixf::Identity;
			cVector3f vPos(0);
			float fYAngle = 0;
			ohpl_me_transform_from_skeleton(mpM, &mtxOut, &vPos, &fYAngle);
			if(apPostion) *apPostion = vPos;
			if(apAngles) *apAngles = cVector3f(0,fYAngle,0);
			return mtxOut;
		}

		/**
		 * Checks collision with the skeletons collider boides
		 * \param *apWorld Physics world
		 * \param *apShape The shape
		 * \param &a_mtxShape The shapes matrix
		 * \param *apPosList A list of positions that all contact points are stored in. can be NULL.
		 * \param *apNumList A list of ints of number of the bone state body hit. can be NULL.
		 */
		bool CheckColliderShapeCollision(iPhysicsWorld *apWorld,
										iCollideShape *apShape, const cMatrixf &a_mtxShape,
										tVector3fList *apPosList, tIntList *apNumList);

		void ResetGraphicsUpdated(){ ohpl_me_reset_graphics(mpM);}

		//Node states
		cNode3D* GetNodeState(int alIndex){ return static_cast<cNode3D*>(ohpl_me_node(mpM, alIndex));}
		int GetNodeStateIndex(const tString &asName){ return ohpl_me_node_index(mpM, asName.data(), asName.size());}
		cNode3D* GetNodeStateFromName(const tString &asName){ return GetNodeState(GetNodeStateIndex(asName));}
		int GetNodeStateNum(){ return ohpl_me_node_num(mpM);}

		//Entity implementation
		tString GetEntityType(){ return "MeshEntity";}

		bool IsVisible(){ return mbIsVisible; }
		void SetVisible(bool abVisible)
		{
			if(abVisible == mbIsVisible) return;
			mbIsVisible = abVisible;
			ohpl_me_fan_visible(mpM, abVisible);
		}
		void SetCulledByDistance(bool abX){ for(int i=0; i<GetSubMeshEntityNum(); ++i) GetSubMeshEntity(i)->SetCulledByDistance(abX); }
		void SetCulledByFog(bool abX){ for(int i=0; i<GetSubMeshEntityNum(); ++i) GetSubMeshEntity(i)->SetCulledByFog(abX); }

		cBoundingVolume* GetBoundingVolume(){ ohpl_me_bounding_volume(mpM); return &mBoundingVolume;}

		void SetStatic(bool abX)
		{
			if(abX == mbStatic) return;
			mbStatic = abX;
			ohpl_me_fan_static(mpM, abX);
		}
		bool IsStatic(){ return mbStatic;}

		void SetRenderFlagBit(tRenderableFlag alFlagBit, bool abSet){ ohpl_me_fan_render_flag(mpM, alFlagBit, abSet);}

		void SetIlluminationAmount(float afX)
		{
			if(mfIlluminationAmount == afX) return;
			mfIlluminationAmount = afX;
			ohpl_me_fan_illum(mpM, afX);
		}
		void SetIlluminationColor(const cColor& aX){ ohpl_me_fan_illum_color(mpM, &aX);}
		void SetColorMul(const cColor& aX){ ohpl_me_fan_color_mul(mpM, &aX);}
		float GetIlluminationAmount(){ return mfIlluminationAmount; }

		void SetCoverageAmount(float afX)
		{
			if(mfCoverageAmount == afX) return;
			mfCoverageAmount = afX;
			ohpl_me_fan_coverage(mpM, afX);
		}
		float GetCoverageAmount(){ return mfCoverageAmount;}

		void SetLiquidAmount(float afX)
		{
			mfMeshLiquidAmount = afX > 0 ? afX : 0;
			ohpl_me_fan_liquid(mpM, mfMeshLiquidAmount);
		}
		void UseAutomaticLiquidAmount()
		{
			mfMeshLiquidAmount = -1;
			ohpl_me_fan_auto_liquid(mpM);
		}
		float GetLiquidAmount(){ return mfMeshLiquidAmount;}

		void UpdateLogic(float afTimeStep){ ohpl_me_update_logic(mpM, mbStatic, afTimeStep);}
		void UpdateGraphicsForFrame(float afFrameTime){ ohpl_me_update_graphics(mpM);}

		void SetBody(iPhysicsBody* apBody){ mpBody = apBody;}
		iPhysicsBody* GetBody(){ return mpBody;}

		void SetUserData(void* apData) { mpUserData = apData; }
		void* GetUserData() { return mpUserData; }

	private:
		void InitSubBounds(cSubMeshEntity *apSub, iVertexBuffer *apVtxBuffer);

		cMaterialManager* mpMaterialManager;
		cMeshManager* mpMeshManager;
		cAnimationManager *mpAnimationManager;

		cWorld *mpWorld = NULL;

		bool mbIsVisible = true;
		float mfIlluminationAmount = 1.0f;
		float mfCoverageAmount = 1.0f;
		float mfMeshLiquidAmount = -1;

		OhplH11MeshCtl mCtl;
		bool mbStatic = false;

		cMesh* mpMesh;
		cMeshEntityCallback *mpCallback = NULL;
		iPhysicsBody *mpBody = NULL;
		void* mpUserData = NULL;
		OhplMeshEnt *mpM;
	};

	//-----------------------------------------------------------------------

};
#endif // HPL_MESH_ENTITY_H
