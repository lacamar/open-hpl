
#ifndef HPL_WORLD_LOADER_HPM_H
#define HPL_WORLD_LOADER_HPM_H

#include "resources/WorldLoader.h"

#include "resources/ResourcesTypes.h"
#include "resources/EngineFileLoading.h"
#include "scene/SceneTypes.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"

#include <map>

namespace hpl {

	class iEntity3D;
	class cXmlElement;
	class iXmlDocument;
	class iPhysicsWorld;
	class cMeshEntity;
	class cParticleSystem;
	class iCollideShape;

	class cWorldLoaderHpm : public iWorldLoader
	{
	public:
		cWorldLoaderHpm();
		~cWorldLoaderHpm();

		cWorld* LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags);

		static const tString& GetLastLoadReportJson() { return msLastLoadReportJson; }
		static cXmlElement* GetCurrentElement() { return mpCurrentElement; }

	private:
		iXmlDocument* OpenSidecar(const tWString& asBaseFile, const tWString& asSuffix, bool abWarnIfMissing);
		cXmlElement* GetTrackRoot(iXmlDocument* apDoc, const tString& asExpectedRootValue);
		void LoadLocalFileIndex(cXmlElement* apSection, const tString& asIndexElement, tStringVec& avIndexOut);

		void LoadGlobalSettings(const tWString& asBaseFile);

		void LoadTrack(const tWString& asBaseFile, const tString& asTrack, const tString& asFileIndexElement);
		tString CreateTrackObject(const tString& asTrack, cXmlElement* apElement, const tStringVec& avFileIndex);
		void ConnectLightBillboards();
		void ConnectLightMasks();
		void CountUnsupportedFlatTracks(const tWString& asBaseFile);
		void LoadDetailMeshesTrack(const tWString& asBaseFile);
		void BuildLoadReport(const tString& asMap, int alTotalTimeMs);
		void LoadExposureAreaTrack(const tWString& asBaseFile);
		void LoadTerrain(const tWString& asBaseFile);
		void CreateTerrain(const tWString& asBaseFile, cXmlElement* apTerrain);
		std::vector<cMaterial*> CreateTerrainBlendMaterials(const tWString& asBaseFile, cXmlElement* apTerrain, float afSize);

		tString CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex);
		tString CreatePlanePrimitive(cXmlElement* apElement);
		tString CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex);
		tString CreateMapArea(cXmlElement* apElement);
		tString CreateDecal(cXmlElement* apElement, const tStringVec& avFileIndex);

		void CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const tString& asName);
		bool CreateStaticBodiesFromEnt(const tString& asFile, const cMatrixf& a_mtxTransform, const cVector3f& avScale, const tString& asName);

		bool CheckTransformValidity(const tString& asName, const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale);

		cWorld* mpCurrentWorld;
		iPhysicsWorld* mpCurrentPhysicsWorld;
		std::map<tString, iCollideShape*> m_mapStaticShapes;

		struct cHpmTrackStats
		{
			cHpmTrackStats() : mlInXml(0), mlCreated(0), mlTimeMs(0), mbFileMissing(false) {}
			int mlInXml;
			int mlCreated;
			int mlTimeMs;
			bool mbFileMissing;
			std::map<tString, int> mmapSkipped;
		};
		std::map<tString, cHpmTrackStats> mmapTrackStats;
		tEFL_LightBillboardConnectionList mlstLightBillboardConnections;
		std::vector<std::pair<cParticleSystem*, tString> > mvLightParticleConnections;
		std::map<unsigned int, std::pair<cVector3f, cVector3f> > mmapLightMasks;
		bool mbTerrainActive;

		static tString msLastLoadReportJson;
		static cXmlElement* mpCurrentElement;
	};

};
#endif // HPL_WORLD_LOADER_HPM_H
