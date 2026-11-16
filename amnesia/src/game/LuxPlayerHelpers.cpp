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
	mS.mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddGoal",0);
	
	mS.mfHeightAddCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddCollapseSpeed",0);
	mS.mfHeightAddAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddAwakeSpeed",0);
	mS.mfRollCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollCollapseSpeed",0);
	mfRollAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollAwakeSpeed",0);

	mS.mfSleepTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepTime",0);

	mfSleepSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepSpeedMul",0);
	mS.mfWakeUpSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_WakeUpSpeedMul",0);

	msStartSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_StartSound", "");
	msAwakenSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_AwakenSound", "");
	msSleepLoopSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_SleepLoopSound", "");
	mfSleepLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepLoopSoundVolume", 0);

	msSleepRandomSound = gpBase->mpGameCfg->GetString("Player_General","InsanityCollapse_SleepRandomSound", "");
	mS.mfSleepRandomMinTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMinTime", 0);
	mS.mfSleepRandomMaxTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMaxTime", 0);

	mS.mfAwakenSanity = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_AwakenSanity", 0);

	//Init sound var here
	mpLoopSound = NULL;
}

cLuxPlayerInsanityCollapse::~cLuxPlayerInsanityCollapse()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Reset()
{
	ohpl_a2b_collapse_clear(&mS, false, true);

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
	ohpl_a2b_collapse_clear(&mS, true, true);
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

	ohpl_a2b_collapse_clear(&mS, false, false);
	mpPlayer->FadeRollTo(0, 10,10);
	mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,0,0),1, 0.1f);
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_collapse_update(&mS, &h, afTimeStep);
}

void cLuxPlayerInsanityCollapse::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerInsanityCollapse *p = (cLuxPlayerInsanityCollapse*)apCtx;
	switch(alKind)
	{
	case kOhplGa2SetHeadAdd: p->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,afA,0)); break;
	case kOhplGa2FadeRoll: p->mpPlayer->FadeRollTo(afA, afB, afC); break;
	case kOhplGa2PlayRandomSound: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msSleepRandomSound, eSoundEntryType_Gui); break;
	case kOhplGa2WakeFx:
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		pSoundHandler->FadeGlobalVolume(1, 0.2f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
		pSoundHandler->FadeGlobalSpeed(1, 0.25f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
		break;
	}
	case kOhplGa2SetCollapseSpeedMul: p->mpPlayer->SetInsanityCollapseSpeedMul(afA); break;
	case kOhplGa2LoopFadeOut:
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		if(p->mpLoopSound && pSoundHandler->IsValid(p->mpLoopSound, p->mlLoopSoundID))
		{
			p->mpLoopSound->FadeOut(0.3f);
		}
		break;
	}
	case kOhplGa2PlayerSetSanity: p->mpPlayer->SetSanity(afA); break;
	case kOhplGa2PlayAwaken: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msAwakenSound, eSoundEntryType_World); break;
	case kOhplGa2SetCrouchDisabled: p->mpPlayer->SetCrouchDisabled(alI != 0); break;
	case kOhplGa2SetJumpDisabled: p->mpPlayer->SetJumpDisabled(alI != 0); break;
	}
}

