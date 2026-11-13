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

#include "scene/ParticleSystem.h"

#include <memory>
#include <vector>

#include "impl/tinyXML/tinyxml.h"

#include "system/String.h"

#include "scene/ParticleEmitter_UserData.h"

#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/ParticleManager.h"

#include "engine/Engine.h"

#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/Light.h"
#include "scene/RustSceneH12.h"
#include "impl/RustBackends.h"

namespace hpl {

	// Calls the Rust lifecycle makes back into the system (OHPL_IMPL=rust); each repeats one statement of the C++ path.
	struct OhplH12PsHostImpl
	{
		static cParticleSystem* S(void *c){ return static_cast<cParticleSystem*>(c); }
		static int32_t Count(void *c){ return S(c)->GetEmitterNum(); }
		static float WarmTime(void *c, int32_t i){ return S(c)->mpData->GetEmitterData((int)i)->GetWarmUpTime(); }
		static float WarmSteps(void *c, int32_t i){ return S(c)->mpData->GetEmitterData((int)i)->GetWarmUpStepsPerSec(); }
		static void Update(void *c, int32_t i, float afStep){ S(c)->GetEmitter((int)i)->UpdateLogic(afStep); }
		static void Apply(void *c, int32_t i, int32_t alOp, bool abArg)
		{
			iParticleEmitter *pPE = S(c)->GetEmitter((int)i);
			switch(alOp)
			{
			case kOhplH12OpVisible: pPE->SetVisible(abArg); break;
			case kOhplH12OpCullDistance: pPE->SetCulledByDistance(abArg); break;
			case kOhplH12OpCullFog: pPE->SetCulledByFog(abArg); break;
			case kOhplH12OpKill: pPE->Kill(); break;
			default: pPE->KillInstantly(); break;
			}
		}
		static bool Query(void *c, int32_t i, int32_t alQuery)
		{
			iParticleEmitter *pPE = S(c)->GetEmitter((int)i);
			return alQuery == kOhplH12QueryDead ? pPE->IsDead() : pPE->IsDying();
		}
		static OhplH12PsHost Make(cParticleSystem *apSystem)
		{
			OhplH12PsHost h = { apSystem, Count, WarmTime, WarmSteps, Update, Apply, Query };
			return h;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// CREATOR
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cParticleSystemData::cParticleSystemData(const tString &asName,
												cResources* apResources,cGraphics *apGraphics)
												: iResourceBase(asName, _W(""),0)
	{
		mpResources = apResources;
		mpGraphics = apGraphics;
	}
	
	//-----------------------------------------------------------------------

	cParticleSystemData::~cParticleSystemData()
	{
		STLDeleteAll(mvEmitterData);
	}

	//-----------------------------------------------------------------------

	void cParticleSystemData::AddEmitterData(iParticleEmitterData *apData)
	{
		mvEmitterData.push_back(apData);
	}

	//-----------------------------------------------------------------------

	cParticleSystem* cParticleSystemData::Create(tString asName, cVector3f avSize)
	{
		if(OhplBackendRustRequested() ? !ohpl_h12b_psdata_can_create((uint32_t)mvEmitterData.size()) : mvEmitterData.empty())
		{
			Warning("Particle system '%s' has no emitters.\n",msName.c_str());
			return NULL;
		}

		cParticleSystem *pPS = hplNew( cParticleSystem, (asName,this,mpResources,mpGraphics) );
		
		for(size_t i=0; i<mvEmitterData.size(); ++i)
		{
			///////////////////////////
			// Create and add
			iParticleEmitter *pPE = static_cast<iParticleEmitter*>(mvEmitterData[i]->Create(asName, avSize));
			pPS->AddEmitter(pPE);
			pPE->SetSystem(pPS);
		}

		return pPS;
	}

	//-----------------------------------------------------------------------

	bool cParticleSystemData::LoadFromFile(const tWString &asFile)
	{
		SetFullPath(asFile);

		iXmlDocument* pXmlDoc = mpResources->GetLowLevel()->CreateXmlDocument();
		if(pXmlDoc->CreateFromFile(asFile)==false)
		{
			hplDelete(pXmlDoc);
			return false;
		}

		bool bRet = LoadFromElement(pXmlDoc);

		hplDelete(pXmlDoc);
		return bRet;
		/*
		
		FILE *pFile = _wfopen(asFile.c_str(),_W("rb"));
		if(pFile==NULL) return false;

		TiXmlDocument* pXmlDoc = hplNew( TiXmlDocument,() );
		
		if(pXmlDoc->LoadFile(pFile)==false)
		{
			Warning("Couldn't open XML file %s\n",cString::To8Char(asFile).c_str());
			fclose(pFile);
			hplDelete(pXmlDoc);
			return false;
		}
		fclose(pFile);
		
		TiXmlElement *pRootElem = pXmlDoc->RootElement();

		bool bRet = LoadFromElement(pRootElem);

        hplDelete(pXmlDoc);
		return bRet;*/
	}

	//-----------------------------------------------------------------------

	bool cParticleSystemData::LoadFromElement(cXmlElement* apElement)
	{
		if(OhplBackendRustRequested())
		{
			if(apElement->GetValue()!="ParticleSystem")
				return ohpl_h12b_psdata_plan(false, NULL, 0, (uint32_t)mvEmitterData.size(), NULL);

			std::vector<tString> vValues;
			cXmlNodeListIterator itV = apElement->GetChildIterator();
			while(itV.HasNext()) vValues.push_back(itV.Next()->ToElement()->GetValue());

			std::unique_ptr<bool[]> vIsEmitter(new bool[vValues.size() + 1]), vLoad(new bool[vValues.size() + 1]);
			for(size_t i=0; i<vValues.size(); ++i) vIsEmitter[i] = vValues[i]=="ParticleEmitter";

			const bool bRet = ohpl_h12b_psdata_plan(apElement->GetValue()=="ParticleSystem", vIsEmitter.get(), (uint32_t)vValues.size(),
													(uint32_t)mvEmitterData.size(), vLoad.get());

			cXmlNodeListIterator itL = apElement->GetChildIterator();
			for(size_t i=0; itL.HasNext(); ++i)
			{
				cXmlElement* pElem = itL.Next()->ToElement();
				if(vLoad[i]==false) continue;

				cParticleEmitterData_UserData *pPE = hplNew( cParticleEmitterData_UserData,("",	mpResources,mpGraphics) );
				pPE->LoadFromElement(pElem);
				mvEmitterData.push_back(pPE);
			}
			return bRet;
		}

		if(apElement->GetValue()!="ParticleSystem")
			return false;

		cXmlNodeListIterator it = apElement->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement* pElem = it.Next()->ToElement();
			if(pElem->GetValue()!="ParticleEmitter")
				continue;

			cParticleEmitterData_UserData *pPE = hplNew( cParticleEmitterData_UserData,("",	mpResources,mpGraphics) );
			pPE->LoadFromElement(pElem);

			mvEmitterData.push_back(pPE);
		}

		/*cXmlElement *pEmitterElem = apElement->GetFirstElement("ParticleEmitter");
		for(; pEmitterElem != NULL; pEmitterElem = pEmitterElem->NextSiblingElement("ParticleEmitter"))
		{
			cParticleEmitterData_UserData *pPE = hplNew( cParticleEmitterData_UserData,("",
																	mpResources,mpGraphics) );

            pPE->LoadFromElement(pEmitterElem);
			
			mvEmitterData.push_back(pPE);
		}*/

		return (mvEmitterData.empty()==false);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cParticleSystem::cParticleSystem(const tString asName, 
										cParticleSystemData *apData,
										cResources *apResources, cGraphics *apGraphics)
		: iEntity3D(asName)
	{
		mpResources = apResources;
		mpGraphics = apGraphics;
		mpParticleManager = NULL;
		mpData = apData;

		mbRemoveWhenDead = true;

		mbIsVisible = true;

		mbFirstUpdate = true;

		mColor = cColor(1,1);
		mfBrightness = 1;
		mLightColor = cColor(1,1);
		mfLightBrightness = 1;
		mpLight = NULL;
		mbFadeAtDistance = false;
		mfMinFadeDistanceStart = 2;
		mfMinFadeDistanceEnd = 1;
		mfMaxFadeDistanceStart = 100;
		mfMaxFadeDistanceEnd = 110;
	}

	//-----------------------------------------------------------------------

	bool cParticleSystem::mbHpl3Color = false;

	cParticleSystem::~cParticleSystem()
	{
		if(mpLight) mpLight->RemoveParticleSystem(this);
		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			hplDelete(mvEmitters[i]);
		}
		if(mpParticleManager) mpParticleManager->Destroy(mpData);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cParticleSystem::SetVisible(bool abVisible)
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_set_visible(&h, &mbIsVisible, abVisible);
			return;
		}

