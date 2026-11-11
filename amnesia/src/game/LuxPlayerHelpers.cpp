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

#include "LuxPlayerHelpers.h"

#include "LuxPlayer.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMoveState_Normal.h"
#include "LuxMusicHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"
#include "LuxEffectHandler.h"
#include "LuxEnemy.h"
#include "LuxProp.h"
#include "LuxProp_CritterBase.h"
#include "LuxHintHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxInputHandler.h"
#include "LuxSaveHandler.h"
#include "LuxPlayerHands.h"
#include "LuxPostEffects.h"
#include "LuxProgressLogHandler.h"
#include "LuxDebugHandler.h"
#include "LuxPlayerState.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMainMenu.h"
#include "RustLuxPlayer.h"


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER INSANITY COLLAPSE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerInsanityCollapse::cLuxPlayerInsanityCollapse(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerInsanityCollapse")
{
	mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddGoal",0);
	
	mfHeightAddCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddCollapseSpeed",0);
	mfHeightAddAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddAwakeSpeed",0);
	mfRollCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollCollapseSpeed",0);
	mfRollAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollAwakeSpeed",0);

	mfSleepTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepTime",0);

	mfSleepSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepSpeedMul",0);
	mfWakeUpSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_WakeUpSpeedMul",0);

	msStartSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_StartSound", "");
	msAwakenSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_AwakenSound", "");
	msSleepLoopSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_SleepLoopSound", "");
	mfSleepLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepLoopSoundVolume", 0);

	msSleepRandomSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_SleepRandomSound", "");
	mfSleepRandomMinTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMinTime", 0);
	mfSleepRandomMaxTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMaxTime", 0);

	mfAwakenSanity = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_AwakenSanity", 0);

	//Init sound var here
	mpLoopSound = NULL;
}

cLuxPlayerInsanityCollapse::~cLuxPlayerInsanityCollapse()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Reset()
{
	mbActive = false;
	mlState =0;
	mfHeightAdd =0;
	mfRoll =0;
	mfT=0;
	mfRandomCount=0;

	if(mpLoopSound && gpBase->mpEngine->GetSound()->GetSoundHandler()->IsValid(mpLoopSound, mlLoopSoundID))
	{
		mpLoopSound->Stop();
		mpLoopSound = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Start()
{
	if(mbActive) return;

	/////////////////
	//Play sound
	gpBase->mpHelpFuncs->PlayGuiSoundData(msStartSound, eSoundEntryType_World);

	/////////////////
	// Setup player
	mpPlayer->ChangeState(eLuxPlayerState_Normal);
	mpPlayer->ChangeMoveState(eLuxMoveState_Normal);
	mpPlayer->GetHelperLantern()->SetActive(false,false, false);
	mpPlayer->SetCurrentHandObjectDrawn(false);
	mpPlayer->SetInsanityCollapseSpeedMul(mfSleepSpeedMul);
	
	/////////////////
	//Set up sound
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(0.75f, 0.15f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
	pSoundHandler->FadeGlobalSpeed(0.5f, 0.125f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);

	/////////////////
	//Loop sound
	mpLoopSound = pSoundHandler->PlayGui(msSleepLoopSound,true,1.0f);
	if(mpLoopSound)
	{
		mpLoopSound->FadeIn(mfSleepLoopSoundVolume, 0.2f);
		mlLoopSoundID = mpLoopSound->GetId();
	}

	/////////////////
	//Attract enemies
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	cLuxEnemyIterator enemyIt = pMap->GetEnemyIterator();
	while(enemyIt.HasNext())
	{
		iLuxEnemy *pEnemy = enemyIt.Next();
        if(pEnemy->IsActive()) pEnemy->AlertOfPlayerPresence();
	}

	/////////////////
	//Setup variables.	
	mbActive = true;

	mlState =0;
	mfHeightAdd =0;
	mfRoll =0;
	mfT=0;
	mfRandomCount=0;
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Stop()
{
	if(mbActive==false) return;

	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(1, 0.3f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
	pSoundHandler->FadeGlobalSpeed(1, 0.5f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);

	mpPlayer->SetInsanityCollapseSpeedMul(1.0f);
	
	if(mpLoopSound && pSoundHandler->IsValid(mpLoopSound, mlLoopSoundID))
	{
		mpLoopSound->FadeOut(1.0f);
	}

	mbActive = false;
	
	mlState =0;
	mfT =0;
	mfRoll =0;
	mfHeightAdd=0;
	mpPlayer->FadeRollTo(0, 10,10);
	mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,0,0),1, 0.1f);
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Update(float afTimeStep)
{
	if(mbActive==false) return;
	OhplGa2Collapse s = {mbActive, mlState, mfHeightAdd, mfRoll, mfT, mfRandomCount, mfHeightAddGoal, mfHeightAddCollapseSpeed,
		mfHeightAddAwakeSpeed, mfRollCollapseSpeed, mfSleepTime, mfWakeUpSpeedMul, mfAwakenSanity, mfSleepRandomMinTime, mfSleepRandomMaxTime};
	OhplGa2Ops ops;
	ohpl_ga2_collapse_update(&s, afTimeStep, &ops);
	mbActive = s.mbActive != 0;
	mlState = s.mlState;
	mfHeightAdd = s.mfHeightAdd;
	mfRoll = s.mfRoll;
	mfT = s.mfT;
	mfRandomCount = s.mfRandomCount;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2SetHeadAdd: mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,op.a,0)); break;
		case kOhplGa2FadeRoll: mpPlayer->FadeRollTo(op.a, op.b, op.c); break;
		case kOhplGa2PlayRandomSound: gpBase->mpHelpFuncs->PlayGuiSoundData(msSleepRandomSound, eSoundEntryType_Gui); break;
		case kOhplGa2RandCount: mfRandomCount = cMath::RandRectf(op.a, op.b); break;
		case kOhplGa2WakeFx:
		{
			cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
			pSoundHandler->FadeGlobalVolume(1, 0.2f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
			pSoundHandler->FadeGlobalSpeed(1, 0.25f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
			break;
		}
		case kOhplGa2SetCollapseSpeedMul: mpPlayer->SetInsanityCollapseSpeedMul(op.a); break;
		case kOhplGa2LoopFadeOut:
		{
			cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
			if(mpLoopSound && pSoundHandler->IsValid(mpLoopSound, mlLoopSoundID))
			{
				mpLoopSound->FadeOut(0.3f);
			}
			break;
		}
		case kOhplGa2PlayerSetSanity: mpPlayer->SetSanity(op.a); break;
		case kOhplGa2PlayAwaken: gpBase->mpHelpFuncs->PlayGuiSoundData(msAwakenSound, eSoundEntryType_World); break;
		case kOhplGa2SetCrouchDisabled: mpPlayer->SetCrouchDisabled(op.i != 0); break;
		case kOhplGa2SetJumpDisabled: mpPlayer->SetJumpDisabled(op.i != 0); break;
		}
	}
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER SIGHT SWAY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerCamDirEffects::cLuxPlayerCamDirEffects(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerCamDirEffects")
{
	mfStartSwayMaxSanity =  gpBase->mpGameCfg->GetFloat("Player_Sanity","StartSwayMaxSanity",0);
	mlMaxPositions = gpBase->mpGameCfg->GetInt("Player_Sanity","SwayMaxSavedPositions",0);
}

cLuxPlayerCamDirEffects::~cLuxPlayerCamDirEffects()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerCamDirEffects::Reset()
{
	mbSwayActive = false;

	mlstPrevAdd.clear();
	mvSwayAdd = 0;
	
	mvNextAdd = 0;

	mfSwayAlpha =0;
}

//-----------------------------------------------------------------------


void cLuxPlayerCamDirEffects::Update(float afTimeStep)
{
	OhplGa2CamDir s = {mfStartSwayMaxSanity, mbSwayActive, mvSwayAdd, mlMaxPositions, mfSwayAlpha, mvNextAdd};
	std::vector<cVector2f> vPrev(mlstPrevAdd.begin(), mlstPrevAdd.end());
	OhplGa2Ops ops;
	ohpl_ga2_camdir_update(&s, vPrev.data(), vPrev.size(), mpPlayer->GetSanity(), afTimeStep, &ops);
	mbSwayActive = s.mbSwayActive != 0;
	mvSwayAdd = s.mvSwayAdd;
	mfSwayAlpha = s.mfSwayAlpha;
	mvNextAdd = s.mvNextAdd;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2ListClear: mlstPrevAdd.clear(); break;
		case kOhplGa2ListPush: mlstPrevAdd.push_back(cVector2f(op.a, op.b)); break;
		case kOhplGa2ListPopFront: mlstPrevAdd.pop_front(); break;
		}
	}
}

//-----------------------------------------------------------------------

float cLuxPlayerCamDirEffects::AddAndGetYawAdd(float afX)
{
	OhplGa2CamDir s = {mfStartSwayMaxSanity, mbSwayActive, mvSwayAdd, mlMaxPositions, mfSwayAlpha, mvNextAdd};
	float fYaw = ohpl_ga2_camdir_add(&s, mlstPrevAdd.empty(), true, afX);
	mvNextAdd = s.mvNextAdd;
	return fYaw;
}

float cLuxPlayerCamDirEffects::AddAndGetPitchAdd(float afX)
{
	OhplGa2CamDir s = {mfStartSwayMaxSanity, mbSwayActive, mvSwayAdd, mlMaxPositions, mfSwayAlpha, mvNextAdd};
	float fPitch = ohpl_ga2_camdir_add(&s, mlstPrevAdd.empty(), false, afX);
	mvNextAdd = s.mvNextAdd;
	return fPitch;
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------




//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SPAWN PS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSpawnPS::cLuxPlayerSpawnPS(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSpawnPS")
{

}

cLuxPlayerSpawnPS::~cLuxPlayerSpawnPS()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Reset()
{
	mbActive = false;
	msFileName = "";

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap && mvSpawnPos.empty()==false)
	{
		cWorld *pWorld = pMap->GetWorld();
		for(size_t i=0; i<mvSpawnPos.size(); ++i)
		{
			cParticleSystem *pPS = mvSpawnPos[i].mpPS;

			if(pWorld->ParticleSystemExists(pPS))
				pWorld->DestroyParticleSystem(pPS);
		}
	}

	mvSpawnPos.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Start(const tString& asFileName)
{
	if(LoadSpawnPSFile(asFileName)==false)
	{
		return;
	}

	msFileName = asFileName;
	mbActive = true;
	
	RespawnAll();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Stop()
{
	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::RespawnAll()
{
	DestroyAllSpawnPoints();
	GenerateAllSpawnPos();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Update(float afTimeStep)
{
	if(mbActive==false) return;

	cVector3f vPlayerPos = mpPlayer->GetCharacterBody()->GetFeetPosition();
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();

	for(size_t i=0; i<mvSpawnPos.size(); ++i)
	{
		cLuxPlayerSpawnPS_SpawnPos &spawnPos = mvSpawnPos[i];

		cVector3f vCurrentLocal = spawnPos.mvPos - vPlayerPos;

		///////////////////
		// Check if outside of "radius" (really a square)
		if(fabs(vCurrentLocal.x) > mfRadius || fabs(vCurrentLocal.z) > mfRadius)
		{
			////////////////////
			//Kill PS
			cParticleSystem *pPS = spawnPos.mpPS;
			if(pPS && pWorld->ParticleSystemExists(pPS))
			{
				pPS->Kill();
			}

			////////////////////
			//Get new position, opposite of the old
			cVector3f vAdd(0);
			cVector3f vMul(1);
			if(vCurrentLocal.x > mfRadius){			vAdd.x = vCurrentLocal.x - mfRadius;	vMul.x=-1; }
			else if(vCurrentLocal.x < -mfRadius){	vAdd.x = vCurrentLocal.x + mfRadius;	vMul.x=-1; }
			if(vCurrentLocal.z > mfRadius){			vAdd.z = vCurrentLocal.z - mfRadius;	vMul.z=-1; }
			else if(vCurrentLocal.z < -mfRadius){	vAdd.z = vCurrentLocal.z + mfRadius;	vMul.z=-1; }

			cVector3f vTemp = vCurrentLocal;
			vCurrentLocal = vCurrentLocal*vMul + vAdd*2;//*2=first remove the offset and then put it inside.

			//Still do random height!
			vCurrentLocal.y = vPlayerPos.y;
			vCurrentLocal.y += mfHeightFromFeet + cMath::RandRectf(mfHeightAddMin, mfHeightAddMax);

			spawnPos.mvPos = vPlayerPos + vCurrentLocal;

			////////////////////
			//Create Particle System
			spawnPos.mpPS = CreatePS(&spawnPos);
		}
		///////////////////
		// Update the local pos
		else
		{
			spawnPos.mvLastLocalPos = vCurrentLocal;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::GenerateAllSpawnPos()
{
	cVector3f vPlayerPos = mpPlayer->GetCharacterBody()->GetFeetPosition();

	cVector3f vStep = cVector3f(1/mfDensity, 0, 1/mfDensity);
	cVector3f vLocalStartPos = cVector3f(-mfRadius, 0, -mfRadius) + vStep*0.5f;
	cVector3f vLocalPos = vLocalStartPos;

	for(; vLocalPos.z < mfRadius; vLocalPos.z += vStep.z)
	{
		vLocalPos.x = vLocalStartPos.x;

		for(; vLocalPos.x < mfRadius; vLocalPos.x += vStep.x)
		{
			cLuxPlayerSpawnPS_SpawnPos spawnPos;

			float fRandRange = (1.0f/mfDensity)*0.2f;
			cVector3f vFinalLocalPos = vLocalPos +	cMath::RandRectVector3f(cVector3f(-fRandRange,0,-fRandRange),
																			cVector3f(fRandRange,0,fRandRange));
			vFinalLocalPos.y += mfHeightFromFeet + cMath::RandRectf(mfHeightAddMin, mfHeightAddMax);

			spawnPos.mvLastLocalPos = vFinalLocalPos;
			spawnPos.mvPos = vPlayerPos + vFinalLocalPos;

			spawnPos.mpPS = CreatePS(&spawnPos);
			if(spawnPos.mpPS)
				mvSpawnPos.push_back(spawnPos);
		}
	}
}

//-----------------------------------------------------------------------

cParticleSystem* cLuxPlayerSpawnPS::CreatePS(cLuxPlayerSpawnPS_SpawnPos *apSpawnPos)
{
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	cParticleSystem *pPS = pWorld->CreateParticleSystem("SpawnPS", msParticleSystem, 1);
	if(pPS)
	{
		pPS->SetPosition(apSpawnPos->mvPos);
		pPS->SetFadeAtDistance(mbFadePS);
		pPS->SetMinFadeDistanceStart(mfPSMinFadeStart);
		pPS->SetMinFadeDistanceEnd(mfPSMinFadeEnd);
		pPS->SetMaxFadeDistanceStart(mfPSMaxFadeStart);
		pPS->SetMaxFadeDistanceEnd(mfPSMaxFadeEnd);
		pPS->SetColor(mPSColor);
	}
	return pPS;
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::DestroyAllSpawnPoints()
{
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	for(size_t i=0; i<mvSpawnPos.size(); ++i)
	{
		//Detroy particle system
		cLuxPlayerSpawnPS_SpawnPos &spawnPos = mvSpawnPos[i];
		cParticleSystem *pPS = spawnPos.mpPS;
		if(pPS && pWorld->ParticleSystemExists(pPS))
		{
			pPS->Kill();
		}		
	}
	mvSpawnPos.clear();
}

//-----------------------------------------------------------------------

bool cLuxPlayerSpawnPS::LoadSpawnPSFile(const tString& asFileName)
{
	tString sFile = cString::SetFileExt(asFileName, "sps");
	cResources *pResources = gpBase->mpEngine->GetResources();

	iXmlDocument *pXmlDoc = pResources->LoadXmlDocument(sFile);
	if(pXmlDoc==NULL)
	{
		Error("Could not load sps file: '%s'\n", sFile.c_str());
		return false;
	}

	msParticleSystem = pXmlDoc->GetAttributeString("ParticleSystem","");
	mfHeightFromFeet = pXmlDoc->GetAttributeFloat("HeightFromFeet",0);
	mfHeightAddMin = pXmlDoc->GetAttributeFloat("HeightAddMin",0);
	mfHeightAddMax = pXmlDoc->GetAttributeFloat("HeightAddMax",0);
	mfDensity = pXmlDoc->GetAttributeFloat("Density",0);
	mfRadius = pXmlDoc->GetAttributeFloat("Radius",0);
	mPSColor = pXmlDoc->GetAttributeColor("PSColor",cColor(0));
	mbFadePS = pXmlDoc->GetAttributeBool("FadePS",true);
	mfPSMinFadeStart = pXmlDoc->GetAttributeFloat("PSMinFadeStart",0);
	mfPSMinFadeEnd = pXmlDoc->GetAttributeFloat("PSMinFadeEnd",0);
	mfPSMaxFadeStart = pXmlDoc->GetAttributeFloat("PSMaxFadeStart",0);
	mfPSMaxFadeEnd = pXmlDoc->GetAttributeFloat("PSMaxFadeEnd",0);
	
	pResources->DestroyXmlDocument(pXmlDoc);

	return true;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HURT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHurt::cLuxPlayerHurt(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHurt")
{
	mfEffectStartHealth =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_EffectStartHealth",0);
	mfMinMoveMul =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MinSpeedMul",0);

	mfMaxPantCount =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MaxPantCount",0);
	mfPantSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSpeed",0);
	mfPantSize =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSize",0);

	mfHealthRegainSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainSpeed",0);
	mfHealthRegainLimit =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainLimit",0);

	mfNoiseAlpha =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseAlpha",0);
	mfNoiseFreq =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseFreq",0);
	mNoiseColor =  gpBase->mpGameCfg->GetColor("Player_General","Hurt_NoiseColor",cColor(0));

	cGui *pGui = gpBase->mpEngine->GetGui();
	mvNoiseGfx.resize(8);
	for(size_t i=0; i<mvNoiseGfx.size(); ++i)
	{
		mvNoiseGfx[i] = pGui->CreateGfxTexture("hud_hurt_noise0"+cString::ToString((int)i)+".dds", eGuiMaterial_Modulative,eTextureType_2D);
	}
	
	//mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Modulative);
}

cLuxPlayerHurt::~cLuxPlayerHurt()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::Reset()
{
	mfAlpha = 0;
	mfPantCount =0;
	mfPantPosAdd = 0;
	mfPantPosAddVel = 0;
	mfPantPosAddDir = 1.0f;
	mlCurrentNoise = 0;
	mfNoiseUpdateCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::Update(float afTimeStep)
{
	float fRegain;
	if(ohpl_ga2_hurt_regain(mpPlayer->GetHealth(), mfHealthRegainLimit, mfHealthRegainSpeed, afTimeStep, &fRegain))
	{
		mpPlayer->AddHealth(fRegain);
	}
	OhplGa2Hurt s = {mfEffectStartHealth, mfMinMoveMul, mfMaxPantCount, mfPantSpeed, mfPantSize, mlCurrentNoise, mfNoiseUpdateCount, mfNoiseFreq,
		mfPantCount, mfPantPosAdd, mfPantPosAddVel, mfPantPosAddDir, mfAlpha, (int)mvNoiseGfx.size()};
	float fSpeed = mpPlayer->GetCharacterBody()->GetVelocity(afTimeStep).Length();
	OhplGa2Ops ops;
	ohpl_ga2_hurt_update(&s, mpPlayer->GetHealth(), mpPlayer->IsDead(), fSpeed, afTimeStep, &ops);
	mlCurrentNoise = s.mlCurrentNoise;
	mfNoiseUpdateCount = s.mfNoiseUpdateCount;
	mfPantCount = s.mfPantCount;
	mfPantPosAdd = s.mfPantPosAdd;
	mfPantPosAddVel = s.mfPantPosAddVel;
	mfPantPosAddDir = s.mfPantPosAddDir;
	mfAlpha = s.mfAlpha;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2SetHurtMoveSpeedMul: mpPlayer->SetHurtMoveSpeedMul(op.a); break;
		case kOhplGa2PlayHurtPant: gpBase->mpHelpFuncs->PlayGuiSoundData("hurt_pant",eSoundEntryType_Gui, op.a); break;
		case kOhplGa2SetHurtHeadAdd: mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Hurt, cVector3f(0,op.a,0)); break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::OnDraw(float afFrameTime)
{
	if(mfAlpha <=0) return;

	cVector2f vNoiseSize(256, 256);
	
	cVector2l vCount = cVector2l( (int)(gpBase->mvHudVirtualSize.x / vNoiseSize.x)+1, (int)(gpBase->mvHudVirtualSize.y / vNoiseSize.y)+1);

	cVector3f vPos = gpBase->mvHudVirtualStartPos + cVector3f(0,0,-1);

	cColor col = mNoiseColor;
	col.a = mfAlpha*mfNoiseAlpha;
	for(int y=0; y<vCount.y; ++y)
	{
		for(int x=0; x<vCount.x; ++x)
		{
			gpBase->mpGameHudSet->DrawGfx(mvNoiseGfx[mlCurrentNoise], vPos, vNoiseSize, cColor(1,1-mfAlpha, 1-mfAlpha));
			vPos.x += vNoiseSize.x;
		}
		vPos.x = gpBase->mvHudVirtualStartPos.x;
		vPos.y += vNoiseSize.y;
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER FLASHBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerFlashback::cLuxPlayerFlashback (cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerFlashback")
{
	mfRadialBlurSize = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurSize", 0.12f);
	mfRadialBlurStartDist = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurStartDist", 0.4f);
	mfWorldSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackWorldSoundVolume", 0.4f);
	mfMoveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackMoveSpeedMul", 1.0f);
	mfRunSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRunSpeedMul", 1.0f);

	Reset();

}
cLuxPlayerFlashback::~cLuxPlayerFlashback ()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Reset()
{
	mfFlashDelay =0;
	mbActive = false;
	mlstFlashbackQueue.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Start(const tString &asFlashbackFile, const tString &asCallback)
{
	if(gpBase->mpDebugHandler->GetDisableFlashBacks()) return;

	ProgLog(eLuxProgressLogLevel_Medium, "Starting flashback "+ asFlashbackFile);
	
	if(mbActive)
	{
		mlstFlashbackQueue.push_back(cLuxFlashbackData(asFlashbackFile, asCallback) );
		return;
	}

	//Disable enemies
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);

	mfFlashDelay = 0.5f; //Show flash effect after a little delay
	gpBase->mpHelpFuncs->PlayGuiSoundData("flashback_flash", eSoundEntryType_Gui);
	
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	pMap->AddCompletionAmount(gpBase->mpCompletionCountHandler->mlFlashbackCompletionValue, 2.5f);

	mfFlashbackStartCount = 1.5f;
	msFlashbackFile = asFlashbackFile;
	msCallback = asCallback;

	//World sounds
	gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(mfWorldSoundVolume, (1-mfWorldSoundVolume) / 1.5f,eSoundEntryType_World,eLuxGlobalVolumeType_Flashback,false);

	mbActive = true;
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Update(float afTimeStep)
{
	OhplGa2Flashback s = {mfRadialBlurSize, mfRadialBlurStartDist, mfMoveSpeedMul, mfRunSpeedMul, mfFlashDelay, mbActive, mfFlashbackStartCount};
	bool bVoiceDone = mbActive ? gpBase->mpEffectHandler->GetPlayVoice()->VoiceDonePlaying() : false;
	OhplGa2Ops ops;
	ohpl_ga2_flashback_update(&s, mlstFlashbackQueue.empty()==false, bVoiceDone, msCallback != "", afTimeStep, &ops);
	mfFlashDelay = s.mfFlashDelay;
	mbActive = s.mbActive != 0;
	mfFlashbackStartCount = s.mfFlashbackStartCount;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2FlashbackStartQueued:
		{
			cLuxFlashbackData data = mlstFlashbackQueue.front();
			mlstFlashbackQueue.pop_front();
			Start(data.msFile, data.msCallback);
			break;
		}
		case kOhplGa2FlashMid:
			gpBase->mpEffectHandler->GetFlash()->Start(0.5f, 0.5f, 2.5f);
			gpBase->mpPlayer->SetEventMoveSpeedMul(op.a);
			gpBase->mpPlayer->SetEventRunSpeedMul(op.b);
			break;
		case kOhplGa2FlashbackBegin:
			gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(1, op.d);
			gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(op.a);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(op.b, op.c);
			LoadAndPlayFlashbackFile(msFlashbackFile);
			break;
		case kOhplGa2FlashbackEnd:
		{
			gpBase->mpEffectHandler->GetFlash()->Start(1.0f, 0.1f, 2.5f);
			gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(0, op.a);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, op.b);
			gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(1, op.a,eSoundEntryType_World,eLuxGlobalVolumeType_Flashback,true);
			gpBase->mpPlayer->SetEventMoveSpeedMul(1.0f);
			gpBase->mpPlayer->SetEventRunSpeedMul(1.0f);
			cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
			if(op.i) pMap->RunScript(msCallback + "()");
			break;
		}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::OnDraw(float afFrameTime)
{
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::LoadAndPlayFlashbackFile(const tString& asFlashbackFile)
{
	tString sFile = cString::SetFileExt(asFlashbackFile,"flash");
	cResources *pResources = gpBase->mpEngine->GetResources();

	iXmlDocument *pXmlDoc = pResources->LoadXmlDocument(sFile);
	if(pXmlDoc==NULL)
	{
		Error("Could not load flashback file: '%s'\n", sFile.c_str());
		return;
	}

	cXmlElement *pVoicesElem = pXmlDoc->GetFirstElement("Voices");
	if(pVoicesElem==NULL)
	{
		Error("Could not find voice element in flashback file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pXmlDoc);
		return;
	}

	cXmlNodeListIterator it = pVoicesElem->GetChildIterator();
	while(it.HasNext())
	{
		cXmlElement *pChildElem = it.Next()->ToElement();

		tString sVoiceFile = pChildElem->GetAttributeString("VoiceSound","");
		tString sEffectFile = pChildElem->GetAttributeString("EffectSound","");
		tString sTextCat = pChildElem->GetAttributeString("TextCat","");
		tString sTextEntry = pChildElem->GetAttributeString("TextEntry","");

		gpBase->mpEffectHandler->GetPlayVoice()->AddVoice(sVoiceFile, sEffectFile, sTextCat, sTextEntry, false,0,0,0);
	}

	pResources->DestroyXmlDocument(pXmlDoc);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HEAD MOVE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLookAt::cLuxPlayerLookAt(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLookAt")
{
	Reset();	
}

//-----------------------------------------------------------------------


cLuxPlayerLookAt::~cLuxPlayerLookAt()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::Update(float afTimeStep)
{
	if(mbActive==false) return;
	cCamera *pCam = mpPlayer->GetCamera();
	cVector3f vGoalAngle = cMath::GetAngleFromPoints3D(pCam->GetPosition(),mvTargetPos);
	OhplGa2LookAt s = {mvCurrentSpeed, mfSpeedMul, mfMaxSpeed};
	OhplGa2Ops ops;
	ohpl_ga2_lookat_update(&s, &vGoalAngle, pCam->GetPitch(), pCam->GetYaw(), afTimeStep, &ops);
	mvCurrentSpeed = s.mvCurrentSpeed;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2LookAddPitch: pCam->AddPitch(op.a); break;
		case kOhplGa2LookAddYaw: pCam->AddYaw(op.a); break;
		case kOhplGa2LookSyncBodyYaw: mpPlayer->GetCharacterBody()->SetYaw(pCam->GetYaw()); break;
		case kOhplGa2LookAtTarget: gpBase->mpMapHandler->GetCurrentMap()->RunScript(msAtTargetCallback+"()"); break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::Reset()
{
	mbActive = false;
	mfMaxSpeed = 9999.0f;
	mfSpeedMul = 1.0f;
	mvTargetPos = cVector3f(0,1,0);
	mvCurrentSpeed =0;

	msAtTargetCallback = "";

	mfDestFovMul = 1.0f;
	mfFov = 1.0f;
	mfFovSpeed = 1.0f;
	mfFovMaxSpeed = 1.0f;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetTarget(const cVector3f &avTargetPos, float afSpeedMul, float afMaxSpeed, const tString& asAtTargetCallback)
{
	mvTargetPos = avTargetPos;
	mfSpeedMul = afSpeedMul;
	mfMaxSpeed = afMaxSpeed;

	msAtTargetCallback = asAtTargetCallback;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetActive(bool abX)
{
	mbActive = abX;
	if(mbActive==false)
	{
		mvCurrentSpeed =0;
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SANITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSanity::cLuxPlayerSanity(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSanity")
{
	mfHitZoomInSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInSpeed",0);
	mfHitZoomOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomOutSpeed",0);
	mfHitZoomInFOVMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInFOVMul",0);
	mfHitZoomInAspectMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInAspectMul",0);

	mfSanityRegainSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainSpeed",0);
	mfSanityRegainLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainLimit",0);

	mfSanityVeryLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityVeryLowLimit",0);
	mfSanityEffectsStart = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityEffectsStart",0);

	mfSanityWaveAlphaMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveAlphaMul",0);
	mfSanityWaveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveSpeedMul",0);

	mfSanityLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimit",0);
	mfSanityLowLimitMaxTime = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimitMaxTime",0);
	mfSanityLowNewSanityAmount = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowNewSanityAmount",0);

	mfCheckNearEnemyInterval = gpBase->mpGameCfg->GetFloat("Player_Sanity","CheckNearEnemyInterval",0);
	mfNearEnemyDecrease = gpBase->mpGameCfg->GetFloat("Player_Sanity","NearEnemyDecrease",0);
	mfNearCritterDecrease = gpBase->mpGameCfg->GetFloat("Player_Sanity","NearCritterDecrease",0);

	Reset();
}

//-----------------------------------------------------------------------

cLuxPlayerSanity::~cLuxPlayerSanity()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::Reset()
{
	mfHitAlpha =0;
	mbHitActive = false;
	mfSanityLostCount =0;
	mfPantCount =1;
	mfCheckEnemySeenCount =0;

	mfT=0;
	mfInsaneWaveAlpha =0;

	mfSanityDrainCount =0;
	mfSanityDrainVolume =0;
	mfSanityHeartbeatCount =0;

	mfSeenEnemyCount =0;
	mbEnemyIsSeen = false;

	mbSanityEffectUpdated = false;

	mfAtLowSanityCount =0;

	mfShowHintTimer =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::StartHit()
{
	mbHitActive = true;

	gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_damage", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::SetSanityLost()
{
	mfSanityLostCount = 1.0f;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::Update(float afTimeStep)
{
	OhplGa2Sanity s;
	OhplGa2Ops ops;
	ToRust(s);
	ohpl_ga2_sanity_begin(&s, mpPlayer->GetSanity(), afTimeStep, &ops);
	FromRust(s);
	Replay(ops);
	UpdateCheckEnemySeen(afTimeStep);
	UpdateEnemySeenEffect(afTimeStep);
	UpdateHit(afTimeStep);
	UpdateInsaneEffects(afTimeStep);
}

void cLuxPlayerSanity::ToRust(OhplGa2Sanity &s)
{
	s.mfHitAlpha = mfHitAlpha;
	s.mbHitActive = mbHitActive;
	s.mfSanityLostCount = mfSanityLostCount;
	s.mfAtLowSanityCount = mfAtLowSanityCount;
	s.mbHitIsUpdated = mbHitIsUpdated;
	s.mbSanityLostIsUpdated = mbSanityLostIsUpdated;
	s.mfPantCount = mfPantCount;
	s.mfCheckEnemySeenCount = mfCheckEnemySeenCount;
	s.mbSanityEffectUpdated = mbSanityEffectUpdated;
	s.mfT = mfT;
	s.mfInsaneWaveAlpha = mfInsaneWaveAlpha;
	s.mfSanityDrainCount = mfSanityDrainCount;
	s.mfSanityDrainVolume = mfSanityDrainVolume;
	s.mfSanityHeartbeatCount = mfSanityHeartbeatCount;
	s.mfSeenEnemyCount = mfSeenEnemyCount;
	s.mbEnemyIsSeen = mbEnemyIsSeen;
	s.mfShowHintTimer = mfShowHintTimer;
	s.mfHitZoomInSpeed = mfHitZoomInSpeed;
	s.mfHitZoomOutSpeed = mfHitZoomOutSpeed;
	s.mfHitZoomInFOVMul = mfHitZoomInFOVMul;
	s.mfHitZoomInAspectMul = mfHitZoomInAspectMul;
	s.mfSanityRegainSpeed = mfSanityRegainSpeed;
	s.mfSanityRegainLimit = mfSanityRegainLimit;
	s.mfSanityVeryLowLimit = mfSanityVeryLowLimit;
	s.mfSanityEffectsStart = mfSanityEffectsStart;
	s.mfSanityWaveAlphaMul = mfSanityWaveAlphaMul;
	s.mfSanityWaveSpeedMul = mfSanityWaveSpeedMul;
	s.mfSanityLowLimit = mfSanityLowLimit;
	s.mfSanityLowLimitMaxTime = mfSanityLowLimitMaxTime;
	s.mfSanityLowNewSanityAmount = mfSanityLowNewSanityAmount;
	s.mfCheckNearEnemyInterval = mfCheckNearEnemyInterval;
}

void cLuxPlayerSanity::FromRust(const OhplGa2Sanity &s)
{
	mfHitAlpha = s.mfHitAlpha;
	mbHitActive = s.mbHitActive != 0;
	mfSanityLostCount = s.mfSanityLostCount;
	mfAtLowSanityCount = s.mfAtLowSanityCount;
	mbHitIsUpdated = s.mbHitIsUpdated != 0;
	mbSanityLostIsUpdated = s.mbSanityLostIsUpdated != 0;
	mfPantCount = s.mfPantCount;
	mfCheckEnemySeenCount = s.mfCheckEnemySeenCount;
	mbSanityEffectUpdated = s.mbSanityEffectUpdated != 0;
	mfT = s.mfT;
	mfInsaneWaveAlpha = s.mfInsaneWaveAlpha;
	mfSanityDrainCount = s.mfSanityDrainCount;
	mfSanityDrainVolume = s.mfSanityDrainVolume;
	mfSanityHeartbeatCount = s.mfSanityHeartbeatCount;
	mfSeenEnemyCount = s.mfSeenEnemyCount;
	mbEnemyIsSeen = s.mbEnemyIsSeen != 0;
	mfShowHintTimer = s.mfShowHintTimer;
}

void cLuxPlayerSanity::Replay(const OhplGa2Ops &ops)
{
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2SanSetSanity: mpPlayer->SetSanity(op.a); break;
		case kOhplGa2SanLower: mpPlayer->LowerSanity(op.a, true); break;
		case kOhplGa2SanEnemyUnseenBlur: gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, op.a); break;
		case kOhplGa2SanHintEnemySeen: gpBase->mpHintHandler->Add("EnemySeen", kTranslate("Hints", "EnemySeen"), 0); break;
		case kOhplGa2SanBlurEnemy:
			gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(op.a);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(op.b, 10.0f);
			break;
		case kOhplGa2SanImageTrail: gpBase->mpEffectHandler->GetImageTrail()->FadeTo(op.a, op.b); break;
		case kOhplGa2SanFadeAspect: mpPlayer->FadeAspectMulTo(op.a, op.b); break;
		case kOhplGa2SanFadeFov: mpPlayer->FadeFOVMulTo(op.a, op.b); break;
		case kOhplGa2SanInsanityActive: gpBase->mpPostEffectHandler->GetInsanity()->SetActive(op.i != 0); break;
		case kOhplGa2SanInsanityFx:
			gpBase->mpPostEffectHandler->GetInsanity()->SetWaveAlpha(op.a);
			gpBase->mpPostEffectHandler->GetInsanity()->SetWaveSpeed(op.b);
			gpBase->mpPostEffectHandler->GetInsanity()->SetZoomAlpha(op.c);
			break;
		case kOhplGa2SanHeartbeat: gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_heartbeat", eSoundEntryType_Gui, op.a); break;
		case kOhplGa2SanDrain:
		{
			tString sSoundFile = op.i == 0 ? "sanity_drain_low" : (op.i == 1 ? "sanity_drain_med" : "sanity_drain_high");
			gpBase->mpHelpFuncs->PlayGuiSoundData(sSoundFile, eSoundEntryType_Gui, op.a);
			break;
		}
		case kOhplGa2SanHintLow: gpBase->mpHintHandler->Add("SanityLow", kTranslate("Hints", "SanityLow"), 0); break;
		case kOhplGa2SanRandPant: mfPantCount = cMath::RandRectf(op.a, op.b); break;
		case kOhplGa2SanPant: gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_pant", eSoundEntryType_Gui); break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::OnDraw(float afFrameTime)
{
	
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateInsaneEffects(float afTimeStep)
{
	OhplGa2Sanity s;
	OhplGa2Ops ops;
	ToRust(s);
	ohpl_ga2_sanity_effects(&s, mpPlayer->GetSanity(), mpPlayer->IsDead(), afTimeStep, &ops);
	FromRust(s);
	Replay(ops);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateCheckEnemySeen(float afTimeStep)
{
	/////////////////////////////////////
	// Check if it is time for a check!
	{
		OhplGa2Sanity s;
		ToRust(s);
		bool bDue = ohpl_ga2_sanity_scan_due(&s, afTimeStep);
		FromRust(s);
		if(bDue==false) return;
	}

	// Init vars
	bool bSeenEnemy = false;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	float fMaxRangeSqrt = 35 * 35;

	cCamera *pCam = mpPlayer->GetCamera();
	cVector3f vPlayerHeadPos = pCam->GetPosition();


	cVector3f vRight = pCam->GetRight();
	cVector3f vUp = pCam->GetUp();
	
	/////////////////////////////////////
	// Iterate critters
	float fMinCritterDistSqrt = 3.0f * 3.0f;
	bool bNearCritter = false;
	cLuxEntityIterator entIt = pMap->GetEntityIterator();
	while(entIt.HasNext())
	{
		iLuxEntity *pEntity = entIt.Next();
		if(pEntity->IsActive()==false)continue;
		if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;

		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		if(pProp->GetPropType() != eLuxPropType_Critter) continue;

		iLuxProp_CritterBase *pCritter = static_cast<iLuxProp_CritterBase*>(pProp);
		
		if(pCritter->CausesSanityDecrease()==false) continue;
		
		float fDistSqrt = cMath::Vector3DistSqr(pProp->GetBody(0)->GetLocalPosition(), vPlayerHeadPos); 
		if(fDistSqrt > fMinCritterDistSqrt) continue;

		bNearCritter = true;
		break;
	}

	if(bNearCritter)
	{
		mpPlayer->LowerSanity(mfNearCritterDecrease, true);
	}
	
	/////////////////////////////////////
	// Iterate enemies
	cLuxEnemyIterator it = pMap->GetEnemyIterator();
    while(it.HasNext())
	{
		iLuxEnemy *pEnemy = it.Next();
		pEnemy->SetIsSeenByPlayer(false);
		
		if(pEnemy->IsActive()==false) continue;
		if(pEnemy->CausesSanityDecrease()==false) continue;

		iCharacterBody *pCharBody = pEnemy->GetCharacterBody();

		//////////////////////////////
		//Check so enemy is in range
		float fDistSqrt = cMath::Vector3DistSqr(pCharBody->GetPosition(), vPlayerHeadPos);
		if(fDistSqrt > fMaxRangeSqrt) continue;
		
		//////////////////////////////
		//Check so enemy is in FOV
		if( pCam->GetFrustum()->CollideBoundingVolume(pCharBody->GetCurrentBody()->GetBoundingVolume()) == eCollision_Outside)
		{
			continue;
		}
		
		//////////////////////////////
		//Cast rays
		cVector3f vHalfSize = pCharBody->GetSize()*0.5f;
		cVector3f vPosAdd[5] = {
			cVector3f(0),
			vRight*vHalfSize.x,
			vRight*vHalfSize.x*-1,
			vUp*vHalfSize.y*0.8f,
			vUp*vHalfSize.y*-0.8f,
		};

		int lCount =0;
		for(int i=0; i<5; ++i)
		{
			if(gpBase->mpMapHelper->CheckLineOfSight(vPlayerHeadPos, pCharBody->GetPosition()+vPosAdd[i], false))
			{
				lCount++;
				if(lCount >=2)
				{
					bSeenEnemy = true;
					pEnemy->SetIsSeenByPlayer(true);
					break;
				}
			}
		}
		
		//if(bSeenEnemy) break; No break, since we check visibility for all enemies.
	}

	/////////////////////////////////////
	// If seen, lower sanity and increase seen count
	{
		OhplGa2Sanity s;
		OhplGa2Ops ops;
		ToRust(s);
		ohpl_ga2_sanity_scan_result(&s, bSeenEnemy, mfNearEnemyDecrease, &ops);
		FromRust(s);
		Replay(ops);
	}
}

//-----------------------------------------------------------------------


void cLuxPlayerSanity::UpdateHit(float afTimeStep)
{
	OhplGa2Sanity s;
	OhplGa2Ops ops;
	ToRust(s);
	ohpl_ga2_sanity_hit(&s, mpPlayer->IsDead(), afTimeStep, &ops);
	FromRust(s);
	Replay(ops);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateEnemySeenEffect(float afTimeStep)
{
	OhplGa2Sanity s;
	OhplGa2Ops ops;
	ToRust(s);
	ohpl_ga2_sanity_enemy_seen(&s, afTimeStep, &ops);
	FromRust(s);
	Replay(ops);
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------



//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER LANTERN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLantern::cLuxPlayerLantern(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLantern")
{
	mDefaultColor = gpBase->mpGameCfg->GetColor("Player_Lantern","Color",cColor(0));
	mfRadius = gpBase->mpGameCfg->GetFloat("Player_Lantern","Radius",0);
	msGobo = gpBase->mpGameCfg->GetString("Player_Lantern","Gobo","");
	mvLocalOffset = gpBase->mpGameCfg->GetVector3f("Player_Lantern","LocalOffset",0);
	mbCastShadows = gpBase->mpGameCfg->GetBool("Player_Lantern","CastShadows",false);
	mfLowerOilSpeed = gpBase->mpGameCfg->GetFloat("Player_Lantern","LowerOilSpeed",0);
	mfFadeLightOilAmount = gpBase->mpGameCfg->GetFloat("Player_Lantern","FadeLightOilAmount",0);

	msOutOfOilSound = gpBase->mpGameCfg->GetString("Player_Lantern","OutOfOilSound","");
	msDisabledSound = gpBase->mpGameCfg->GetString("Player_Lantern","DisabledSound","");
	msTurnOnSound = gpBase->mpGameCfg->GetString("Player_Lantern","TurnOnSound","");
	msTurnOffSound = gpBase->mpGameCfg->GetString("Player_Lantern","TurnOffSound","");

	Reset();
}

cLuxPlayerLantern::~cLuxPlayerLantern()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::OnStart()
{

}
void cLuxPlayerLantern::Reset()
{
	mbDisabled = false;
	mbActive = false;
	mpLight = NULL;
	mfAlpha =0;
}

//-----------------------------------------------------------------------


void cLuxPlayerLantern::Update(float afTimeStep)
{
	OhplGa2Lantern s = {{mDefaultColor.r, mDefaultColor.g, mDefaultColor.b, mDefaultColor.a}, mfFadeLightOilAmount, mfLowerOilSpeed, mbActive, mfAlpha};
	OhplGa2Ops ops;
	bool bEmotionFlash = mbActive ? gpBase->mpEffectHandler->GetEmotionFlash()->IsActive() : false;
	if(ohpl_ga2_lantern_update(&s, gpBase->mpPlayer->GetLampOil(), bEmotionFlash, afTimeStep, &ops) == false) return;
	mfAlpha = s.mfAlpha;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2LanternColor: mpLight->SetDiffuseColor(cColor(op.a, op.b, op.c, op.d)); break;
		case kOhplGa2LanternOutOfOil:
			gpBase->mpHelpFuncs->PlayGuiSoundData(msOutOfOilSound, eSoundEntryType_Gui);
			SetActive(false, true);
			break;
		case kOhplGa2LanternSetOil: mpPlayer->SetLampOil(op.a); break;
		}
	}
	// Update light matrix
	if(mpLight)
	{
		cCamera *pCam = mpPlayer->GetCamera();
		cVector3f vCamRotation( pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
		cMatrixf mtxCamTransform = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
		mtxCamTransform.SetTranslation(pCam->GetPosition());
		cMatrixf mtxLocalLight = cMath::MatrixTranslate(mvLocalOffset);
		mpLight->SetMatrix(cMath::MatrixMul(mtxCamTransform, mtxLocalLight));
	}
}

//-----------------------------------------------------------------------
void cLuxPlayerLantern::OnMapEnter(cLuxMap *apMap)
{
	
}

void cLuxPlayerLantern::OnMapLeave(cLuxMap *apMap)
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	cCamera *pCam = mpPlayer->GetCamera();

	mpLight = pWorld->CreateLightPoint("PlayerLantern",msGobo,false);
	mpLight->SetDiffuseColor(cColor(0,0));
	mpLight->SetRadius(mfRadius);
	
	mpLight->SetIsSaved(false);

	if(mbActive)
	{
		mbActive=false;
		SetActive(true, false);
	}
}

void cLuxPlayerLantern::DestroyWorldEntities(cLuxMap *apMap)
{
	if(mpLight) apMap->GetWorld()->DestroyLight(mpLight);
	mpLight = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::SetActive(bool abX, bool abUseEffects, bool abCheckForOilAndItems, bool abCheckIfAllowed)
{
	OhplGa2LanternSet p;
	p.mbActive = abX;
	p.mbUseEffects = abUseEffects;
	p.mbCheckOilItems = abCheckForOilAndItems;
	p.mbCheckAllowed = abCheckIfAllowed;
	p.mbCurrent = mbActive;
	p.mbAllowed = abCheckIfAllowed ? mpPlayer->GetCurrentStateData()->AllowLantern() : true;
	p.mbHasItem = abCheckForOilAndItems ? gpBase->mpInventory->HasItemOfType(eLuxItemType_Lantern) : true;
	p.mbDisabled = mbDisabled;
	p.mfOil = mpPlayer->GetLampOil();
	OhplGa2Ops ops;
	ohpl_ga2_lantern_set_active(&p, &ops);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2LanternHintNoItem: gpBase->mpHintHandler->Add("LanternNoItem", kTranslate("Hints", "LanternNoItem"), 0); break;
		case kOhplGa2LanternHintNoOil: gpBase->mpHintHandler->Add("LanternNoOil", kTranslate("Hints", "LanternNoOil"), 0); break;
		case kOhplGa2LanternSoundOut: gpBase->mpHelpFuncs->PlayGuiSoundData(msOutOfOilSound, eSoundEntryType_Gui); break;
		case kOhplGa2LanternSetActive: mbActive = op.i != 0; break;
		case kOhplGa2LanternSoundOn: gpBase->mpHelpFuncs->PlayGuiSoundData(msTurnOnSound, eSoundEntryType_Gui); break;
		case kOhplGa2LanternSoundOff: gpBase->mpHelpFuncs->PlayGuiSoundData(msTurnOffSound, eSoundEntryType_Gui); break;
		case kOhplGa2LanternHand:
			if(op.i) mpPlayer->GetHands()->SetActiveHandObject("lantern");
			else mpPlayer->GetHands()->SetActiveHandObject("");
			break;
		case kOhplGa2LanternCallback:
		{
			cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
			if(pMap->GetLanternLitCallback()!="")
			{
				pMap->RunScript(pMap->GetLanternLitCallback()+"(" + (op.i ? "true" : "false") + ")" );
			}
			break;
		}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::SetDisabled(bool abX)
{
	mbDisabled = abX;

	if(mbDisabled)
		SetActive(false, true);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER DEATH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerDeath::cLuxPlayerDeath(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "PlayerDeath")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	mpWhiteModGfx = pGui->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Modulative);
	
	mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAdd",0);
	mfHeightAddGoalCrouch = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddCrouch",0);
	mfFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_General","Death_FadeTime",1000);
	
	if(mpPlayer->UsePermaDeath()) mfFadeOutTime /= 2.0f;

	mfMaxSanityGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxSanityGain",0);
	mfMaxHealthGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxHealthGain",0);
	mfMaxOilGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxOilGain",0);

	mfMinSanityGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinSanityGain",0);
	mfMinHealthGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinHealthGain",0);
	mfMinOilGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinOilGain",0);

	mfHeightAddSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddSpeed",0);
	mfRollSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_RollSpeed",0);

	if(mpPlayer->UsePermaDeath()) mfHeightAddSpeed *= 1.2f;
	if(mpPlayer->UsePermaDeath()) mfRollSpeed *= 1.2f;

	msStartSound = gpBase->mpGameCfg->GetString("Player_General","Death_StartSound", "");
	msAwakenSound = gpBase->mpGameCfg->GetString("Player_General","Death_AwakenSound", "");

	mpFont = NULL;

	mFlashOscill.SetUp(0,1,0,0.5,0.5);

	mbToMainMenu = false;
}

cLuxPlayerDeath::~cLuxPlayerDeath()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::LoadFonts()
{
	mpFont = LoadFont("game_default.fnt");
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::LoadUserConfig()
{
	mbShowHint = gpBase->mpUserConfig->GetBool("Game", "ShowDeathHints", true);
}

void cLuxPlayerDeath::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowDeathHints", mbShowHint);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Reset()
{
	mfHeightAdd =0;
	mfRoll =0;
	mfTextAlpha1 =0;
	mfTextAlpha2 =0;
	mfWhiteCount =0;

	mpVoiceEntry = NULL;

	mbSkipStartSound = false;

	msCurrentHintText = _W("");

	msHintCat = "";
	msHintEntry = "";

	mfFadeAlpha =0;
	mfTextOnScreenCount =0;

	mlState =0;

	mfT =0;

	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Start()
{
	if(mbActive) return;

	mbActive = true;

	//////////////////////////////////
	//Progress log
	gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_High, "Player died!");

	//////////////////////////////////
	//Set some player stuff
	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	mpPlayer->ChangeState(eLuxPlayerState_Normal);
	mpPlayer->ChangeMoveState(eLuxMoveState_Normal);
	mpPlayer->GetHelperLantern()->SetActive(false,false, false);
	mpPlayer->SetCurrentHandObjectDrawn(false);
	mpPlayer->GetInsanityCollapse()->Stop();

	//////////////////////////////////
	//Text
	msCurrentHintText = msHintCat != "" ? kTranslate(msHintCat, msHintEntry) : kTranslate("Hints", "DefaultDeath");

	//////////////////////////////////
	// HARDMODE
	if (gpBase->mbHardMode && (msHintEntry != "DeathGrunt_22_Chancel"))
	{
		msCurrentHintText = kTranslate("Hints", "HardModeDeath");
	}

	//////////////////////////////////
	//Sound
	for(int i=0; i<=gpBase->mpMusicHandler->GetMaxPrio(); ++i)
	{
		gpBase->mpMusicHandler->Stop(0.2f,i);
	}

	if(mbSkipStartSound == false)
		gpBase->mpHelpFuncs->PlayGuiSoundData(msStartSound, eSoundEntryType_World);
	mbSkipStartSound = false;
	
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(0, 0.15f,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	pSoundHandler->FadeGlobalSpeed(0.5f, 0.125f,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	
	///////////////////////////
	// Broadcast to all enemies
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_PlayerDead, false,0,0);

	///////////////////////////
	// Reset variables
	mfHeightAdd =0;
	mfRoll =0;

	mfMinHeightAdd = mfHeightAddGoal;

	if(mpPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetCurrentMoveStateData());
		if(pNormalMove->IsCrouching()) mfMinHeightAdd = mfHeightAddGoalCrouch;
	}

	mlState =0;

	mfFadeAlpha =0;
	mfWhiteCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Update(float afTimeStep)
{
	if(mbActive==false) return;
	mFlashOscill.Update(afTimeStep);
	OhplGa2Ops ops;
	{
		OhplGa2Death s;
		ToRust(s);
		ohpl_ga2_death_phase1(&s, mpPlayer->UsePermaDeath(), afTimeStep, &ops);
		FromRust(s);
		Replay(ops);
	}
	{
		OhplGa2Death s;
		bool bHardActive = gpBase->mbHardMode && gpBase->mpPlayer->IsActive();
		ToRust(s);
		ohpl_ga2_death_phase2(&s, bHardActive, afTimeStep, &ops);
		FromRust(s);
		Replay(ops);
	}
	bool bVoiceGone = false;
	if(mlState == 4)
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		bVoiceGone = mpVoiceEntry==NULL || pSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryId)==false;
	}
	ohpl_ga2_death_phase3(mlState, bVoiceGone, &ops);
	Replay(ops);
}

void cLuxPlayerDeath::ToRust(OhplGa2Death &s)
{
	s.mbActive = mbActive;
	s.mbShowHint = mbShowHint;
	s.mlState = mlState;
	s.mfHeightAddSpeed = mfHeightAddSpeed;
	s.mfRollSpeed = mfRollSpeed;
	s.mfFadeOutTime = mfFadeOutTime;
	s.mfHeightAdd = mfHeightAdd;
	s.mfRoll = mfRoll;
	s.mfT = mfT;
	s.mfMinHeightAdd = mfMinHeightAdd;
	s.mfFadeAlpha = mfFadeAlpha;
	s.mfTextAlpha1 = mfTextAlpha1;
	s.mfTextOnScreenCount = mfTextOnScreenCount;
	s.mfWhiteCount = mfWhiteCount;
}

void cLuxPlayerDeath::FromRust(const OhplGa2Death &s)
{
	mbActive = s.mbActive != 0;
	mlState = s.mlState;
	mfHeightAdd = s.mfHeightAdd;
	mfRoll = s.mfRoll;
	mfT = s.mfT;
	mfFadeAlpha = s.mfFadeAlpha;
	mfTextAlpha1 = s.mfTextAlpha1;
	mfTextOnScreenCount = s.mfTextOnScreenCount;
	mfWhiteCount = s.mfWhiteCount;
}

void cLuxPlayerDeath::Replay(const OhplGa2Ops &ops)
{
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2SetHeadAdd: mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,op.a,0)); break;
		case kOhplGa2FadeRoll: mpPlayer->FadeRollTo(op.a, op.b, op.c); break;
		case kOhplGa2DeathPermaSound:
		{
			cLuxSoundExtraData extraData;
			if(gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->GetCurrentPermaDeathSound(), eSoundEntryType_Gui, 1, eSoundEntityType_Main, true, &extraData))
			{
				mpVoiceEntry = extraData.mpSoundEntry;
				mlVoiceEntryId = mpVoiceEntry->GetId();
			}
			break;
		}
		case kOhplGa2DeathResetGame: ResetGame(); break;
		case kOhplGa2DeathFlash: gpBase->mpEffectHandler->GetFlash()->Start(0.7f, 1.2f, 2.5f); break;
		case kOhplGa2DeathAwakenSound: gpBase->mpHelpFuncs->PlayGuiSoundData(msAwakenSound, eSoundEntryType_Gui); break;
		case kOhplGa2DeathReset:
		{
			tString sTempCat = msHintCat;
			tString sTempEntry = msHintEntry;
			Reset();
			msHintCat = sTempCat;
			msHintEntry = sTempEntry;
			break;
		}
		case kOhplGa2DeathAutoload:
			if (gpBase->mpSaveHandler->AutoLoad(true) == false)
			{
				mbToMainMenu = true;
			}
			break;
		case kOhplGa2DeathExit: gpBase->mpEngine->Exit(); break;
		}
	}
}


void cLuxPlayerDeath::PostUpdate(float afTimeStep)
{
	///////////////////////////
	// HARDMODE
	if (gpBase->mbHardMode == false) return;

	///////////////////////////
	// Wants to go to main menu, 
	// cant be in update since the playerclass will go bananas
	if (mbToMainMenu == false) return;
	
	//////////////////////////
	// Set MainMenu container
	gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
	gpBase->mpLoadScreenHandler->DrawMenuScreen();

	//Reset game
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
	gpBase->SetCustomStory(NULL);

	//Start up menu again
	gpBase->mpMainMenu->OnLeaveContainer("");
	gpBase->mpMainMenu->OnEnterContainer("");

	mbToMainMenu = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::OnDraw(float afFrameTime)
{
	if(mbActive==false) return;

	gpBase->mpGameHudSet->DrawGfx(mpWhiteModGfx,gpBase->mvHudVirtualStartPos+ cVector3f(0,0,3), gpBase->mvHudVirtualSize,cColor(1-mfFadeAlpha,1));

	if(mbShowHint==false) return; //Skip death hint

	cVector2f vFontSize = 32;
	float fSizeMul = 1.5f;
	float fSizeMulExtra =  (1- mfTextAlpha1)*4;
	float fMul = fSizeMul + fSizeMulExtra;

	tWStringVec vRows;
	mpFont->GetWordWrapRows(550,vFontSize.y, vFontSize,msCurrentHintText,&vRows);

	float fY = 300;
	for(size_t row=0; row<vRows.size(); ++row)
	{
		tWString &sStr = vRows[row];
		float fSize = mpFont->GetLength(vFontSize* fMul, sStr.c_str());
		float fX = 400 - (fSize/2.0f);

		for(size_t i=0;i<sStr.length(); ++i)
		{
			float fTAdd = 0.3f * (float)i;
			float fYAdd = sin(mfT*0.5f + fTAdd) * 5.0f + cos(mfT*0.97f - fTAdd*2.73f) * 3.5f;
					
			tWString sChar = cString::SubW(sStr, i,1);
			cVector3f vPos = cVector3f(fX, fY + fYAdd - vFontSize.y*0.5f*fMul, 6);
			cVector2f vSize = cVector2f(vFontSize.x, vFontSize.y + fabs(fYAdd)*1.5f) * fMul;

			gpBase->mpGameHudSet->DrawFont(sChar, mpFont,vPos,vSize, cColor(1, mfTextAlpha1),eFontAlign_Left);

			float fBlurAlpha = 0;
			if(mfTextAlpha1 < 0.5f)	fBlurAlpha = mfTextAlpha1 / 0.5f;
			else					fBlurAlpha = 1 - (mfTextAlpha1-0.5f) / 0.5f;

			gpBase->mpGameHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.05,vSize.y*0.05, -1), vSize*1.1, cColor(1, fBlurAlpha * 0.3f ),eFontAlign_Right);
			gpBase->mpGameHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.15,vSize.y*0.15, -2), vSize*1.3, cColor(1, fBlurAlpha * 0.2f ),eFontAlign_Right);

			fX += mpFont->GetLength(vFontSize* fMul, sChar.c_str());
		}

		fY += vFontSize.y * 1.35f;
	}

	//gpBase->mpGameHudSet->DrawFont(kTranslate("Game", "DeathPress"),mpFont, cVector3f(400, 550, 6), 17, cColor(1,mFlashOscill.val,mFlashOscill.val, mfTextAlpha2),eFontAlign_Center);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::OnPressButton()
{
	if(mbActive==false || mfFadeAlpha < 1) return;

	mfTextOnScreenCount = 100;

    /*if(gpBase->mpSaveHandler->AutoLoad())
	{
		//////////////////////
		// Increase player attributes if low
		float fHealth = mpPlayer->GetHealth();
		float fSanity = mpPlayer->GetSanity();
		float fOil = mpPlayer->GetLampOil();

		if(fHealth < mfMaxHealthGain)
		{
			fHealth = cMath::RandRectf(cMath::Max(fHealth, mfMinHealthGain), mfMaxHealthGain);
		}
		if(fSanity < mfMaxSanityGain)
		{
			fSanity = cMath::RandRectf(cMath::Max(fSanity, mfMinSanityGain), mfMaxSanityGain);
		}
		if(fOil < mfMaxHealthGain)
		{
			fOil = cMath::RandRectf(cMath::Max(fOil, mfMinOilGain), mfMaxOilGain);
		}

		mpPlayer->SetHealth(fHealth);
		mpPlayer->SetSanity(fSanity);
		mpPlayer->SetLampOil(fOil);
	}*/
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::SetHint(const tString& asCat, const tString& asEntry)
{
	msHintCat = asCat;
	msHintEntry = asEntry;
}

void cLuxPlayerDeath::ResetGame()
{
	/////////////////////////////////////
	// Player
	mpPlayer->SetRoll(0);
	mpPlayer->GetCamera()->SetRoll(0);
	mpPlayer->GetCamera()->SetPitch(0);
	mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,0,0));
	mpPlayer->SetCurrentHandObjectDrawn(true);
	mpPlayer->GetCharacterBody()->SetForceVelocity(0);
	mpPlayer->GetCharacterBody()->SetMoveSpeed(eCharDir_Forward,0);
	mpPlayer->GetCharacterBody()->SetMoveSpeed(eCharDir_Right,0);
	
	mpPlayer->SetHealth(50.0f);
	if(mpPlayer->GetSanity()<40.0f) mpPlayer->SetSanity(40.0f);

	//////////////////////////////////
	//Sound
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(1, 1,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	pSoundHandler->FadeGlobalSpeed(1, 0.5,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);

	//////////////////////////////////
	//Check point
	gpBase->mpMapHandler->GetCurrentMap()->LoadCheckPoint();	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER LEAN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLean::cLuxPlayerLean(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "PlayerLean")
{
	mfMaxMovement = 0.5f;
	mfMaxRotation = cMath::ToRad(15);

	mpHeadShape = NULL;
}

cLuxPlayerLean::~cLuxPlayerLean()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLean::Reset()
{
	mfDir = 0;
	mfDirAdd = 0;
	mfMaxTime = 0.8f;
	mfMovement = 0;
	mfRotation =0;

	mfMoveSpeed = 0;

	mbPressed = false;

	mpHeadShape = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerLean::CreateWorldEntities(cLuxMap *apMap)
{
	iPhysicsWorld *pPhysicsWorld = apMap->GetPhysicsWorld();

	float fRadius = mpPlayer->GetCharacterBody()->GetSize().x/2 * 0.68f;
	float fHeight = 0.05f * 2;
	if(fHeight < 0)fHeight = fHeight * -1;
	cMatrixf mtxOffset = cMath::MatrixRotateZ(kPi2f);
	mpHeadShape = pPhysicsWorld->CreateCylinderShape(fRadius,fHeight,&mtxOffset);
}

void cLuxPlayerLean::DestroyWorldEntities(cLuxMap *apMap)
{
	iPhysicsWorld *pPhysicsWorld = apMap->GetPhysicsWorld();

	if(mpHeadShape) pPhysicsWorld->DestroyShape(mpHeadShape);
	mpHeadShape = NULL;
}


//-----------------------------------------------------------------------

void cLuxPlayerLean::Update(float afTimeStep)
{
	if(mpPlayer->IsDead()) return;

	////////////////////////////////
	//If pressed move in direction
	if(mbPressed)
	{
		float fDir = mfDir + mfDirAdd;

		mbPressed = false;

		float fGoalPos = mfMaxMovement * fDir;
		float fGoalRot = mfMaxRotation * -fDir;

		//////////////
		//Position
		float fPrevMovement = mfMovement;
		float fMoveSpeed = (fGoalPos - mfMovement);
		if(fabsf(fMoveSpeed) <0.1f) fMoveSpeed = 0.1f*fDir;
		mfMovement += fMoveSpeed * afTimeStep * 3;

		if(fGoalPos < 0 && mfMovement < fGoalPos) mfMovement =fGoalPos;
		if(fGoalPos > 0 && mfMovement > fGoalPos) mfMovement =fGoalPos;

		//////////////
		//Rotation
		float fPrevRotation = mfRotation;
		float fRotSpeed = fGoalRot - mfRotation;
		if(fabsf(fRotSpeed) <0.13f) fRotSpeed = 0.13f*-fDir;

		mfRotation += fRotSpeed * afTimeStep * 2;

		if(fGoalRot < 0 && mfRotation < fGoalRot) mfRotation = fGoalRot;
		if(fGoalRot > 0 && mfRotation > fGoalRot) mfRotation = fGoalRot;

		////////////////////
		//Check collision
		cCamera *pCam = mpPlayer->GetCamera();
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();

		float fReverseMov = fPrevMovement - mfMovement;
		float fReverseRot = fPrevRotation - mfRotation;

		iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
		float fHeightAdd = pCharBody->GetSize().y + mpPlayer->GetCameraPosAdd().y;
		cVector3f vStartPos = pCharBody->GetFeetPosition() + cVector3f(0,fHeightAdd,0);

		cVector3f vPos = vStartPos + pCam->GetRight() * mfMovement;

		int lCount = 0;
		while(pPhysicsWorld->CheckShapeWorldCollision(NULL,mpHeadShape, cMath::MatrixTranslate(vPos),NULL,false,true,NULL,false))
		{
			mfMovement += fReverseMov;
			mfRotation += fReverseRot;

			if(fReverseMov < 0 && mfMovement <0) {
				mfMovement =0;
				mfRotation =0;
				break;
			}
			if(fReverseMov > 0 && mfMovement >0){
				mfMovement =0;
				mfRotation =0;
				break;
			}

			vPos = vStartPos + pCam->GetRight() * mfMovement;
			lCount++;
			if(lCount >10){
				mfMovement =0;
				mfRotation =0;
				break;
			}
		}

		mpPlayer->FadeLeanRollTo(mfRotation, 5,3);
		mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Lean, cVector3f(mfMovement,0,0),2,0.05f);
	}
	////////////////////////////
	// Not pressed move back
	else if (mfMovement !=0 || mfRotation != 0)
	{
		mfRotation =0;
		mfMovement =0;
		mfDir = 0;
		mfDirAdd = 0;

		mpPlayer->FadeLeanRollTo(0, 4,2);
		mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Lean, cVector3f(0,0,0), 1.3f, 0.1);
	}
}

//-----------------------------------------------------------------------


void cLuxPlayerLean::SetLean(float afMul)
{
	mfDir = cMath::Clamp(afMul, -1.0f, 1.0f);
	if (fabsf(afMul) > 0)
	{
		mfDirAdd = 0;
		mbPressed = true;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLean::AddLean(float afAdd)
{
	mfDirAdd = cMath::Clamp(mfDirAdd+afAdd, -1.0f, 1.0f);
	mbPressed = fabsf(mfDirAdd) > 0;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HUD EFFECT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHudEffect::cLuxPlayerHudEffect(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHudEffect")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	//////////////////////////////////
	// Create damage types
	mvDamageTypes.resize(eLuxDamageType_LastEnum);

	LoadDamageData(&mvDamageTypes[eLuxDamageType_BloodSplat], "bloodsplat");
	LoadDamageData(&mvDamageTypes[eLuxDamageType_Claws], "claws");
	LoadDamageData(&mvDamageTypes[eLuxDamageType_Slash], "slash");

	//////////////////////////////////
	// Create flash gfx
	mpFlashGfx = pGui->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Alpha);
}

cLuxPlayerHudEffect::~cLuxPlayerHudEffect()
{

}
//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::AddDamageSplash(eLuxDamageType aType)
{
	
	//////////////////////////
	//Pick image to use
	cLuxPlayerDamageData *pDamageData = &mvDamageTypes[aType];
	if(pDamageData->mvImages.empty()) return;
	int lImageNum = cMath::RandRectl(0, (int)pDamageData->mvImages.size()-1);
	
	cGuiGfxElement *pGfxElem = pDamageData->mvImages[lImageNum];
	cVector2f vImageSize = pGfxElem->GetActiveSize();
	cVector2f vPos = cMath::RandRectVector2f(0, gpBase->mvHudVirtualCenterSize-vImageSize);

	//////////////////////////
	//Set properties
	for(int i=0; i<2; ++i)
	{
		cLuxPlayerHudEffect_Splash splash;

		splash.mpImage = pGfxElem;

		splash.mvPos = cVector3f(vPos.x, vPos.y, 1);
		
		splash.mvSize = vImageSize;
		splash.mfAlpha = 1.0f;

		if(i==0)
		{
			splash.mfAlphaMul = 0.3f;
			splash.mvPosVel = cVector3f(0,1.0f, 0);
			splash.mvSizeVel = cVector2f(0, 8.0f);
		}
		else
		{
			splash.mfAlphaMul = 0.7f;
			splash.mvPosVel = cVector3f(0,6.0f, 0);
			splash.mvSizeVel = cVector2f(0, 16.0f);
		}
		
		splash.mfAlphaVel = 0.4f;
		splash.mfAlphaMoveStart = 0.8f;

		mlstSplashes.push_back(splash);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Flash(const cColor& aColor, eGuiMaterial aFlashMaterial, float afInTime, float afOutTime)
{
	mFlashColor = aColor;
	mFlashMaterial = aFlashMaterial;
	mfFlashAlpha = 0.0f;
	mfFlashAlphaSpeed = afInTime==0 ? 100000.0f : 1.0f / afInTime;
	mfFlashAlphaOutSpeed = afOutTime==0 ? -100000.0f : -1.0f / afOutTime;
	mbFlashActive = true;
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::OnDraw(float afFrameTime)
{
	DrawSplashes(afFrameTime);
	DrawFlash(afFrameTime);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Update(float afTimeStep)
{
	UpdateSplashes(afTimeStep);
	UpdateFlash(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Reset()
{
	mlstSplashes.clear();
	mfFlashAlpha =0;
	mbFlashActive = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawSplashes(float afFrameTime)
{
	cLuxPlayerHudEffect_SplashListIt it = mlstSplashes.begin();
	for(; it != mlstSplashes.end(); ++it)
	{
		cLuxPlayerHudEffect_Splash *pSplash = &(*it);

        gpBase->mpGameHudSet->DrawGfx(pSplash->mpImage, pSplash->mvPos,pSplash->mvSize, cColor(1, pSplash->mfAlpha * pSplash->mfAlphaMul));
	}
}

void cLuxPlayerHudEffect::UpdateSplashes(float afTimeStep)
{
	cLuxPlayerHudEffect_SplashListIt it = mlstSplashes.begin();
	for(; it != mlstSplashes.end();)
	{
		cLuxPlayerHudEffect_Splash *pSplash = &(*it);

		if(pSplash->mfAlpha < pSplash->mfAlphaMoveStart)
		{
			pSplash->mvPos += pSplash->mvPosVel * afTimeStep;
			pSplash->mvSize += pSplash->mvSizeVel * afTimeStep;
		}
		
		pSplash->mfAlpha -= pSplash->mfAlphaVel * afTimeStep;
		if(pSplash->mfAlpha < 0)
		{
			it = mlstSplashes.erase(it);
			continue;
		}

		++it;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawFlash(float afFrameTime)
{
	if(mfFlashAlpha <=0 || mbFlashActive==false) return;

	cColor col = mFlashColor;
	
	col.a *= mfFlashAlpha;
	
	gpBase->mpGameHudSet->DrawGfx(mpFlashGfx,gpBase->mvHudVirtualStartPos +cVector3f(0,0,0), gpBase->mvHudVirtualSize, col, mFlashMaterial);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::UpdateFlash(float afTimeStep)
{
	if(mbFlashActive==false) return;
	
	mfFlashAlpha += mfFlashAlphaSpeed * afTimeStep;

	if(mfFlashAlpha > 1.0f && mfFlashAlphaSpeed > 0)
	{
		mfFlashAlpha = 1.0f;
		mfFlashAlphaSpeed = mfFlashAlphaOutSpeed;
	}
	if(mfFlashAlpha < 0 && mfFlashAlphaSpeed < 0)
	{
		mfFlashAlpha = 0;
		mbFlashActive = false;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::LoadDamageData(cLuxPlayerDamageData *apData, const tString& asName)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	cFileSearcher *pFileSearcher = gpBase->mpEngine->GetResources()->GetFileSearcher();

	tString sFileNameBase = "graphics/hud/damage_"+asName;

	int lCount = 0;
	tString sFile = sFileNameBase + cString::ToString(lCount)+".tga";

	while(pFileSearcher->GetFilePath(sFile) != _W(""))
	{
		cGuiGfxElement *pGfx = pGui->CreateGfxImage(sFile, eGuiMaterial_Alpha);
		if(pGfx) apData->mvImages.push_back(pGfx);

		lCount++;
		sFile = sFileNameBase + cString::ToString(lCount)+".tga";
	}
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER LIGHT LEVEL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLightLevel::cLuxPlayerLightLevel(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLightLevel")
{
	mfRadiusAdd = gpBase->mpGameCfg->GetFloat("Player_Darkness","RadiusAdd",0);
}

cLuxPlayerLightLevel::~cLuxPlayerLightLevel()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerLightLevel::OnStart()
{
	
}

void cLuxPlayerLightLevel::Reset()
{
	mfExtendedLightLevel = 1.0f;
	mfNormalLightLevel = 1.0f;
	mfUpdateCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerLightLevel::Update(float afTimeStep)
{
	if(ohpl_ga2_light_timer(&mfUpdateCount, afTimeStep))
	{
		iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
		cVector3f vPos = pCharBody->GetPosition();
		cVector3f vSize = pCharBody->GetSize();
		cVector3f vForward = pCharBody->GetForward();
		cVector3f vTestPos[5];
		ohpl_ga2_light_positions(&vPos, &vSize, &vForward, vTestPos);
		std::vector<iLight*> vSkipLights;
		vSkipLights.push_back(mpPlayer->GetHelperInDarkness()->GetAmbientLight());
		bool bLantern = mpPlayer->GetHelperLantern()->IsActive();
		float vExt[5], vNormal[5];
		for(int i=0; i<5; ++i)
		{
			vExt[i] = gpBase->mpMapHelper->GetLightLevelAtPos(vTestPos[i], &vSkipLights, mfRadiusAdd);
			vNormal[i] = gpBase->mpMapHelper->GetLightLevelAtPos(vTestPos[i], &vSkipLights, 0);
		}
		ohpl_ga2_light_combine(bLantern, vExt, vNormal, &mfExtendedLightLevel, &mfNormalLightLevel);
	}
}


//-----------------------------------------------------------------------



void cLuxPlayerLightLevel::OnMapEnter(cLuxMap *apMap)
{
	mfUpdateCount =0;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER IN DARKNESS
//////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------

cLuxPlayerInDarkness::cLuxPlayerInDarkness(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerInDarkness")
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	mfMinDarknessLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","MinLightLevel",0);
	
	mfAmbientLightMinLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightMinLightLevel",0);
	mfAmbientLightRadius = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightRadius",0);
	mfAmbientLightIntensity = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightIntensity",0);
	mfAmbientLightFadeInTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeInTime",0);
	mfAmbientLightFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeOutTime",0);
	mAmbientLightColor = gpBase->mpGameCfg->GetColor("Player_Darkness","AmbientLightColor",cColor(0));

	msLoopSoundFile  = gpBase->mpGameCfg->GetString("Player_Darkness", "LoopSoundFile","");
	mfLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundVolume",0);
	mfLoopSoundStartupTime = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundStartupTime",0);
	mfLoopSoundFadeInSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeInSpeed",0);
	mfLoopSoundFadeOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeOutSpeed",0);

	mfSanityLossPerSecond = gpBase->mpGameCfg->GetFloat("Player_Darkness", "SanityLossPerSecond",0);
}

cLuxPlayerInDarkness::~cLuxPlayerInDarkness()
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::OnStart()
{

}
void cLuxPlayerInDarkness::Reset()
{
	mbActive = true;

	mpAmbientLight =NULL;

	mbAmbientLightIsOn = false;
	mbInDarkness = false;

	mpLoopSound = NULL;
	mfLoopSoundCount =0;

	mfSanityLossMul =0;

	mfShowHintTimer = 0;
}

//-----------------------------------------------------------------------


void cLuxPlayerInDarkness::Update(float afTimeStep)
{
	if (!mbActive) return;
	OhplGa2DarknessIn in;
	in.mfExtLevel = mpPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	in.mfNormalLevel = mpPlayer->GetHelperLightLevel()->GetNormalLightLevel();
	cVector3f vCamPos = mpPlayer->GetCamera()->GetPosition();
	mpAmbientLight->SetPosition(vCamPos - cVector3f(0,0.3f,0));
	in.mbHardMode = gpBase->mbHardMode;
	in.mbFlashbackActive = mpPlayer->GetHelperFlashback()->IsActive();
	in.mbDrainDisabled = mpPlayer->GetSanityDrainDisabled();
	in.mbEmotionFlash = gpBase->mpEffectHandler->GetEmotionFlash()->IsActive();
	OhplGa2Darkness s = {mfMinDarknessLightLevel, mfAmbientLightMinLightLevel, mfLoopSoundStartupTime, mfSanityLossPerSecond,
		mbAmbientLightIsOn, mbInDarkness, mfLoopSoundCount, mfSanityLossMul, mfShowHintTimer};
	OhplGa2Ops ops;
	ohpl_ga2_darkness_update(&s, &in, afTimeStep, &ops);
	mbAmbientLightIsOn = s.mbAmbientLightIsOn != 0;
	mbInDarkness = s.mbInDarkness != 0;
	mfLoopSoundCount = s.mfLoopSoundCount;
	mfSanityLossMul = s.mfSanityLossMul;
	mfShowHintTimer = s.mfShowHintTimer;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		const OhplGa2Op &op = ops.ops[i];
		switch(op.kind)
		{
		case kOhplGa2AmbientOff: mpAmbientLight->FadeTo(cColor(0.0f, 0.0f),mpAmbientLight->GetRadius(),mfAmbientLightFadeOutTime); break;
		case kOhplGa2AmbientOn:
			if(op.i)
				mpAmbientLight->FadeTo(mAmbientLightColor*mfAmbientLightIntensity * 0.75f , mpAmbientLight->GetRadius(), mfAmbientLightFadeInTime * 2.5f);
			else
				mpAmbientLight->FadeTo(mAmbientLightColor*mfAmbientLightIntensity, mpAmbientLight->GetRadius(), mfAmbientLightFadeInTime);
			break;
		case kOhplGa2DarknessLowerSanity: mpPlayer->LowerSanity(op.a, true); break;
		case kOhplGa2HintDarkness: gpBase->mpHintHandler->Add("DarknessDecrease", kTranslate("Hints", "DarknessDecrease"), 0); break;
		}
	}
}

//-----------------------------------------------------------------------
void cLuxPlayerInDarkness::OnMapEnter(cLuxMap *apMap)
{
	mbAmbientLightIsOn = false;
	mbInDarkness = false;
}

void cLuxPlayerInDarkness::OnMapLeave(cLuxMap *apMap)
{

}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	mpAmbientLight = pWorld->CreateLightPoint("PlayerDarknessAmbient","",false);
	mpAmbientLight->SetDiffuseColor(cColor(0.0f, 0.0f));

	mpAmbientLight->SetRadius(mfAmbientLightRadius);

	/////////////////////
	// HARDMODE
	if(gpBase->mbHardMode)
		mpAmbientLight->SetRadius(mfAmbientLightRadius*0.5f);


	mpAmbientLight->SetCastShadows(false);
	mpAmbientLight->SetIsSaved(false);
}

void cLuxPlayerInDarkness::DestroyWorldEntities(cLuxMap *apMap)
{
	if(mpAmbientLight) apMap->GetWorld()->DestroyLight(mpAmbientLight);
	mpAmbientLight = NULL;
}

//-----------------------------------------------------------------------

bool cLuxPlayerInDarkness::InDarkness()
{
	float fLightLevel = mpPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	return fLightLevel <= mfMinDarknessLightLevel;
}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::SetActive(bool abX)
{
	mbActive = abX;

	if (abX == false)
	{
		if(mbAmbientLightIsOn)
		{
			mbAmbientLightIsOn = false;
			mpAmbientLight->FadeTo(cColor(0.0f, 0.0f),mpAmbientLight->GetRadius(),mfAmbientLightFadeOutTime);
		}
        
		mfLoopSoundCount = 0;
		mbInDarkness = false;
		mfSanityLossMul = 0;
	}
}

//-----------------------------------------------------------------------