float cLuxPlayerInsanityCollapse::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	if(alKind == kOhplA2bQRand) return cMath::RandRectf(afA, afB);
	return 0;
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER SIGHT SWAY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerCamDirEffects::cLuxPlayerCamDirEffects(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerCamDirEffects")
{
	mS.k.mfStartSwayMaxSanity =  gpBase->mpGameCfg->GetFloat("Player_Sanity","StartSwayMaxSanity",0);
	mS.k.mlMaxPositions = gpBase->mpGameCfg->GetInt("Player_Sanity","SwayMaxSavedPositions",0);
	mS.mlCount = 0;
}

cLuxPlayerCamDirEffects::~cLuxPlayerCamDirEffects()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerCamDirEffects::Reset()
{
	ohpl_a2b_camdir_reset(&mS);
}

//-----------------------------------------------------------------------


void cLuxPlayerCamDirEffects::Update(float afTimeStep)
{
	OhplA2bHost h = {this, NULL, HostQuery, NULL};
	ohpl_a2b_camdir_update(&mS, &h, afTimeStep);
}

//-----------------------------------------------------------------------

float cLuxPlayerCamDirEffects::AddAndGetYawAdd(float afX)
{
	return ohpl_a2b_camdir_add(&mS, true, afX);
}

float cLuxPlayerCamDirEffects::AddAndGetPitchAdd(float afX)
{
	return ohpl_a2b_camdir_add(&mS, false, afX);
}

float cLuxPlayerCamDirEffects::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerCamDirEffects *p = (cLuxPlayerCamDirEffects*)apCtx;
	if(alKind == kOhplA2bQSanity) return p->mpPlayer->GetSanity();
	return 0;
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------




//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SPAWN PS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSpawnPS::cLuxPlayerSpawnPS(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSpawnPS"), mpS(ohpl_a2c_sps_new())
{

}

cLuxPlayerSpawnPS::~cLuxPlayerSpawnPS()
{
	ohpl_a2c_sps_free(mpS);
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Reset()
{
	msFileName = "";

	OhplA2cSpsHost h = {this, HostCreate, HostExists, HostKill, HostDestroy, HostRand, HostFeet};
	ohpl_a2c_sps_reset(mpS, &h, gpBase->mpMapHandler->GetCurrentMap() != NULL);
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Start(const tString& asFileName)
{
	OhplA2cSpsCfg cfg;
	if(LoadSpawnPSFile(asFileName, cfg)==false)
	{
		return;
	}

	msFileName = asFileName;

	OhplA2cSpsHost h = {this, HostCreate, HostExists, HostKill, HostDestroy, HostRand, HostFeet};
	ohpl_a2c_sps_start(mpS, &h, cfg);
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Stop()
{
	ohpl_a2c_sps_set_active(mpS, false);
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::RespawnAll()
{
	OhplA2cSpsHost h = {this, HostCreate, HostExists, HostKill, HostDestroy, HostRand, HostFeet};
	ohpl_a2c_sps_respawn(mpS, &h);
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Update(float afTimeStep)
{
	OhplA2cSpsHost h = {this, HostCreate, HostExists, HostKill, HostDestroy, HostRand, HostFeet};
	ohpl_a2c_sps_update(mpS, &h);
}

//-----------------------------------------------------------------------

size_t cLuxPlayerSpawnPS::HostCreate(void *apCtx, const cVector3f *apPos)
{
	cLuxPlayerSpawnPS *p = (cLuxPlayerSpawnPS*)apCtx;
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	cParticleSystem *pPS = pWorld->CreateParticleSystem("SpawnPS", p->msParticleSystem, 1);
	if(pPS)
	{
		pPS->SetPosition(*apPos);
		pPS->SetFadeAtDistance(p->mbFadePS);
		pPS->SetMinFadeDistanceStart(p->mfPSMinFadeStart);
		pPS->SetMinFadeDistanceEnd(p->mfPSMinFadeEnd);
		pPS->SetMaxFadeDistanceStart(p->mfPSMaxFadeStart);
		pPS->SetMaxFadeDistanceEnd(p->mfPSMaxFadeEnd);
		pPS->SetColor(p->mPSColor);
	}
	return (size_t)pPS;
}

bool cLuxPlayerSpawnPS::HostExists(void *apCtx, size_t alPS)
{
	return gpBase->mpMapHandler->GetCurrentMap()->GetWorld()->ParticleSystemExists((cParticleSystem*)alPS);
}

void cLuxPlayerSpawnPS::HostKill(void *apCtx, size_t alPS)
{
	((cParticleSystem*)alPS)->Kill();
}

void cLuxPlayerSpawnPS::HostDestroy(void *apCtx, size_t alPS)
{
	gpBase->mpMapHandler->GetCurrentMap()->GetWorld()->DestroyParticleSystem((cParticleSystem*)alPS);
}

float cLuxPlayerSpawnPS::HostRand(void *apCtx, float afMin, float afMax)
{
	return cMath::RandRectf(afMin, afMax);
}

void cLuxPlayerSpawnPS::HostFeet(void *apCtx, cVector3f *apOut)
{
	*apOut = ((cLuxPlayerSpawnPS*)apCtx)->mpPlayer->GetCharacterBody()->GetFeetPosition();
}

//-----------------------------------------------------------------------

bool cLuxPlayerSpawnPS::LoadSpawnPSFile(const tString& asFileName, OhplA2cSpsCfg &aCfg)
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
	aCfg.mfHeightFromFeet = pXmlDoc->GetAttributeFloat("HeightFromFeet",0);
	aCfg.mfHeightAddMin = pXmlDoc->GetAttributeFloat("HeightAddMin",0);
	aCfg.mfHeightAddMax = pXmlDoc->GetAttributeFloat("HeightAddMax",0);
	aCfg.mfDensity = pXmlDoc->GetAttributeFloat("Density",0);
	aCfg.mfRadius = pXmlDoc->GetAttributeFloat("Radius",0);
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
	mS.k.mfEffectStartHealth =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_EffectStartHealth",0);
	mS.k.mfMinMoveMul =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MinSpeedMul",0);

	mS.k.mfMaxPantCount =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MaxPantCount",0);
	mS.k.mfPantSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSpeed",0);
	mS.k.mfPantSize =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSize",0);

	mS.mfHealthRegainSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainSpeed",0);
	mS.mfHealthRegainLimit =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainLimit",0);

	mfNoiseAlpha =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseAlpha",0);
	mS.k.mfNoiseFreq =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseFreq",0);
	mNoiseColor =  gpBase->mpGameCfg->GetColor("Player_General","Hurt_NoiseColor",cColor(0));

	cGui *pGui = gpBase->mpEngine->GetGui();
	mvNoiseGfx.resize(8);
	mS.k.mlNoiseCount = (int)mvNoiseGfx.size();
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
	ohpl_a2b_hurt_reset(&mS);
}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_hurt_update(&mS, &h, afTimeStep);
}

void cLuxPlayerHurt::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerHurt *p = (cLuxPlayerHurt*)apCtx;
	switch(alKind)
	{
	case kOhplA2bAddHealth: p->mpPlayer->AddHealth(afA); break;
	case kOhplGa2SetHurtMoveSpeedMul: p->mpPlayer->SetHurtMoveSpeedMul(afA); break;
	case kOhplGa2PlayHurtPant: gpBase->mpHelpFuncs->PlayGuiSoundData("hurt_pant",eSoundEntryType_Gui, afA); break;
	case kOhplGa2SetHurtHeadAdd: p->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Hurt, cVector3f(0,afA,0)); break;
	}
}

float cLuxPlayerHurt::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerHurt *p = (cLuxPlayerHurt*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQHealth: return p->mpPlayer->GetHealth();
	case kOhplA2bQDead: return p->mpPlayer->IsDead() ? 1.0f : 0.0f;
	case kOhplA2bQSpeed: return p->mpPlayer->GetCharacterBody()->GetVelocity(afA).Length();
	case kOhplA2bQRandInt: return (float)cMath::RandRectl((int)afA, (int)afB);
	}
	return 0;
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
			gpBase->mpGameHudSet->DrawGfx(mvNoiseGfx[mS.k.mlCurrentNoise], vPos, vNoiseSize, cColor(1,1-mfAlpha, 1-mfAlpha));
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
	mS.mfRadialBlurSize = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurSize", 0.12f);
	mS.mfRadialBlurStartDist = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurStartDist", 0.4f);
	mfWorldSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackWorldSoundVolume", 0.4f);
	mS.mfMoveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackMoveSpeedMul", 1.0f);
	mfRunSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRunSpeedMul", 1.0f);

	Reset();

}
cLuxPlayerFlashback::~cLuxPlayerFlashback ()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Reset()
{
	ohpl_a2b_flashback_reset(&mS);
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
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_flashback_update(&mS, &h, afTimeStep);
}

void cLuxPlayerFlashback::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerFlashback *p = (cLuxPlayerFlashback*)apCtx;
	switch(alKind)
	{
	case kOhplGa2FlashbackStartQueued:
	{
		cLuxFlashbackData data = p->mlstFlashbackQueue.front();
		p->mlstFlashbackQueue.pop_front();
		p->Start(data.msFile, data.msCallback);
		break;
	}
	case kOhplGa2FlashMid:
		gpBase->mpEffectHandler->GetFlash()->Start(0.5f, 0.5f, 2.5f);
		gpBase->mpPlayer->SetEventMoveSpeedMul(afA);
		gpBase->mpPlayer->SetEventRunSpeedMul(afB);
		break;
	case kOhplGa2FlashbackBegin:
		gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(1, afD);
		gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(afA);
		gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(afB, afC);
		p->LoadAndPlayFlashbackFile(p->msFlashbackFile);
		break;
	case kOhplGa2FlashbackEnd:
	{
		gpBase->mpEffectHandler->GetFlash()->Start(1.0f, 0.1f, 2.5f);
		gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(0, afA);
		gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, afB);
		gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(1, afA,eSoundEntryType_World,eLuxGlobalVolumeType_Flashback,true);
		gpBase->mpPlayer->SetEventMoveSpeedMul(1.0f);
		gpBase->mpPlayer->SetEventRunSpeedMul(1.0f);
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		if(alI) pMap->RunScript(p->msCallback + "()");
		break;
	}
	}
}

