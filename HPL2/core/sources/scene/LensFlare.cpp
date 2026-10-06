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

namespace hpl {

	//-----------------------------------------------------------------------

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
		cMatrixf *pSizeRot = mpParent->GetSizeAndRotation(mType, apFrustum, mpParent->mfVis);
		m_mtxModel = cMath::MatrixMul(*mpParent->GetModelMatrix(apFrustum), *pSizeRot);
		return &m_mtxModel;
	}

	bool cLensFlareType::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
		float fVis = mpParent->mfVis;
		if(fVis <= 0) return false;
		cColor c = mpParent->GetFlareColorAndBrightness(mType, apFrustum, 1);
		float fB = mpParent->mfBrightness;
		c = cColor(c.r*c.r*fB, c.g*c.g*fB, c.b*c.b*fB, c.a) * (0.5f*fVis + 0.5f);
		if(mType == eLensFlareType_AnamorphicFlare) c = c * fVis;
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
		mfVisTarget = 0;
		mfVis = 1;
		mfVisVel = 0;
		mlVisFrame = -1;

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
		mfOuterCos = cMath::Min(cosf(afAngle*0.5f), mfInnerCos);
	}

	void cLensFlare::SetInnerFieldOfView(float afAngle)
	{
		mfInnerFov = afAngle;
		mfInnerCos = cosf(afAngle*0.5f);
		mfOuterCos = cMath::Min(mfOuterCos, mfInnerCos);
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
		mfFade = cMath::Clamp(mfFade + afTimeStep*mfFadeRate, 0, 1);
	}

	// ponytail: smoothing state lives in the flare, not per render settings; a skipped frame restarts from 0
	void cLensFlare::UpdateVisibility(float afTimeStep, int alFrame)
	{
		if(alFrame == mlVisFrame) return;
		if(mlVisFrame < 0)
		{
			mfVis = mfVisTarget;
			mfVisVel = 0;
		}
		else
		{
			if(alFrame - mlVisFrame > 1) mfVis = mfVisVel = 0;
			float fT = cMath::Min(1.0f, afTimeStep*15);
			float fDiff = mfVisTarget - mfVis;
			if(mfVisVel*fDiff <= 0) mfVisVel = 0;
			mfVisVel += fT*fDiff;
			mfVis = cMath::Clamp(mfVis + mfVisVel*fT, 0, 1);
		}
		mlVisFrame = alFrame;
	}

	//-----------------------------------------------------------------------

	static cVector3f FacingAxis(iEntity3D *apEnt)
	{
		const cMatrixf &m = apEnt->GetWorldMatrix();
		if(apEnt->GetEntityParent() || apEnt->GetParent()) return cVector3f(m.m[0][2], m.m[1][2], m.m[2][2]);
		return m.GetForward();
	}

	static cVector3f DirToCamera(iEntity3D *apEnt, cFrustum *apFrustum)
	{
		cVector3f vDir = apFrustum->GetOrigin() - apEnt->GetWorldPosition();
		float fLen = vDir.Length();
		return fLen > 1e-8f ? vDir / fLen : vDir;
	}

	float cLensFlare::FovFactor(cFrustum *apFrustum)
	{
		float fC = cMath::Vector3Dot(DirToCamera(this, apFrustum), FacingAxis(this)*-1);
		return (cMath::Clamp(fC, mfOuterCos - 1e-4f, mfInnerCos) - mfOuterCos + 1e-4f) / (mfInnerCos - mfOuterCos + 1e-4f);
	}

	static float ScreenEdge(const cVector3f &avNdc){ return cMath::Min(1 - fabsf(avNdc.x), 1 - fabsf(avNdc.y));}

	cColor cLensFlare::GetFlareColorAndBrightness(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		if(IsFlareActive(aType)==false) return cColor(0,0);

		float fFov = FovFactor(apFrustum);
		cVector3f vView = cMath::MatrixMul(apFrustum->GetViewMatrix(), GetWorldPosition());
		cVector3f vNdc = cMath::MatrixMulDivideW(apFrustum->GetProjectionMatrix(), vView);
		float fD = -vView.z;

		float fScreenSize = ScaleFromDistance(fD) * mvSize[GetFirstActiveType()].Length() / (8*fD);
		float fEdge = ScreenEdge(vNdc);
		if(fEdge - 0.5f*fScreenSize < 0) fFov *= (fEdge + 0.5f*fScreenSize) / fScreenSize;

		float fDist = vView.Length();
		if(mvRangeMax.y > 0) fFov *= cMath::Clamp((mvRangeMax.y - fDist)/(mvRangeMax.y - mvRangeMax.x), 0, 1);
		if(mvRangeMin.y > 0) fFov *= cMath::Clamp((fDist - mvRangeMin.x)/(mvRangeMin.y - mvRangeMin.x), 0, 1);

		float fV = cMath::Max(fFov, 0.0f);
		if(aType == eLensFlareType_AnamorphicFlare) fV *= fV;
		return mColor[aType] * (fV * mfFade) * (mbShrinkWhenOccluded ? 0.5f*afVis + 0.5f : afVis);
	}

	cColor cLensFlare::GetGlare(cFrustum *apFrustum, cVector3f &avScreenPos, float afScale)
	{
		cVector3f vToCam = DirToCamera(this, apFrustum);
		cVector3f vAxis = FacingAxis(this);
		float fC = -cMath::Vector3Dot(vToCam, vAxis);

		float fG = (cMath::Max(fC, mfGlareCos) - mfGlareCos) / (1 - mfGlareCos + 1e-4f);
		float fDv = cMath::Vector3Dot(vToCam, apFrustum->GetViewMatrix().GetForward());
		if(fDv > 0.9125f) fG += (fDv - 0.9125f) / 0.087599978f * mfGlareStareAt;
		fG *= 0.33f*cMath::Vector3Dot(vToCam, vAxis) + 0.67f;
		if(mvGlareRange.y > 0)
		{
			float fDist = cMath::Vector3Dist(apFrustum->GetOrigin(), GetWorldPosition());
			fG *= cMath::Clamp((mvGlareRange.y - fDist)/(mvGlareRange.y - mvGlareRange.x), 0, 1);
		}
		avScreenPos = 0;
		if(fG > 0)
		{
			cVector3f vView = cMath::MatrixMul(apFrustum->GetViewMatrix(), GetWorldPosition());
			cVector3f vNdc = cMath::MatrixMulDivideW(apFrustum->GetProjectionMatrix(), vView);
			avScreenPos = cVector3f(vNdc.x*0.5f + 0.5f, vNdc.y*0.5f + 0.5f, 0);
		}
		fG *= mfGlareBrightness;

		cColor col = GetFlareColorAndBrightness(GetFirstActiveType(), apFrustum, 1);
		return cColor(col.r*col.r*fG, col.g*col.g*fG, col.b*col.b*fG, col.a) * afScale;
	}

	//-----------------------------------------------------------------------

	cMatrixf* cLensFlare::GetSizeAndRotation(eLensFlareType aType, cFrustum *apFrustum, float afVis)
	{
		cMatrixf &mtx = m_mtxSizeRot[aType];
		if(IsFlareActive(aType)==false || aType == eLensFlareType_MultiIris)
		{
			mtx = cMatrixf::Identity;
			return &mtx;
		}

		cVector3f vView = cMath::MatrixMul(apFrustum->GetViewMatrix(), GetWorldPosition());
		cVector3f vNdc = cMath::MatrixMulDivideW(apFrustum->GetProjectionMatrix(), vView);
		float fD = -vView.z;

		float fV = mbShrinkWhenOccluded ? afVis : 1;
		cVector2f vSize = mvSize[aType];
		cVector3f vScale(vSize.x*fV, vSize.y*fV, fV);
		float fScreenSize = ScaleFromDistance(fD) * vSize.Length() / (4*fD);
		float fEdge = ScreenEdge(vNdc);
		if(fEdge - 0.5f*fScreenSize < 0) vScale = vScale * ((fEdge - 0.5f*fScreenSize + fScreenSize) / fScreenSize);

		if(aType == eLensFlareType_AnamorphicFlare)
		{
			cVector3f vVec(vSize.x, vSize.y, 1);
			if(vSize.x > vSize.y) vVec.x = (1 + 2*vNdc.x*vNdc.x) * vScale.x;
			if(vSize.y > vVec.x) vVec.y = (1 + 2*vNdc.y*vNdc.y) * vScale.y;
			mtx = cMath::MatrixScale(vVec);
		}
		else
		{
			float fRot = -(0.5f*atan2f(0.4f, vNdc.x) - 0.78539819f);
			mtx = cMath::MatrixMul(cMath::MatrixRotateZ(fRot), cMath::MatrixScale(vScale));
		}
		return &mtx;
	}

	cMatrixf* cLensFlare::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL) return &GetWorldMatrix();

		cVector3f vPos = GetWorldPosition();
		cVector3f vToCam = DirToCamera(this, apFrustum);
		const cMatrixf &mtxView = apFrustum->GetViewMatrix();
		cVector3f vCamZ = mtxView.GetForward();
		cVector3f vUp = mtxView.GetUp();
		cVector3f vFwd = vCamZ * (cMath::Vector3Dot(vToCam, vCamZ) > 0 ? 1.0f : -1.0f);
		cVector3f vRight = cMath::Vector3Cross(vUp, vFwd);

		float fC = -cMath::Vector3Dot(vToCam, FacingAxis(this));
		float fFov = (cMath::Clamp(fC, mfOuterCos - 1e-4f, mfInnerCos) - mfOuterCos + 1e-4f) * 0.5f / (mfInnerCos - mfOuterCos + 1e-4f) + 0.5f;
		float fScale = 2 * ScaleFromDistance(cMath::Vector3Dot(vCamZ, apFrustum->GetOrigin() - vPos)) * fFov;

		cMatrixf &m = m_mtxBillboard;
		m = cMatrixf::Identity;
		for(int i=0; i<3; ++i)
		{
			m.m[i][0] = vRight.v[i]*fScale;
			m.m[i][1] = vUp.v[i]*fScale;
			m.m[i][2] = vFwd.v[i]*fScale;
			m.m[i][3] = vPos.v[i];
		}
		return &m;
	}

	//-----------------------------------------------------------------------

	void cLensFlare::UpdateBV()
	{
		float fR = 0.125f;
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
			if(mbActive[i]) fR = cMath::Max(fR, mvSize[i].Length());
		mBoundingVolume.SetLocalMinMax(-fR, fR);
		SetTransformUpdated();
	}

	bool cLensFlare::CollidesWithFrustum(cFrustum *apFrustum)
	{
		if(IsAnyTypeActive()==false) return false;

		cBoundingVolume bv = *GetBoundingVolume();
		float fD2 = cMath::Vector3DistSqr(apFrustum->GetOrigin(), bv.GetWorldCenter());
		if(mvRangeMax.y > 0 && fD2 > mvRangeMax.y*mvRangeMax.y) return false;
		if(mvRangeMin.y > 0 && mvRangeMin.x*mvRangeMin.x > fD2) return false;

		float fS = ScaleFromDistance(-cMath::MatrixMul(apFrustum->GetViewMatrix(), bv.GetWorldCenter()).z);
		cVector3f vC = bv.GetLocalCenter();
		bv.SetLocalMinMax(vC + (bv.GetLocalMin() - vC)*fS, vC + (bv.GetLocalMax() - vC)*fS);
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
		mfVisTarget = 0;
		if(IsAnyTypeActive()==false) return false;

		float fVis = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 0);
		float fTot = (float)apRenderer->RetrieveOcclusionObjectSamples(this, 1);
		if(fVis <= 0) return false;

		mOcclusionBV.SetTransform(GetWorldMatrix());
		cVector3f vMin, vMax;
		if(cMath::GetNormalizedClipRectFromBV(vMin, vMax, mOcclusionBV, apRenderer->GetCurrentFrustum(), 0)==false) return true;

		float fRawArea = (vMax.x - vMin.x)*(vMax.y - vMin.y);
		vMin.x = cMath::Max(vMin.x, -1.0f); vMin.y = cMath::Max(vMin.y, -1.0f);
		vMax.x = cMath::Min(vMax.x, 1.0f); vMax.y = cMath::Min(vMax.y, 1.0f);
		float fClampArea = (vMax.x - vMin.x)*(vMax.y - vMin.y);
		if(fRawArea > 0) mfVisTarget = cMath::Clamp(fabsf(fClampArea/fRawArea) * fVis/(fTot + 1e-4f), 0, 1);
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

		cVector3f vView = cMath::MatrixMul(apFrustum->GetViewMatrix(), GetWorldPosition());
		cVector3f vNdc = cMath::MatrixMulDivideW(apFrustum->GetProjectionMatrix(), vView);
		float fSx = vNdc.x*0.5f + 0.5f;
		float fSy = 1 - (vNdc.y*0.5f + 0.5f);
		float fZ = vNdc.z*0.5f + 0.5f;

		cColor col = GetFlareColorAndBrightness(eLensFlareType_MultiIris, apFrustum, 1);
		cColor c = cColor(col.r*col.r*mfBrightness, col.g*col.g*mfBrightness, col.b*col.b*mfBrightness, col.a) * afVis;
		if(mbMulMultiIrisWithGlare)
		{
			cVector3f vTemp;
			c = c * (GetGlare(apFrustum, vTemp, 1) * 2);
		}

		// official MSVCR100 rand() sequence
		unsigned int lState = (unsigned int)mlMultiIrisSeed;
		auto Rand = [&](){ lState = lState*214013u + 2531011u; return (int)((lState>>16) & 0x7fff); };
		auto RandF = [&](float a, float b){ return (float)Rand() * 3.0518509e-05f * (b-a) + a; };
		auto RandL = [&](int a, int b){ return a + Rand() % (b-a+1); };

		Rand();
		float fRy = RandF(0.495f, 0.505f);
		float fRx = RandF(0.495f, 0.505f);

		cVector2f vDir(fRx - fSx, fRy - fSy);
		float fL = vDir.Length();
		float fSizeScale = afVis*0.25f + 0.25f;
		cVector2f vHalfBase(mvSize[eLensFlareType_MultiIris].x*fSizeScale, mvSize[eLensFlareType_MultiIris].y*apFrustum->GetAspect()*fSizeScale);
		float fDenom = 3*fL + 1;
		int lGx = mvAtlasGrid.x, lGy = mvAtlasGrid.y;
		cVector2f vDuv(1.0f/(float)lGx, 1.0f/(float)lGy);

		float *pPos = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		float *pCol = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);
		float *pUv = mpMultiIrisVtxBuffer->GetFloatArray(eVertexBufferElement_Texture0);
		for(int i=0; i<lCount; ++i)
		{
			float fR = RandF(-0.5f, 2.25f);
			float fS1 = RandF(0.33f, 1);
			float fS2 = RandF(0.33f, 1);
			float fRot = RandF(0, k2Pif);
			float fAmp = RandF(0.5f, 1);
			int lIy = RandL(0, lGy);
			int lIx = RandL(0, lGx);

			cVector2f vC = cVector2f(fSx, fSy) + vDir*fR;
			cVector2f vH = vHalfBase * (fS1*fS2/fDenom*(fabsf(0.25f*fR) + 0.5f));
			float fM = sinf(fRot + fL*fR*kPif) * fAmp;
			cColor ci = c * (fM*fM);
			cVector2f vUv0(lIx*vDuv.x, lIy*vDuv.y);

			cVector2f vCorner[4] = {cVector2f(-1,-1), cVector2f(1,-1), cVector2f(1,1), cVector2f(-1,1)};
			for(int j=0; j<4; ++j)
			{
				pPos[0] = vC.x + vCorner[j].x*vH.x; pPos[1] = vC.y + vCorner[j].y*vH.y; pPos[2] = fZ; pPos[3] = 1;
				pCol[0] = ci.r; pCol[1] = ci.g; pCol[2] = ci.b; pCol[3] = ci.a;
				pUv[0] = vUv0.x + (vCorner[j].x > 0 ? vDuv.x : 0); pUv[1] = vUv0.y + (vCorner[j].y > 0 ? vDuv.y : 0); pUv[2] = 0;
				pPos += 4; pCol += 4; pUv += 3;
			}
		}
		mpMultiIrisVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0, false);
		return mpMultiIrisVtxBuffer;
	}
}
