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

#include "LuxProp_Lamp.h"
#include <cstring>

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxMap.h"
#include "LuxInventory.h"
#include "LuxMessageHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxGlobalDataHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Lamp::cLuxPropLoader_Lamp(const tString& asName) : iLuxPropLoader(asName)
{
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Lamp::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Lamp, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Lamp::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Lamp  *pLamp = static_cast<cLuxProp_Lamp*>(apProp);

	OhplGa4VarsAdapter vars(this);
	ohpl_ga4_lamp_load(vars.Vars(), &pLamp->mP);

	const OhplGa4Grab &g = pLamp->mP.grab;
	pLamp->mGrabData.mbGrabUseDepth = g.use_depth;
	pLamp->mGrabData.mfGrabDepth = g.depth;
	pLamp->mGrabData.mfGrabDepthInc = g.depth_inc;
	pLamp->mGrabData.mfGrabMinDepth = g.min_depth;
	pLamp->mGrabData.mfGrabMaxDepth = g.max_depth;
	pLamp->mGrabData.mvGrabPositionOffset = cVector3f(g.position_offset[0], g.position_offset[1], g.position_offset[2]);
	pLamp->mGrabData.mvGrabRotationOffset = cVector3f(g.rotation_offset[0], g.rotation_offset[1], g.rotation_offset[2]);
	pLamp->mGrabData.mfGrabThrowImpulse = g.throw_impulse;
	pLamp->mGrabData.mbGrabUseOffset = g.use_offset;
	pLamp->mGrabData.mfGrabMassMul = g.mass_mul;
	pLamp->mGrabData.mfForceMul = g.force_mul;
	pLamp->mGrabData.mfTorqueMul = g.torque_mul;
	pLamp->mGrabData.mbUseRotation = g.use_rotation;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Lamp::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
    cLuxProp_Lamp  *pLamp = static_cast<cLuxProp_Lamp*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4Slot_ConnectedLight, pLamp->msConnectionLight);
	OhplGa4EffectsIn env = pLamp->Ga4EffectsEnv(true, false);

	OhplGa4Run run(pLamp);
	ohpl_ga4_lamp_load_instance(vars.Vars(), &pLamp->mS, &pLamp->mBase, &env, pLamp->Ga4Lights(), pLamp->Ga4LightNum(), run.Sink());
	run.Finish();
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LAMP LIGHT CONNECTION
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxLampLightConnection::cLuxLampLightConnection(iLight *apLight)
{
	mpLight = apLight;
}

cLuxLampLightConnection::~cLuxLampLightConnection()
{
	tLuxLampLightConnection_LampListIt it = mlstLamps.begin();
	for(; it != mlstLamps.end(); ++it)
	{
		cLuxLampLightConnection_Lamp *pLampConnection = *it;
		pLampConnection->mpLamp->mpLightConnection = NULL;
	}

	STLDeleteAll(mlstLamps);
}

//-----------------------------------------------------------------------

void cLuxLampLightConnection::Update(float afTimeStep)
{
	cColor defaultColor = mpLight->GetDefaultDiffuseColor();

	std::vector<OhplGa4LampLink> vLinks;
	tLuxLampLightConnection_LampListIt it = mlstLamps.begin();
	for(; it != mlstLamps.end(); ++it)
	{
		cLuxLampLightConnection_Lamp *pLampConnection = *it;
		cLuxProp_Lamp *pLamp = pLampConnection->mpLamp;

		OhplGa4LampLink link;
		memset(&link, 0, sizeof(link));
		link.has_lights = pLamp->mvLights.empty()==false;
		if(link.has_lights)
		{
			cColor lightColor = pLampConnection->mbUseLightOnColor ?	pLamp->mvEffectLightData[0].mOnColor :
																		pLamp->mvLights[0]->GetDiffuseColor();
			link.color[0] = lightColor.r; link.color[1] = lightColor.g; link.color[2] = lightColor.b; link.color[3] = lightColor.a;
		}
		link.use_spec = pLampConnection->mbUseLightSpec;
		link.amount = pLampConnection->mfAmount;
		link.effects_alpha = pLamp->GetEffectsAlpha();
		vLinks.push_back(link);
	}

	float vDefault[4] = { defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a };
	float vFinal[4];
	ohpl_ga4_lamp_light_connection_color(vDefault, vLinks.empty() ? NULL : &vLinks[0], (int)vLinks.size(), vFinal);

	mpLight->SetDiffuseColor(cColor(vFinal[0], vFinal[1], vFinal[2], vFinal[3]));
}