float cLuxPlayerFlashback::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerFlashback *p = (cLuxPlayerFlashback*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQVoiceDone: return gpBase->mpEffectHandler->GetPlayVoice()->VoiceDonePlaying() ? 1.0f : 0.0f;
	case kOhplA2bQQueueNonEmpty: return p->mlstFlashbackQueue.empty() ? 0.0f : 1.0f;
	case kOhplA2bQHasCallback: return p->msCallback != "" ? 1.0f : 0.0f;
	}
	return 0;
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
	OhplA2bHost h = {this, HostOp, HostQuery, HostFill};
	ohpl_a2b_lookat_update(&mS, &h, afTimeStep);
}

void cLuxPlayerLookAt::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerLookAt *p = (cLuxPlayerLookAt*)apCtx;
	cCamera *pCam = p->mpPlayer->GetCamera();
	switch(alKind)
	{
	case kOhplGa2LookAddPitch: pCam->AddPitch(afA); break;
	case kOhplGa2LookAddYaw: pCam->AddYaw(afA); break;
	case kOhplGa2LookSyncBodyYaw: p->mpPlayer->GetCharacterBody()->SetYaw(pCam->GetYaw()); break;
	case kOhplGa2LookAtTarget: gpBase->mpMapHandler->GetCurrentMap()->RunScript(p->msAtTargetCallback+"()"); break;
	}
}

float cLuxPlayerLookAt::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerLookAt *p = (cLuxPlayerLookAt*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQCamPitch: return p->mpPlayer->GetCamera()->GetPitch();
	case kOhplA2bQCamYaw: return p->mpPlayer->GetCamera()->GetYaw();
	}
	return 0;
}

