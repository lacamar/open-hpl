/*
 * SOMA Phase 1 data loading: a NEW, additive world loader for SOMA's split
 * ".hpm" map format.
 *
 * SOMA's level editor saves a map as one root ".hpm" file (global settings +
 * a per-user "RegisteredUsers" list, no actual scene content) plus a set of
 * sibling "track" files sharing the same base name, one per object category:
 * ".hpm_StaticObject", ".hpm_Primitive", ".hpm_Entity", ".hpm_Light",
 * ".hpm_Area", ".hpm_Sound", ".hpm_Decal", ".hpm_Terrain", etc.
 *
 * This is the SAME conceptual schema Amnesia's single-file ".map" format
 * uses (see WorldLoaderHplMap.h/.cpp) - StaticObjects with a FileIndex
 * table, Primitives (procedural planes), Entities with their own FileIndex
 * table, Lights, Areas, Sounds - just split into separate files instead of
 * one "MapContents" element, and with each object category further split
 * into per-editor-user "Section" blocks (SOMA's map format supports
 * multi-user collaborative editing; Amnesia's does not). Critically, each
 * Section carries its OWN local FileIndex table scoped to that section's
 * objects only - there is no single map-wide file index like Amnesia has.
 *
 * This class is a NEW, standalone loader - it does not modify or subclass
 * cWorldLoaderHplMap (Amnesia's live load-bearing loader). It registers for
 * the "hpm" extension only, so Amnesia's ".map"/".cmap" loading is
 * completely unaffected. Where SOMA's per-object XML schema is identical to
 * Amnesia's (StaticObject/Plane primitive attributes, and the Light/Sound/
 * Billboard/FogArea/ParticleSystem element schemas), this loader calls the
 * exact same shared, stateless cEngineFileLoading helpers Amnesia's loader
 * uses - those are reused unmodified, not reimplemented.
 *
 * Deliberately out of scope for Phase 1 (see soma/plan notes):
 *  - Terrain (".hpm_Terrain") is logged and skipped unconditionally. HPL2
 *    has no terrain renderer at all; every SOMA map's terrain track has
 *    Active="false" in practice, so this is a safe, confirmed no-op.
 *  - Physics/collision bodies are NOT created for any loaded geometry.
 *    Phase 1 has no player controller, so collision is unnecessary; this
 *    also lets us skip Amnesia's substantial mesh/body-combining pipeline
 *    (cRenderableContainer_BoxTree leaf batching) entirely and instead
 *    create one cMeshEntity per source object directly - functionally
 *    equivalent geometry, just not batched for render/physics efficiency.
 *  - Decals, Billboards, ParticleSystems, FogAreas, LensFlares,
 *    ExposureAreas, LightMasks, StaticComboAreas, DetailMeshes and
 *    StaticObjectCombos/Compounds (grouping-only, does not affect final
 *    world-space transforms) are all logged and skipped. None of these are
 *    required to render recognizable room/prop geometry.
 */

#ifndef HPL_WORLD_LOADER_HPM_H
#define HPL_WORLD_LOADER_HPM_H

#include "resources/WorldLoader.h"

#include "resources/ResourcesTypes.h"
#include "scene/SceneTypes.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"

namespace hpl {

	class iEntity3D;
	class cXmlElement;
	class iXmlDocument;
	class iPhysicsWorld;

	//----------------------------------------

	class cWorldLoaderHpm : public iWorldLoader
	{
	public:
		cWorldLoaderHpm();
		~cWorldLoaderHpm();

		cWorld* LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags);

	private:
		////////////////////////////////////////
		// Sidecar-file helpers
		iXmlDocument* OpenSidecar(const tWString& asBaseFile, const tWString& asSuffix, bool abWarnIfMissing);
		cXmlElement* GetTrackRoot(iXmlDocument* apDoc, const tString& asExpectedRootValue);
		void LoadLocalFileIndex(cXmlElement* apSection, const tString& asIndexElement, tStringVec& avIndexOut);

		////////////////////////////////////////
		// Root .hpm: GlobalSettings (fog/skybox)
		void LoadGlobalSettings(const tWString& asBaseFile);

		////////////////////////////////////////
		// Per-track loaders (each opens its own sidecar file)
		void LoadStaticObjectsTrack(const tWString& asBaseFile);
		void LoadPrimitivesTrack(const tWString& asBaseFile);
		void LoadEntitiesTrack(const tWString& asBaseFile);
		void LoadLightsTrack(const tWString& asBaseFile);
		void LoadAreasTrack(const tWString& asBaseFile);
		void LoadSoundsTrack(const tWString& asBaseFile);
		void CheckTerrainTrackInactive(const tWString& asBaseFile);

		////////////////////////////////////////
		// Per-object creation (mirrors cWorldLoaderHplMap's per-object logic)
		void CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex);
		void CreatePlanePrimitive(cXmlElement* apElement);
		void CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex);
		void CreateMapArea(cXmlElement* apElement);

		bool CheckTransformValidity(const tString& asName, const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale);

		cWorld* mpCurrentWorld;
		iPhysicsWorld* mpCurrentPhysicsWorld;

		int mlStaticObjectsCreated;
		int mlPrimitivesCreated;
		int mlEntitiesCreated;
		int mlLightsCreated;
		int mlAreasCreated;
		int mlSoundsCreated;
	};

};
#endif // HPL_WORLD_LOADER_HPM_H