		if(mbIsVisible == abVisible) return;

		mbIsVisible = abVisible;
		
		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			mvEmitters[i]->SetVisible(mbIsVisible);
		}
		
	}

	void cParticleSystem::SetCulledByDistance(bool abX)
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_apply_all(&h, kOhplH12OpCullDistance, abX);
			return;
		}
		for(size_t i=0; i< mvEmitters.size(); ++i) mvEmitters[i]->SetCulledByDistance(abX);
	}

	void cParticleSystem::SetCulledByFog(bool abX)
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_apply_all(&h, kOhplH12OpCullFog, abX);
			return;
		}
		for(size_t i=0; i< mvEmitters.size(); ++i) mvEmitters[i]->SetCulledByFog(abX);
	}
	
	//-----------------------------------------------------------------------

	bool cParticleSystem::IsDead()
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			return ohpl_h12b_ps_all(&h, kOhplH12QueryDead);
		}

		size_t lCount =0;

		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			iParticleEmitter *pPE = mvEmitters[i];

			if(pPE->IsDead()) lCount++;
		}

		if(lCount == mvEmitters.size()) return true;

		return false;
	}

	bool cParticleSystem::IsDying()
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			return ohpl_h12b_ps_all(&h, kOhplH12QueryDying);
		}

		size_t lCount =0;

		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			iParticleEmitter *pPE = mvEmitters[i];

			if(pPE->IsDying()) lCount++;
		}

		if(lCount == mvEmitters.size()) return true;

		return false;
	}

	//-----------------------------------------------------------------------

	void cParticleSystem::Kill()
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_apply_all(&h, kOhplH12OpKill, false);
			return;
		}
		//SetIsSaved(false);
		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			iParticleEmitter *pPE = mvEmitters[i];

			pPE->Kill();
		}
	}
	
	//-----------------------------------------------------------------------

	void cParticleSystem::KillInstantly()
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_apply_all(&h, kOhplH12OpKillInstantly, false);
			return;
		}
		//SetIsSaved(false);
		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			iParticleEmitter *pPE = mvEmitters[i];

			pPE->KillInstantly();
		}
	}

	//-----------------------------------------------------------------------
	
	void cParticleSystem::UpdateLogic(float afTimeStep)
	{
		if(OhplBackendRustRequested())
		{
			const OhplH12PsHost h = OhplH12PsHostImpl::Make(this);
			ohpl_h12b_ps_update_logic(&h, &mbFirstUpdate, IsActive(), afTimeStep);
			return;
		}

		if(IsActive()==false) return;

		for(size_t i=0; i< mvEmitters.size(); ++i)
		{
			iParticleEmitter *pPE = mvEmitters[i];

			//////////////////////////
			//Warm Up
			if(mbFirstUpdate)
			{
				iParticleEmitterData *pData =  mpData->GetEmitterData((int)i);
				
				if(pData->GetWarmUpTime() >0)
				{
					float fTime = pData->GetWarmUpTime();
					float fStepSize = 1.0f /pData->GetWarmUpStepsPerSec();

					while(fTime >0)
					{
						pPE->UpdateLogic(fStepSize);
						fTime -= fStepSize;
					}
				}
			}
			
			//////////////////////////
			//Update
			pPE->UpdateLogic(afTimeStep);
		}

		//No loner first time!
		mbFirstUpdate = false;
	}

	//-----------------------------------------------------------------------

	void cParticleSystem::AddEmitter(iParticleEmitter* apEmitter)
	{
		mvEmitters.push_back(apEmitter);

		AddChild(apEmitter);
	}
	
	//-----------------------------------------------------------------------

	iParticleEmitter* cParticleSystem::GetEmitter(int alIdx)
	{
		return mvEmitters[alIdx];
	}
	
	//-----------------------------------------------------------------------

	int cParticleSystem::GetEmitterNum()
	{
		return (int)mvEmitters.size();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	//-----------------------------------------------------------------------
}