void cLuxPlayerLookAt::HostFill(void *apCtx, uint32_t alKind, float *apBuf)
{
	cLuxPlayerLookAt *p = (cLuxPlayerLookAt*)apCtx;
	if(alKind != kOhplA2bFGoalAngle) return;
	cVector3f vGoal = cMath::GetAngleFromPoints3D(p->mpPlayer->GetCamera()->GetPosition(), cVector3f(apBuf[0], apBuf[1], apBuf[2]));
	apBuf[3] = vGoal.x;
	apBuf[4] = vGoal.y;
	apBuf[5] = vGoal.z;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::Reset()
{
	ohpl_a2b_lookat_reset(&mS);
	msAtTargetCallback = "";
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetTarget(const cVector3f &avTargetPos, float afSpeedMul, float afMaxSpeed, const tString& asAtTargetCallback)
{
	ohpl_a2b_lookat_set_target(&mS, avTargetPos.x, avTargetPos.y, avTargetPos.z, afSpeedMul, afMaxSpeed);
	msAtTargetCallback = asAtTargetCallback;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetActive(bool abX)
{
	ohpl_a2b_lookat_set_active(&mS, abX);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SANITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSanity::cLuxPlayerSanity(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSanity")
{
	mS.mfHitZoomInSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInSpeed",0);
	mS.mfHitZoomOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomOutSpeed",0);
	mS.mfHitZoomInFOVMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInFOVMul",0);
	mS.mfHitZoomInAspectMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInAspectMul",0);

	mS.mfSanityRegainSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainSpeed",0);
	mS.mfSanityRegainLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainLimit",0);

	mS.mfSanityVeryLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityVeryLowLimit",0);
	mS.mfSanityEffectsStart = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityEffectsStart",0);

	mS.mfSanityWaveAlphaMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveAlphaMul",0);
	mS.mfSanityWaveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveSpeedMul",0);

	mS.mfSanityLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimit",0);
	mS.mfSanityLowLimitMaxTime = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimitMaxTime",0);
	mS.mfSanityLowNewSanityAmount = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowNewSanityAmount",0);

	mS.mfCheckNearEnemyInterval = gpBase->mpGameCfg->GetFloat("Player_Sanity","CheckNearEnemyInterval",0);
	mS.mfNearEnemyDecrease = gpBase->mpGameCfg->GetFloat("Player_Sanity","NearEnemyDecrease",0);
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
	ohpl_a2c_sanity_reset(&mS);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::StartHit()
{
	mS.mbHitActive = true;

	gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_damage", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::SetSanityLost()
{
	mS.mfSanityLostCount = 1.0f;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2c_sanity_update(&mS, &h, afTimeStep);
}

void cLuxPlayerSanity::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerSanity *p = (cLuxPlayerSanity*)apCtx;
	switch(alKind)
	{
	case kOhplGa2SanSetSanity: p->mpPlayer->SetSanity(afA); break;
	case kOhplGa2SanLower: p->mpPlayer->LowerSanity(afA, true); break;
	case kOhplGa2SanEnemyUnseenBlur: gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, afA); break;
	case kOhplGa2SanHintEnemySeen: gpBase->mpHintHandler->Add("EnemySeen", kTranslate("Hints", "EnemySeen"), 0); break;
	case kOhplGa2SanBlurEnemy:
		gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(afA);
		gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(afB, 10.0f);
		break;
	case kOhplGa2SanImageTrail: gpBase->mpEffectHandler->GetImageTrail()->FadeTo(afA, afB); break;
	case kOhplGa2SanFadeAspect: p->mpPlayer->FadeAspectMulTo(afA, afB); break;
	case kOhplGa2SanFadeFov: p->mpPlayer->FadeFOVMulTo(afA, afB); break;
	case kOhplGa2SanInsanityActive: gpBase->mpPostEffectHandler->GetInsanity()->SetActive(alI != 0); break;
	case kOhplGa2SanInsanityFx:
		gpBase->mpPostEffectHandler->GetInsanity()->SetWaveAlpha(afA);
		gpBase->mpPostEffectHandler->GetInsanity()->SetWaveSpeed(afB);
		gpBase->mpPostEffectHandler->GetInsanity()->SetZoomAlpha(afC);
		break;
	case kOhplGa2SanHeartbeat: gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_heartbeat", eSoundEntryType_Gui, afA); break;
	case kOhplGa2SanDrain:
	{
		tString sSoundFile = alI == 0 ? "sanity_drain_low" : (alI == 1 ? "sanity_drain_med" : "sanity_drain_high");
		gpBase->mpHelpFuncs->PlayGuiSoundData(sSoundFile, eSoundEntryType_Gui, afA);
		break;
	}
	case kOhplGa2SanHintLow: gpBase->mpHintHandler->Add("SanityLow", kTranslate("Hints", "SanityLow"), 0); break;
	case kOhplGa2SanPant: gpBase->mpHelpFuncs->PlayGuiSoundData("sanity_pant", eSoundEntryType_Gui); break;
	}
}

float cLuxPlayerSanity::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerSanity *p = (cLuxPlayerSanity*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQSanity: return p->mpPlayer->GetSanity();
	case kOhplA2bQDead: return p->mpPlayer->IsDead() ? 1.0f : 0.0f;
	case kOhplA2bQRand: return cMath::RandRectf(afA, afB);
	case kOhplA2cScan: return p->ScanForEnemies() ? 1.0f : 0.0f;
	}
	return 0;
}

//-----------------------------------------------------------------------

bool cLuxPlayerSanity::ScanForEnemies()
{
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

	return bSeenEnemy;
}

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
	mS.k.mfLowerOilSpeed = gpBase->mpGameCfg->GetFloat("Player_Lantern","LowerOilSpeed",0);
	mS.k.mfFadeLightOilAmount = gpBase->mpGameCfg->GetFloat("Player_Lantern","FadeLightOilAmount",0);

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
	ohpl_a2b_lantern_reset(&mS);
	mpLight = NULL;
}

//-----------------------------------------------------------------------


void cLuxPlayerLantern::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_lantern_update(&mS, &h, afTimeStep);
}

void cLuxPlayerLantern::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerLantern *p = (cLuxPlayerLantern*)apCtx;
	switch(alKind)
	{
	case kOhplGa2LanternColor: p->mpLight->SetDiffuseColor(cColor(afA, afB, afC, afD)); break;
	case kOhplGa2LanternOutOfOil:
		gpBase->mpHelpFuncs->PlayGuiSoundData(p->msOutOfOilSound, eSoundEntryType_Gui);
		p->SetActive(false, true);
		break;
	case kOhplGa2LanternSetOil: p->mpPlayer->SetLampOil(afA); break;
	case kOhplA2bLanternMatrix:
		// Update light matrix
		if(p->mpLight)
		{
			cCamera *pCam = p->mpPlayer->GetCamera();
			cVector3f vCamRotation( pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
			cMatrixf mtxCamTransform = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
			mtxCamTransform.SetTranslation(pCam->GetPosition());
			cMatrixf mtxLocalLight = cMath::MatrixTranslate(p->mvLocalOffset);
			p->mpLight->SetMatrix(cMath::MatrixMul(mtxCamTransform, mtxLocalLight));
		}
		break;
	case kOhplGa2LanternHintNoItem: gpBase->mpHintHandler->Add("LanternNoItem", kTranslate("Hints", "LanternNoItem"), 0); break;
	case kOhplGa2LanternHintNoOil: gpBase->mpHintHandler->Add("LanternNoOil", kTranslate("Hints", "LanternNoOil"), 0); break;
	case kOhplGa2LanternSoundOut: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msOutOfOilSound, eSoundEntryType_Gui); break;
	case kOhplGa2LanternSetActive: p->mbActive = alI != 0; break;
	case kOhplGa2LanternSoundOn: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msTurnOnSound, eSoundEntryType_Gui); break;
	case kOhplGa2LanternSoundOff: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msTurnOffSound, eSoundEntryType_Gui); break;
	case kOhplGa2LanternHand:
		if(alI) p->mpPlayer->GetHands()->SetActiveHandObject("lantern");
		else p->mpPlayer->GetHands()->SetActiveHandObject("");
		break;
	case kOhplGa2LanternCallback:
	{
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		if(pMap->GetLanternLitCallback()!="")
		{
			pMap->RunScript(pMap->GetLanternLitCallback()+"(" + (alI ? "true" : "false") + ")" );
		}
		break;
	}
	}
}

