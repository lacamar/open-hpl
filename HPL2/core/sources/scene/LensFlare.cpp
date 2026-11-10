#include "scene/LensFlare.h"

#include "resources/Resources.h"
#include "resources/MaterialManager.h"

#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"

#include "scene/SubMeshEntity.h"
#include "math/Math.h"
#include "math/Frustum.h"
#include "scene/RustLight.h"

namespace hpl {

	//-----------------------------------------------------------------------

	static cVector3f FacingAxis(iEntity3D *apEnt)
	{
		const cMatrixf &m = apEnt->GetWorldMatrix();
		if(apEnt->GetEntityParent() || apEnt->GetParent()) return cVector3f(m.m[0][2], m.m[1][2], m.m[2][2]);
		return m.GetForward();
	}

	static OhplFlareEnt FlareEnt(iEntity3D *apEnt)
	{
		OhplFlareEnt ent;
		ent.pos = apEnt->GetWorldPosition();
		ent.axis = FacingAxis(apEnt);
		return ent;
	}

	static OhplFlareView FlareView(cFrustum *apFrustum)
	{
		OhplFlareView view;
		view.origin = apFrustum->GetOrigin();
		view.view = apFrustum->GetViewMatrix();
		view.proj = apFrustum->GetProjectionMatrix();
		view.aspect = apFrustum->GetAspect();
		return view;
	}

