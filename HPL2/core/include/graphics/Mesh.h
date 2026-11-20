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

#ifndef HPL_MESH_H
#define HPL_MESH_H

#include <vector>
#include <map>

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "resources/ResourceBase.h"

#include "scene/Light.h"
#include "graphics/RustMeshMm.h"
#include "graphics/SubMesh.h"
#include "graphics/Animation.h"

namespace hpl {

	class cMaterialManager;
	class cAnimationManager;
	class cSubMesh;
	class cMeshEntity;
	class cSkeleton;
	class cAnimation;
	class cNode3D;
	class iCollideShape;
	class iPhysicsWorld;
	class iPhysicsBody;
	class iPhysicsJoint;
	class cBillboard;
	class cBeam;
	class cParticleSystem;
	class cSoundEntity;
	class cWorld;

	//--------------------------------------------------

	typedef std::vector<cAnimation*> tAnimationVec;
	typedef tAnimationVec::iterator tAnimationVecIt;

	typedef std::map<tString, int> tAnimationIndexMap;
	typedef tAnimationIndexMap::iterator tAnimationIndexMapIt;

	typedef std::vector<cSubMesh*> tSubMeshVec;
	typedef std::vector<cSubMesh*>::iterator tSubMeshVecIt;

	typedef std::multimap<tString,cSubMesh*> tSubMeshMap;
	typedef tSubMeshMap::iterator tSubMeshMapIt;

	typedef std::vector<cNode3D*> tNode3DVec;
	typedef tNode3DVec::iterator tNode3DVecIt;

	//--------------------------------------------------

	class cMesh : public iResourceBase
	{
	friend class cSubMesh;
	friend class cMeshEntity;
	public:
		cMesh(const tString& asName, const tWString& asFullPath, cMaterialManager* apMaterialManager, cAnimationManager * apAnimationManager) :
			iResourceBase(asName, asFullPath,0),
			mpRs(ohpl_mm_mesh_new(OhplMmHostGet(), this, apMaterialManager)),
			mpMaterialManager(apMaterialManager), mpAnimationManager(apAnimationManager) {}
		~cMesh();

		bool CreateFromFile(const tString asFile){ return false; }

		cSubMesh* CreateSubMesh(const tString &asName)
		{
			cSubMesh* pSubMesh = hplNew( cSubMesh, (asName,mpMaterialManager) );
			ohpl_mm_mesh_add_sub(mpRs, pSubMesh, pSubMesh->mpS, asName.data(), asName.size());
			return pSubMesh;
		}
		cSubMesh* GetSubMesh(unsigned int alIdx){ return static_cast<cSubMesh*>(ohpl_mm_mesh_sub(mpRs, alIdx)); }
		int GetSubMeshIndex(const tString &asName){ return ohpl_mm_mesh_sub_index(mpRs, asName.data(), asName.size()); }
		cSubMesh* GetSubMeshName(const tString &asName){ return static_cast<cSubMesh*>(ohpl_mm_mesh_sub_by_name(mpRs, asName.data(), asName.size())); }
		int GetSubMeshNum(){ return ohpl_mm_mesh_sub_num(mpRs); }

		int GetTriangleCount(){ return ohpl_mm_mesh_triangle_count(mpRs); }

		void SetSkeleton(cSkeleton* apSkeleton){ ohpl_mm_mesh_set_skeleton(mpRs, apSkeleton); }
		cSkeleton* GetSkeleton(){ return static_cast<cSkeleton*>(ohpl_mm_mesh_skeleton(mpRs)); }

		void AddAnimation(cAnimation *apAnimation)
		{
			const tString sAnimName = apAnimation->GetName();
			ohpl_mm_mesh_add_animation(mpRs, apAnimation, sAnimName.data(), sAnimName.size());
		}
		cAnimation* GetAnimation(int alIndex){ return static_cast<cAnimation*>(ohpl_mm_mesh_animation(mpRs, alIndex)); }
		cAnimation* GetAnimationFromName(const tString& asName)
		{
			int lIdx = GetAnimationIndex(asName);
			return lIdx >= 0 ? GetAnimation(lIdx) : NULL;
		}
		int GetAnimationIndex(const tString& asName){ return ohpl_mm_mesh_animation_index(mpRs, asName.data(), asName.size()); }
		void ClearAnimations(bool abDeleteAll){ ohpl_mm_mesh_clear_animations(mpRs, abDeleteAll); }
		int GetAnimationNum(){ return ohpl_mm_mesh_animation_num(mpRs); }

		void CompileBonesAndSubMeshes(){ ohpl_mm_mesh_compile_bones(mpRs); }
		float GetBoneBoundingRadius(int alIdx){ return ohpl_mm_mesh_radius(mpRs, alIdx);}

		//Node
		cNode3D* GetRootNode(){ return static_cast<cNode3D*>(ohpl_mm_mesh_root(mpRs)); }
		void AddNode(cNode3D* apNode){ ohpl_mm_mesh_add_node(mpRs, apNode); }
		int GetNodeNum(){ return ohpl_mm_mesh_node_num(mpRs); }
		cNode3D* GetNode(int alIdx){ return static_cast<cNode3D*>(ohpl_mm_mesh_node(mpRs, alIdx)); }
		cNode3D* GetNodeByName(const tString &asName){ return static_cast<cNode3D*>(ohpl_mm_mesh_node_by_name(mpRs, asName.data(), asName.size())); }

		//Resources implementation
		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

	private:
		OhplMmMesh* mpRs;
		cMaterialManager* mpMaterialManager;
		cAnimationManager * mpAnimationManager;
	};

};
#endif // HPL_MESH_H
