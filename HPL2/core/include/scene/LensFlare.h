#ifndef HPL_LENS_FLARE_H
#define HPL_LENS_FLARE_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "scene/SceneTypes.h"
#include "graphics/Renderable.h"
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
		cLensFlareType(cLensFlare *apParent, eLensFlareType aType);

		cMaterial *GetMaterial();
		iVertexBuffer* GetVertexBuffer();
		cMatrixf* GetModelMatrix(cFrustum *apFrustum);
		int GetMatrixUpdateCount();
		eRenderableType GetRenderType(){ return eRenderableType_LensFlareType;}
		bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime);
		cBoundingVolume* GetBoundingVolume();
		tString GetEntityType(){ return "LensFlareType";}

	private:
		cLensFlare *mpParent;
		eLensFlareType mType;
		cMatrixf m_mtxModel;
	};

	//------------------------------------------

	class cLensFlare : public iRenderable
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
		friend class cLensFlareType;
	public:
		cLensFlare(const tString& asName, cResources *apResources, cGraphics *apGraphics);
		~cLensFlare();

		void FadeIn(float afTime){ mfFadeRate = (1 - mfFade) / (afTime + 1e-4f);}
		void FadeOut(float afTime){ mfFadeRate = -mfFade / (afTime + 1e-4f);}

		void SetFlareSourceSize(cVector3f avSize);
		cVector3f GetFlareSourceSize(){ return mvSourceSize;}
		void SetOuterFieldOfView(float afAngle);
		float GetOuterFieldOfView(){ return mfOuterFov;}
		void SetInnerFieldOfView(float afAngle);
		float GetInnerFieldOfView(){ return mfInnerFov;}
		void SetMultiIrisTextureAtlasGrid(cVector2l avGrid){ mvAtlasGrid = avGrid;}
		cVector2l GetMultiIrisTextureAtlasGrid(){ return mvAtlasGrid;}
		void SetMultiIrisSeed(int alSeed){ mlMultiIrisSeed = alSeed;}
		int GetMultiIrisSeed(){ return mlMultiIrisSeed;}
		void SetMultiIrisCount(int alCount){ mlMultiIrisCount = alCount;}
		int GetMultiIrisCount(){ return mlMultiIrisCount;}
		void SetRangeMax(float afStart, float afEnd){ mvRangeMax = cVector2f(afStart, afEnd);}
		void DisableRangeMax(){ mvRangeMax = -1;}
		float GetRangeMaxStart(){ return mvRangeMax.x;}
		float GetRangeMaxEnd(){ return mvRangeMax.y;}
		void SetRangeMin(float afStart, float afEnd){ mvRangeMin = cVector2f(afStart, afEnd);}
		void DisableRangeMin(){ mvRangeMin = -1;}
		float GetRangeMinStart(){ return mvRangeMin.x;}
		float GetRangeMinEnd(){ return mvRangeMin.y;}
		void SetGlareBrightness(float afX){ mfGlareBrightness = afX;}
		float GetGlareBrightness(){ return mfGlareBrightness;}
		void SetGlareFieldOfView(float afAngle);
		float GetGlareFieldOfView(){ return mfGlareFov;}
		void SetGlareStareAt(float afX){ mfGlareStareAt = afX;}
		void SetGlareRange(float afStart, float afEnd){ mvGlareRange = cVector2f(afStart, afEnd);}
		float GetGlareRangeMaxStart(){ return mvGlareRange.x;}
		float GetGlareRangeMaxEnd(){ return mvGlareRange.y;}
		void SetSizeChangeBasedOnDistance(float afX);
		float GetSizeChangeBasedOnDistance(){ return mfSizeByDist;}
		void SetFlareSize(eLensFlareType aType, cVector2f avSize);
		cVector2f GetFlareSize(eLensFlareType aType){ return mvSize[aType];}
		void SetFlareColor(eLensFlareType aType, cColor aColor){ mColor[aType] = aColor;}
		cColor GetFlareColor(eLensFlareType aType){ return mColor[aType];}
		void SetFlareActive(eLensFlareType aType, bool abX);
		bool IsFlareActive(eLensFlareType aType){ return mbActive[aType] && mpMaterial[aType];}
		bool GetFlareActive(eLensFlareType aType){ return mbActive[aType];}
		void SetUseParentMeshForOcclusion(bool abX){ mbUseParentMeshForOcclusion = abX;}
		bool GetUseParentMeshForOcclusion(){ return mbUseParentMeshForOcclusion;}
		void SetShrinkWhenOccluded(bool abX){ mbShrinkWhenOccluded = abX;}
		bool GetShrinkWhenOccluded(){ return mbShrinkWhenOccluded;}
		void SetMultiplyGlareWithMultiIris(bool abX){ mbMulMultiIrisWithGlare = abX;}
		bool GetMultiplyGlareWithMultiIris(){ return mbMulMultiIrisWithGlare;}
		void SetBrightness(float afX){ mfBrightness = afX;}
		float GetBrightness(){ return mfBrightness;}
		eLensFlareType GetFirstActiveType();
		void SetMaterial(eLensFlareType aType, cMaterial *apMaterial);
		cMaterial* GetTypeMaterial(eLensFlareType aType){ return mpMaterial[aType];}
		cLensFlareType* GetTypeRenderable(eLensFlareType aType){ return mpTypes[aType];}

		bool IsAnyTypeActive();
		float GetVisibility(){ return mfVis;}
		void UpdateVisibility(float afTimeStep, int alFrame);

		cColor GetFlareColorAndBrightness(eLensFlareType aType, cFrustum *apFrustum, float afVis);
		cColor GetGlare(cFrustum *apFrustum, cVector3f &avScreenPos, float afScale);
		iVertexBuffer* PrepareMultiIrisVertexBuffer(cFrustum *apFrustum, float afVis);

		void UpdateLogic(float afTimeStep);
		tString GetEntityType(){ return "LensFlare";}
		bool IsVisible();

		cMaterial *GetMaterial();
		iVertexBuffer* GetVertexBuffer(){ return mpVtxBuffer;}
		cMatrixf* GetModelMatrix(cFrustum *apFrustum);
		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}
		eRenderableType GetRenderType(){ return eRenderableType_LensFlare;}
		bool CollidesWithFrustum(cFrustum *apFrustum);

		bool UsesOcclusionQuery(){ return true;}
		void AssignOcclusionQuery(iRenderer *apRenderer);
		bool RetrieveOcculsionQuery(iRenderer *apRenderer);

	private:
		cMatrixf* GetSizeAndRotation(eLensFlareType aType, cFrustum *apFrustum, float afVis);
		float ScaleFromDistance(float afDist){ return cMath::Max(0.0f, 1.5f * ((1 - mfSizeByDist) * (afDist - 1) + 1.5f));}
		float FovFactor(cFrustum *apFrustum);
		void UpdateBV();

		cMaterialManager* mpMaterialManager;
		iLowLevelGraphics* mpLowLevelGraphics;
		iVertexBuffer* mpVtxBuffer;
		iVertexBuffer* mpMultiIrisVtxBuffer;
		cLensFlareType* mpTypes[eLensFlareType_LastEnum];

		bool mbActive[eLensFlareType_LastEnum];
		cColor mColor[eLensFlareType_LastEnum];
		cVector2f mvSize[eLensFlareType_LastEnum];
		cMaterial* mpMaterial[eLensFlareType_LastEnum];
		cMatrixf m_mtxSizeRot[eLensFlareType_LastEnum];

		cVector2l mvAtlasGrid;
		int mlMultiIrisSeed;
		int mlMultiIrisCount;
		float mfSizeByDist;
		bool mbUseParentMeshForOcclusion;
		bool mbShrinkWhenOccluded;
		bool mbMulMultiIrisWithGlare;
		cVector2f mvRangeMax;
		cVector2f mvRangeMin;
		float mfOuterFov, mfInnerFov, mfGlareFov;
		float mfOuterCos, mfInnerCos, mfGlareCos;
		float mfGlareBrightness;
		float mfGlareStareAt;
		cVector2f mvGlareRange;
		float mfFade;
		float mfFadeRate;
		float mfBrightness;
		cVector3f mvSourceSize;

		float mfVisTarget;
		float mfVis;
		float mfVisVel;
		int mlVisFrame;

		cBoundingVolume mOcclusionBV;
		cMatrixf m_mtxOccluder;
		cMatrixf m_mtxBillboard;
	};

};
#endif // HPL_LENS_FLARE_H