	OhplFlareIn cLensFlare::MakeIn()
	{
		OhplFlareIn in;
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			in.active[i] = IsFlareActive((eLensFlareType)i);
			in.color[i] = mColor[i];
			in.size[i] = mvSize[i];
		}
		in.range_max = mvRangeMax;
		in.range_min = mvRangeMin;
		in.glare_range = mvGlareRange;
		in.atlas_grid = mvAtlasGrid;
		in.multi_iris_seed = mlMultiIrisSeed;
		in.multi_iris_count = mlMultiIrisCount;
		in.size_by_dist = mfSizeByDist;
		in.outer_cos = mfOuterCos;
		in.inner_cos = mfInnerCos;
		in.glare_cos = mfGlareCos;
		in.glare_brightness = mfGlareBrightness;
		in.glare_stare_at = mfGlareStareAt;
		in.fade = mfFade;
		in.brightness = mfBrightness;
		in.shrink_when_occluded = mbShrinkWhenOccluded;
		in.mul_iris_with_glare = mbMulMultiIrisWithGlare;
		return in;
	}

	cLensFlareType::cLensFlareType(cLensFlare *apParent, eLensFlareType aType) : iRenderable(apParent->GetName())
	{
		mpParent = apParent;
		mType = aType;
	}

	cMaterial* cLensFlareType::GetMaterial(){ return mpParent->mpMaterial[mType];}
	iVertexBuffer* cLensFlareType::GetVertexBuffer(){ return mpParent->mpVtxBuffer;}
	int cLensFlareType::GetMatrixUpdateCount(){ return mpParent->GetTransformUpdateCount();}
	cBoundingVolume* cLensFlareType::GetBoundingVolume(){ return mpParent->GetBoundingVolume();}

	cMatrixf* cLensFlareType::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL) return mpParent->GetModelMatrix(NULL);
		cMatrixf *pSizeRot = mpParent->GetSizeAndRotation(mType, apFrustum, mpParent->mVis.vis);
		m_mtxModel = cMath::MatrixMul(*mpParent->GetModelMatrix(apFrustum), *pSizeRot);
		return &m_mtxModel;
	}

	bool cLensFlareType::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
		OhplFlareIn in = mpParent->MakeIn();
		OhplFlareEnt ent = FlareEnt(mpParent);
		OhplFlareView view = FlareView(apFrustum);
		cColor c;
		if(ohpl_flare_type_color(&in, &ent, &view, mType, mpParent->mVis.vis, &c)==false) return false;
		SetColorMul(c);
		return true;
	}

	//-----------------------------------------------------------------------

	cLensFlare::cLensFlare(const tString& asName, cResources *apResources, cGraphics *apGraphics) : iRenderable(asName)
	{
		mpMaterialManager = apResources->GetMaterialManager();
		mpLowLevelGraphics = apGraphics->GetLowLevel();
		mpMultiIrisVtxBuffer = NULL;

		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			mbActive[i] = false;
			mColor[i] = cColor(1,1);
			mvSize[i] = 0.125f;
			mpMaterial[i] = NULL;
			mpTypes[i] = hplNew(cLensFlareType, (this, (eLensFlareType)i));
		}
		mvSize[eLensFlareType_AnamorphicFlare] = cVector2f(0.25f, 0.0625f);

		mvAtlasGrid = 1;
		mlMultiIrisSeed = rand();
		mlMultiIrisCount = 16;
		mfSizeByDist = 0.5f;
		mbUseParentMeshForOcclusion = true;
		mbShrinkWhenOccluded = true;
		mbMulMultiIrisWithGlare = false;
		mvRangeMax = -1;
		mvRangeMin = -1;
		mfOuterFov = mfInnerFov = kPif;
		mfGlareFov = 0;
		mfOuterCos = mfInnerCos = -1;
		mfGlareCos = 1;
		mfGlareBrightness = 0.5f;
		mfGlareStareAt = 0.2f;
		mvGlareRange = -1;
		mfFade = 1;
		mfFadeRate = 1;
		mfBrightness = 1;
		mvSourceSize = 0.5f;
		mVis.target = 0;
		mVis.vis = 1;
		mVis.vel = 0;
		mVis.frame = -1;

		mpVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware,eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,4,6);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		cVector3f vCorners[4] = {cVector3f(1,-1,0), cVector3f(-1,-1,0), cVector3f(-1,1,0), cVector3f(1,1,0)};
		for(int i=0;i<4;i++)
		{
			cVector3f vUv = vCorners[i];
			if(cRendererDeferred::GetHdr()) vUv.y = -vUv.y;
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vCorners[i]*0.5f);
			mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1));
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, (vUv + cVector3f(1,1,0))/2);
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,cVector3f(0,0,1));
		}
		unsigned int vIdx[6] = {0,1,2,2,3,0};
		for(int i=0;i<6;i++) mpVtxBuffer->AddIndex(vIdx[i]);
		mpVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		mOcclusionBV.SetSize(mvSourceSize);
		UpdateBV();
	}

	cLensFlare::~cLensFlare()
	{
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			if(mpMaterial[i]) mpMaterialManager->Destroy(mpMaterial[i]);
			hplDelete(mpTypes[i]);
		}
		hplDelete(mpVtxBuffer);
		if(mpMultiIrisVtxBuffer) hplDelete(mpMultiIrisVtxBuffer);
	}

	//-----------------------------------------------------------------------

	void cLensFlare::SetFlareSourceSize(cVector3f avSize)
	{
		mvSourceSize = avSize;
		mOcclusionBV.SetSize(mvSourceSize);
	}

	// HPL3 quirk kept: the outer cone is clamped against the inner one before it is set
	void cLensFlare::SetOuterFieldOfView(float afAngle)
	{
		mfOuterFov = afAngle;
		mfOuterCos = ohpl_flare_outer_cos(afAngle, mfInnerCos);
	}

	void cLensFlare::SetInnerFieldOfView(float afAngle)
	{
		mfInnerFov = afAngle;
		ohpl_flare_set_inner_fov(afAngle, mfOuterCos, &mfInnerCos, &mfOuterCos);
	}

	void cLensFlare::SetGlareFieldOfView(float afAngle)
	{
		mfGlareFov = afAngle;
		mfGlareCos = cosf(afAngle*0.5f);
	}

	void cLensFlare::SetSizeChangeBasedOnDistance(float afX){ mfSizeByDist = cMath::Clamp(afX, 0, 1);}

	void cLensFlare::SetFlareSize(eLensFlareType aType, cVector2f avSize)
	{
		mvSize[aType] = avSize;
		UpdateBV();
	}

	void cLensFlare::SetFlareActive(eLensFlareType aType, bool abX)
	{
		mbActive[aType] = abX;
		UpdateBV();
	}

	void cLensFlare::SetMaterial(eLensFlareType aType, cMaterial *apMaterial)
	{
		if(mpMaterial[aType]) mpMaterialManager->Destroy(mpMaterial[aType]);
		mpMaterial[aType] = apMaterial;
	}

	eLensFlareType cLensFlare::GetFirstActiveType()
	{
		if(IsFlareActive(eLensFlareType_Halo)) return eLensFlareType_Halo;
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
			if(IsFlareActive((eLensFlareType)i)) return (eLensFlareType)i;
		return eLensFlareType_Halo;
	}

	bool cLensFlare::IsAnyTypeActive()
	{
		if(mfFade <= 0) return false;
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
			if(IsFlareActive((eLensFlareType)i)) return true;
		return false;
	}

	bool cLensFlare::IsVisible(){ return mbIsVisible && IsAnyTypeActive();}

	cMaterial* cLensFlare::GetMaterial(){ return mpMaterial[GetFirstActiveType()];}

	void cLensFlare::UpdateLogic(float afTimeStep)
	{
		mfFade = ohpl_flare_update_fade(mfFade, mfFadeRate, afTimeStep);
	}

	// ponytail: smoothing state lives in the flare, not per render settings; a skipped frame restarts from 0
	void cLensFlare::UpdateVisibility(float afTimeStep, int alFrame)
	{
		ohpl_flare_update_visibility(&mVis, afTimeStep, alFrame);
	}

	//-----------------------------------------------------------------------

	cColor cLensFlare::GetFlareColorAndBrightness(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		OhplFlareIn in = MakeIn();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		cColor col(0,0);
		ohpl_flare_color(&in, &ent, &view, aType, afVis, &col);
		return col;
	}

	cColor cLensFlare::GetGlare(cFrustum *apFrustum, cVector3f &avScreenPos, float afScale)
	{
		OhplFlareIn in = MakeIn();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		cColor col;
		ohpl_flare_glare(&in, &ent, &view, afScale, &col, &avScreenPos);
		return col;
	}

	//-----------------------------------------------------------------------

	cMatrixf* cLensFlare::GetSizeAndRotation(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		OhplFlareIn in = MakeIn();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		cMatrixf &mtx = m_mtxSizeRot[aType];
		ohpl_flare_size_rotation(&in, &ent, &view, aType, afVis, &mtx);
		return &mtx;
	}

	cMatrixf* cLensFlare::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL) return &GetWorldMatrix();

		OhplFlareIn in = MakeIn();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		ohpl_flare_billboard(&in, &ent, &view, &m_mtxBillboard);
		return &m_mtxBillboard;
	}

	//-----------------------------------------------------------------------

	void cLensFlare::UpdateBV()
	{
		bool vbActive[eLensFlareType_LastEnum];
		for(int i=0; i<eLensFlareType_LastEnum; ++i) vbActive[i] = mbActive[i];
		float fR = ohpl_flare_bv_radius(vbActive, mvSize);
		mBoundingVolume.SetLocalMinMax(-fR, fR);
		SetTransformUpdated();
	}

	bool cLensFlare::CollidesWithFrustum(cFrustum *apFrustum)
	{
		if(IsAnyTypeActive()==false) return false;

		cBoundingVolume bv = *GetBoundingVolume();
		OhplFlareIn in = MakeIn();
		OhplFlareView view = FlareView(apFrustum);
		cVector3f vWorldCenter = bv.GetWorldCenter();
		cVector3f vMin = bv.GetLocalMin(), vMax = bv.GetLocalMax(), vC = bv.GetLocalCenter();
		cVector3f vNewMin, vNewMax;
		if(ohpl_flare_collide_box(&in, &view, &vWorldCenter, &vMin, &vMax, &vC, &vNewMin, &vNewMax)==false) return false;
		bv.SetLocalMinMax(vNewMin, vNewMax);
		return apFrustum->CollideBoundingVolume(&bv) != eCollision_Outside;
	}

	//-----------------------------------------------------------------------

	void cLensFlare::AssignOcclusionQuery(iRenderer *apRenderer)
	{
		if(IsAnyTypeActive()==false) return;

		iVertexBuffer *pVtx = apRenderer->GetShapeBoxVertexBuffer();
		cMatrixf *pMtx = &m_mtxOccluder;
		m_mtxOccluder = cMath::MatrixMul(GetWorldMatrix(), cMath::MatrixScale(mvSourceSize));

		iEntity3D *pParent = GetEntityParent();
		if(mbUseParentMeshForOcclusion && pParent && pParent->GetEntityType() == "SubMesh")
		{
			cSubMeshEntity *pSubMesh = static_cast<cSubMeshEntity*>(pParent);
			pVtx = pSubMesh->GetVertexBuffer();
			pMtx = pSubMesh->GetModelMatrix(NULL);
		}
		apRenderer->AssignOcclusionObject(this, 0, pVtx, pMtx, true);
		apRenderer->AssignOcclusionObject(this, 1, pVtx, pMtx, false);
	}

	bool cLensFlare::RetrieveOcculsionQuery(iRenderer *apRenderer)
	{
		mVis.target = 0;
		if(IsAnyTypeActive()==false) return false;

		float fVis = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 0);
		float fTot = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 1);
		if(fVis <= 0) return false;

		mOcclusionBV.SetTransform(GetWorldMatrix());
		cVector3f vMin, vMax;
		if(cMath::GetNormalizedClipRectFromBV(vMin, vMax, mOcclusionBV, apRenderer->GetCurrentFrustum(), 0)==false) return true;

		mVis.target = ohpl_flare_occlusion_target(fVis, fTot, &vMin, &vMax);
		return true;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cLensFlare::PrepareMultiIrisVertexBuffer(cFrustum *apFrustum, float afVis)
	{
		int lCount = mlMultiIrisCount;
		if(IsFlareActive(eLensFlareType_MultiIris)==false || lCount <= 0) return NULL;

		if(mpMultiIrisVtxBuffer && mpMultiIrisVtxBuffer->GetVertexNum() != lCount*4)
		{
			hplDelete(mpMultiIrisVtxBuffer);
			mpMultiIrisVtxBuffer = NULL;
		}
		if(mpMultiIrisVtxBuffer==NULL)
		{
			mpMultiIrisVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic, lCount*4, lCount*6);
			mpMultiIrisVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
			mpMultiIrisVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
			mpMultiIrisVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
			for(int i=0; i<lCount*4; ++i)
			{
				mpMultiIrisVtxBuffer->AddVertexVec4f(eVertexBufferElement_Position, 0, 1);
				mpMultiIrisVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(0,0));
				mpMultiIrisVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, 0);
			}
			unsigned int vIdx[6] = {0,1,2,0,2,3};
			for(int i=0; i<lCount; ++i)
				for(int j=0; j<6; ++j) mpMultiIrisVtxBuffer->AddIndex(i*4 + vIdx[j]);
			mpMultiIrisVtxBuffer->Compile(0);
		}

		float *pPos = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		float *pCol = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);
		float *pUv = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Texture0);
		OhplFlareIn in = MakeIn();
		OhplFlareEnt ent = FlareEnt(this);
		OhplFlareView view = FlareView(apFrustum);
		ohpl_flare_multi_iris(&in, &ent, &view, afVis, pPos, pCol, pUv, lCount);
		mpMultiIrisVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0, false);
		return mpMultiIrisVtxBuffer;
	}
}
