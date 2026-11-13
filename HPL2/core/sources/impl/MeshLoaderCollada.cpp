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

#include "impl/MeshLoaderCollada.h"
#include "impl/MeshLoaderMSH.h"
#include "impl/RustL6.h"

namespace hpl {

	bool cMeshLoaderCollada::mbConvertUnit = true; 
	bool cMeshLoaderCollada::mbConvertUnitFromAnyTool = false;
	bool cMeshLoaderCollada::mbUnscaledSkeleton = false;

	void cMeshLoaderCollada::SetUnscaledSkeleton(bool abX)
	{
		mbUnscaledSkeleton = abX;
		cMeshLoaderMSH::mbBoneUnitScale = abX;
	}

	cMeshLoaderCollada::cMeshLoaderCollada(iLowLevelGraphics *apLowLevelGraphics, cMeshLoaderMSH *apMeshLoaderMSH, bool abLoadAndSaveMSHFormat) 
		: iMeshLoader(apLowLevelGraphics)
	{
		mpMeshLoaderMSH = apMeshLoaderMSH;
		mbLoadAndSaveMSHFormat = abLoadAndSaveMSHFormat;

		AddSupportedExtension("dae");
		AddSupportedExtension("dae_anim");
	}

	cMeshLoaderCollada::~cMeshLoaderCollada()
	{
	}

	cMesh* cMeshLoaderCollada::LoadMesh(const tWString& asFile,tMeshLoadFlag aFlags)
	{
		OhplL6CfgBuf cfg;
		OhplL6FillCfg(cfg, mpMeshManager, mbLoadAndSaveMSHFormat, aFlags);
		OhplL6Env env = { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, mpMeshLoaderMSH };
		return OhplL6LoadMesh(eOhplL6KindCollada, env, cfg.cfg, asFile);
	}

	cAnimation* cMeshLoaderCollada::LoadAnimation(const tWString& asFile)
	{
		OhplL6CfgBuf cfg;
		OhplL6FillCfg(cfg, mpMeshManager, mbLoadAndSaveMSHFormat, 0);
		OhplL6Env env = { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, mpMeshLoaderMSH };
		return OhplL6LoadAnim(eOhplL6KindCollada, env, cfg.cfg, asFile);
	}

}
