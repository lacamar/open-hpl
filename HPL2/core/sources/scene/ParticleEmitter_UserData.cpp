/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

// NEW
#include "graphics/Graphics.h"
// ---
#include "scene/ParticleEmitter_UserData.h"
//#include "graphics/Mesh.h"
//#include "graphics/SubMesh.h"


#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/MaterialManager.h"
//#include "resources/MeshManager.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "scene/World.h"
#include "scene/RustSceneH12.h"

#include "math/Math.h"

#include "system/String.h"

namespace hpl {

	
	//////////////////////////////////////////////////////////////////////////
	// LOADER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	static const char* H12NoAttr(void*, const char*){ return NULL; }

	cParticleEmitterData_UserData::cParticleEmitterData_UserData(
		const tString &asName,cResources* apResources,cGraphics *apGraphics)
		: iParticleEmitterData(asName,apResources,apGraphics), mpDesc(ohpl_h12_ps_parse(H12NoAttr, NULL))
	{
	}

	//-----------------------------------------------------------------------

	iParticleEmitter* cParticleEmitterData_UserData::Create(tString asName, cVector3f avSize)
	{
		iParticleEmitter *pPE = hplNew( cParticleEmitter_UserData, (asName,&mvMaterials,avSize, 
																mpGraphics,mpResources,this) );
		
		const OhplH12PsDesc &d = D();
		cMatrixf mtxOffset = cMath::MatrixRotate(d.mvAngleOffset,eEulerRotationOrder_XYZ);
		mtxOffset.SetTranslation(d.mvPosOffset);
		pPE->SetMatrix(mtxOffset);

		return pPE;
	}

	//////////////////////////

	static const char* H12Attr(void *apCtx, const char *asName)
	{
		return static_cast<cXmlElement*>(apCtx)->GetAttribute(asName);
	}

	void cParticleEmitterData_UserData::LoadFromElement(cXmlElement *apElement, bool abLoadMaterials)
	{
		ohpl_h12_ps_free(mpDesc);
		mpDesc = ohpl_h12_ps_parse(H12Attr, apElement);
		const OhplH12PsDesc &d = D();

		msName = d.msName;
		mfWarmUpTime = d.mfWarmUpTime;
		mfWarmUpStepsPerSec = d.mfWarmUpStepsPerSec;

		if(abLoadMaterials)
		{
			tString sMaterial = d.msMaterial;
			if(d.mlMaterialNum <= 1)
			{
				sMaterial = cString::SetFileExt(sMaterial,"mat");
				cMaterial *pMaterial = mpResources->GetMaterialManager()->CreateMaterial(sMaterial);
				if(pMaterial) mvMaterials.push_back(pMaterial);
			}
			else
			{
				for(int i=1; i<d.mlMaterialNum+1; ++i)
				{
					tString sFileName;
					if(i>9)		sFileName = sMaterial + cString::ToString(i);
					else		sFileName = sMaterial + "0"+cString::ToString(i);

					sFileName = cString::SetFileExt(sFileName,"mat");

					cMaterial *pMaterial = mpResources->GetMaterialManager()->CreateMaterial(sFileName);
					if(pMaterial) mvMaterials.push_back(pMaterial);
				}
			}
		}

		const cVector2l vFirstSubDiv = mvMaterials.empty() ? cVector2l(0) : mvMaterials[0]->GetUVSubDivisions();
		ohpl_h12_ps_finish(mpDesc, (uint32_t)mvMaterials.size(), mvMaterials.empty() ? NULL : &vFirstSubDiv);
	}

	//-----------------------------------------------------------------------

	bool cParticleEmitterData_UserData::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		if(pBody->IsActive()==false || pBody->GetCollide()==false || pBody->IsCharacter())
		{
			return true;
		}

		if(mfShortestDist > apParams->mfDist)
		{
			mbIntersected = true;
			mfShortestDist = apParams->mfDist;
			*mpIntersectPos = apParams->mvPoint;
			*mpIntersectNormal = apParams->mvNormal;
		}

		return true;		
	}

	bool cParticleEmitterData_UserData::CheckCollision(	const cVector3f& avStart, 
																const cVector3f &avEnd,
																iPhysicsWorld *apPhysicsWorld,
																cVector3f *apNormalVec,
																cVector3f *apPosVec)
	{
		cVector3f vNormal;
		mfShortestDist = 99999.0f;
		mbIntersected = false;

		mpIntersectPos = apPosVec;
		mpIntersectNormal = apNormalVec;

		apPhysicsWorld->CastRay(this,avStart,avEnd,true,true,true);

		return mbIntersected;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cParticleEmitter_UserData::cParticleEmitter_UserData(
									tString asName,tMaterialVec* apMaterials,
									cVector3f avSize, cGraphics* apGraphics,cResources *apResources,
									cParticleEmitterData_UserData *apData)
					: iParticleEmitter(	asName,apMaterials,apData->D().mlMaxParticleNum,
											avSize,apGraphics,apResources)
	{
		mpData = apData;

		const OhplH12PsDesc &d = apData->D();
		const OhplPeParams &p = d.mParams;

		mvDrawSize = 1;

		mCoordSystem = (eParticleEmitterCoordSystem)p.mlCoordSystem;
		mbMultiplyRGBWithAlpha = p.mbMultiplyRGBWithAlpha;
		mDrawType = (eParticleEmitterType)p.mlDrawType;
		mbUsesDirection = p.mbUsesDirection;

		mpRust = ohpl_pe_new(&p, d.mlMaxParticleNum, d.mbRespawn, &d.mvSubDiv, (uint32_t)mvMaterials->size());

		mbUsePartSpin = p.mbUsePartSpin;
		mbUseRevolution = p.mbUseRevolution;
		mPEType = (ePEType)p.mlPEType;

		//Calculate max size of particles.
		float fSizeMul = p.mfStartRelSize;
		if(fSizeMul < p.mfMiddleRelSize) fSizeMul = p.mfMiddleRelSize;
		if(fSizeMul < p.mfEndRelSize) fSizeMul = p.mfEndRelSize;
		mvMaxDrawSize = p.mvMaxStartSize * fSizeMul;
	}

	//-----------------------------------------------------------------------

	cParticleEmitter_UserData::~cParticleEmitter_UserData()
	{	

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	static bool CollideThunk(void *apCtx, const cVector3f *apStart, const cVector3f *apEnd,
							 cVector3f *apNormal, cVector3f *apPoint)
	{
		cParticleEmitter_UserData *pPE = static_cast<cParticleEmitter_UserData*>(apCtx);
		return pPE->CheckCollision(*apStart, *apEnd, apNormal, apPoint);
	}

	bool cParticleEmitter_UserData::CheckCollision(const cVector3f &avStart, const cVector3f &avEnd,
												   cVector3f *apNormal, cVector3f *apPoint)
	{
		return mpData->CheckCollision(avStart, avEnd, mpWorld->GetPhysicsWorld(), apNormal, apPoint);
	}

	void cParticleEmitter_UserData::UpdateMotion(float afTimeStep)
	{
		const OhplPeParams &p = mpData->D().mParams;
		const cMatrixf &mtxStart = p.mlCoordSystem == eParticleEmitterCoordSystem_World ?
									GetWorldMatrix() : GetLocalMatrix();
		ohpl_pe_update_motion(mpRust, afTimeStep, &mtxStart, p.mbCollides ? CollideThunk : NULL, this);
	}

	//-----------------------------------------------------------------------
}
