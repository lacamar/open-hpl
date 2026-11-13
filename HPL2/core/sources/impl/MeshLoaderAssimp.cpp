#include "impl/MeshLoaderAssimp.h"
#include "impl/RustL6.h"

namespace hpl {

	cMeshLoaderAssimp::cMeshLoaderAssimp(iLowLevelGraphics *apLowLevelGraphics) : iMeshLoader(apLowLevelGraphics)
	{
		AddSupportedExtension("fbx");
	}

	cMesh* cMeshLoaderAssimp::LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags)
	{
		OhplL6CfgBuf cfg;
		OhplL6FillCfg(cfg, mpMeshManager, false, aFlags);
		OhplL6Env env = { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, NULL };
		return OhplL6LoadMesh(eOhplL6KindAssimp, env, cfg.cfg, asFile);
	}

	cAnimation* cMeshLoaderAssimp::LoadAnimation(const tWString& asFile)
	{
		OhplL6CfgBuf cfg;
		OhplL6FillCfg(cfg, mpMeshManager, false, 0);
		OhplL6Env env = { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, NULL };
		return OhplL6LoadAnim(eOhplL6KindAssimp, env, cfg.cfg, asFile);
	}

}
