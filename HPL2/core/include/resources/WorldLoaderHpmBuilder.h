#ifndef HPL_WORLD_LOADER_HPM_BUILDER_H
#define HPL_WORLD_LOADER_HPM_BUILDER_H

#include "resources/HpmBuilder.h"
#include "resources/RustLdTerrain.h"

#include "resources/ResourcesTypes.h"
#include "resources/EngineFileLoading.h"
#include "scene/SceneTypes.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"

#include <map>
#include <tuple>

namespace hpl {

	class iEntity3D;
	class cResources;
	class cGraphics;
	class cWorld;
	class iPhysicsWorld;
	class cMeshEntity;
	class cParticleSystem;
	class iCollideShape;

	class cHpmWorldBuilder : public iHpmBuilder
	{
	public:
		cHpmWorldBuilder(cWorld* apWorld, iPhysicsWorld* apPhysicsWorld, cResources* apResources, cGraphics* apGraphics,
						 void (*apObjectDone)(cWorld*), cXmlElement** appCurrentElement);

		// Static batches, mesh combining and world compile once everything is parsed.
		void Finish();

		uint64_t NowMs();
		void ObjectDone();
		void Warn(const char* asMessage);
		void Log(const char* asMessage);
		int ReadFile(const tWString& asPath, std::vector<char>& avData);

		void DirLight(const OhplHpmDirLight& aLight);
		void EnvParticles(const OhplHpmEnvParticles& aParticles);
		void PostEffects(const OhplHpmPostEffects& aEffects);
		void SkyBox(const OhplHpmSkyBox& aSky);

		bool StaticObject(const OhplHpmStaticObject& aObject);
		bool Plane(const OhplHpmPlane& aPlane);
		bool Entity(const OhplHpmEntity& aEntity, cXmlElement* apElement);
		bool Area(const OhplHpmArea& aArea, cXmlElement* apElement);
		bool Decal(const OhplHpmDecal& aDecal, cXmlElement* apMeshElement);


		int DetailMeshes(const OhplHpmDetailMeshes& aMeshes);
		void ExposureArea(const OhplHpmExposureArea& aArea);
		void Terrain(const OhplHpmTerrain& aTerrain);

	private:
		void CreateTerrainDecals(const OhplHpmTerrain& aTerrain, const std::vector<cSubMeshEntity*>& avPatches);
		void CreateTerrainUndergrowth(const OhplHpmTerrain& aTerrain, const OhplLdTerrain* apTerrain, int alSize, float afUnit);
		std::vector<cMaterial*> CreateTerrainBlendMaterials(const OhplHpmTerrain& aTerrain, const tWString& asBaseFile, float afSize);

		void CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const cMatrixf& a_mtxTransform, const cVector3f& avScale);
		bool CreateStaticBodiesFromEnt(const tString& asFile, const cMatrixf& a_mtxTransform, const cVector3f& avScale, const tString& asName);

		cWorld* mpCurrentWorld;
		iPhysicsWorld* mpCurrentPhysicsWorld;
		cResources* mpResources;
		cGraphics* mpGraphics;
		void (*mpObjectDoneCallback)(cWorld*);
		cXmlElement** mppCurrentElement;

		struct cStaticBatch
		{
			std::vector<float> mvPos;
			std::vector<unsigned int> mvIdx;
		};
		typedef std::tuple<tString, bool, int, int, int> tStaticBatchKey;
		void FlushStaticBatch(const tStaticBatchKey& aKey, cStaticBatch& aBatch);
		void BuildShapeBodies();
		std::map<tStaticBatchKey, cStaticBatch> m_mapStaticBatches;
		int mlCombinedObjects;
		void CombineStaticMeshes();
		std::vector<cMeshEntity*> mvCombineCandidates;
		int mlCombinedMeshes;

		struct cStaticShapeBatch
		{
			tString msName;
			std::vector<iCollideShape*> mvShapes;
		};
		typedef std::tuple<tString, bool, bool, bool, int, int, int> tStaticShapeBatchKey;
		std::map<tStaticShapeBatchKey, cStaticShapeBatch> m_mapStaticShapeBatches;
	};

};
#endif // HPL_WORLD_LOADER_HPM_BUILDER_H
