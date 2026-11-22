#ifndef HPL_RUST_WD_H
#define HPL_RUST_WD_H

// C ABI of rust/src/scene/wd.rs: the Rust-owned cWorld state (read in place) and its operations.

#include "math/RustTypes.h"
#include "scene/RustSceneH13.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
	class cGraphics;
	class cResources;
	class cSound;
	class cPhysics;
	class cScene;
	class cSystem;
	class cAI;
	class cWorld;
	class cNode3D;
	class cTerrain;
	class cLightDirectional;
	class iPhysicsWorld;
	class iRenderableContainer;
	class iVertexBuffer;
	class iTexture;

	struct OhplWdPod
	{
		cGraphics *mpGraphics;
		cSound *mpSound;
		cResources *mpResources;
		cPhysics *mpPhysics;
		cScene *mpScene;
		cSystem *mpSystem;
		cAI *mpAI;
		iPhysicsWorld *mpPhysicsWorld;
		iRenderableContainer *mpRenderableContainer[2];
		iVertexBuffer *mpSkyBoxVtxBuffer;
		iTexture *mpSkyBoxTexture;
		cTerrain *mpTerrain;
		cLightDirectional *mpDirectionalLight;
		iTexture *mpFogSkyboxTexture;
		cNode3D *mpRootNode;
		OhplH13Reg *mpH13Reg;
		cWorld *mpOwner;
		cVector3f mvWorldSize;
		cColor mSkyBoxColor;
		cVector3f mvSkyBoxRotation;
		cColor mFogColor, mSecondaryFogColor, mNightVisionColor, mAmbientColor;
		cVector3f mvFogNoiseTurbulence;
		float mfSkyBoxBrightness;
		float mfDistanceCullMinRange, mfDistanceCullScreenSize, mfDistanceCullRandomSize, mfDistanceCullMaxRange, mfDistanceCullFadeSpeed;
		float mfFogStart, mfFogEnd, mfFogFalloffExp, mfFogBrightness, mfSecondaryFogBrightness;
		float mfFogNoiseStrength, mfFogNoiseSize;
		float mfSecondaryFogStart, mfSecondaryFogEnd, mfSecondaryFogFalloffExp;
		float mfFogDensity, mfFogHeightDensity, mfFogHeightHorizon;
		float mfSecondaryFogDensity, mfSecondaryFogHeightDensity, mfSecondaryFogHeightHorizon;
		float mfDepthOfFieldFocusStart, mfDepthOfFieldFocusEnd, mfDepthOfFieldFalloff;
		float mfNightVisionBrightness, mfNightVisionRadiusMin, mfNightVisionRadiusMax, mfNightVisionFalloff;
		float mfToneMappingKey, mfToneMappingExposure, mfToneMappingWhiteCut;
		int mlSoundCreationIDCount;
		bool mbActive, mbAutoDeletePhysicsWorld, mbIsSoundEmitter, mbAutoDestroySkybox, mbSkyBoxActive,
			mbDirectionalLightActive, mbEnvParticlesActive, mbFogActive, mbFogCulling, mbDistanceCullActive,
			mbFogUnderwater, mbFogUseSkybox, mbFogApplyAfterFogAreas, mbSecondaryFogActive, mbFogLighten,
			mbFogHeightBased, mbFogExponential, mbDepthOfFieldActive;
	};

	static_assert(sizeof(OhplWdPod) == 416 && offsetof(OhplWdPod, mpH13Reg) == 128 && offsetof(OhplWdPod, mlSoundCreationIDCount) == 388
				  && offsetof(OhplWdPod, mbActive) == 392, "world pod layout");

	struct OhplWdCreate
	{
		const char *name;
		size_t nameLen;
		cVector2f size2;
		int32_t ty, which;
		bool flag;
		int32_t id;
		void *a, *b;
	};

	struct OhplWdDumpItem
	{
		const char *name;
		size_t nameLen;
		const char *kindName;
		size_t kindLen;
		int32_t uid;
		bool crouch;
		float mtx[16];
		float size[3];
	};

	struct OhplWdField
	{
		cVector3f min, max;
		float radius;
		bool usable;
	};

	struct OhplWdLoad
	{
		const char *name;
		size_t nameLen;
		int32_t id;
		bool active;
		const cMatrixf *mtx;
		const cVector3f *scale;
		void *vars;
	};

	struct OhplWdHost
	{
		void (*regForeign)(void*);
		void* (*rootNew)();
		void (*rootDelete)(void*);
		void* (*contNew)(bool);
		void (*contDelete)(void*);
		void (*contAdd)(void*, void*);
		void (*contRemove)(void*, void*);
		void (*contRemoveSet)(void*, const size_t*, size_t);
		void (*contCompile)(void*);
		void (*contBounds)(void*, cVector3f*);
		void* (*rendOf)(uint32_t, void*);
		bool (*isStatic)(uint32_t, void*);
		void (*setStatic)(uint32_t, void*, bool);
		void (*setWorld)(uint32_t, void*, void*);
		void* (*create)(uint32_t, OhplWdPod*, const OhplWdCreate*);
		void (*lightGobo)(void*, bool, void*, const char*, size_t);
		void (*bbMaterial)(void*, void*);
		void (*flareHalo)(void*, const cVector2f*);
		void (*flareMaterial)(void*, void*);
		size_t (*meshSubs)(void*, void**, size_t, bool);
		void (*meshDrawBb)(void*, void*, const cColor*);
		void* (*psCreate)(void*, int32_t, const char*, size_t, const char*, size_t, void*, const cVector3f*);
		size_t (*psEmitters)(void*, void**, size_t);
		void (*psRemoveWhenDead)(void*, bool);
		int32_t (*update)(uint32_t, void*, float);
		void (*physUpdate)(void*, float);
		void (*physAttach)(void*, void*);
		void (*physDestroy)(void*, void*);
		void (*physSetSize)(void*, const cVector3f*);
		void (*physClearSounds)(void*);
		void (*soundSilent)(void*, bool);
		void* (*soundData)(void*, const char*, size_t);
		int32_t (*soundCreationId)(void*);
		void* (*skyVbNew)(void*);
		void* (*dirlightNew)(void*);
		void (*dirlightDelete)(void*);
		void (*terrainDelete)(void*);
		void* (*entfileCreate)(void*, const char*, size_t);
		void (*entfileDestroy)(void*, void*);
		void (*entfileInfo)(void*, const char**, size_t*, const char**, size_t*, bool*);
		void* (*loaderGet)(void*, const char*, size_t);
		bool (*loaderStatic)(void*);
		void* (*loaderLoad)(void*, void*, void*, const OhplWdLoad*);
		void (*entitySource)(void*, void*);
		void (*tempNodesClear)(void*);
		void (*dumpItem)(uint32_t, void*, OhplWdDumpItem*);
		void (*fieldInfo)(void*, OhplWdField*);
		void (*log)(bool, const uint8_t*, size_t);
	};

	extern "C" {
	void ohpl_wd_set_host(const OhplWdHost*);
	OhplWdPod* ohpl_wd_new(cWorld*, cGraphics*, cResources*, cSound*, cPhysics*, cScene*, cSystem*, cAI*, const char*, size_t);
	void ohpl_wd_free(OhplWdPod*);
	const char* ohpl_wd_name(const OhplWdPod*, size_t*);
	void ohpl_wd_set_hpl3_skybox(bool);
	void ohpl_wd_destroy_all(OhplWdPod*, uint32_t);
	void* ohpl_wd_create_light(OhplWdPod*, int32_t, const char*, size_t, const char*, size_t, bool);
	void* ohpl_wd_create_billboard(OhplWdPod*, const char*, size_t, const cVector2f*, int32_t, const char*, size_t, bool);
	void* ohpl_wd_create_lens_flare(OhplWdPod*, const char*, size_t, const cVector2f*, const char*, size_t, bool);
	void* ohpl_wd_create_simple(OhplWdPod*, uint32_t, const char*, size_t, bool, bool, void*, int32_t);
	void* ohpl_wd_create_env_particles(OhplWdPod*, const char*, size_t);
	void* ohpl_wd_create_mesh(OhplWdPod*, const char*, size_t, void*, bool);
	void ohpl_wd_destroy_mesh(OhplWdPod*, void*);
	void ohpl_wd_destroy_static_meshes(OhplWdPod*, const size_t*, size_t);
	void ohpl_wd_make_mesh_dynamic(OhplWdPod*, void*);
	void ohpl_wd_make_renderable_dynamic(OhplWdPod*, void*);
	void ohpl_wd_destroy(OhplWdPod*, uint32_t, void*);
	void* ohpl_wd_create_particle_system(OhplWdPod*, int32_t, const char*, size_t, const char*, size_t, void*, const cVector3f*, bool);
	void ohpl_wd_destroy_particle_system(OhplWdPod*, void*);
	void ohpl_wd_destroy_all_particle_systems(OhplWdPod*);
	void* ohpl_wd_create_sound(OhplWdPod*, const char*, size_t, const char*, size_t, bool);
	void ohpl_wd_destroy_all_sounds(OhplWdPod*);
	bool ohpl_wd_sound_exists(OhplWdPod*, void*, int32_t);
	int32_t ohpl_wd_force_fields(OhplWdPod*, const cVector3f*, const cVector3f*, void**);
	void ohpl_wd_draw_mesh_bounding_boxes(OhplWdPod*, const cColor*, bool);
	void* ohpl_wd_dir_light(OhplWdPod*);
	void ohpl_wd_set_terrain(OhplWdPod*, void*);
	void ohpl_wd_set_physics_world(OhplWdPod*, void*, bool);
	void ohpl_wd_compile(OhplWdPod*, bool);
	void ohpl_wd_set_skybox(OhplWdPod*, void*, bool);
	void ohpl_wd_set_skybox_color(OhplWdPod*, const cColor*);
	void ohpl_wd_set_skybox_brightness(OhplWdPod*, float);
	void* ohpl_wd_create_entity(OhplWdPod*, const char*, size_t, const cMatrixf*, const char*, size_t, int32_t, bool, const cVector3f*, void*, bool);
	void ohpl_wd_update(OhplWdPod*, float);
	void ohpl_wd_pre_update(OhplWdPod*, float, float);
	void ohpl_wd_dump(OhplWdPod*, const char*, const char*);
	}
}

#endif
