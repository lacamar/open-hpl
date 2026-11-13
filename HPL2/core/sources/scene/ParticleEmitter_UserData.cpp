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

#include "impl/tinyXML/tinyxml.h"

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
	
	cParticleEmitterData_UserData::cParticleEmitterData_UserData(
		const tString &asName,cResources* apResources,cGraphics *apGraphics)
		: iParticleEmitterData(asName,apResources,apGraphics)
	{
	}

	//-----------------------------------------------------------------------

	iParticleEmitter* cParticleEmitterData_UserData::Create(tString asName, cVector3f avSize)
	{
		iParticleEmitter *pPE = hplNew( cParticleEmitter_UserData, (asName,&mvMaterials,avSize, 
																mpGraphics,mpResources,this) );
		
		cMatrixf mtxOffset = cMath::MatrixRotate(mvAngleOffset,eEulerRotationOrder_XYZ);
		mtxOffset.SetTranslation(mvPosOffset);
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
		OhplH12PsBox *pBox = ohpl_h12_ps_parse(H12Attr, apElement);
		const OhplH12PsDesc &d = *ohpl_h12_ps_desc(pBox);
		const OhplPeParams &p = d.mParams;

		msName = d.msName;
		mPEType = (ePEType)p.mlPEType;
		mlMaxParticleNum = d.mlMaxParticleNum;
		mbRespawn = d.mbRespawn;
		mfParticlesPerSecond = p.mfParticlesPerSecond;
		mfStartTimeOffset = p.mfStartTimeOffset;
		mfWarmUpTime = d.mfWarmUpTime;
		mfWarmUpStepsPerSec = d.mfWarmUpStepsPerSec;
		mfMinPauseLength = p.mfMinPauseLength;
		mfMaxPauseLength = p.mfMaxPauseLength;
		mfMinPauseInterval = p.mfMinPauseInterval;
		mfMaxPauseInterval = p.mfMaxPauseInterval;
		mvPosOffset = d.mvPosOffset;
		mvAngleOffset = d.mvAngleOffset;

		mvSubDiv = d.mvSubDiv;
		mSubDivType = (ePESubDivType)p.mlSubDivType;

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

		if(d.mbHasSubDiv==false && mvMaterials.empty()==false)
			mvSubDiv = mvMaterials[0]->GetUVSubDivisions();

		mfFrameStep = ohpl_h12_ps_frame_step(d.mfAnimationLength, (uint32_t)mvMaterials.size());
		mfMaxFrameTime = ohpl_h12_ps_max_frame_time((uint32_t)mvMaterials.size());

		mStartPosType = (ePEStartPosType)p.mlStartPosType;
		mvMinStartPos = p.mvMinStartPos;
		mvMaxStartPos = p.mvMaxStartPos;
		mvMinStartAngles = p.mvMinStartAngles;
		mvMaxStartAngles = p.mvMaxStartAngles;
		mfMinStartRadius = p.mfMinStartRadius;
		mfMaxStartRadius = p.mfMaxStartRadius;

		mStartVelType = (ePEStartPosType)p.mlStartVelType;
		mvMinStartVel = p.mvMinStartVel;
		mvMaxStartVel = p.mvMaxStartVel;
		mvMinStartVelAngles = p.mvMinStartVelAngles;
		mvMaxStartVelAngles = p.mvMaxStartVelAngles;
		mfMinStartVelSpeed = p.mfMinStartVelSpeed;
		mfMaxStartVelSpeed = p.mfMaxStartVelSpeed;
		mfMinSpeedMultiply = p.mfMinSpeedMultiply;
		mfMaxSpeedMultiply = p.mfMaxSpeedMultiply;
		mvMinStartAcc = p.mvMinStartAcc;
		mvMaxStartAcc = p.mvMaxStartAcc;
		mfMinVelMaximum = p.mfMinVelMaximum;
		mfMaxVelMaximum = p.mfMaxVelMaximum;
		mbUsesDirection = p.mbUsesDirection;
		mGravityType = (ePEGravityType)p.mlGravityType;
		mvGravityAcc = p.mvGravityAcc;
		mCoordSystem = (eParticleEmitterCoordSystem)p.mlCoordSystem;

		mbUsePartSpin = p.mbUsePartSpin;
		mPartSpinType = (ePEPartSpinType)p.mlPartSpinType;
		mfMinSpinRange = p.mfMinSpinRange;
		mfMaxSpinRange = p.mfMaxSpinRange;
		mbUseRevolution = p.mbUseRevolution;
		mvMinRevVel = p.mvMinRevVel;
		mvMaxRevVel = p.mvMaxRevVel;

		mfMinLifeSpan = p.mfMinLifeSpan;
		mfMaxLifeSpan = p.mfMaxLifeSpan;
		mDeathType = ePEDeathType_Age;
		msDeathPS = d.msDeathPS;

		mDrawType = (eParticleEmitterType)p.mlDrawType;
		mvMinStartSize = p.mvMinStartSize;
		mvMaxStartSize = p.mvMaxStartSize;
		mfStartRelSize = p.mfStartRelSize;
		mfMiddleRelSize = p.mfMiddleRelSize;
		mfMiddleRelSizeTime = p.mfMiddleRelSizeTime;
		mfMiddleRelSizeLength = p.mfMiddleRelSizeLength;
		mfEndRelSize = p.mfEndRelSize;
		mbMultiplyRGBWithAlpha = p.mbMultiplyRGBWithAlpha;

		mMinStartColor = p.mMinStartColor;
		mMaxStartColor = p.mMaxStartColor;
		mStartRelColor = p.mStartRelColor;
		mMiddleRelColor = p.mMiddleRelColor;
		mfMiddleRelColorTime = p.mfMiddleRelColorTime;
		mfMiddleRelColorLength = p.mfMiddleRelColorLength;
		mEndRelColor = p.mEndRelColor;

		mbCollides = p.mbCollides;
		mfMinBounceAmount = p.mfMinBounceAmount;
		mfMaxBounceAmount = p.mfMaxBounceAmount;
		mlMinCollisionMax = p.mlMinCollisionMax;
		mlMaxCollisionMax = p.mlMaxCollisionMax;
		mlCollisionUpdateRate = p.mlCollisionUpdateRate;

		mbUseBeamNoise = d.mbUseBeamNoise;
		mlLowFreqPoints = d.mlLowFreqPoints;
		mlHighFreqPoints = d.mlHighFreqPoints;
		mvMinLowFreqNoise = d.mvMinLowFreqNoise;
		mvMaxLowFreqNoise = d.mvMaxLowFreqNoise;
		mvMinHighFreqNoise = d.mvMinHighFreqNoise;
		mvMaxHighFreqNoise = d.mvMaxHighFreqNoise;

		ohpl_h12_ps_free(pBox);
		BuildRustParams();
	}

	void cParticleEmitterData_UserData::BuildRustParams()
	{
		OhplPeParams &p = mRustParams;
		p.mlPEType = mPEType;
		p.mlDrawType = mDrawType;
		p.mlCoordSystem = mCoordSystem;
		p.mlSubDivType = mSubDivType;
		p.mlStartPosType = mStartPosType;
		p.mlStartVelType = mStartVelType;
		p.mlGravityType = mGravityType;
		p.mlPartSpinType = mPartSpinType;
		p.mlMinCollisionMax = mlMinCollisionMax;
		p.mlMaxCollisionMax = mlMaxCollisionMax;
		p.mlCollisionUpdateRate = mlCollisionUpdateRate;
		p.mbUsesDirection = mbUsesDirection;
		p.mbUsePartSpin = mbUsePartSpin;
		p.mbUseRevolution = mbUseRevolution;
		p.mbMultiplyRGBWithAlpha = mbMultiplyRGBWithAlpha;
		p.mbCollides = mbCollides;
		p.mfParticlesPerSecond = mfParticlesPerSecond;
		p.mfStartTimeOffset = mfStartTimeOffset;
		p.mfMinPauseLength = mfMinPauseLength;
		p.mfMaxPauseLength = mfMaxPauseLength;
		p.mfMinPauseInterval = mfMinPauseInterval;
		p.mfMaxPauseInterval = mfMaxPauseInterval;
		p.mvMinStartPos = mvMinStartPos;
		p.mvMaxStartPos = mvMaxStartPos;
		p.mvMinStartAngles = mvMinStartAngles;
		p.mvMaxStartAngles = mvMaxStartAngles;
		p.mfMinStartRadius = mfMinStartRadius;
		p.mfMaxStartRadius = mfMaxStartRadius;
		p.mvMinStartVel = mvMinStartVel;
		p.mvMaxStartVel = mvMaxStartVel;
		p.mvMinStartVelAngles = mvMinStartVelAngles;
		p.mvMaxStartVelAngles = mvMaxStartVelAngles;
		p.mfMinStartVelSpeed = mfMinStartVelSpeed;
		p.mfMaxStartVelSpeed = mfMaxStartVelSpeed;
		p.mfMinSpeedMultiply = mfMinSpeedMultiply;
		p.mfMaxSpeedMultiply = mfMaxSpeedMultiply;
		p.mvMinStartAcc = mvMinStartAcc;
		p.mvMaxStartAcc = mvMaxStartAcc;
		p.mfMinVelMaximum = mfMinVelMaximum;
		p.mfMaxVelMaximum = mfMaxVelMaximum;
		p.mvGravityAcc = mvGravityAcc;
		p.mfMinSpinRange = mfMinSpinRange;
		p.mfMaxSpinRange = mfMaxSpinRange;
		p.mvMinRevVel = mvMinRevVel;
		p.mvMaxRevVel = mvMaxRevVel;
		p.mfMinLifeSpan = mfMinLifeSpan;
		p.mfMaxLifeSpan = mfMaxLifeSpan;
		p.mvMinStartSize = mvMinStartSize;
		p.mvMaxStartSize = mvMaxStartSize;
		p.mfStartRelSize = mfStartRelSize;
		p.mfMiddleRelSize = mfMiddleRelSize;
		p.mfMiddleRelSizeTime = mfMiddleRelSizeTime;
		p.mfMiddleRelSizeLength = mfMiddleRelSizeLength;
		p.mfEndRelSize = mfEndRelSize;
		p.mMinStartColor = mMinStartColor;
		p.mMaxStartColor = mMaxStartColor;
		p.mStartRelColor = mStartRelColor;
		p.mMiddleRelColor = mMiddleRelColor;
		p.mfMiddleRelColorTime = mfMiddleRelColorTime;
		p.mfMiddleRelColorLength = mfMiddleRelColorLength;
		p.mEndRelColor = mEndRelColor;
		p.mfMinBounceAmount = mfMinBounceAmount;
		p.mfMaxBounceAmount = mfMaxBounceAmount;
		p.mfFrameStep = mfFrameStep;
		p.mfMaxFrameTime = mfMaxFrameTime;
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
					: iParticleEmitter(	asName,apMaterials,apData->mlMaxParticleNum,
											avSize,apGraphics,apResources)
	{
		mpData = apData;

		mvDrawSize = 1;

		mCoordSystem = apData->mCoordSystem;
		
		mbMultiplyRGBWithAlpha =  apData->mbMultiplyRGBWithAlpha;

		mDrawType = apData->mDrawType;

		mbUsesDirection = apData->mbUsesDirection;

		mpRust = ohpl_pe_new(&mpData->mRustParams, mpData->mlMaxParticleNum, apData->mbRespawn,
							&mpData->mvSubDiv, (uint32_t)mvMaterials->size());

		// NEW
		mbUsePartSpin = apData->mbUsePartSpin;
		mbUseRevolution = apData->mbUseRevolution;
		
		mPEType = apData->mPEType;


		//Calculate max size of particles.
		float fSizeMul = apData->mfStartRelSize;
		if(fSizeMul < apData->mfMiddleRelSize) fSizeMul = apData->mfMiddleRelSize;
		if(fSizeMul < apData->mfEndRelSize) fSizeMul = apData->mfEndRelSize;
		mvMaxDrawSize = apData->mvMaxStartSize * fSizeMul;



/*		int i = 0;										
		int j = 0;
		int k = 0;
		switch ( mPEType )
		{
			case ePEType_Beam:
				hplDelete(mpVtxBuffer);								// Destroy the current Vertex Buffer, as it's filled with quads we don't need now :)

                if ( apData->mbUseBeamNoise )
				{
                    // Calculate the Frequency Points Lookup Table, this is needed to create the Vertex Buffer.It is also needed by every beam particle created,
					// as it needs to know the type and index of each when updating.
					while ( (i < apData->mlLowFreqPoints)  && (j < apData->mlHighFreqPoints) )
					{
						float fLowRelPos = (float) i / (apData->mlLowFreqPoints -1 );
						float fHighRelPos = (float) j / (apData->mlHighFreqPoints -1 );
						float fLastLowRelPos = 0.0f;	

						tBeamNoisePoint tempBNP;
					
			//			tempBNP.lLowFreqNoiseIdx = i;
			//			tempBNP.lHighFreqNoiseIdx = j;
						tempBNP.fRelToBendPos = (fHighRelPos-fLastLowRelPos)/(fLowRelPos-fLastLowRelPos);
							
						if ( fLowRelPos <= fHighRelPos )
						{
							if ( fLowRelPos == fHighRelPos )
							{
								tempBNP.noiseType = ePENoiseType_Both;
								++i;
								++j;
							}
							else
							{
								tempBNP.noiseType = ePENoiseType_LowFreq;
								++i;
							}
							
							tempBNP.fRelToBeamPos = fLowRelPos;
							apData->mvLFIndices.push_back (k);
							
						}
						else
						{
							tempBNP.fRelToBeamPos = fHighRelPos;
							tempBNP.noiseType = ePENoiseType_HighFreq;
							++j;
						}		

						fLastLowRelPos = fLowRelPos;


						mpData->mvBeamNoisePoints.push_back(tempBNP);
						++k;
					}
				}
				else
				{
                    tBeamNoisePoint tempBNP;

					tempBNP.fRelToBeamPos = 0.0f;
					tempBNP.fRelToBendPos = 0.0f;
					tempBNP.noiseType = ePENoiseType_None;

					mpData->mvBeamNoisePoints.push_back(tempBNP);

					tempBNP.fRelToBeamPos = 1.0f;
					tempBNP.fRelToBendPos = 1.0f;
					tempBNP.noiseType = ePENoiseType_None;

					mpData->mvBeamNoisePoints.push_back(tempBNP);
				}

				mpVtxBuffer = apGraphics->GetLowLevel()->CreateVertexBuffer ( eVertexElementFlag_Position,		// Now that we know how many points we have, we create the Vertex Buffer
						eVertexBufferDrawType_Tri, eVertexBufferUsageType_Stream,
						mlMaxParticles*mpData->mvBeamNoisePoints.size()*2, mlMaxParticles*(mpData->mvBeamNoisePoints.size()-1)*6);

				for ( int i = 0; i < (int) mlMaxParticles; ++i )
				{
					for ( int j = 0; j < (int) mpData->mvBeamNoisePoints.size()-1; ++j )
					{
						int lStart = j*2;
						mpVtxBuffer->AddIndex(lStart);		// Half quad ( tri )
						mpVtxBuffer->AddIndex(lStart+1);
						mpVtxBuffer->AddIndex(lStart+3);
							
						mpVtxBuffer->AddIndex(lStart+3);	// The other half
						mpVtxBuffer->AddIndex(lStart+2);
						mpVtxBuffer->AddIndex(lStart);
					}

				}

				for ( int i = 0; i < (int) mlMaxParticles; ++i )
				{
					for (int j = 0; j < (int) mpData->mvBeamNoisePoints.size(); ++j )
					{
						mpVtxBuffer->AddVertex(eVertexElementFlag_Texture0, cVector2f(mpData->mvBeamNoisePoints[j].fRelToBeamPos,1));
						mpVtxBuffer->AddVertex(eVertexElementFlag_Texture0, cVector2f(mpData->mvBeamNoisePoints[j].fRelToBeamPos,0));
					}
				}		

				for ( int i = 0; i < (int) mlMaxParticles; ++i )
				{
					for (int j = 0; j < (int) mpData->mvBeamNoisePoints.size()*2; ++j )
					{
                        mpVtxBuffer->AddVertex(eVertexElementFlag_Position, 0);
						mpVtxBuffer->AddColor(eVertexElementFlag_Color0, cColor(1,1));
					}
				}

				mpVtxBuffer->Compile(0);

				
				
				
				
				break;
			case ePEType_Normal:
				break;
			default:
				break;
			}
*/		
		// ---

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
		const cMatrixf &mtxStart = mpData->mCoordSystem == eParticleEmitterCoordSystem_World ?
									GetWorldMatrix() : GetLocalMatrix();
		ohpl_pe_update_motion(mpRust, afTimeStep, &mtxStart, mpData->mbCollides ? CollideThunk : NULL, this);
	}

	//-----------------------------------------------------------------------
}