//-----------------------------------------------------------------------

void cLuxLampLightConnection::AddLamp(cLuxProp_Lamp *apLamp, float afAmount, bool abUseOnColor, bool abUseSpec)
{
	cLuxLampLightConnection_Lamp *pLampConnection = hplNew( cLuxLampLightConnection_Lamp, () );
	pLampConnection->mpLamp =apLamp;
	pLampConnection->mfAmount = afAmount;
	pLampConnection->mbUseLightOnColor = abUseOnColor;
	pLampConnection->mbUseLightSpec = abUseSpec;

	mlstLamps.push_back(pLampConnection);
}

//-----------------------------------------------------------------------

void cLuxLampLightConnection::RemoveLamp(cLuxProp_Lamp *apLamp)
{
	tLuxLampLightConnection_LampListIt it = mlstLamps.begin();
	for(; it != mlstLamps.end(); )
	{
		cLuxLampLightConnection_Lamp *pLampConnection = *it;
		if(pLampConnection->mpLamp == apLamp)
		{
			it = mlstLamps.erase(it);
        }
		else
		{
			++it;
		}
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Lamp::cLuxProp_Lamp(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Lamp)
{
	memset(&mP, 0, sizeof(mP));
	ohpl_ga4_lamp_state_new(&mS);

	mpLightConnection = NULL;
}

//-----------------------------------------------------------------------

cLuxProp_Lamp::~cLuxProp_Lamp()
{
	if(mpLightConnection)
		mpLightConnection->RemoveLamp(this);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Lamp::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4_lamp_can_interact(&mS, &mP, msInteractCallback != "");
}

//-----------------------------------------------------------------------

bool cLuxProp_Lamp::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	//////////////////////
	//Turn off
	if(mS.lit)
	{
		//SetLit(false, true);
		//Nothing...
	}
	//////////////////////
	//Ignite
	else
	{
		/////////////////////
		// Check so has enough tinderboxes
		if(gpBase->mpPlayer->GetTinderboxes()<=0)
		{
			gpBase->mpMessageHandler->SetMessage(kTranslate("Game","NoMoreTinderboxes"), 0);
			return false;
		}

		/////////////////////
		// Add sanity
		float fSanityAdd = ohpl_ga4_lamp_sanity_add(gpBase->mpPlayer->GetSanity(), gpBase->mpGlobalDataHandler->GetLightLampMinSanityIncrease(), gpBase->mpGlobalDataHandler->GetLightLampMaxSanityIncrease());
		gpBase->mpPlayer->AddSanity(fSanityAdd, false);

		////////////////////
		// Negate tinderboxes
		gpBase->mpPlayer->AddTinderboxes(-1);
		
		gpBase->mpHelpFuncs->PlayGuiSoundData("ui_use_tinderbox", eSoundEntryType_Gui);

		RunCallbackFunc("OnIgnite");
		
		SetLit(true, true);
	}

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::OnSetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::UpdatePropSpecific(float afTimeStep)
{
	if(mS.light_connection_setup == false)
	{
		SetupLampLightConnection();
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::FadeTo(float afR, float afG, float afB, float afA, float afRadius, float afTime)
{
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		iLight *pLight = mvLights[i];
		cColor lightColor = pLight->GetDiffuseColor();
		float vLight[4] = { lightColor.r, lightColor.g, lightColor.b, lightColor.a };
		float vWanted[4] = { afR, afG, afB, afA };
		float vNew[4];
		float fNewRadius = ohpl_ga4_lamp_fade_target(vLight, pLight->GetRadius(), vWanted, afRadius, vNew);
		cColor newColor(vNew[0], vNew[1], vNew[2], vNew[3]);

		pLight->SetFlickerActive(false);
		pLight->SetVisible(true);
		pLight->FadeTo(newColor, fNewRadius, afTime);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::BeforePropDestruction()
{
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Lamp::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	switch(ohpl_ga4_lamp_focus_crosshair(&mS, &mP, mCustomFocusCrossHair==eLuxFocusCrosshair_Default, msInteractCallback != ""))
	{
	case 1: return mCustomFocusCrossHair;
	case 2: return eLuxFocusCrosshair_Ignite;
	}

	return eLuxFocusCrosshair_Default;
}

tWString cLuxProp_Lamp::GetFocusText()
{
	if(CanInteract(GetMainBody()) && mS.lit==false)
	{
		return _W("x ") + cString::ToStringW(gpBase->mpPlayer->GetTinderboxes());
	}
	return _W("");
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::SetLit(bool abX, bool abUseEffects)
{
	OhplGa4EffectsIn env = Ga4EffectsEnv(abX, abUseEffects);
	OhplGa4Run run(this);
	ohpl_ga4_lamp_set_lit(&mS, &mBase, &env, Ga4Lights(), Ga4LightNum(), abX, abUseEffects, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	OhplGa4EffectsIn env = Ga4EffectsEnv(true, true);
	OhplGa4Run run(this);
	ohpl_ga4_lamp_on_connection_state(&mS, &mBase, &env, Ga4Lights(), Ga4LightNum(), alState, run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxProp_Lamp::SetupLampLightConnection()
{
	if(msConnectionLight != "")
	{
		iLight *pConnectionLight = NULL;
		//Iterate all lights and get light with name and with NO parent (otherwise a light connected to an entity is gotten).
		cLightListIterator lightIt = mpWorld->GetLightIterator();
		while(lightIt.HasNext())
		{
			iLight *pLight = lightIt.Next();
            if(pLight->GetEntityParent() == NULL && pLight->GetName() == msConnectionLight)
			{
				pConnectionLight = pLight;
				break;
			}
		}
		
			
		if(pConnectionLight)
		{
			mpMap->AddLampLightConnection(this, pConnectionLight, mS.connection_light_amount, mS.connection_light_use_on_color, mS.connection_light_use_spec);
		}
		else
		{
			Error("Light '%s' that is connected to lamp '%s' does not exist!\n", msConnectionLight.c_str(), msName.c_str());
		}
	}
	mS.light_connection_setup = true;
}

//-----------------------------------------------------------------------

bool cLuxProp_Lamp::CanBeIgnitByPlayer()
{
	return ohpl_ga4_lamp_can_ignite(&mS, &mP);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Lamp_SaveData, iLuxProp_SaveData)
kSerializeVar(mbLit,	eSerializeType_Bool)
kSerializeVar(mbLightConnectionSetup,	eSerializeType_Bool)
kSerializeVar(msConnectionLight,			eSerializeType_String)
kSerializeVar(mfConnectionLightAmount,		eSerializeType_Float32)
kSerializeVar(mbConnectionLightUseOnColor,	eSerializeType_Bool)
kSerializeVar(mbConnectionLightUseSpec,		eSerializeType_Bool)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Lamp::CreateSaveData()
{
	return hplNew(cLuxProp_Lamp_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Lamp_SaveData *pData = static_cast<cLuxProp_Lamp_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	pData->mbLit = mS.lit;
	pData->mbLightConnectionSetup = mS.light_connection_setup;
	kCopyToVar(pData,msConnectionLight);
	pData->mfConnectionLightAmount = mS.connection_light_amount;
	pData->mbConnectionLightUseOnColor = mS.connection_light_use_on_color;
	pData->mbConnectionLightUseSpec = mS.connection_light_use_spec;
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Lamp_SaveData *pData = static_cast<cLuxProp_Lamp_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	mS.lit = pData->mbLit;
	mS.light_connection_setup = pData->mbLightConnectionSetup;
	kCopyFromVar(pData,msConnectionLight);
	mS.connection_light_amount = pData->mfConnectionLightAmount;
	mS.connection_light_use_on_color = pData->mbConnectionLightUseOnColor;
	mS.connection_light_use_spec = pData->mbConnectionLightUseSpec;
}

//-----------------------------------------------------------------------

void cLuxProp_Lamp::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

	SetupLampLightConnection();
}

//-----------------------------------------------------------------------
