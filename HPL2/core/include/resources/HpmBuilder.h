#ifndef HPL_HPM_BUILDER_H
#define HPL_HPM_BUILDER_H

// Seam between the .hpm map parser and the engine objects it creates. The parameter structs are
// plain C and mirrored by rust/src/world_hpm/abi.rs; strings are NUL-terminated and, like arrays,
// valid only for the duration of the call.

#include <cstddef>
#include <cstdint>

#include "system/SystemTypes.h"

struct OhplHpmFog
{
	bool active;
	float color[4];
	float falloffExp, fadeStart, fadeEnd;
	bool culling;
	float brightness;
	bool underwater, useSkybox, applyAfterFogAreas;
	float noiseStrength, noiseSize, noiseTurbulence[3];
	bool secondaryActive;
	float secondaryColor[4];
	float secondaryFadeStart, secondaryFadeEnd, secondaryFalloffExp;
	bool lighten, heightBased, exponential;
	float density, heightDensity, heightHorizon;
	float secondaryDensity, secondaryHeightDensity, secondaryHeightHorizon;
};

struct OhplHpmDistanceCull
{
	bool active;
	float minRange, screenSize, randomSize;
};

struct OhplHpmDirLight
{
	bool active;
	float diffuseColor[4];
	float brightness;
	float direction[3];
	float skyColor[4], groundColor[4];
	bool castShadows;
	float shadowCasterDist, shadowMapBiasMul, shadowMapSlopeScaleBiasMul, shadowBlurAmount;
	bool autoShadowSliceSettings;
	float autoShadowSliceLogTerm;
};

struct OhplHpmEnvParticles
{
	const char* texture;
	float subDivUv[2];
	float boxSize;
	int numParticles;
	float particleSize[2];
	bool affectedByLight;
	float color[4];
	float brightness, boxDistance;
	float gravityVelocity[3];
	float gravitySpeedRandomAmount;
	float windVelocity[3];
	float windSpeedRandomAmount, windDirRandomAmount;
	float rotateVelocity[3];
	float rotateSpeedRandomAmount;
	bool rotateBothDirs;
	float numIterations, fadeInStart, fadeInEnd, fadeOutStart, fadeOutEnd;
	bool active;
};

struct OhplHpmPostEffects
{
	float toneMappingKey, toneMappingExposure, toneMappingWhiteCut;
	const char* colorGradingTexture;
};

struct OhplHpmSkyBox
{
	bool active;
	float color[4];
	float brightness;
	const char* texture;
};

struct OhplHpmStaticObject
{
	const char* name;
	const char* filename;
	float position[3], scale[3], rotation[3];
	bool castsShadows, collides;
	int id;
	bool culledByDistance, culledByFog;
	float colorMul[4];
	float illumBrightness;
	float illumColor[4];
	bool hasPose;
	const float* pose;
	size_t numPose;
};

struct OhplHpmPlane
{
	const char* name;
	const char* material;
	bool castsShadows, collides;
	int id;
	float position[3], scale[3], rotation[3];
	float startCorner[3], endCorner[3];
	float uv[4][2];
	bool culledByDistance, culledByFog;
};

struct OhplHpmEntity
{
	const char* name;
	const char* filename;
	int id;
	bool active, isStatic;
	float position[3], scale[3], rotation[3];
	bool culledByDistance, culledByFog;
};

struct OhplHpmArea
{
	const char* name;
	const char* areaType;
	int id;
	bool active;
	float position[3], scale[3], rotation[3];
};

struct OhplHpmDecal
{
	const char* name;
	const char* material;
	float color[4];
	int id;
};

struct OhplHpmLightMask
{
	unsigned int id;
	float worldPos[3], rotation[3], size[3], scale[3];
};

struct OhplHpmExposureArea
{
	const char* name;
	float rotation[3], worldPos[3], scale[3];
	float exposure, whiteCut, transitionTime;
};

struct OhplHpmDetailMeshes
{
	const char* file;
	int num;
	int firstIndex;
	const float* positions;
	const float* rotations;
	const float* radii;
	size_t numRadii;
};

struct OhplHpmTerrainMat
{
	const char* file;
	float tileAmount, startFadeValue;
	float detailAmount[3];
};

