#ifndef HPL_WORLD_LOADER_HPM_BUILDER_H
#define HPL_WORLD_LOADER_HPM_BUILDER_H

#include "resources/HpmBuilder.h"

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

	struct cHpmTrackStats
	{
		cHpmTrackStats() : mlInXml(0), mlCreated(0), mlTimeMs(0), mbFileMissing(false) {}
		int mlInXml;
		int mlCreated;
		int mlTimeMs;
		bool mbFileMissing;
		std::map<tString, int> mmapSkipped;
	};

	class cHpmWorldBuilder : public iHpmBuilder
	{
	public:
		cHpmWorldBuilder(cWorld* apWorld, iPhysicsWorld* apPhysicsWorld, cResources* apResources, cGraphics* apGraphics,
						 void (*apObjectDone)(cWorld*), cXmlElement** appCurrentElement);

		// Static batches, mesh combining and world compile once everything is parsed.
		void Finish();

		const std::map<tString, cHpmTrackStats>& GetTrackStats() { return mmapTrackStats; }
		bool IsTerrainActive() { return mbTerrainActive; }

		uint64_t NowMs();
		void ObjectDone();
		void Warn(const char* asMessage);
		void Log(const char* asMessage);
		int ReadFile(const tWString& asPath, std::vector<char>& avData);

		void GlobalFog(const OhplHpmFog& aFog);
		void DistanceCull(const OhplHpmDistanceCull& aCull);
		void DirLight(const OhplHpmDirLight& aLight);
		void EnvParticles(const OhplHpmEnvParticles& aParticles);
		void PostEffects(const OhplHpmPostEffects& aEffects);
		void SkyBox(const OhplHpmSkyBox& aSky);

		bool StaticObject(const OhplHpmStaticObject& aObject);
		bool Plane(const OhplHpmPlane& aPlane);
		bool Entity(const OhplHpmEntity& aEntity, cXmlElement* apElement);
		bool Light(cXmlElement* apElement);
		bool Area(const OhplHpmArea& aArea, cXmlElement* apElement);
		bool Sound(cXmlElement* apElement, bool abActive);
		bool Decal(const OhplHpmDecal& aDecal, cXmlElement* apMeshElement);
		bool Billboard(cXmlElement* apElement);
		bool ParticleSystem(cXmlElement* apElement, const char* asConnectLight);
		bool FogArea(cXmlElement* apElement);
		bool LensFlare(cXmlElement* apElement);
		void LightMaskBox(const OhplHpmLightMask& aMask);

		void ConnectLights(int* apMissingBillboards, int* apMissingParticles);
		void ConnectLightMasks();

		int DetailMeshes(const OhplHpmDetailMeshes& aMeshes);
		void ExposureArea(const OhplHpmExposureArea& aArea);
		void Terrain(const OhplHpmTerrain& aTerrain);

		void TrackReport(const char* asTrack, int alInXml, int alCreated, int alTimeMs, bool abFileMissing,
						 const OhplHpmSkip* apSkipped, size_t alNumSkipped);

	private:
		void CreateTerrainDecals(const OhplHpmTerrain& aTerrain, const std::vector<cSubMeshEntity*>& avPatches);
		void CreateTerrainUndergrowth(const OhplHpmTerrain& aTerrain, const std::vector<float>& avHeight, int alSize, float afUnit);
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

		std::map<tString, cHpmTrackStats> mmapTrackStats;
		tEFL_LightBillboardConnectionList mlstLightBillboardConnections;
		std::vector<std::pair<cParticleSystem*, tString> > mvLightParticleConnections;
		std::map<unsigned int, std::pair<cVector3f, cVector3f> > mmapLightMasks;
		bool mbTerrainActive;
	};

};
#endif // HPL_WORLD_LOADER_HPM_BUILDER_H
