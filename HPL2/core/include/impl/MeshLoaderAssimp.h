#ifndef HPL_MESH_LOADER_ASSIMP_H
#define HPL_MESH_LOADER_ASSIMP_H


#include "resources/MeshLoader.h"
#include "impl/RustL6.h"

namespace hpl {

	class cMeshLoaderAssimp : public iMeshLoader
	{
	public:
		cMeshLoaderAssimp(iLowLevelGraphics *apLowLevelGraphics) : iMeshLoader(apLowLevelGraphics) { AddSupportedExtension("fbx"); }

		cMesh* LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags) { return Load(asFile, aFlags, true).mesh; }
		bool SaveMesh(cMesh* apMesh,const tWString& asFile){ return false; }

		cAnimation* LoadAnimation(const tWString& asFile) { return Load(asFile, 0, false).anim; }
		bool SaveAnimation(cAnimation* apAnimation, const tWString& asFile){ return false; }

	private:
		struct Result { cMesh* mesh; cAnimation* anim; };
		Result Load(const tWString& asFile, uint32_t aFlags, bool abMesh)
		{
			OhplL6CfgBuf cfg;
			OhplL6FillCfg(cfg, mpMeshManager, false, aFlags);
			OhplL6Env env = { mpLowLevelGraphics, mpMaterialManager, mpAnimationManager, NULL };
			Result r = { NULL, NULL };
			if(abMesh) r.mesh = OhplL6LoadMesh(eOhplL6KindAssimp, env, cfg.cfg, asFile);
			else r.anim = OhplL6LoadAnim(eOhplL6KindAssimp, env, cfg.cfg, asFile);
			return r;
		}
	};

}
#endif // HPL_MESH_LOADER_ASSIMP_H
