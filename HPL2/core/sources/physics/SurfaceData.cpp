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
#include "physics/RustPhysBook.h"
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

	size_t cSurfaceData::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		cSurfaceData *pS = reinterpret_cast<cSurfaceData*>(a->p[0]);
		tString sArg;
		if(alOp == 206 || alOp == 207 || alOp == 210 || alOp == 211) sArg = tString(reinterpret_cast<const char*>(a->p[alOp >= 210 ? 2 : 1]), a->p[alOp >= 210 ? 3 : 2]);
		cSurfaceImpactData *pI = reinterpret_cast<cSurfaceImpactData*>(a->p[1]);
		const tString *ps = NULL;
		switch(alOp)
		{
		case 201: hplDelete(pI); return 0;
		case 202: { cSurfaceImpactData *pNew = hplNew( cSurfaceImpactData, () ); pNew->mfMinSpeed = a->f[0]; return reinterpret_cast<size_t>(pNew); }
		case 203: pI->msSoundName = tString(reinterpret_cast<const char*>(a->p[2]), a->p[3]); return 0;
		case 204: pI->msPSName = tString(reinterpret_cast<const char*>(a->p[2]), a->p[3]); return 0;
		case 205: pI->mlPSPrio = a->i[0]; return 0;
		case 206: pS->mpResources->GetSoundEntityManager()->Preload(sArg); return 0;
		case 207: pS->mpResources->GetParticleManager()->Preload(sArg); return 0;
		case 208: ps = a->i[0] == 0 ? &pS->msUserVar : a->i[0] == 1 ? &pS->msStepType : a->i[0] == 2 ? &pS->msScrapeSoundName : &pS->msRollSoundName; break;
		case 209: ps = a->i[0] == 0 ? &pI->msSoundName : &pI->msPSName; break;
		case 210: return reinterpret_cast<size_t>(reinterpret_cast<iPhysicsWorld*>(a->p[1])->GetMaterialFromName(sArg));
		case 211: return reinterpret_cast<size_t>(reinterpret_cast<iPhysicsWorld*>(a->p[1])->CreateMaterial(sArg));
		case 212:
		{
			iPhysicsMaterial *pM = reinterpret_cast<iPhysicsMaterial*>(a->p[1]);
			if(a->i[0] == 0) pM->SetElasticity(a->f[0]); else if(a->i[0] == 1) pM->SetKineticFriction(a->f[0]); else pM->SetStaticFriction(a->f[0]);
			return 0;
		}
		case 213:
		{
			iPhysicsMaterial *pM = reinterpret_cast<iPhysicsMaterial*>(a->p[1]);
			ePhysicsMaterialCombMode mode = (ePhysicsMaterialCombMode)(a->i[0] >> 8);
			if((a->i[0] & 0xff) == 0) pM->SetElasticityCombMode(mode); else pM->SetFrictionCombMode(mode);
			return 0;
		}
		case 214: reinterpret_cast<iPhysicsMaterial*>(a->p[1])->SetSurfaceData(pS); return 0;
		}
		if(ps){ a->s = ps->c_str(); a->n = ps->size(); }
		return 0;
	}

	cSurfaceData::cSurfaceData(const tString &asName, cPhysics *apPhysics,cResources *apResources)
	{
		msName = asName;
		mpPhysics = apPhysics;
		mpResources = apResources;

		if(OhplBackendRustRequested()){ ohpl_h15s_new(HostOp, reinterpret_cast<size_t>(this), &mS); return; }

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
	}

	//-----------------------------------------------------------------------

	cSurfaceData::~cSurfaceData()
	{
		if(mS.rs){ ohpl_h15s_free(mS.rs); return; }

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
		if(mS.rs)
		{
			ohpl_h15s_params(&mS, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(mpPhysics), OhplS(msScrapeSoundName), OhplS(msRollSoundName), &aParams);
			return;
		}
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
		if(mS.rs){ ohpl_h15s_set_f32(&mS, 0, afElasticity); return; }
		mfElasticity = afElasticity;
	}

	float cSurfaceData::GetElasticity() const
	{
		if(mS.rs) return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 0);
		return mfElasticity;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetStaticFriction(float afElasticity)
	{
		if(mS.rs){ ohpl_h15s_set_f32(&mS, 1, afElasticity); return; }
		mfStaticFriction = afElasticity;
	}

	float cSurfaceData::GetStaticFriction() const
	{
		if(mS.rs) return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 1);
		return mfStaticFriction;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetKineticFriction(float afElasticity)
	{
		if(mS.rs){ ohpl_h15s_set_f32(&mS, 2, afElasticity); return; }
		mfKineticFriction = afElasticity;
	}

	float cSurfaceData::GetKineticFriction() const
	{
		if(mS.rs) return ohpl_h15s_get_f32(const_cast<OhplSurfaceState*>(&mS), 2);
		return mfKineticFriction;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetPriority(int alPriority)
	{
		if(mS.rs){ ohpl_h15s_set_i32(&mS, 0, alPriority); return; }
		mlPriority = alPriority;
	}

	int cSurfaceData::GetPriority() const
	{
		if(mS.rs) return ohpl_h15s_get_i32(&mS, 0);
		return mlPriority;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetFrictionCombMode(ePhysicsMaterialCombMode aMode)
	{
		if(mS.rs){ ohpl_h15s_set_i32(&mS, 1, aMode); return; }
		mFrictionMode = aMode;
	}

	ePhysicsMaterialCombMode cSurfaceData::GetFrictionCombMode() const
	{
		if(mS.rs) return (ePhysicsMaterialCombMode)ohpl_h15s_get_i32(&mS, 1);
		return mFrictionMode;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::SetElasticityCombMode(ePhysicsMaterialCombMode aMode)
	{
		if(mS.rs){ ohpl_h15s_set_i32(&mS, 2, aMode); return; }
		mElasticityMode = aMode;
	}

	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode cSurfaceData::GetElasticityCombMode() const
	{
		if(mS.rs) return (ePhysicsMaterialCombMode)ohpl_h15s_get_i32(&mS, 2);
		return mElasticityMode;
	}

	//-----------------------------------------------------------------------

	void cSurfaceData::PreloadData()
	{
		if(mS.rs){ ohpl_h15s_preload(mS.rs); return; }

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
		if(mS.rs) return reinterpret_cast<iPhysicsMaterial*>(ohpl_h15s_to_material(mS.rs, &mS, msName.c_str(), msName.size(), reinterpret_cast<size_t>(apWorld)));

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
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_create_impact(mS.rs, afMinSpeed, false));
		cSurfaceImpactData *pData = hplNew( cSurfaceImpactData, () );
		pData->mfMinSpeed = afMinSpeed;

		mvImpactData.push_back(pData);

		return pData;
	}

	cSurfaceImpactData* cSurfaceData::GetImpactData(int alIdx)
	{
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_at(mS.rs, false, alIdx));
		return mvImpactData[alIdx];
	}

	int cSurfaceData::GetImpactDataNum()
	{
		if(mS.rs) return ohpl_h15s_impact_num(mS.rs, false);
		return (int)mvImpactData.size();
	}

	cSurfaceImpactData* cSurfaceData::GetImpactDataFromSpeed(float afSpeed)
	{
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_from_speed(mS.rs, false, afSpeed));
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
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_create_impact(mS.rs, afMinSpeed, true));
		cSurfaceImpactData *pData = hplNew( cSurfaceImpactData, () );
		pData->mfMinSpeed = afMinSpeed;

		mvHitData.push_back(pData);

		return pData;
	}

	cSurfaceImpactData* cSurfaceData::GetHitData(int alIdx)
	{
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_at(mS.rs, true, alIdx));
		return mvHitData[alIdx];
	}

	int cSurfaceData::GetHitDataNum()
	{
		if(mS.rs) return ohpl_h15s_impact_num(mS.rs, true);
		return (int)mvHitData.size();
	}

	cSurfaceImpactData* cSurfaceData::GetHitDataFromSpeed(float afSpeed)
	{
		if(mS.rs) return reinterpret_cast<cSurfaceImpactData*>(ohpl_h15s_impact_from_speed(mS.rs, true, afSpeed));
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
