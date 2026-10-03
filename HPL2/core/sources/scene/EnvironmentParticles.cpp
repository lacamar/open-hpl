#include "scene/EnvironmentParticles.h"

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

	static float FastRandomFloat(int alX)
	{
		unsigned int x = ((unsigned int)alX << 13) ^ (unsigned int)alX;
		unsigned int v = (x * (x * x * 15731u + 789221u) + 1376312589u) & 0x7fffffffu;
		return 1.0f - (float)v / 1073741824.0f;
	}

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

		cVector2l vSubDiv(cMath::Max(avSubDivUV.x, 1), cMath::Max(avSubDivUV.y, 1));
		cVector2f vCellSize(1.0f / vSubDiv.x, 1.0f / vSubDiv.y);
		const cVector2f vCorners[4] = {cVector2f(1,1), cVector2f(0,1), cVector2f(0,0), cVector2f(1,0)};
		const cVector2f vOffsets[4] = {cVector2f(0.5f,-0.5f), cVector2f(-0.5f,-0.5f), cVector2f(-0.5f,0.5f), cVector2f(0.5f,0.5f)};
		for(int i=0; i<alNum; ++i)
		{
			cVector3f vPos = cMath::RandRectVector3f(0, afBoxSize);
			cVector2f vCell(cMath::RandRectl(0, vSubDiv.x-1) * vCellSize.x, cMath::RandRectl(0, vSubDiv.y-1) * vCellSize.y);
			for(int j=0; j<4; ++j)
			{
				mpVtxBuffer->AddVertexVec4f(eVertexBufferElement_Position, vPos, 1);
				mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 1));
				cVector2f vUv = vCell + vCorners[j] * vCellSize;
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(vUv.x, vUv.y, 0));
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture1, cVector3f(vOffsets[j].x * avParticleSize.x, vOffsets[j].y * avParticleSize.y, 0));
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

		mvIterGravityVel[0] = mvGravityVel;
		mvIterWindVel[0] = mvWindVel;
		mvIterRotateVel[0] = mvRotateVel;
		for(int i=1; i<kMaxEnvParticleIterations; ++i)
		{
			int s = 22 + 13*(i-1);
			mvIterGravityVel[i] = mvGravityVel + mvGravityVel * mfGravitySpeedRand * FastRandomFloat(s-9);
			mvIterWindVel[i] = mvWindVel + mvWindVel * mfWindSpeedRand * FastRandomFloat(s-8);
			mvIterRotateVel[i] = mvRotateVel + mvRotateVel * mfRotateSpeedRand * FastRandomFloat(s-7);
			if(mbRotateBothDirs)
				for(int k=0; k<3; ++k) if(FastRandomFloat(s-6+k) > 0) mvIterRotateVel[i].v[k] = -mvIterRotateVel[i].v[k];
			if(mfWindDirRand > 0)
			{
				cVector3f vAxis = cMath::Vector3Normalize(cVector3f(FastRandomFloat(s-3), FastRandomFloat(s-2), FastRandomFloat(s-1)));
				cMatrixf mtxRot = cMath::MatrixRotate(vAxis * (mfWindDirRand * kPif * FastRandomFloat(s)), eEulerRotationOrder_XYZ);
				mvIterWindVel[i] = cMath::MatrixMul(mtxRot, mvIterWindVel[i]);
			}
		}
	}

	void cEnvironmentParticles::Update(float afTimeStep)
	{
		UpdateVelocityVectors();
		for(int i=0; i<mlIterationNum; ++i)
		{
			mvIterOffset[i] += (mvIterGravityVel[i] + mvIterWindVel[i]) * afTimeStep;
			mvIterRotation[i] += mvIterRotateVel[i] * afTimeStep;
			for(int k=0; k<3; ++k)
			{
				if(std::fabs(mvIterOffset[i].v[k]) > 10000) mvIterOffset[i].v[k] = cMath::Modulus(mvIterOffset[i].v[k], 10000);
				if(std::fabs(mvIterRotation[i].v[k]) > 10000) mvIterRotation[i].v[k] = cMath::Modulus(mvIterRotation[i].v[k], 10000);
			}
		}

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
