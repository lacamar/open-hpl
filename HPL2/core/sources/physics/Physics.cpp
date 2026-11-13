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

#include "physics/Physics.h"
#include "physics/LowLevelPhysics.h"

#include "physics/PhysicsWorld.h"
#include "physics/SurfaceData.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "physics/RustPhysics.h"
#include "physics/RustPhysBook.h"
#include "physics/PhysicsWorld.h"

#include "impl/tinyXML/tinyxml.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	size_t cPhysics::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		cPhysics *pPhysics = reinterpret_cast<cPhysics*>(a->p[0]);
		cSurfaceData *pSurface = reinterpret_cast<cSurfaceData*>(a->p[1]);
		tString sArg;
		if(alOp == 103 || alOp == 106) sArg = tString(reinterpret_cast<const char*>(a->p[1]), a->p[2]);
		switch(alOp)
		{
		case 101: return reinterpret_cast<size_t>(pPhysics->mpLowLevelPhysics->CreateWorld());
		case 102: hplDelete(reinterpret_cast<iPhysicsWorld*>(a->p[1])); return 0;
		case 103: return reinterpret_cast<size_t>(hplNew( cSurfaceData, (sArg, pPhysics, pPhysics->mpResources) ));
		case 104: hplDelete(pSurface); return 0;
		case 105: reinterpret_cast<cSurfaceData*>(a->p[0])->ToMaterial(reinterpret_cast<iPhysicsWorld*>(a->p[1])); return 0;
		case 106: Error("Couldn't load XML file '%s'!\n", sArg.c_str()); return 0;
		case 107: return reinterpret_cast<size_t>(pSurface->RustState());
		case 108:
		{
			tString sVal(reinterpret_cast<const char*>(a->p[2]), a->p[3]);
			if(a->i[0] == 0) pSurface->SetUserVar(sVal);
			else if(a->i[0] == 1) pSurface->GetStepType(sVal);
			else if(a->i[0] == 2) pSurface->SetScrapeSoundName(sVal);
			else pSurface->SetRollSoundName(sVal);
			return 0;
		}
		}
		return 0;
	}

	cPhysics::cPhysics(iLowLevelPhysics *apLowLevelPhysics)  : iUpdateable("HPL_Physics")
	{
		mpLowLevelPhysics = apLowLevelPhysics;

		mRs = OhplBackendRustRequested() ? ohpl_h15p_new(HostOp, reinterpret_cast<size_t>(this), &mS) : 0;
		if(mRs){ mpImpactCounts = NULL; return; }

		mlMaxImpacts = 6;
		mfImpactDuration = 0.4f;

		mbLog = false;
		mpImpactCounts = ohpl_phys_impacts_new();
	}

	//-----------------------------------------------------------------------

	cPhysics::~cPhysics()
	{
		Log("Exiting Physics Module\n");
		Log("--------------------------------------------------------\n");

		if(mRs)
		{
			ohpl_h15p_free(mRs);
			Log("--------------------------------------------------------\n\n");
			return;
		}
		STLDeleteAll(mlstWorlds);
		ohpl_phys_impacts_free(mpImpactCounts);
		STLMapDeleteAll(m_mapSurfaceData);

		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cPhysics::Init(cResources *apResources)
	{
		mpResources = apResources;
	}

	//-----------------------------------------------------------------------

	void cPhysics::Update(float afTimeStep)
	{
		if(mRs){ ohpl_h15p_update_impacts(mRs, afTimeStep, mfImpactDuration); return; }
		UpdateImpactCounts(afTimeStep);
	}

	//-----------------------------------------------------------------------

	iPhysicsWorld* cPhysics::CreateWorld(bool abAddSurfaceData)
	{
		if(mRs) return reinterpret_cast<iPhysicsWorld*>(ohpl_h15p_create_world(mRs, abAddSurfaceData));
		iPhysicsWorld * pWorld = mpLowLevelPhysics->CreateWorld();
		mlstWorlds.push_back(pWorld);

		if(abAddSurfaceData)
		{
			tSurfaceDataMapIt it = m_mapSurfaceData.begin();
			for(; it != m_mapSurfaceData.end(); ++it)
			{
				cSurfaceData *apData = it->second;
				apData->ToMaterial(pWorld);
			}
		}

		return pWorld;
	}

	//-----------------------------------------------------------------------

	void cPhysics::DestroyWorld(iPhysicsWorld* apWorld)
	{
		if(mRs){ ohpl_h15p_destroy_world(mRs, reinterpret_cast<size_t>(apWorld)); return; }
		STLFindAndDelete(mlstWorlds, apWorld);
	}

	//-----------------------------------------------------------------------

	bool cPhysics::CanPlayImpact()
	{
		if(mRs) return ohpl_h15p_can_play(mRs, mlMaxImpacts);
		return ohpl_phys_impacts_can_play(mpImpactCounts, mlMaxImpacts);
	}

	void cPhysics::AddImpact()
	{
		if(mRs){ ohpl_h15p_add_impact(mRs); return; }
		ohpl_phys_impacts_add(mpImpactCounts);
	}

	int cPhysics::GetNumOfImpacts()
	{
		if(mRs) return (int)ohpl_h15p_impact_count(mRs);
		return (int)ohpl_phys_impacts_len(mpImpactCounts);
	}

	//-----------------------------------------------------------------------

	cSurfaceDataIterator cPhysics::GetSurfaceDataIterator()
	{
		if(mRs)
		{
			m_mapSurfaceData.clear();
			for(size_t i=0, n=ohpl_h15p_surface_len(mRs); i<n; ++i)
			{
				OhplStr name; size_t obj;
				if(ohpl_h15p_surface_at(mRs, i, &name, &obj)) m_mapSurfaceData[tString(name.p, name.n)] = reinterpret_cast<cSurfaceData*>(obj);
			}
		}
		return cSurfaceDataIterator(&m_mapSurfaceData);
	}

	cSurfaceData *cPhysics::CreateSurfaceData(const tString& asName)
	{
		if(mRs) return reinterpret_cast<cSurfaceData*>(ohpl_h15p_create_surface(mRs, asName.c_str(), asName.size()));
		cSurfaceData *pData = hplNew( cSurfaceData, (asName, this,mpResources) );

		m_mapSurfaceData.insert(tSurfaceDataMap::value_type(asName, pData));

		return pData;
	}

	cSurfaceData *cPhysics::GetSurfaceData(const tString& asName)
	{
		if(mRs) return reinterpret_cast<cSurfaceData*>(ohpl_h15p_get_surface(mRs, asName.c_str(), asName.size()));
		tSurfaceDataMapIt it = m_mapSurfaceData.find(asName);
		if(it == m_mapSurfaceData.end()) return NULL;

		return it->second;
	}

	//-----------------------------------------------------------------------

	bool cPhysics::LoadSurfaceData(const tString& asFile)
	{
		if(mRs) return ohpl_h15p_load_surface(mRs, asFile.c_str(), asFile.size());

		std::vector<char> vData;
		bool bReadOk = false;
		FILE *pFile = fopen(asFile.c_str(), "rb");
		if(pFile == NULL)
		{
			Error("Couldn't load XML file '%s'!\n",asFile.c_str());
			return false;
		}
		fseek(pFile, 0, SEEK_END);
		long lLength = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		if(lLength <= 0)
		{
			bReadOk = true;
		}
		else
		{
			vData.resize(lLength);
			bReadOk = fread(vData.data(), lLength, 1, pFile) == 1;
		}
		fclose(pFile);

		bool bLoaded = ohpl_phys_surface_parse(vData.data(), vData.size(), bReadOk,
			[](void *apContext, const OhplSurfaceDef *apDef)
			{
				cPhysics *pPhysics = static_cast<cPhysics*>(apContext);
				cSurfaceData *pData = pPhysics->CreateSurfaceData(tString(apDef->name.p, apDef->name.n));

				pData->SetElasticity(apDef->elasticity);
				pData->SetKineticFriction(apDef->kineticFriction);
				pData->SetStaticFriction(apDef->staticFriction);
				pData->SetPriority(apDef->priority);
				pData->SetUserVar(tString(apDef->userVar.p, apDef->userVar.n));
				pData->SetElasticityCombMode((ePhysicsMaterialCombMode)apDef->elasticityMode);
				pData->SetFrictionCombMode((ePhysicsMaterialCombMode)apDef->frictionMode);
				pData->GetStepType(tString(apDef->stepType.p, apDef->stepType.n));

				pData->SetMinScrapeSpeed(apDef->minScrapeSpeed);
				pData->SetMinScrapeFreq(apDef->minScrapeFreq);
				pData->SetMinScrapeFreqSpeed(apDef->minScrapeFreqSpeed);
				pData->SetMiddleScrapeSpeed(apDef->middleScrapeSpeed);
				pData->SetMaxScrapeFreqSpeed(apDef->maxScrapeFreqSpeed);
				pData->SetMaxScrapeFreq(apDef->maxScrapeFreq);
				pData->SetMinScrapeContacts(apDef->minScrapeContacts);
				pData->SetScrapeSoundName(tString(apDef->scrapeSound.p, apDef->scrapeSound.n));

				pData->SetMinRollSpeed(apDef->minRollSpeed);
				pData->SetMinRollFreq(apDef->minRollFreq);
				pData->SetMinRollVolume(apDef->minRollVolume);
				pData->SetMinRollFreqSpeed(apDef->minRollFreqSpeed);
				pData->SetMiddleRollSpeed(apDef->middleRollSpeed);
				pData->SetMaxRollFreqSpeed(apDef->maxRollFreqSpeed);
				pData->SetMaxRollFreq(apDef->maxRollFreq);
				pData->SetMaxRollVolume(apDef->maxRollVolume);
				pData->SetRollSoundName(tString(apDef->rollSound.p, apDef->rollSound.n));
				pData->SetRollAxisFlags(apDef->rollAxisFlags);

				for(size_t i=0; i<apDef->nImpacts; ++i)
				{
					const OhplImpactDef &def = apDef->impacts[i];
					cSurfaceImpactData *pImpactData = pData->CreateImpactData(def.minSpeed);
					pImpactData->SetSoundName(tString(def.sound.p, def.sound.n));
					pImpactData->SetPSName(tString(def.ps.p, def.ps.n));
					pImpactData->SetPSPrio(def.psPrio);
				}
				for(size_t i=0; i<apDef->nHits; ++i)
				{
					const OhplImpactDef &def = apDef->hits[i];
					cSurfaceImpactData *pHitData = pData->CreateHitData(def.minSpeed);
					pHitData->SetSoundName(tString(def.sound.p, def.sound.n));
					pHitData->SetPSName(tString(def.ps.p, def.ps.n));
					pHitData->SetPSPrio(def.psPrio);
				}
			}, this);

		if(bLoaded == false)
		{
			Error("Couldn't load XML file '%s'!\n",asFile.c_str());
			return false;
		}
		return true;
	}
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode cPhysics::GetCombMode(const char *apName)
	{
		if(apName == NULL) return ePhysicsMaterialCombMode_Average;

		tString sMode = cString::ToLowerCase(apName);

		if(sMode == "average") return ePhysicsMaterialCombMode_Average;
		if(sMode == "min") return ePhysicsMaterialCombMode_Min;
		if(sMode == "max") return ePhysicsMaterialCombMode_Max;
		if(sMode == "multiply") return ePhysicsMaterialCombMode_Multiply;

		return ePhysicsMaterialCombMode_Average;
	}

	//-----------------------------------------------------------------------

	void cPhysics::UpdateImpactCounts(float afTimeStep)
	{
		if(mRs){ ohpl_h15p_update_impacts(mRs, afTimeStep, mfImpactDuration); return; }
		ohpl_phys_impacts_update(mpImpactCounts, afTimeStep, mfImpactDuration);
	}

	//-----------------------------------------------------------------------

}
