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

#ifndef HPL_MESH_LOADER_COLLADA_H
#define HPL_MESH_LOADER_COLLADA_H


#include "math/MathTypes.h"
#include "resources/MeshLoader.h"
#include "graphics/VertexBuffer.h"
#include "physics/PhysicsJoint.h"
#include "system/MemoryManager.h"
#include "impl/MeshLoaderMSH.h"
#include "impl/RustL6.h"


namespace hpl {

	class cMesh;
	class cNode3D;
	class iVertexBuffer;
	class cBone;
	class cAnimation;
	class cAnimationTrack;
	class cSkeleton;
	class cWorld;
	class cSector;
	class cMeshEntity;
	class iPhysicsBody;
	class cMeshJoint;
	class cSystem;
	class cMeshLoaderMSH;

	class cMeshLoaderCollada : public iMeshLoader
	{
	public:
		cMeshLoaderCollada(iLowLevelGraphics *apLowLevelGraphics, cMeshLoaderMSH *apMeshLoaderMSH, bool abLoadAndSaveMSHFormat)
			: iMeshLoader(apLowLevelGraphics), mpMeshLoaderMSH(apMeshLoaderMSH), mbLoadAndSaveMSHFormat(abLoadAndSaveMSHFormat)
		{
			AddSupportedExtension("dae");
			AddSupportedExtension("dae_anim");
		}

		void SetLoadAndSaveMSHFormat(bool abX){ mbLoadAndSaveMSHFormat = abX;} //Needed for tools!

		cMesh* LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags)
		{
			OhplL6CfgBuf cfg;
			OhplL6FillCfg(cfg, mpMeshManager, mbLoadAndSaveMSHFormat, aFlags);
			return OhplL6LoadMesh(eOhplL6KindCollada, Env(), cfg.cfg, asFile);
		}
		bool SaveMesh(cMesh* apMesh,const tWString& asFile){return false;}

		cAnimation* LoadAnimation(const tWString& asFile)
		{
			OhplL6CfgBuf cfg;
			OhplL6FillCfg(cfg, mpMeshManager, mbLoadAndSaveMSHFormat, 0);
			return OhplL6LoadAnim(eOhplL6KindCollada, Env(), cfg.cfg, asFile);
		}
		bool SaveAnimation(cAnimation* apAnimation, const tWString& asFile){return false;}

		static void SetConvertUnit(bool abX) { mbConvertUnit = abX; }
		static bool GetConvertUnit() { return mbConvertUnit; }
		static void SetConvertUnitFromAnyTool(bool abX) { mbConvertUnitFromAnyTool = abX; }
		static bool GetConvertUnitFromAnyTool() { return mbConvertUnitFromAnyTool; }
		static void SetUnscaledSkeleton(bool abX) { mbUnscaledSkeleton = abX; cMeshLoaderMSH::mbBoneUnitScale = abX; }
		static bool GetUnscaledSkeleton() { return mbUnscaledSkeleton; }

	private:
		OhplL6Env Env() const { return { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, mpMeshLoaderMSH }; }
		cMeshLoaderMSH *mpMeshLoaderMSH;
		bool mbLoadAndSaveMSHFormat;

		inline static bool mbConvertUnit = true;
		inline static bool mbConvertUnitFromAnyTool = false;
		inline static bool mbUnscaledSkeleton = false;
	};

};
#endif // HPL_MESH_LOADER_COLLADA_H
