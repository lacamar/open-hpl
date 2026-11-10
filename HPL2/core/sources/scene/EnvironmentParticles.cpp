#include "scene/EnvironmentParticles.h"
#include "scene/RustParticles.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/Texture.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "scene/Entity3D.h"
#include "scene/LightSpot.h"
#include "math/Math.h"
#include "math/Frustum.h"
#include "system/PreprocessParser.h"
#include "system/String.h"

namespace hpl {

	enum
	{
		kVar_avCameraUp,
		kVar_avCameraRight,
		kVar_avOffset,
		kVar_avBoxSize,
		kVar_avBoxWorldPos,
		kVar_a_mtxRotation,
		kVar_avColor,
		kVar_avFadeProp,
		kVar_avLightProbeLevels,
		kVar_avDepthOfFieldParams,
		kVar_a_mtxLightViewProj,
		kVar_avLightPosScale,
		kVar_avLightColor,
		kVar_afLightFalloff,
		kVar_afSpotFalloffPow,
		kVar_avLightProbePositions,
		kVar_a_mtxIntersectClip = kVar_avLightProbePositions + 3,
		kVar_a_mtxSubtractClip = kVar_a_mtxIntersectClip + 64,
	};

	float (*cEnvironmentParticles::mpLightLevelFunc)(const cVector3f& avPos) = NULL;

	//-----------------------------------------------------------------------

	cEnvironmentParticles::cEnvironmentParticles(const tString& asName, cWorld *apWorld, cGraphics *apGraphics, cResources *apResources)
		: msName(asName), mpWorld(apWorld), mpGraphics(apGraphics), mpResources(apResources)
	{
		for(int i=0; i<kMaxEnvParticleIterations; ++i)
		{
			mvIterOffset[i] = cMath::RandRectVector3f(0, 100);
			mvIterRotation[i] = cMath::RandRectVector3f(0, 100);
		}
	}

	cEnvironmentParticles::~cEnvironmentParticles()
	{
		if(mpVtxBuffer) hplDelete(mpVtxBuffer);
		if(mpTexture) mpResources->GetTextureManager()->Destroy(mpTexture);
		for(int i=0; i<2; ++i) if(mpProgram[i]) mpGraphics->DestroyGpuProgram(mpProgram[i]);
	}

	//-----------------------------------------------------------------------