float cLuxPlayerLantern::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerLantern *p = (cLuxPlayerLantern*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQEmotionFlash: return gpBase->mpEffectHandler->GetEmotionFlash()->IsActive() ? 1.0f : 0.0f;
	case kOhplA2bQOil: return p->mpPlayer->GetLampOil();
	case kOhplA2bQAllowed: return p->mpPlayer->GetCurrentStateData()->AllowLantern() ? 1.0f : 0.0f;
	case kOhplA2bQHasItem: return gpBase->mpInventory->HasItemOfType(eLuxItemType_Lantern) ? 1.0f : 0.0f;
	}
	return 0;
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
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_lantern_set_active(&mS, &h, abX, abUseEffects, abCheckForOilAndItems, abCheckIfAllowed);
}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::SetDisabled(bool abX)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2b_lantern_set_disabled(&mS, &h, abX);
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

	mS.mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAdd",0);
	mS.mfHeightAddGoalCrouch = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddCrouch",0);
	mS.mfFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_General","Death_FadeTime",1000);

	if(mpPlayer->UsePermaDeath()) mS.mfFadeOutTime /= 2.0f;

	mS.mfHeightAddSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddSpeed",0);
	mS.mfRollSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_RollSpeed",0);

	if(mpPlayer->UsePermaDeath()) mS.mfHeightAddSpeed *= 1.2f;
	if(mpPlayer->UsePermaDeath()) mS.mfRollSpeed *= 1.2f;

	msStartSound = gpBase->mpGameCfg->GetString("Player_General","Death_StartSound", "");
	msAwakenSound = gpBase->mpGameCfg->GetString("Player_General","Death_AwakenSound", "");

	mpFont = NULL;

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
	mS.mbShowHint = gpBase->mpUserConfig->GetBool("Game", "ShowDeathHints", true);
}

void cLuxPlayerDeath::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowDeathHints", mS.mbShowHint != 0);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Reset()
{
	mpVoiceEntry = NULL;

	mbSkipStartSound = false;

	msCurrentHintText = _W("");

	msHintCat = "";
	msHintEntry = "";

	ohpl_a2c_death_reset(&mS);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Start()
{
	if(mS.mbActive) return;

	mS.mbActive = true;

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

	bool bCrouching = false;
	if(mpPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetCurrentMoveStateData());
		bCrouching = pNormalMove->IsCrouching();
	}

	ohpl_a2c_death_start(&mS, bCrouching);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2c_death_update(&mS, &h, afTimeStep);
}

