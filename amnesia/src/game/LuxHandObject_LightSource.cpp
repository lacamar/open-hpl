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

#include "LuxHandObject_LightSource.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHands.h"
#include "LuxMapHelper.h"
#include "LuxHelpFuncs.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxHandObject_LightSource::cLuxHandObject_LightSource(const tString& asName, cLuxPlayerHands *apHands) : iLuxHandObject(asName, apHands)
{
	mSway.angle =0;
	mSway.vel =0;
}

cLuxHandObject_LightSource::~cLuxHandObject_LightSource()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::LoadImplementedVars(cXmlElement *apVarsElem)
{
	OhplGa4bLightAttrs attrs;
	attrs.fade_in_time = apVarsElem->GetAttributeFloat("FadeInTime", 0);
	attrs.fade_out_time = apVarsElem->GetAttributeFloat("FadeOutTime", 0);
	attrs.has_sway = apVarsElem->GetAttributeBool("HasSwayPhysics", false);
	attrs.max_sway_vel = apVarsElem->GetAttributeFloat("MaxSwayVel", 0);
	cVector2f vSwayLimits = apVarsElem->GetAttributeVector2f("SwayAngleLimits", 0);
	attrs.sway_limits[0] = vSwayLimits.x; attrs.sway_limits[1] = vSwayLimits.y;
	cVector2f vSwayDownLimits = apVarsElem->GetAttributeVector2f("SwayDownAngleLimits", 0);
	attrs.sway_down_limits[0] = vSwayDownLimits.x; attrs.sway_down_limits[1] = vSwayDownLimits.y;
	attrs.gravity = apVarsElem->GetAttributeFloat("SwayGravity", 0);
	attrs.friction = apVarsElem->GetAttributeFloat("SwayFriction", 0);
	cVector3f vPinDir = apVarsElem->GetAttributeVector3f("SwayPinDir", 1);
	attrs.pin_dir[0] = vPinDir.x; attrs.pin_dir[1] = vPinDir.y; attrs.pin_dir[2] = vPinDir.z;
	attrs.player_speed_mul = apVarsElem->GetAttributeFloat("SwayPlayerSpeedMul", 0);
	attrs.camera_roll_mul = apVarsElem->GetAttributeFloat("SwayCameraRollMul", 0);
	ohpl_ga4b_hand_light_load(&attrs, &mP);

	msSkipSwaySubMesh = apVarsElem->GetAttributeString("SkipSwaySubMesh", "");
}

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::ImplementedCreateEntity(cLuxMap *apMap)
{
	mvDefaultLightColors.resize(mvLights.size());
	mvDefaultLightFlicker.resize(mvLights.size());
	mvLightFadeOutColor.resize(mvLights.size());

	for(size_t i=0; i<mvLights.size(); ++i)
	{
		mvDefaultLightColors[i] = mvLights[i]->GetDiffuseColor();
		mvDefaultLightFlicker[i] =mvLights[i]->GetFlickerActive();
		mvLights[i]->SetFlickerActive(false);
	}

	mvDefaultSubMeshMatrix.resize(mpMeshEntity->GetSubMeshEntityNum());
	for(size_t i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity *pSubEnt = mpMeshEntity->GetSubMeshEntity(i);
		mvDefaultSubMeshMatrix[i] = pSubEnt->GetLocalMatrix();
	}
}

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::ImplementedDestroyEntity(cLuxMap *apMap)
{
}

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::ImplementedReset()
{
	mSway.angle =0;
	mSway.vel =0;
}

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::Update(float afTimeStep)
{
	bool bUpdate = false;
	bool bUpdateDone = false;

	///////////////////
	// Sway Physics
    if(mP.has_sway)
	{
		UpdateSwayPhysics(afTimeStep);	
	}

	///////////////////
	// Fade out
	bool bHolster = mpHands->GetState() == eLuxHandsState_Holster;
	if(bHolster)
	{
		for(size_t i=0; i<mvLights.size(); ++i)
		{
			if(mvDefaultLightFlicker[i])
			{
				mvLights[i]->SetFlickerActive(false);
				mvLights[i]->StopFading();
			}
		}
	}

	OhplGa4bFade fade;
	ohpl_ga4b_hand_light_fade(&mP, bHolster, mpHands->mfHandObjectAlpha, afTimeStep, &fade);
	mpHands->mfHandObjectAlpha = fade.alpha;
	bUpdate = fade.update;
	bUpdateDone = fade.done;

	if(mpHands->GetState() != eLuxHandsState_Holster)
	{
		for(size_t i=0; i<mvLights.size(); ++i)
		{
			mvLightFadeOutColor[i] = mvLights[i]->GetDiffuseColor();
		}
	}
	

	///////////////////
	// Set alpha
	if(bUpdate)
	{
		mpMeshEntity->SetIlluminationAmount(mpHands->mfHandObjectAlpha);

		for(size_t i=0; i<mvBillboards.size(); ++i)
		{
			cColor col = mvBillboards[i]->GetColor();
			col.a = mpHands->mfHandObjectAlpha;
			mvBillboards[i]->SetColor(col);
		}
		
		for(size_t i=0; i<mvParticleSystems.size(); ++i)
		{
			cColor col = mvParticleSystems[i]->GetColor();
			col.a = mpHands->mfHandObjectAlpha;
			mvParticleSystems[i]->SetColor(col);
		}
		
		for(size_t i=0; i<mvLights.size(); ++i)
		{
			if(mpHands->GetState() == eLuxHandsState_Holster)
			{
				mvLights[i]->SetDiffuseColor(mvLightFadeOutColor[i] * mpHands->mfHandObjectAlpha);
			}
			else
			{
				mvLights[i]->SetDiffuseColor(mvDefaultLightColors[i] * mpHands->mfHandObjectAlpha);
			}
		}
	}

	if(bUpdateDone)
	{
		for(size_t i=0; i<mvLights.size(); ++i)
		{
			if(mvDefaultLightFlicker[i]) mvLights[i]->SetFlickerActive(true);
		}
	}
}

