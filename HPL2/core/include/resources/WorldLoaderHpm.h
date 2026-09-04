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
 *  - Physics/collision bodies are NOT batched into combined per-region
 *    shapes the way Amnesia's loader does (cRenderableContainer_BoxTree leaf
 *    batching) - this still creates one cMeshEntity per source object
 *    directly. A real player controller (see soma/src/game/SomaPlayer.h)
 *    does now exist, though, so StaticObject/Primitive geometry DOES get a
 *    real static (mass 0) collision body each, one (possibly compound, for
 *    multi-submesh meshes) iCollideShape per object rather than one combined
 *    shape per spatial group - see CreateStaticBodyForMesh() in the .cpp.
 *    This is real functional collision, just not batched for physics
 *    broadphase efficiency the way Amnesia's is (a real map's few hundred
 *    static objects is nowhere near what made Amnesia's batching necessary,
 *    which existed mainly to keep triangle counts in single mesh shapes
 *    sane, not for HPL2's Newton-based broadphase specifically).
 *    Dynamic map Entities (props, furniture, doors - CreateMapEntity()) do
 *    NOT get a collision body from this loader at all yet - only
 *    StaticObjects/Primitives (walls/floors/ceilings). A real map's props
 *    are still walk-through until that's added too.
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
	class cMeshEntity;

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
		void LoadExposureAreaTrack(const tWString& asBaseFile);
		void CheckTerrainTrackInactive(const tWString& asBaseFile);

		////////////////////////////////////////
		// Per-object creation (mirrors cWorldLoaderHplMap's per-object logic)
		void CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex);
		void CreatePlanePrimitive(cXmlElement* apElement);
		void CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex);
		void CreateMapArea(cXmlElement* apElement);

		// Static collision body for a StaticObject/Primitive mesh entity, one
		// (possibly compound, for multi-submesh meshes) static (mass 0)
		// iPhysicsBody per call - see the .cpp file for why this exists now
		// (it didn't in the original Phase 1 loader, see the class comment
		// in this header). apMeshEntity's WORLD matrix must already be fully
		// set (rotation+scale+position) before calling this.
		void CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const tString& asName);

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