void cLuxPlayerDeath::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerDeath *p = (cLuxPlayerDeath*)apCtx;
	switch(alKind)
	{
	case kOhplGa2SetHeadAdd: p->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,afA,0)); break;
	case kOhplGa2FadeRoll: p->mpPlayer->FadeRollTo(afA, afB, afC); break;
	case kOhplGa2DeathPermaSound:
	{
		cLuxSoundExtraData extraData;
		if(gpBase->mpHelpFuncs->PlayGuiSoundData(p->mpPlayer->GetCurrentPermaDeathSound(), eSoundEntryType_Gui, 1, eSoundEntityType_Main, true, &extraData))
		{
			p->mpVoiceEntry = extraData.mpSoundEntry;
			p->mlVoiceEntryId = p->mpVoiceEntry->GetId();
		}
		break;
	}
	case kOhplGa2DeathResetGame: p->ResetGame(); break;
	case kOhplGa2DeathFlash: gpBase->mpEffectHandler->GetFlash()->Start(0.7f, 1.2f, 2.5f); break;
	case kOhplGa2DeathAwakenSound: gpBase->mpHelpFuncs->PlayGuiSoundData(p->msAwakenSound, eSoundEntryType_Gui); break;
	case kOhplGa2DeathReset:
	{
		tString sTempCat = p->msHintCat;
		tString sTempEntry = p->msHintEntry;
		p->Reset();
		p->msHintCat = sTempCat;
		p->msHintEntry = sTempEntry;
		break;
	}
	case kOhplGa2DeathAutoload:
		if (gpBase->mpSaveHandler->AutoLoad(true) == false)
		{
			p->mbToMainMenu = true;
		}
		break;
	case kOhplGa2DeathExit: gpBase->mpEngine->Exit(); break;
	}
}

float cLuxPlayerDeath::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerDeath *p = (cLuxPlayerDeath*)apCtx;
	switch(alKind)
	{
	case kOhplA2cPerma: return p->mpPlayer->UsePermaDeath() ? 1.0f : 0.0f;
	case kOhplA2cHardActive: return gpBase->mbHardMode && gpBase->mpPlayer->IsActive() ? 1.0f : 0.0f;
	case kOhplA2bQVoiceDone:
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		return p->mpVoiceEntry==NULL || pSoundHandler->IsValid(p->mpVoiceEntry, p->mlVoiceEntryId)==false ? 1.0f : 0.0f;
	}
	}
	return 0;
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
	if(mS.mbActive==false) return;

	gpBase->mpGameHudSet->DrawGfx(mpWhiteModGfx,gpBase->mvHudVirtualStartPos+ cVector3f(0,0,3), gpBase->mvHudVirtualSize,cColor(1-mS.mfFadeAlpha,1));

	if(mS.mbShowHint==false) return; //Skip death hint

	cVector2f vFontSize = 32;
	float fSizeMul = 1.5f;
	float fSizeMulExtra =  (1- mS.mfTextAlpha1)*4;
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
			float fYAdd = sin(mS.mfT*0.5f + fTAdd) * 5.0f + cos(mS.mfT*0.97f - fTAdd*2.73f) * 3.5f;

			tWString sChar = cString::SubW(sStr, i,1);
			cVector3f vPos = cVector3f(fX, fY + fYAdd - vFontSize.y*0.5f*fMul, 6);
			cVector2f vSize = cVector2f(vFontSize.x, vFontSize.y + fabs(fYAdd)*1.5f) * fMul;

			gpBase->mpGameHudSet->DrawFont(sChar, mpFont,vPos,vSize, cColor(1, mS.mfTextAlpha1),eFontAlign_Left);

			float fBlurAlpha = 0;
			if(mS.mfTextAlpha1 < 0.5f)	fBlurAlpha = mS.mfTextAlpha1 / 0.5f;
			else					fBlurAlpha = 1 - (mS.mfTextAlpha1-0.5f) / 0.5f;

			gpBase->mpGameHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.05,vSize.y*0.05, -1), vSize*1.1, cColor(1, fBlurAlpha * 0.3f ),eFontAlign_Right);
			gpBase->mpGameHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.15,vSize.y*0.15, -2), vSize*1.3, cColor(1, fBlurAlpha * 0.2f ),eFontAlign_Right);

			fX += mpFont->GetLength(vFontSize* fMul, sChar.c_str());
		}

		fY += vFontSize.y * 1.35f;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::OnPressButton()
{
	if(mS.mbActive==false || mS.mfFadeAlpha < 1) return;

	mS.mfTextOnScreenCount = 100;
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
	mS.mfMaxMovement = 0.5f;
	mS.mfMaxRotation = cMath::ToRad(15);

	mpHeadShape = NULL;
}

cLuxPlayerLean::~cLuxPlayerLean()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLean::Reset()
{
	ohpl_a2c_lean_reset(&mS);

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
	OhplA2bHost h = {this, HostOp, HostQuery, NULL};
	ohpl_a2c_lean_update(&mS, &h, afTimeStep);
}

void cLuxPlayerLean::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerLean *p = (cLuxPlayerLean*)apCtx;
	switch(alKind)
	{
	case kOhplA2cLeanRoll: p->mpPlayer->FadeLeanRollTo(afA, afB, afC); break;
	case kOhplA2cLeanHead: p->mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Lean, cVector3f(afA,0,0), afB, afC); break;
	}
}

float cLuxPlayerLean::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerLean *p = (cLuxPlayerLean*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQDead: return p->mpPlayer->IsDead() ? 1.0f : 0.0f;
	case kOhplA2cLeanCollide:
	{
		cCamera *pCam = p->mpPlayer->GetCamera();
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();

		iCharacterBody *pCharBody = p->mpPlayer->GetCharacterBody();
		float fHeightAdd = pCharBody->GetSize().y + p->mpPlayer->GetCameraPosAdd().y;
		cVector3f vStartPos = pCharBody->GetFeetPosition() + cVector3f(0,fHeightAdd,0);

		cVector3f vPos = vStartPos + pCam->GetRight() * afA;

		return pPhysicsWorld->CheckShapeWorldCollision(NULL,p->mpHeadShape, cMath::MatrixTranslate(vPos),NULL,false,true,NULL,false) ? 1.0f : 0.0f;
	}
	}
	return 0;
}