struct OhplHpmTerrainLayer
{
	const char* id;
	int numMats;
	OhplHpmTerrainMat mats[4];
};

struct OhplHpmTerrainDetail
{
	const char* file;
	float scale;
};

struct OhplHpmTerrainDecal
{
	const char* file;
	float position[2], size[2], angle, color[4];
	int currentUvSubDiv;
};

struct OhplHpmUndergrowthArea
{
	const char* file;
	bool sub;
	float maxInfluence, fadeBorder;
	bool circle;
	float center[2], radius;
	const float* points;
	size_t numPoints;
};

struct OhplHpmTerrain
{
	int heightMapSize, geometryPatchSize;
	float unitSize, maxHeight, baseMaterialTileAmount;
	const char* baseMaterialFile;
	float baseMaterialDetailTextureAmount[3];
	bool hasBlendLayers;
	const OhplHpmTerrainLayer* layers;
	size_t numLayers;
	bool hasDetails;
	float detailFadeStart, detailFadeEnd;
	OhplHpmTerrainDetail details[3];
	int numDetails;
	bool hasSections;
	const OhplHpmTerrainDecal* decals;
	size_t numDecals;
	float undergrowthGridSize, undergrowthFadeStart, undergrowthFadeEnd;
	const OhplHpmUndergrowthArea* areas;
	size_t numAreas;
};

struct OhplHpmSkip
{
	const char* reason;
	int count;
};

namespace hpl {

	class cXmlElement;
	class iXmlDocument;

	// Every create method returns true when the object was made.
	class iHpmBuilder
	{
	public:
		virtual ~iHpmBuilder() {}

		virtual uint64_t NowMs() = 0;
		virtual void ObjectDone() = 0;
		virtual void Warn(const char* asMessage) = 0;
		virtual iXmlDocument* OpenXml(const tWString& asPath) = 0;

		virtual void GlobalFog(const OhplHpmFog& aFog) = 0;
		virtual void DistanceCull(const OhplHpmDistanceCull& aCull) = 0;
		virtual void DirLight(const OhplHpmDirLight& aLight) = 0;
		virtual void EnvParticles(const OhplHpmEnvParticles& aParticles) = 0;
		virtual void PostEffects(const OhplHpmPostEffects& aEffects) = 0;
		virtual void SkyBox(const OhplHpmSkyBox& aSky) = 0;

		virtual bool StaticObject(const OhplHpmStaticObject& aObject) = 0;
		virtual bool Plane(const OhplHpmPlane& aPlane) = 0;
		virtual bool Entity(const OhplHpmEntity& aEntity, cXmlElement* apElement) = 0;
		virtual bool Light(cXmlElement* apElement) = 0;
		virtual bool Area(const OhplHpmArea& aArea, cXmlElement* apElement) = 0;
		virtual bool Sound(cXmlElement* apElement, bool abActive) = 0;
		virtual bool Decal(const OhplHpmDecal& aDecal, cXmlElement* apMeshElement) = 0;
		virtual bool Billboard(cXmlElement* apElement) = 0;
		virtual bool ParticleSystem(cXmlElement* apElement, const char* asConnectLight) = 0;
		virtual bool FogArea(cXmlElement* apElement) = 0;
		virtual bool LensFlare(cXmlElement* apElement) = 0;
		virtual void LightMaskBox(const OhplHpmLightMask& aMask) = 0;

		virtual void ConnectLights(int* apMissingBillboards, int* apMissingParticles) = 0;
		virtual void ConnectLightMasks() = 0;

		// Returns how many instances were made; stops at the first mesh that fails to load.
		virtual int DetailMeshes(const OhplHpmDetailMeshes& aMeshes) = 0;
		virtual void ExposureArea(const OhplHpmExposureArea& aArea) = 0;
		virtual void Terrain(const OhplHpmTerrain& aTerrain) = 0;

		virtual void TrackReport(const char* asTrack, int alInXml, int alCreated, int alTimeMs, bool abFileMissing,
								 const OhplHpmSkip* apSkipped, size_t alNumSkipped) = 0;
	};

	// Parses the map at asFile and its sidecar files, driving aBuilder.
	void ParseHpmMap(iHpmBuilder& aBuilder, const tWString& asFile);

};
#endif // HPL_HPM_BUILDER_H
