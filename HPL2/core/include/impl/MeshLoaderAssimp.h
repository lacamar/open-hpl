/*
 * FBX meshes through assimp. SOMA ships 121 meshes as .fbx only (characters,
 * creatures, animated props). Static geometry in bind pose: no skeleton, no
 * animations yet.
 */

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

		cAnimation* LoadAnimation(const tWString& asFile){ return NULL; }
		bool SaveAnimation(cAnimation* apAnimation, const tWString& asFile){ return false; }
	};

}
#endif // HPL_MESH_LOADER_ASSIMP_H
