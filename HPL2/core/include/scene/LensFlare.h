#ifndef HPL_LENS_FLARE_H
#define HPL_LENS_FLARE_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "scene/SceneTypes.h"
#include "graphics/Renderable.h"
#include "scene/RustSL.h"
#include "scene/RustLs.h"
#include "scene/RustLight.h"
#include "math/Math.h"

namespace hpl {

	class cMaterialManager;
	class cResources;
	class cGraphics;
	class iLowLevelGraphics;
	class cMaterial;
	class iVertexBuffer;
	class cLensFlare;

	//------------------------------------------

	class cLensFlareType : public iRenderable
	{
	public:
		cLensFlareType(cLensFlare *apParent, const tString &asName, eLensFlareType aType, OhplSLFlare *apFlare) : iRenderable(asName),
			mpS(ohpl_ls_ft_new((uintptr_t)apParent, Rs(), apFlare, aType)){}
		~cLensFlareType();

		cMaterial *GetMaterial(){ return (cMaterial*)ohpl_ls_ft_material(mpS);}
		iVertexBuffer* GetVertexBuffer(){ return (iVertexBuffer*)ohpl_ls_ft_vertex_buffer(mpS);}
		cMatrixf* GetModelMatrix(cFrustum *apFrustum){ return ohpl_ls_ft_model(mpS, OhplLsFrustum(apFrustum));}
		int GetMatrixUpdateCount(){ return ohpl_ls_ft_update_count(mpS);}
		eRenderableType GetRenderType(){ return eRenderableType_LensFlareType;}
		bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime){ return ohpl_ls_ft_update_viewport(mpS, OhplLsFrustum(apFrustum));}
		cBoundingVolume* GetBoundingVolume();
		tString GetEntityType(){ return "LensFlareType";}