//-----------------------------------------------------------------------


void cLuxPlayerLean::SetLean(float afMul)
{
	ohpl_a2c_lean_set(&mS, afMul);
}

//-----------------------------------------------------------------------

void cLuxPlayerLean::AddLean(float afAdd)
{
	ohpl_a2c_lean_add(&mS, afAdd);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HUD EFFECT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHudEffect::cLuxPlayerHudEffect(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHudEffect")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	memset(&mS, 0, sizeof(mS));

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
	OhplA2bHost h = {this, NULL, HostQuery, HostFill};
	ohpl_a2c_hud_add_splash(&mS, &h, (int32_t)aType);
}

float cLuxPlayerHudEffect::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerHudEffect *p = (cLuxPlayerHudEffect*)apCtx;
	switch(alKind)
	{
	case kOhplA2cHudImages: return (float)p->mvDamageTypes[(int)afA].mvImages.size();
	case kOhplA2bQRand: return cMath::RandRectf(afA, afB);
	case kOhplA2bQRandInt: return (float)cMath::RandRectl((int)afA, (int)afB);
	}
	return 0;
}

void cLuxPlayerHudEffect::HostFill(void *apCtx, uint32_t alKind, float *apBuf)
{
	cLuxPlayerHudEffect *p = (cLuxPlayerHudEffect*)apCtx;
	switch(alKind)
	{
	case kOhplA2cFHudImageSize:
	{
		cVector2f vSize = p->mvDamageTypes[(int)apBuf[0]].mvImages[(int)apBuf[1]]->GetActiveSize();
		apBuf[0] = vSize.x;
		apBuf[1] = vSize.y;
		break;
	}
	case kOhplA2cFHudCenter:
		apBuf[0] = gpBase->mvHudVirtualCenterSize.x;
		apBuf[1] = gpBase->mvHudVirtualCenterSize.y;
		break;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Flash(const cColor& aColor, eGuiMaterial aFlashMaterial, float afInTime, float afOutTime)
{
	mFlashColor = aColor;
	mFlashMaterial = aFlashMaterial;
	ohpl_a2c_hud_flash(&mS, afInTime, afOutTime);
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
	ohpl_a2c_hud_update(&mS, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Reset()
{
	ohpl_a2c_hud_reset(&mS);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawSplashes(float afFrameTime)
{
	for(uint32_t i=0; i<mS.mlCount; ++i)
	{
		const OhplA2cSplash &splash = mS.mvSplashes[i];
		cGuiGfxElement *pImage = mvDamageTypes[splash.mlImage>>8].mvImages[splash.mlImage&255];

		gpBase->mpGameHudSet->DrawGfx(pImage, splash.mvPos, cVector2f(splash.mvSize[0], splash.mvSize[1]), cColor(1, splash.mfAlpha * splash.mfAlphaMul));
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawFlash(float afFrameTime)
{
	if(mS.mfFlashAlpha <=0 || mS.mbFlashActive==false) return;

	cColor col = mFlashColor;

	col.a *= mS.mfFlashAlpha;

	gpBase->mpGameHudSet->DrawGfx(mpFlashGfx,gpBase->mvHudVirtualStartPos +cVector3f(0,0,0), gpBase->mvHudVirtualSize, col, mFlashMaterial);
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
	mS.mfRadiusAdd = gpBase->mpGameCfg->GetFloat("Player_Darkness","RadiusAdd",0);
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
	ohpl_a2b_light_reset(&mS);
}

//-----------------------------------------------------------------------

void cLuxPlayerLightLevel::Update(float afTimeStep)
{
	OhplA2bHost h = {this, NULL, HostQuery, HostFill};
	ohpl_a2b_light_update(&mS, &h, afTimeStep);
}

void cLuxPlayerLightLevel::SampleLevels(const cVector3f *apPos, float *apExt, float *apNormal)
{
	std::vector<iLight*> vSkipLights;
	vSkipLights.push_back(mpPlayer->GetHelperInDarkness()->GetAmbientLight());
	for(int i=0; i<5; ++i)
	{
		apExt[i] = gpBase->mpMapHelper->GetLightLevelAtPos(apPos[i], &vSkipLights, mS.mfRadiusAdd);
		apNormal[i] = gpBase->mpMapHelper->GetLightLevelAtPos(apPos[i], &vSkipLights, 0);
	}
}

float cLuxPlayerLightLevel::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerLightLevel *p = (cLuxPlayerLightLevel*)apCtx;
	if(alKind == kOhplA2bQLanternActive) return p->mpPlayer->GetHelperLantern()->IsActive() ? 1.0f : 0.0f;
	return 0;
}

void cLuxPlayerLightLevel::HostFill(void *apCtx, uint32_t alKind, float *apBuf)
{
	cLuxPlayerLightLevel *p = (cLuxPlayerLightLevel*)apCtx;
	if(alKind == kOhplA2bFBody)
	{
		iCharacterBody *pCharBody = p->mpPlayer->GetCharacterBody();
		cVector3f vPos = pCharBody->GetPosition();
		cVector3f vSize = pCharBody->GetSize();
		cVector3f vForward = pCharBody->GetForward();
		memcpy(apBuf, &vPos, sizeof(float)*3);
		memcpy(apBuf+3, &vSize, sizeof(float)*3);
		memcpy(apBuf+6, &vForward, sizeof(float)*3);
	}
	else if(alKind == kOhplA2bFLights)
	{
		cVector3f vTestPos[5];
		memcpy(vTestPos, apBuf, sizeof(float)*15);
		p->SampleLevels(vTestPos, apBuf+16, apBuf+21);
	}
}


//-----------------------------------------------------------------------



void cLuxPlayerLightLevel::OnMapEnter(cLuxMap *apMap)
{
	ohpl_a2b_light_map_enter(&mS);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER IN DARKNESS
//////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------

cLuxPlayerInDarkness::cLuxPlayerInDarkness(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerInDarkness")
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	mS.k.mfMinDarknessLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","MinLightLevel",0);
	
	mS.k.mfAmbientLightMinLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightMinLightLevel",0);
	mfAmbientLightRadius = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightRadius",0);
	mfAmbientLightIntensity = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightIntensity",0);
	mfAmbientLightFadeInTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeInTime",0);
	mfAmbientLightFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeOutTime",0);
	mAmbientLightColor = gpBase->mpGameCfg->GetColor("Player_Darkness","AmbientLightColor",cColor(0));

	msLoopSoundFile  = gpBase->mpGameCfg->GetString("Player_Darkness", "LoopSoundFile","");
	mfLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundVolume",0);
	mS.k.mfLoopSoundStartupTime = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundStartupTime",0);
	mfLoopSoundFadeInSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeInSpeed",0);
	mfLoopSoundFadeOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeOutSpeed",0);

	mS.k.mfSanityLossPerSecond = gpBase->mpGameCfg->GetFloat("Player_Darkness", "SanityLossPerSecond",0);
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
	ohpl_a2b_dark_reset(&mS);
	mpAmbientLight =NULL;
	mpLoopSound = NULL;
}

//-----------------------------------------------------------------------


void cLuxPlayerInDarkness::Update(float afTimeStep)
{
	OhplA2bHost h = {this, HostOp, HostQuery, HostFill};
	ohpl_a2b_dark_update(&mS, &h, afTimeStep);
}

void cLuxPlayerInDarkness::HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayerInDarkness *p = (cLuxPlayerInDarkness*)apCtx;
	switch(alKind)
	{
	case kOhplA2bAmbientPos:
		p->mpAmbientLight->SetPosition(p->mpPlayer->GetCamera()->GetPosition() - cVector3f(0,0.3f,0));
		break;
	case kOhplGa2AmbientOff: p->mpAmbientLight->FadeTo(cColor(0.0f, 0.0f),p->mpAmbientLight->GetRadius(),p->mfAmbientLightFadeOutTime); break;
	case kOhplGa2AmbientOn:
		if(alI)
			p->mpAmbientLight->FadeTo(p->mAmbientLightColor*p->mfAmbientLightIntensity * 0.75f , p->mpAmbientLight->GetRadius(), p->mfAmbientLightFadeInTime * 2.5f);
		else
			p->mpAmbientLight->FadeTo(p->mAmbientLightColor*p->mfAmbientLightIntensity, p->mpAmbientLight->GetRadius(), p->mfAmbientLightFadeInTime);
		break;
	case kOhplGa2DarknessLowerSanity: p->mpPlayer->LowerSanity(afA, true); break;
	case kOhplGa2HintDarkness: gpBase->mpHintHandler->Add("DarknessDecrease", kTranslate("Hints", "DarknessDecrease"), 0); break;
	}
}

float cLuxPlayerInDarkness::HostQuery(void *apCtx, uint32_t alKind, float afA, float afB)
{
	cLuxPlayerInDarkness *p = (cLuxPlayerInDarkness*)apCtx;
	switch(alKind)
	{
	case kOhplA2bQHardMode: return gpBase->mbHardMode ? 1.0f : 0.0f;
	case kOhplA2bQFlashbackActive: return p->mpPlayer->GetHelperFlashback()->IsActive() ? 1.0f : 0.0f;
	case kOhplA2bQDrainDisabled: return p->mpPlayer->GetSanityDrainDisabled() ? 1.0f : 0.0f;
	case kOhplA2bQEmotionFlash: return gpBase->mpEffectHandler->GetEmotionFlash()->IsActive() ? 1.0f : 0.0f;
	}
	return 0;
}

void cLuxPlayerInDarkness::HostFill(void *apCtx, uint32_t alKind, float *apBuf)
{
	cLuxPlayerInDarkness *p = (cLuxPlayerInDarkness*)apCtx;
	if(alKind != kOhplA2bFDarkLevels) return;
	apBuf[0] = p->mpPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	apBuf[1] = p->mpPlayer->GetHelperLightLevel()->GetNormalLightLevel();
}

//-----------------------------------------------------------------------
void cLuxPlayerInDarkness::OnMapEnter(cLuxMap *apMap)
{
	ohpl_a2b_dark_map_enter(&mS);
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

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::SetActive(bool abX)
{
	OhplA2bHost h = {this, HostOp, HostQuery, HostFill};
	ohpl_a2b_dark_set_active(&mS, &h, abX);
}

//-----------------------------------------------------------------------