	void cEnvironmentParticles::Setup(float afBoxSize, int alNum, const cVector2f& avParticleSize, const cVector2l& avSubDivUV,
									  bool abAffectedByLight, iTexture *apTexture)
	{
		mfBoxSize = afBoxSize;
		mvParticleSize = avParticleSize;
		mbAffectedByLight = abAffectedByLight;
		mpTexture = apTexture;

		if(mpVtxBuffer) hplDelete(mpVtxBuffer);
		mpVtxBuffer = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	 eVertexBufferUsageType_Static, alNum*4, alNum*6);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture1, eVertexBufferElementFormat_Float, 3);

		std::vector<float> vPosData(alNum > 0 ? alNum*16 : 0), vUv0Data(alNum > 0 ? alNum*12 : 0), vUv1Data(alNum > 0 ? alNum*12 : 0);
		ohpl_env_setup(afBoxSize, alNum, &avParticleSize, &avSubDivUV, vPosData.data(), vUv0Data.data(), vUv1Data.data());
		for(int i=0; i<alNum; ++i)
		{
			for(int j=0; j<4; ++j)
			{
				const float *p = &vPosData[i*16 + j*4];
				mpVtxBuffer->AddVertexVec4f(eVertexBufferElement_Position, cVector3f(p[0], p[1], p[2]), p[3]);
				mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 1));
				const float *u0 = &vUv0Data[i*12 + j*3];
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(u0[0], u0[1], u0[2]));
				const float *u1 = &vUv1Data[i*12 + j*3];
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture1, cVector3f(u1[0], u1[1], u1[2]));
			}
			int lIdx[6] = {0, 1, 2, 2, 3, 0};
			for(int j=0; j<6; ++j) mpVtxBuffer->AddIndex(i*4 + lIdx[j]);
		}
		mpVtxBuffer->Compile(0);
	}

	//-----------------------------------------------------------------------

	void cEnvironmentParticles::SetIterationNum(float afNum)
	{
		mfIterationNum = cMath::Clamp(afNum, 0.0f, (float)kMaxEnvParticleIterations);
		mlIterationNum = (int)ceilf(mfIterationNum);
		mbVelDirty = true;
	}

	void cEnvironmentParticles::UpdateVelocityVectors()
	{
		if(mbVelDirty == false) return;
		mbVelDirty = false;

		OhplEnvVelIn in;
		in.mvGravityVel = mvGravityVel;
		in.mvWindVel = mvWindVel;
		in.mvRotateVel = mvRotateVel;
		in.mfGravitySpeedRand = mfGravitySpeedRand;
		in.mfWindSpeedRand = mfWindSpeedRand;
		in.mfWindDirRand = mfWindDirRand;
		in.mfRotateSpeedRand = mfRotateSpeedRand;
		in.mbRotateBothDirs = mbRotateBothDirs;
		ohpl_env_velocity_vectors(&in, mvIterGravityVel, mvIterWindVel, mvIterRotateVel);
	}

	void cEnvironmentParticles::Update(float afTimeStep)
	{
		UpdateVelocityVectors();
		ohpl_env_update(mlIterationNum, afTimeStep, mvIterGravityVel, mvIterWindVel, mvIterRotateVel, mvIterOffset, mvIterRotation);

		float fShrink = mvParticleSize.Length() * 0.5f;
		for(size_t i=0; i<mvClipAreas.size(); ++i)
		{
			cClipArea &clip = mvClipAreas[i];
			if(clip.mpEntity->IsActive() == false) { clip.m_mtxClip = cMath::MatrixScale(10000); continue; }
			cBoundingVolume *pBV = clip.mpEntity->GetBoundingVolume();
			cVector3f vMin = pBV->GetLocalMin(), vMax = pBV->GetLocalMax();
			cVector3f vHalf = (vMax - vMin) * 0.5f - fShrink;
			for(int k=0; k<3; ++k) vHalf.v[k] = cMath::Max(vHalf.v[k], 0.0001f);
			cMatrixf mtxBox = cMath::MatrixMul(cMath::MatrixTranslate((vMin + vMax) * 0.5f), cMath::MatrixScale(vHalf));
			clip.m_mtxClip = cMath::MatrixInverse(cMath::MatrixMul(pBV->GetTransform(), mtxBox));
		}
	}

	//-----------------------------------------------------------------------

	bool cEnvironmentParticles::IsVisible(cFrustum *apFrustum)
	{
		if(mbVisible == false || mpTexture == NULL || mpVtxBuffer == NULL) return false;
		if(mbClipActive == false) return true;

		float fMaxDist = cMath::Min(mfFadeOutEnd, mfBoxSize * 0.5f);
		int lIntersect = 0;
		for(size_t i=0; i<mvClipAreas.size(); ++i)
		{
			cClipArea &clip = mvClipAreas[i];
			if(clip.mbSubtractive || clip.mpEntity->IsActive() == false) continue;
			++lIntersect;
			cBoundingVolume *pBV = clip.mpEntity->GetBoundingVolume();
			cVector3f vOrigin = apFrustum->GetOrigin(), vMin = pBV->GetMin(), vMax = pBV->GetMax();
			cVector3f vClosest(cMath::Clamp(vOrigin.x, vMin.x, vMax.x), cMath::Clamp(vOrigin.y, vMin.y, vMax.y), cMath::Clamp(vOrigin.z, vMin.z, vMax.z));
			if(cMath::Vector3Dist(vOrigin, vClosest) < fMaxDist &&
			   apFrustum->CollideBoundingVolume(pBV) != eCollision_Outside)
				return true;
		}
		return lIntersect == 0;
	}

	//-----------------------------------------------------------------------

	void cEnvironmentParticles::AddClipArea(iEntity3D *apClipEntity, bool abSubtractive)
	{
		if(apClipEntity == NULL) return;
		RemoveClipArea(apClipEntity);
		cClipArea clip = {apClipEntity, abSubtractive, cMatrixf::Identity};
		mvClipAreas.push_back(clip);
	}

	void cEnvironmentParticles::RemoveClipArea(iEntity3D *apClipEntity)
	{
		for(size_t i=0; i<mvClipAreas.size(); ++i)
			if(mvClipAreas[i].mpEntity == apClipEntity) { mvClipAreas.erase(mvClipAreas.begin() + i); return; }
	}

	//-----------------------------------------------------------------------

	iGpuProgram* cEnvironmentParticles::GetProgram(bool abDepthOfField)
	{
		int lIntersect = 0, lSubtract = 0;
		if(mbClipActive)
			for(size_t i=0; i<mvClipAreas.size(); ++i) (mvClipAreas[i].mbSubtractive ? lSubtract : lIntersect)++;
		lIntersect = cMath::Min(lIntersect, 64);
		lSubtract = cMath::Min(lSubtract, 64);
		bool bProbes = mbAffectedByLight && mpLightLevelFunc;

		int lKey = (bProbes ? 1 : 0) | (mpSpotLight ? 2 : 0) | (lIntersect << 2) | (lSubtract << 9);
		int d = abDepthOfField ? 1 : 0;
		if(mlProgramKey[d] == lKey) return mpProgram[d];
		if(mpProgram[d]) mpGraphics->DestroyGpuProgram(mpProgram[d]);

		cParserVarContainer vars;
		vars.Add("UseDiffuse");
		vars.Add("UseUv");
		vars.Add("UseColor");
		if(bProbes) vars.Add("UseLightProbes");
		if(abDepthOfField) vars.Add("UseDepthOfField");
		if(mpSpotLight) vars.Add("UseSpotLight");
		if(lIntersect + lSubtract > 0)
		{
			vars.Add("UseClipping");
			if(lIntersect) { vars.Add("IntersectClip"); vars.Add("kNumIntersect", lIntersect); }
			if(lSubtract) { vars.Add("SubtractClip"); vars.Add("kNumSubtract", lSubtract); }
		}
		iGpuProgram *pProg = mpGraphics->CreateGpuProgramFromShaders("EnvironmentParticles_" + cString::ToString(lKey*2 + d),
																	 "deferred_env_particles_vtx.glsl", "deferred_base_frag.glsl", &vars);
		mpProgram[d] = pProg;
		mlProgramKey[d] = lKey;
		if(pProg == NULL) return NULL;

		const char *vNames[] = {"avCameraUp", "avCameraRight", "avOffset", "avBoxSize", "avBoxWorldPos", "a_mtxRotation", "avColor",
								"avFadeProp", "avLightProbeLevels", "avDepthOfFieldParams", "a_mtxLightViewProj", "avLightPosScale",
								"avLightColor", "afLightFalloff", "afSpotFalloffPow"};
		for(int i=0; i<(int)(sizeof(vNames)/sizeof(vNames[0])); ++i) pProg->GetVariableAsId(vNames[i], i);
		for(int i=0; i<3; ++i) pProg->GetVariableAsId("avLightProbePositions[" + cString::ToString(i) + "]", kVar_avLightProbePositions + i);
		for(int i=0; i<lIntersect; ++i) pProg->GetVariableAsId("a_mtxIntersectClip[" + cString::ToString(i) + "]", kVar_a_mtxIntersectClip + i);
		for(int i=0; i<lSubtract; ++i) pProg->GetVariableAsId("a_mtxSubtractClip[" + cString::ToString(i) + "]", kVar_a_mtxSubtractClip + i);
		return pProg;
	}

	void cEnvironmentParticles::SetupProgramBase(iGpuProgram *apProg, cFrustum *apFrustum, const float *afDofParams)
	{
		const cMatrixf& mtxView = apFrustum->GetViewMatrix();
		cVector3f vOrigin = apFrustum->GetOrigin();
		cVector3f vFwd = mtxView.GetForward();
		apProg->SetVec3f(kVar_avCameraUp, mtxView.GetUp());
		apProg->SetVec3f(kVar_avCameraRight, mtxView.GetRight());
		apProg->SetVec3f(kVar_avBoxWorldPos, vOrigin + vFwd * mfBoxDistance - mfBoxSize * 0.5f);
		apProg->SetVec3f(kVar_avBoxSize, mfBoxSize, mfBoxSize, mfBoxSize);
		apProg->SetVec4f(kVar_avColor, mColor.r*mColor.r*mfBrightness, mColor.g*mColor.g*mfBrightness, mColor.b*mColor.b*mfBrightness, mColor.a);
		apProg->SetVec4f(kVar_avFadeProp, mfFadeInStart, mfFadeInEnd - mfFadeInStart, mfFadeOutStart, mfFadeOutEnd - mfFadeOutStart);
		apProg->SetVec4f(kVar_avDepthOfFieldParams, afDofParams[0], afDofParams[1], afDofParams[2], afDofParams[3]);

		if(mbAffectedByLight && mpLightLevelFunc)
		{
			cVector3f vProbes[3] = {vOrigin, vOrigin - vFwd * (mfBoxSize * 0.3f), vOrigin - vFwd * (mfBoxSize * 0.75f)};
			float fLevels[3];
			for(int i=0; i<3; ++i)
			{
				apProg->SetVec3f(kVar_avLightProbePositions + i, vProbes[i]);
				fLevels[i] = mpLightLevelFunc(vProbes[i]);
			}
			apProg->SetVec3f(kVar_avLightProbeLevels, fLevels[0], fLevels[1], fLevels[2]);
		}

		if(mpSpotLight)
		{
			cLightSpot *pSpot = mpSpotLight;
			cColor col = pSpot->GetDiffuseColor();
			float fMul = pSpot->GetBrightness() * mfSpotLightMul * (pSpot->IsVisible() ? 1.0f : 0.0f);
			apProg->SetMatrixf(kVar_a_mtxLightViewProj, pSpot->GetViewProjMatrix());
			cVector3f vPos = pSpot->GetWorldPosition();
			apProg->SetVec4f(kVar_avLightPosScale, vPos.x, vPos.y, vPos.z, 1.0f / pSpot->GetRadius());
			apProg->SetVec4f(kVar_avLightColor, col.r*col.r*fMul, col.g*col.g*fMul, col.b*col.b*fMul, col.a);
			apProg->SetFloat(kVar_afLightFalloff, pSpot->GetFalloffPow() * 2);
			apProg->SetFloat(kVar_afSpotFalloffPow, pSpot->GetSpotFalloffPow() * 2);
		}

		if(mbClipActive)
		{
			int lIntersect = 0, lSubtract = 0;
			for(size_t i=0; i<mvClipAreas.size(); ++i)
			{
				if(mvClipAreas[i].mbSubtractive) apProg->SetMatrixf(kVar_a_mtxSubtractClip + lSubtract++, mvClipAreas[i].m_mtxClip);
				else							 apProg->SetMatrixf(kVar_a_mtxIntersectClip + lIntersect++, mvClipAreas[i].m_mtxClip);
			}
		}
	}

	void cEnvironmentParticles::SetupProgramIteration(iGpuProgram *apProg, int alIdx)
	{
		apProg->SetVec3f(kVar_avOffset, mvIterOffset[alIdx]);
		apProg->SetMatrixf(kVar_a_mtxRotation, cMath::MatrixRotate(mvIterRotation[alIdx], eEulerRotationOrder_XYZ));

		float fFrac = mfIterationNum - floorf(mfIterationNum);
		if(alIdx == mlIterationNum-1 && fFrac > 0.001f)
			apProg->SetVec4f(kVar_avColor, mColor.r*mColor.r*mfBrightness, mColor.g*mColor.g*mfBrightness, mColor.b*mColor.b*mfBrightness, fFrac);
	}
};