	private:
		OhplSLFlareType *mpS;
	};

	//------------------------------------------

	class cLensFlare : public iRenderable
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
		friend class cLensFlareType;
	public:
		cLensFlare(const tString& asName, cResources *apResources, cGraphics *apGraphics) : iRenderable(asName),
			mpS(ohpl_ls_flare_new(this, mpE, mBoundingVolume.Rs(), (uintptr_t)apResources, (uintptr_t)apGraphics)){}
		~cLensFlare();

		void FadeIn(float afTime){ ohpl_sl_flare_fade(mpS, true, afTime);}
		void FadeOut(float afTime){ ohpl_sl_flare_fade(mpS, false, afTime);}

		void SetFlareSourceSize(cVector3f avSize){ ohpl_ls_flare_set_source_size(mpS, &avSize);}
		cVector3f GetFlareSourceSize(){ return mpS->sourceSize;}
		void SetOuterFieldOfView(float afAngle){ ohpl_sl_flare_set_outer_fov(mpS, afAngle);}
		float GetOuterFieldOfView(){ return mpS->outerFov;}
		void SetInnerFieldOfView(float afAngle){ ohpl_sl_flare_set_inner_fov(mpS, afAngle);}
		float GetInnerFieldOfView(){ return mpS->innerFov;}
		void SetMultiIrisTextureAtlasGrid(cVector2l avGrid){ mpS->atlasGrid = avGrid;}
		cVector2l GetMultiIrisTextureAtlasGrid(){ return mpS->atlasGrid;}
		void SetMultiIrisSeed(int alSeed){ mpS->seed = alSeed;}
		int GetMultiIrisSeed(){ return mpS->seed;}
		void SetMultiIrisCount(int alCount){ mpS->count = alCount;}
		int GetMultiIrisCount(){ return mpS->count;}
		void SetRangeMax(float afStart, float afEnd){ mpS->rangeMax = cVector2f(afStart, afEnd);}
		void DisableRangeMax(){ mpS->rangeMax = -1;}
		float GetRangeMaxStart(){ return mpS->rangeMax.x;}
		float GetRangeMaxEnd(){ return mpS->rangeMax.y;}
		void SetRangeMin(float afStart, float afEnd){ mpS->rangeMin = cVector2f(afStart, afEnd);}
		void DisableRangeMin(){ mpS->rangeMin = -1;}
		float GetRangeMinStart(){ return mpS->rangeMin.x;}
		float GetRangeMinEnd(){ return mpS->rangeMin.y;}
		void SetGlareBrightness(float afX){ mpS->glareBrightness = afX;}
		float GetGlareBrightness(){ return mpS->glareBrightness;}
		void SetGlareFieldOfView(float afAngle){ ohpl_sl_flare_set_glare_fov(mpS, afAngle);}
		float GetGlareFieldOfView(){ return mpS->glareFov;}
		void SetGlareStareAt(float afX){ mpS->glareStareAt = afX;}
		void SetGlareRange(float afStart, float afEnd){ mpS->glareRange = cVector2f(afStart, afEnd);}
		float GetGlareRangeMaxStart(){ return mpS->glareRange.x;}
		float GetGlareRangeMaxEnd(){ return mpS->glareRange.y;}
		void SetSizeChangeBasedOnDistance(float afX){ ohpl_sl_flare_set_size_by_dist(mpS, afX);}
		float GetSizeChangeBasedOnDistance(){ return mpS->sizeByDist;}
		void SetFlareSize(eLensFlareType aType, cVector2f avSize){ ohpl_ls_flare_set_size(mpS, aType, &avSize);}
		cVector2f GetFlareSize(eLensFlareType aType){ return mpS->size[aType];}
		void SetFlareColor(eLensFlareType aType, cColor aColor){ mpS->color[aType] = aColor;}
		cColor GetFlareColor(eLensFlareType aType){ return mpS->color[aType];}
		void SetFlareActive(eLensFlareType aType, bool abX){ ohpl_ls_flare_set_active(mpS, aType, abX);}
		bool IsFlareActive(eLensFlareType aType){ return mpS->active[aType] && mpS->material[aType];}
		bool GetFlareActive(eLensFlareType aType){ return mpS->active[aType];}
		void SetUseParentMeshForOcclusion(bool abX){ mpS->useParentMesh = abX;}
		bool GetUseParentMeshForOcclusion(){ return mpS->useParentMesh;}
		void SetShrinkWhenOccluded(bool abX){ mpS->shrinkWhenOccluded = abX;}
		bool GetShrinkWhenOccluded(){ return mpS->shrinkWhenOccluded;}
		void SetMultiplyGlareWithMultiIris(bool abX){ mpS->mulMultiIris = abX;}
		bool GetMultiplyGlareWithMultiIris(){ return mpS->mulMultiIris;}
		void SetBrightness(float afX){ mpS->brightness = afX;}
		float GetBrightness(){ return mpS->brightness;}
		eLensFlareType GetFirstActiveType(){ return (eLensFlareType)ohpl_sl_flare_first_active(mpS);}
		void SetMaterial(eLensFlareType aType, cMaterial *apMaterial){ ohpl_ls_flare_set_material(mpS, aType, (uintptr_t)apMaterial);}
		cMaterial* GetTypeMaterial(eLensFlareType aType){ return ((cMaterial*)mpS->material[aType]);}
		cLensFlareType* GetTypeRenderable(eLensFlareType aType){ return ((cLensFlareType*)mpS->types[aType]);}

		bool IsAnyTypeActive(){ return ohpl_sl_flare_any_active(mpS);}
		float GetVisibility(){ return mpS->vis.vis;}
		void UpdateVisibility(float afTimeStep, int alFrame){ ohpl_sl_flare_update_visibility(mpS, afTimeStep, alFrame);}

		cColor GetFlareColorAndBrightness(eLensFlareType aType, cFrustum *apFrustum, float afVis)
		{
			cColor col;
			ohpl_ls_flare_color(mpS, OhplLsFrustum(apFrustum), aType, afVis, &col);
			return col;
		}
		cColor GetGlare(cFrustum *apFrustum, cVector3f &avScreenPos, float afScale)
		{
			cColor col;
			ohpl_ls_flare_glare(mpS, OhplLsFrustum(apFrustum), &avScreenPos, afScale, &col);
			return col;
		}
		iVertexBuffer* PrepareMultiIrisVertexBuffer(cFrustum *apFrustum, float afVis)
		{
			return (iVertexBuffer*)ohpl_ls_flare_multi_iris(mpS, OhplLsFrustum(apFrustum), afVis);
		}

		void UpdateLogic(float afTimeStep){ ohpl_sl_flare_update(mpS, afTimeStep);}
		tString GetEntityType(){ return "LensFlare";}
		OhplSLFlare* Pod(){ return mpS;}
		bool IsVisible(){ return mbIsVisible && IsAnyTypeActive();}

		cMaterial *GetMaterial(){ return (cMaterial*)mpS->material[GetFirstActiveType()];}
		iVertexBuffer* GetVertexBuffer(){ return (iVertexBuffer*)mpS->vtx;}
		cMatrixf* GetModelMatrix(cFrustum *apFrustum){ return ohpl_ls_flare_model(mpS, OhplLsFrustum(apFrustum));}
		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}
		eRenderableType GetRenderType(){ return eRenderableType_LensFlare;}
		bool CollidesWithFrustum(cFrustum *apFrustum){ return ohpl_ls_flare_collides(mpS, OhplLsFrustum(apFrustum));}

		bool UsesOcclusionQuery(){ return true;}
		void AssignOcclusionQuery(iRenderer *apRenderer){ ohpl_ls_flare_assign_occlusion(mpS, apRenderer);}
		bool RetrieveOcculsionQuery(iRenderer *apRenderer){ return ohpl_ls_flare_retrieve_occlusion(mpS, apRenderer);}

	private:
		OhplSLFlare *mpS;
	};

	inline cBoundingVolume* cLensFlareType::GetBoundingVolume(){ return ((cLensFlare*)mpS->parent)->GetBoundingVolume();}

};
#endif // HPL_LENS_FLARE_H
