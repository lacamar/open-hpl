
#ifndef HPL_MESH_LOADER_ASSIMP_H
#define HPL_MESH_LOADER_ASSIMP_H

#include "resources/MeshLoader.h"

namespace hpl {

	class cMeshLoaderAssimp : public iMeshLoader
	{
	public:
		cMeshLoaderAssimp(iLowLevelGraphics *apLowLevelGraphics);

		cMesh* LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags);
		bool SaveMesh(cMesh* apMesh,const tWString& asFile){ return false; }

		cAnimation* LoadAnimation(const tWString& asFile);

		static cAnimation* LoadHpl3Anm(const tString& asAnmFile, const tWString& asSourceFile, float afUnitScale);
		static bool IsHpl3Anm(const tString& asAnmFile);
		bool SaveAnimation(cAnimation* apAnimation, const tWString& asFile){ return false; }
	};

}
#endif // HPL_MESH_LOADER_ASSIMP_H
