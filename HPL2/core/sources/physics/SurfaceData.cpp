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

#include "physics/SurfaceData.h"

#include "physics/Physics.h"
#include "physics/RustPhysics.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"

#include "scene/ParticleSystem.h"

#include "scene/World.h"
#include "scene/SoundEntity.h"
#include "sound/Sound.h"
#include "sound/SoundChannel.h"
#include "sound/SoundHandler.h"

#include "resources/Resources.h"
#include "resources/SoundEntityManager.h"
#include "resources/ParticleManager.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSurfaceData::cSurfaceData(const tString &asName, cPhysics *apPhysics,cResources *apResources)
	{
		msName = asName;
		mpPhysics = apPhysics;
		mpResources = apResources;

		//Setup default properties
		mFrictionMode = ePhysicsMaterialCombMode_Average;
		mElasticityMode  = ePhysicsMaterialCombMode_Average;

		mfElasticity = 0.5f;
		mfStaticFriction = 0.3f;
		mfKineticFriction = 0.3f;

		mlPriority = 0;
		
		mfMinScrapeSpeed = 0.6f;
		mfMinScrapeFreq = 0.7f;
		mfMinScrapeFreqSpeed = 1; 
		mfMaxScrapeFreq = 2;
		mfMaxScrapeFreqSpeed = 3;
		mfMiddleScrapeSpeed = 2;
		msScrapeSoundName = "";

		mpHapticSurface = NULL;
	}

	//-----------------------------------------------------------------------

	cSurfaceData::~cSurfaceData()
	{
		STLDeleteAll(mvImpactData);
		STLDeleteAll(mvHitData);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cSurfaceData::RustParams(OhplSurfaceParams &aParams) const
	{
		aParams.self = reinterpret_cast<size_t>(this);
		aParams.physics = reinterpret_cast<size_t>(mpPhysics);
		aParams.minScrapeSpeed = mfMinScrapeSpeed;
		aParams.minScrapeFreq = mfMinScrapeFreq;
		aParams.minScrapeFreqSpeed = mfMinScrapeFreqSpeed;
		aParams.maxScrapeFreq = mfMaxScrapeFreq;
		aParams.maxScrapeFreqSpeed = mfMaxScrapeFreqSpeed;
		aParams.middleScrapeSpeed = mfMiddleScrapeSpeed;
		aParams.minScrapeContacts = mlMinScrapeContacts;
		aParams.minRollSpeed = mfMinRollSpeed;
		aParams.minRollFreq = mfMinRollFreq;
		aParams.minRollVolume = mfMinRollVolume;
		aParams.minRollFreqSpeed = mfMinRollFreqSpeed;
		aParams.maxRollFreq = mfMaxRollFreq;
		aParams.maxRollVolume = mfMaxRollVolume;
		aParams.maxRollFreqSpeed = mfMaxRollFreqSpeed;
		aParams.middleRollSpeed = mfMiddleRollSpeed;
		aParams.rollAxisFlags = mRollAxisFlags;
		aParams.scrapeSound = OhplS(msScrapeSoundName);
		aParams.rollSound = OhplS(msRollSoundName);
	}

	void cSurfaceData::OnImpact(float afSpeed,const cVector3f &avPos,int alContacts, iPhysicsBody *apBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_on_impact(OhplPhysHost(), &params, afSpeed, &avPos, reinterpret_cast<size_t>(apBody));
	}
	
	//-----------------------------------------------------------------------

	void cSurfaceData::OnSlide(float afSpeed,const cVector3f &avPos,int alContacts, iPhysicsBody *apBody,
								iPhysicsBody *apSlideAgainstBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_on_slide(OhplPhysHost(), &params, afSpeed, &avPos, alContacts,
									reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apSlideAgainstBody));
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::CreateImpactEffect(	float afSpeed,const cVector3f &avPos,int alContacts,
											cSurfaceData *apSecondSurface, iPhysicsWorld *apPhysicsWorld)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_create_impact_effect(OhplPhysHost(), &params, afSpeed, &avPos,
												reinterpret_cast<size_t>(apSecondSurface), reinterpret_cast<size_t>(apPhysicsWorld));
	}


	//-----------------------------------------------------------------------

	void cSurfaceData::UpdateRollEffect(iPhysicsBody *apBody)
	{
		OhplSurfaceParams params;
		RustParams(params);
		ohpl_phys_surface_update_roll_effect(OhplPhysHost(), &params, reinterpret_cast<size_t>(apBody));
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetElasticity(float afElasticity)
	{
		mfElasticity = afElasticity;
	}

	float cSurfaceData::GetElasticity() const
	{
		return mfElasticity;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetStaticFriction(float afElasticity)
	{
		mfStaticFriction = afElasticity;
	}

	float cSurfaceData::GetStaticFriction() const
	{
		return mfStaticFriction;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetKineticFriction(float afElasticity)
	{
		mfKineticFriction = afElasticity;
	}

	float cSurfaceData::GetKineticFriction() const
	{
		return mfKineticFriction;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetPriority(int alPriority)
	{
		mlPriority = alPriority;
	}

	int cSurfaceData::GetPriority() const
	{
		return mlPriority;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetFrictionCombMode(ePhysicsMaterialCombMode aMode)
	{
		mFrictionMode = aMode;
	}

	ePhysicsMaterialCombMode cSurfaceData::GetFrictionCombMode() const
	{
		return mFrictionMode;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetElasticityCombMode(ePhysicsMaterialCombMode aMode)
	{
		mElasticityMode = aMode;
	}

	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode cSurfaceData::GetElasticityCombMode() const
	{
		return mElasticityMode;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::PreloadData()
	{
		if(msRollSoundName!="")mpResources->GetSoundEntityManager()->Preload(msRollSoundName);
		if(msScrapeSoundName!="")mpResources->GetSoundEntityManager()->Preload(msScrapeSoundName);
		
		for(size_t i=0; i< mvImpactData.size(); ++i)
		{
			if(mvImpactData[i]->msSoundName!="") 
			{
				mpResources->GetSoundEntityManager()->Preload(mvImpactData[i]->msSoundName);
			}
			if(mvImpactData[i]->msPSName!="")
			{
				mpResources->GetParticleManager()->Preload(mvImpactData[i]->msPSName);
			}
		}
		for(size_t i=0; i< mvHitData.size(); ++i)
		{
			if(mvHitData[i]->msSoundName!="") 
				mpResources->GetSoundEntityManager()->Preload(mvHitData[i]->msSoundName);
			if(mvHitData[i]->msPSName!="")
				mpResources->GetParticleManager()->Preload(mvHitData[i]->msPSName);
		}
	}
	
	//-----------------------------------------------------------------------

	iPhysicsMaterial* cSurfaceData::ToMaterial(iPhysicsWorld *apWorld)
	{
		iPhysicsMaterial *pMat=NULL;

		pMat = apWorld->GetMaterialFromName(msName);
		
		if(pMat==NULL)
		{
			pMat = apWorld->CreateMaterial(msName);
		}
		
		pMat->SetElasticity(mfElasticity);
		pMat->SetKineticFriction(mfKineticFriction);
		pMat->SetStaticFriction(mfStaticFriction);

		pMat->SetElasticityCombMode(mElasticityMode);
		pMat->SetFrictionCombMode(mFrictionMode);

		pMat->SetSurfaceData(this);

		return pMat;
	}

	//-----------------------------------------------------------------------

	cSurfaceImpactData* cSurfaceData::CreateImpactData(float afMinSpeed)
	{
		cSurfaceImpactData *pData = hplNew( cSurfaceImpactData, () );
		pData->mfMinSpeed = afMinSpeed;

		mvImpactData.push_back(pData);

		return pData;
	}

	cSurfaceImpactData* cSurfaceData::GetImpactData(int alIdx)
	{
		return mvImpactData[alIdx];
	}

	int cSurfaceData::GetImpactDataNum()
	{
		return (int)mvImpactData.size();
	}

	cSurfaceImpactData* cSurfaceData::GetImpactDataFromSpeed(float afSpeed)
	{	
		for(size_t i=0; i< mvImpactData.size(); ++i)
		{
			if(afSpeed >= mvImpactData[i]->GetMinSpeed())
			{
				return mvImpactData[i];
			}
		}
		return NULL;
	}

	//-----------------------------------------------------------------------

	cSurfaceImpactData* cSurfaceData::CreateHitData(float afMinSpeed)
	{
		cSurfaceImpactData *pData = hplNew( cSurfaceImpactData, () );
		pData->mfMinSpeed = afMinSpeed;

		mvHitData.push_back(pData);

		return pData;
	}

	cSurfaceImpactData* cSurfaceData::GetHitData(int alIdx)
	{
		return mvHitData[alIdx];
	}

	int cSurfaceData::GetHitDataNum()
	{
		return (int)mvHitData.size();
	}

	cSurfaceImpactData* cSurfaceData::GetHitDataFromSpeed(float afSpeed)
	{	
		for(size_t i=0; i< mvHitData.size(); ++i)
		{
			if(afSpeed >= mvHitData[i]->GetMinSpeed())
			{
				return mvHitData[i];
			}
		}
		return NULL;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