//-----------------------------------------------------------------------

bool cLuxHandObject_LightSource::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	return false;
}

//-----------------------------------------------------------------------

bool cLuxHandObject_LightSource::AnimationIsOver()
{
	return true;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxHandObject_LightSource::UpdateSwayPhysics(float afTimeStep)
{
	cMatrixf mtxSway;
	{
		cCamera *pCam = gpBase->mpPlayer->GetCamera();

		iCharacterBody *pCharBody = gpBase->mpPlayer->GetCharacterBody();
		float fPlayerSpeed = pCharBody->GetVelocity(gpBase->mpEngine->GetStepSize()).Length();
		bool bMovingBack = pCharBody->GetMoveSpeed(eCharDir_Forward)<0;

		ohpl_ga4b_hand_light_sway(&mP, &mSway, pCam->GetPitch(), pCam->GetRoll(), fPlayerSpeed, bMovingBack, afTimeStep, &mtxSway.m[0][0]);
	}
	//cMatrixf mtxTrans = cMath::MatrixMul(m_mtxOffset, mtxSway);
	//mpMeshEntity->SetMatrix(mtxTrans);

	for(size_t i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity *pSubEnt = mpMeshEntity->GetSubMeshEntity(i);
		if(pSubEnt->GetSubMesh()->GetName() == msSkipSwaySubMesh) continue;
		//Log("'%s'\n",pSubEnt->GetSubMesh()->GetName().c_str());
		
		pSubEnt->SetMatrix(cMath::MatrixMul(mtxSway, mvDefaultSubMeshMatrix[i]) );
	}
	mpMeshEntity->SetMatrix(m_mtxOffset);
}

//-----------------------------------------------------------------------

