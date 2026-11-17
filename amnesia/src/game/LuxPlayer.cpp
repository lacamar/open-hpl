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

#include "LuxPlayer.h"

#include "LuxMap.h"
#include "LuxInventory.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxHelpFuncs.h"

#include "LuxPlayerState_Normal.h"
#include "LuxPlayerState_HandObject.h"
#include "LuxPlayerState_UseItem.h"
#include "LuxPlayerState_InteractGrab.h"
#include "LuxPlayerState_InteractPush.h"
#include "LuxPlayerState_InteractSlide.h"
#include "LuxPlayerState_InteractSwingDoor.h"
#include "LuxPlayerState_InteractLever.h"
#include "LuxPlayerState_InteractWheel.h"
#include "LuxPlayerState_Ladder.h"
#include "LuxHintHandler.h"
#include "LuxEffectHandler.h"

#include "LuxEntity.h"

#include "LuxMoveState_Normal.h"
#include "LuxMoveState_ClimbLedge.h"

#include "LuxPlayerHelpers.h"
#include "LuxA2bDump.h"
#include "LuxA2cDump.h"
#include "LuxPlayerHands.h"

#include "LuxAreaNodes.h"
#include "RustLuxPlayer.h"


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayer::cLuxPlayer() : iLuxUpdateable("LuxPlayer"), iLuxCollideCallbackContainer()
{
	//////////////////////////////////
	// Init data pointers
	mpCharBody = NULL;
	mpTerrorSound = NULL;

	//////////////////////////////////
	// Create and setup camera
	mpCamera = gpBase->mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Walk);

	mbFreeCameraActive = false;
	mfFreeCameraSpeed = 0.1f;

	//TODO: More setup?
	cVector2f vScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	mfAspect = vScreenSize.x / vScreenSize.y;
	mfFOV = cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_General","FOV", 0));
	
	mpCamera->SetFOV(mfFOV);
	mpCamera->SetAspect(mfAspect);
	mpCamera->SetFarClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","FarClipPlane",0));
	mpCamera->SetNearClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","NearClipPlane",0));
	mpCamera->SetPitchLimits(-cMath::ToRad(70), cMath::ToRad(70) );

	//////////////////////////////////
	// Init General properties
	mCore.mfHeadSpinDamageSpeed = gpBase->mpGameCfg->GetFloat("Player_General","HeadSpinDamageSpeed",0);
	mCore.mfHeadSpinDeacc = gpBase->mpGameCfg->GetFloat("Player_General","HeadSpinDeacc",0);

	msTerrorSound = gpBase->mpGameCfg->GetString("Player_General","TerrorSound","");

	mCore.mfTerrorIncSpeed = gpBase->mpGameCfg->GetFloat("Player_General","TerrorIncSpeed",0);
	mCore.mfTerrorDecSpeed = gpBase->mpGameCfg->GetFloat("Player_General","TerrorDecSpeed",0);

	ohpl_a2d_init(&mD, gpBase->mpGameCfg->GetInt("Player_General","MaxPrevSpeedsSaved",0), gpBase->mpGameCfg->GetFloat("Player_General","AutoKillYPos",0));

	//////////////////////////////////
	// Init body properties
	mvBodySize = gpBase->mpGameCfg->GetVector3f("Player_Body","Size",0);
	mvBodyCrouchSize = gpBase->mpGameCfg->GetVector3f("Player_Body","CrouchSize",0);
	
	mvCameraPosAdd = gpBase->mpGameCfg->GetVector3f("Player_Body","CameraPosAdd",0);

	mfDefaultMass = gpBase->mpGameCfg->GetFloat("Player_Body","Mass",0);


	//////////////////////////////////
	// Init some other variables
	mbUsePermaDeath = gpBase->mbPTestActivated;
	mbUsesDragFootsteps = false;
	mbFadingPitch = false;
	
	//////////////////////////////////
	// Create states
	mvStates.resize(eLuxPlayerState_LastEnum);

	mvStates[eLuxPlayerState_Normal] = hplNew( cLuxPlayerState_Normal, (this) );
	mvStates[eLuxPlayerState_HandObject] = hplNew( cLuxPlayerState_HandObject, (this) );
	mvStates[eLuxPlayerState_UseItem] = hplNew( cLuxPlayerState_UseItem, (this) );
	mvStates[eLuxPlayerState_InteractGrab] = hplNew( cLuxPlayerState_InteractGrab, (this) );
	mvStates[eLuxPlayerState_InteractPush] = hplNew( cLuxPlayerState_InteractPush, (this) );
	mvStates[eLuxPlayerState_InteractSlide] = hplNew( cLuxPlayerState_InteractSlide, (this) );
	mvStates[eLuxPlayerState_InteractSwingDoor] = hplNew( cLuxPlayerState_InteractSwingDoor, (this) );
	mvStates[eLuxPlayerState_InteractLever] = hplNew( cLuxPlayerState_InteractLever, (this) );
	mvStates[eLuxPlayerState_InteractWheel] = hplNew( cLuxPlayerState_InteractWheel, (this) );
	mvStates[eLuxPlayerState_Ladder] = hplNew( cLuxPlayerState_Ladder, (this) );

	//////////////////////////////////
	// Create move states
	mvMoveStates.resize(eLuxMoveState_LastEnum);

	mvMoveStates[eLuxMoveState_Normal] = hplNew( cLuxMoveState_Normal, (this) );
	mvMoveStates[eLuxMoveState_ClimbLedge] = hplNew( cLuxMoveState_ClimbLedge, (this) );

	//////////////////////////////////
	// Create head pos adds
	mvHeadPosAdds.resize(eLuxHeadPosAdd_LastEnum);


	//////////////////////////////////
	// Create graphics
	mpFocusFont = NULL;
	
	//////////////////////////////////
	// Create helpers
	mpHudEffect = hplNew( cLuxPlayerHudEffect, (this) );
	mvHelpers.push_back(mpHudEffect);

	mpHelperLightLevel = hplNew( cLuxPlayerLightLevel, (this) );
	mvHelpers.push_back(mpHelperLightLevel);

	mpHelperInDarkness = hplNew( cLuxPlayerInDarkness, (this) );
	mvHelpers.push_back(mpHelperInDarkness);

	mpLean = hplNew( cLuxPlayerLean, (this) );
	mvHelpers.push_back(mpLean);

	mpDeath = hplNew( cLuxPlayerDeath, (this) );
	mvHelpers.push_back(mpDeath);

	mpHands = hplNew( cLuxPlayerHands, (this) );
	mvHelpers.push_back(mpHands);

	mpLantern = hplNew( cLuxPlayerLantern, (this) );
	mvHelpers.push_back(mpLantern);

	mpSanity = hplNew( cLuxPlayerSanity, (this) );
	mvHelpers.push_back(mpSanity);

	mpLookAt = hplNew( cLuxPlayerLookAt, (this) );
	mvHelpers.push_back(mpLookAt);

	mpFlashback = hplNew( cLuxPlayerFlashback, (this) );
	mvHelpers.push_back(mpFlashback);

	mpHurt = hplNew( cLuxPlayerHurt, (this) );
	mvHelpers.push_back(mpHurt);

	mpSpawnPS = hplNew( cLuxPlayerSpawnPS, (this) );
	mvHelpers.push_back(mpSpawnPS);

	mpCamDirEffects = hplNew( cLuxPlayerCamDirEffects, (this));
	mvHelpers.push_back(mpCamDirEffects);

	mpInsanityCollapse = hplNew( cLuxPlayerInsanityCollapse, (this));
	mvHelpers.push_back(mpInsanityCollapse);
}

//-----------------------------------------------------------------------

cLuxPlayer::~cLuxPlayer()
{
	DestroyCollideCallbacks();

	STLDeleteAll(mvHelpers);
	STLDeleteAll(mvStates);
	STLDeleteAll(mvMoveStates);

	ohpl_a2d_free(&mD);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayer::LoadFonts()
{
	tString sFontFile = gpBase->mpGameCfg->GetString("Player_General","FocusTextFont","");
	mpFocusFont = LoadFont(sFontFile);

	RunHelperLuxMessage(eLuxUpdateableMessage_LoadFonts,NULL);
}

//-----------------------------------------------------------------------

void cLuxPlayer::Reset()
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_reset(&mCore, &mD, &dh);

	msWaterStepSound = "";
	mvHeadSpeed =0;
	mpEntityInFocus = NULL;
	mpBodyInFocus = NULL;
	mpCharBody = NULL;
	msCurrentPermaDeathSound = "";
	msFocusText = _W("");
	msLastFocusText = _W("");

	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	if(mpTerrorSound && pSoundHandler->IsValid(mpTerrorSound, mlTerrorSoundID)) mpTerrorSound->Stop();
	mpTerrorSound = NULL;
	mlTerrorSoundID = -1;

	for(size_t i=0; i<mvHeadPosAdds.size(); ++i)
	{
		mvHeadPosAdds[i].mbMoving = false;
		mvHeadPosAdds[i].mvAdd = 0;
	}

	DestroyCollideCallbacks();

	m_setTerrorEnemies.clear();

	mpCamera->SetRoll(0.0f);
	mpCamera->SetPitch(0.0f);
	mpCamera->SetYaw(0.0f);
	mpCamera->SetFOV(mfFOV*mfFOVMul);
	mpCamera->SetAspect(mfAspect*mfAspectMul);

	RunHelperMessage(eUpdateableMessage_Reset,0);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayer::Update(float afTimeStep)
{
	if(OhplA2bTick(this)) return;
	OhplA2cPre(this, afTimeStep);

	OhplA2Host h = {this, A2HostOp, A2HostQuery};
	OhplA2dHost dh = DispHost();
	ohpl_a2d_update(&mCore, &mD, &h, &dh, afTimeStep);

	OhplA2cPost(this);
}

//-----------------------------------------------------------------------

void cLuxPlayer::PostUpdate(float afTimeStep)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_post_update(&mD, &dh, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnDraw(float afFrameTime)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_on_draw(&mCore, &mD, &dh, afFrameTime);
}

//-----------------------------------------------------------------------

void cLuxPlayer::LoadUserConfig()
{
	mbShowCrossHair = gpBase->mpUserConfig->GetBool("Game", "ShowCrossHair", true);
	mFocusIconStyle = StringToFocusIconStyle(gpBase->mpUserConfig->GetString("Game", "FocusIconStyle", "Default"));

	RunHelperLuxMessage(eLuxUpdateableMessage_LoadUserConfig,0);
}

void cLuxPlayer::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowCrossHair", mbShowCrossHair);
	gpBase->mpUserConfig->SetString("Game", "FocusIconStyle", FocusIconStyleToString(mFocusIconStyle));

	RunHelperLuxMessage(eLuxUpdateableMessage_SaveUserConfig,0);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnMapEnter(cLuxMap *apMap)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_map_enter(&mCore, &mD, &dh, apMap);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnMapLeave(cLuxMap *apMap)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_map_leave(&mCore, &mD, &dh, apMap);
}

//-----------------------------------------------------------------------

void cLuxPlayer::CreateWorldEntities(cLuxMap *apMap)
{
	////////////////////////////////
	//Create and Init character body
	CreateCharacterBody(apMap->GetPhysicsWorld());
	
	////////////////////////
	// Run Helper message
	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->CreateWorldEntities(apMap);
}

void cLuxPlayer::DestroyWorldEntities(cLuxMap *apMap)
{
	////////////////////////////////
	//Destroy character body
	if(mpCharBody)
	{
		apMap->GetPhysicsWorld()->DestroyCharacterBody(mpCharBody);
		mpCharBody = NULL;
	}

	////////////////////////
	// Run Helper message
	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->DestroyWorldEntities(apMap);
}
//-----------------------------------------------------------------------

void cLuxPlayer::OnEnterContainer(const tString& asOldContainer)
{
	
}

void cLuxPlayer::OnLeaveContainer(const tString& asNewContainer)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_on_leave_container(&mD, &dh);
}

//-----------------------------------------------------------------------

void cLuxPlayer::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	GetCurrentStateData()->RenderSolid(apFunctions);

	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->RenderSolid(apFunctions);
}

void cLuxPlayer::RenderTrans(cRendererCallbackFunctions* apFunctions)
{
	GetCurrentStateData()->RenderTrans(apFunctions);
}


//-----------------------------------------------------------------------

void cLuxPlayer::GiveDamage(float afAmount, int alStrength, eLuxDamageType aType, bool abSpinHead, bool abLethal)
{
	RunVitals(kOhplGa2GiveDamage, afAmount, (int)aType, (abSpinHead ? 1u : 0u) | (abLethal ? 2u : 0u));
}

//-----------------------------------------------------------------------

void cLuxPlayer::GiveSanityDamage(float afAmount)
{
	gpBase->mpHintHandler->Add("SanityHit", kTranslate("Hints", "SanityHit"), 0);

	LowerSanity(afAmount, true);

	mpSanity->StartHit();
}


void cLuxPlayer::LowerSanity(float afAmount, bool abUseEffect)
{
	RunVitals(kOhplGa2LowerSanityOp, afAmount, 0, abUseEffect ? 1u : 0u);
}

//-----------------------------------------------------------------------

void cLuxPlayer::Move(eCharDir aDir, float afMul)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_move(&mD, &dh, (int)aDir, afMul);
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddYaw(float afAmount)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_add_yaw(&mD, &dh, afAmount);
}

void cLuxPlayer::AddPitch(float afAmount)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_add_pitch(&mD, &dh, afAmount);
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetLean(float afMul)
{
	mpLean->SetLean(afMul);
}

void cLuxPlayer::AddLean(float afAdd)
{
	mpLean->AddLean(afAdd);
}

//-----------------------------------------------------------------------

void cLuxPlayer::Scroll(float afAmount)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_scroll(&mD, &dh, afAmount);
}

//-----------------------------------------------------------------------


void cLuxPlayer::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_do_action(&mD, &dh, (int)aAction, abPressed);
}

//-----------------------------------------------------------------------


void cLuxPlayer::Run(bool abPressed)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_run(&mD, &dh, abPressed);
}

void cLuxPlayer::Jump(bool abPressed)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_jump(&mD, &dh, abPressed);
}

void cLuxPlayer::Crouch(bool abPressed)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_crouch(&mD, &dh, abPressed);
}

//-----------------------------------------------------------------------

void cLuxPlayer::ChangeState(eLuxPlayerState aState)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_change_state(&mD, &dh, (int)aState);
}

//-----------------------------------------------------------------------

void cLuxPlayer::ChangeMoveState(eLuxMoveState aState)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_change_move_state(&mD, &dh, (int)aState);
}

//-----------------------------------------------------------------------

void cLuxPlayer::MoveHeadPosAdd(eLuxHeadPosAdd aType, const cVector3f& avGoal, float afSpeed, float afSlowdownDist)
{
	mvHeadPosAdds[aType].mbMoving = true;
	mvHeadPosAdds[aType].mvMoveGoal = avGoal;
	mvHeadPosAdds[aType].mfMoveSpeed = afSpeed;
	mvHeadPosAdds[aType].mfMoveSlowDownDist = afSlowdownDist;
}

//-----------------------------------------------------------------------

void cLuxPlayer::PlaceAtStartNode(cLuxNode_PlayerStart *apNode)
{
	ChangeState(eLuxPlayerState_Normal);

	SetIsInWater(false);//Needs to rest this or else player will spalsh when walking on the ground.
	if(mpSpawnPS->IsActive())
		mpSpawnPS->RespawnAll(); //Needs to respawn all!

	mpCharBody->StopMovement(); //clear stale fall speed
	mpCharBody->SetFeetPosition(apNode->GetPosition());
	mpCharBody->SetYaw(apNode->GetAngle());
	mpCamera->SetYaw(apNode->GetAngle());
	mpCamera->SetPitch(0);
	mpCharBody->Update(0.001f);
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetHeadPosAdd(eLuxHeadPosAdd aType, const cVector3f& avVector)
{
	mvHeadPosAdds[aType].mvAdd = avVector;
}

const cVector3f& cLuxPlayer::GetHeadPosAdd(eLuxHeadPosAdd aType)
{
	return mvHeadPosAdds[aType].mvAdd;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetActive(bool abX)
{
	mbActive = abX;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetHealth(float afX)
{
	RunVitals(kOhplGa2SetHealthOp, afX, 0, 0);
}

void cLuxPlayer::SetSanity(float afX)
{
	RunVitals(kOhplGa2SetSanityOp, afX, 0, 0);
}

void cLuxPlayer::SetLampOil(float afX)
{
	mfLampOil = afX;
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddHealth(float afX)
{
	RunVitals(kOhplGa2AddHealthOp, afX, 0, 0);
}

void cLuxPlayer::AddSanity(float afX, bool abShowEffect)
{
	RunVitals(kOhplGa2AddSanityOp, afX, 0, abShowEffect ? 1u : 0u);
}

void cLuxPlayer::AddLampOil(float afX)
{
	RunVitals(kOhplGa2AddLampOilOp, afX, 0, 0);
}

void cLuxPlayer::RunVitals(int alWhich, float afAmount, int alType, unsigned int alFlags)
{
	OhplA2Host h = {this, A2HostOp, A2HostQuery};
	ohpl_a2_vitals(&mCore, &h, alWhich, afAmount, alType, alFlags);
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddTerrorEnemy(iLuxEnemy *apEnemy)
{
	m_setTerrorEnemies.insert(apEnemy);
}

void cLuxPlayer::RemoveTerrorEnemy(iLuxEnemy *apEnemy)
{
	m_setTerrorEnemies.erase(apEnemy);
}

void cLuxPlayer::ClearTerrorEnemies()
{
	m_setTerrorEnemies.clear();
}

void cLuxPlayer::StopTerrorSound()
{
	if(mpTerrorSound && gpBase->mpEngine->GetSound()->GetSoundHandler()->IsValid(mpTerrorSound, mlTerrorSoundID))
	{
		mpTerrorSound->Stop();
		mpTerrorSound = NULL;
		mlTerrorSoundID = -1;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetFocusText(const tWString &asText)
{
	if(msFocusText == asText) return;

	msLastFocusText = msFocusText;
	msFocusText = asText;
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddCoins(int alX)
{
	mlCoins += alX;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetCurrentHandObjectDrawn(bool abX)
{
	cLuxInventory_Item *pItem = gpBase->mpInventory->GetEquippedHandItem();
	if(pItem == NULL) return;

	////////////////
	// Draw
    if(abX)
	{
		mpHands->SetActiveHandObject(OhplInvSubType(pItem));
	}
	////////////////
	// Holster
	else
	{
		mpHands->SetActiveHandObject("");
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::FadeFOVMulTo(float afX, float afSpeed)
{
	mfFOVMulGoal = afX;
	mfFOVMulSpeed = afSpeed;
}

void cLuxPlayer::FadeAspectMulTo(float afX, float afSpeed)
{
	mfAspectMulGoal = afX;
	mfAspectMulSpeed = afSpeed;
}

void cLuxPlayer::FadeRollTo(float afX, float afSpeedMul, float afMaxSpeed)
{
	mfRollGoal = afX;
	mfRollSpeedMul = afSpeedMul;
	mfRollMaxSpeed = afMaxSpeed;
}

void cLuxPlayer::FadePitchTo(float afX, float afSpeedMul, float afMaxSpeed)
{
	afX = ohpl_ga2_clamp_pitch_goal(afX, mpCamera->GetPitchMinLimit(), mpCamera->GetPitchMaxLimit());
	mfPitchGoal = afX;
	mfPitchSpeedMul = afSpeedMul;
	mfPitchMaxSpeed = afMaxSpeed;
	mbFadingPitch = true;
}

void cLuxPlayer::FadeLeanRollTo(float afX, float afSpeedMul, float afMaxSpeed)
{
	mCore.cam.mfLeanRollGoal = afX;
	mCore.cam.mfLeanRollSpeedMul = afSpeedMul;
	mCore.cam.mfLeanRollMaxSpeed = afMaxSpeed;
}

void cLuxPlayer::SetRoll(float afX)
{
	mfRoll = afX;
	mfRollGoal = afX;

}

//-----------------------------------------------------------------------

eLuxFocusIconStyle cLuxPlayer::StringToFocusIconStyle(const tString& asX)
{
	tString sLowCase = cString::ToLowerCase(asX);
	
	if(sLowCase=="default")
		return eLuxFocusIconStyle_Default;
	else if(sLowCase=="simple")
		return eLuxFocusIconStyle_Simple;
	else
		Log("Error converting string to FocusIconStyle : '%s'\n", asX.c_str());

	return eLuxFocusIconStyle_Default;
}

tString cLuxPlayer::FocusIconStyleToString(eLuxFocusIconStyle aX)
{
	if(aX==eLuxFocusIconStyle_Default)
		return "Default";
	else if(aX==eLuxFocusIconStyle_Simple)
		return "Simple";

	return "Default";
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// Free cam
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayer::SetFreeCamActive(bool abX)
{
	OhplA2dHost dh = DispHost();
	ohpl_a2d_set_free_cam(&mD, &dh, abX);
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetFreeCamSpeed(float afSpeed)
{
	mfFreeCameraSpeed = afSpeed;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayer::RunHelperMessage(eUpdateableMessage aMessage, float afX)
{
	for(size_t i=0; i<mvHelpers.size(); ++i)
	{
		iLuxPlayerHelper *pHelper = mvHelpers[i];
		pHelper->RunMessage(aMessage, afX);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::RunHelperLuxMessage(eLuxUpdateableMessage aMessage, void *apData)
{
	for(size_t i=0; i<mvHelpers.size(); ++i)
	{
		iLuxPlayerHelper *pHelper = mvHelpers[i];
		pHelper->LuxRunMessage(aMessage, apData);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateHeadPosAdd(float afTimeStep)
{
	std::vector<OhplGa2HeadPosAdd> vAdds(mvHeadPosAdds.size());
	for(size_t i=0; i<vAdds.size(); ++i)
	{
		const cLuxHeadPosAdd &add = mvHeadPosAdds[i];
		vAdds[i].mbMoving = add.mbMoving;
		vAdds[i].mvMoveGoal = add.mvMoveGoal;
		vAdds[i].mfMoveSpeed = add.mfMoveSpeed;
		vAdds[i].mfMoveSlowDownDist = add.mfMoveSlowDownDist;
		vAdds[i].mvAdd = add.mvAdd;
	}
	cVector3f vAdd, vSum;
	ohpl_ga2_update_head_pos_add(vAdds.data(), vAdds.size(), &mvCameraPosAdd, afTimeStep, &vAdd, &vSum);
	for(size_t i=0; i<vAdds.size(); ++i)
	{
		mvHeadPosAdds[i].mbMoving = vAdds[i].mbMoving != 0;
		mvHeadPosAdds[i].mvAdd = vAdds[i].mvAdd;
	}
	mvHeadPosAddSum = vSum;
	mpCharBody->SetCameraPosAdd(vAdd);
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateTerrorSound()
{
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	if(mfTerror > 0)
	{
		if(mpTerrorSound == NULL)
		{
			mpTerrorSound = pSoundHandler->PlayGui(msTerrorSound,true,1.0f);
			if(mpTerrorSound)
			{
				mpTerrorSound->SetVolumeMul(0.0f);
				mlTerrorSoundID = mpTerrorSound->GetId();
			}
		}
	}
	if(mpTerrorSound && pSoundHandler->IsValid(mpTerrorSound, mlTerrorSoundID))
	{
		mpTerrorSound->SetVolumeMul(mfTerror);
		if(mfTerror <= 0)
		{
			mpTerrorSound->GetChannel()->Stop();
			mpTerrorSound = NULL;
			mlTerrorSoundID = -1;
		}
	}
}

//-----------------------------------------------------------------------

float cLuxPlayer::DispCall(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, void *apP)
{
	cLuxPlayer *p = (cLuxPlayer*)apCtx;
	switch(alKind)
	{
	case kOhplA2dMsEnter: p->mvMoveStates[alI]->OnEnterState((eLuxMoveState)(int)afA); break;
	case kOhplA2dMsLeave: p->mvMoveStates[alI]->OnLeaveState((eLuxMoveState)(int)afA); break;
	case kOhplA2dMsUpdate: p->mvMoveStates[alI]->Update(afA); break;
	case kOhplA2dMsOnMove: return p->mvMoveStates[alI]->OnMove((eCharDir)(int)afA, afB);
	case kOhplA2dMsOnYaw: return p->mvMoveStates[alI]->OnAddYaw(afA);
	case kOhplA2dMsOnPitch: return p->mvMoveStates[alI]->OnAddPitch(afA);
	case kOhplA2dMsOnRun: p->mvMoveStates[alI]->OnRun(afA != 0); break;
	case kOhplA2dMsOnJump: p->mvMoveStates[alI]->OnJump(afA != 0); break;
	case kOhplA2dMsOnCrouch: p->mvMoveStates[alI]->OnCrouch(afA != 0); break;
	case kOhplA2dMsMapEnter: p->mvMoveStates[alI]->OnMapEnter(); break;
	case kOhplA2dMsOnDraw: p->mvMoveStates[alI]->OnDraw(afA); break;
	case kOhplA2dStEnter: p->mvStates[alI]->OnEnterState((eLuxPlayerState)(int)afA); break;
	case kOhplA2dStLeave: p->mvStates[alI]->OnLeaveState((eLuxPlayerState)(int)afA); break;
	case kOhplA2dStSetPrev: p->mvStates[alI]->SetPreviousState((eLuxPlayerState)(int)afA); break;
	case kOhplA2dStUpdate: p->mvStates[alI]->Update(afA); break;
	case kOhplA2dStPostUpdate: p->mvStates[alI]->PostUpdate(afA); break;
	case kOhplA2dStOnMove: return p->mvStates[alI]->OnMove((eCharDir)(int)afA, afB);
	case kOhplA2dStOnYaw: return p->mvStates[alI]->OnAddYaw(afA);
	case kOhplA2dStOnPitch: return p->mvStates[alI]->OnAddPitch(afA);
	case kOhplA2dStOnScroll: p->mvStates[alI]->OnScroll(afA); break;
	case kOhplA2dStOnAction: return p->mvStates[alI]->OnDoAction((eLuxPlayerAction)(int)afA, afB != 0);
	case kOhplA2dStOnRun: return p->mvStates[alI]->OnRun(afA != 0);
	case kOhplA2dStOnJump: return p->mvStates[alI]->OnJump(afA != 0);
	case kOhplA2dStOnCrouch: return p->mvStates[alI]->OnCrouch(afA != 0);
	case kOhplA2dStMapEnter: p->mvStates[alI]->OnMapEnter((cLuxMap*)apP); break;
	case kOhplA2dStMapLeave: p->mvStates[alI]->OnMapLeave((cLuxMap*)apP); break;
	case kOhplA2dStOnDraw: p->mvStates[alI]->OnDraw(gpBase->mpGameHudSet, afA); break;
	case kOhplA2dFreeMove:
		if(alI == eCharDir_Forward) p->mpCamera->MoveForward(afA);
		else if(alI == eCharDir_Right) p->mpCamera->MoveRight(afA);
		break;
	case kOhplA2dBodyMove: p->mpCharBody->Move((eCharDir)alI, afA); break;
	case kOhplA2dFreeYaw: p->mpCamera->AddYaw(p->mpCamDirEffects->AddAndGetPitchAdd(afA * 2.0f)); break; //sic: the pitch accumulator
	case kOhplA2dFreePitch: p->mpCamera->AddPitch(p->mpCamDirEffects->AddAndGetPitchAdd(afA * 2.0f)); break;
	case kOhplA2dYaw:
		p->mpCamera->AddYaw(p->mpCamDirEffects->AddAndGetYawAdd(afA * 2.0f));
		p->mpCharBody->SetYaw(p->mpCamera->GetYaw());
		break;
	case kOhplA2dPitch: p->mpCamera->AddPitch(p->mpCamDirEffects->AddAndGetPitchAdd(afA * 2.0f)); break;
	case kOhplA2dLanternToggle: p->mpLantern->SetActive(!p->mpLantern->IsActive(), true); break;
	case kOhplA2dCollapseActive: return p->mpInsanityCollapse->IsActive();
	case kOhplA2dHeadPosAdd: p->UpdateHeadPosAdd(afA); break;
	case kOhplA2dHelpersUpdate: p->RunHelperMessage(eUpdateableMessage_Update, afA); break;
	case kOhplA2dHelpersPost: p->RunHelperMessage(eUpdateableMessage_PostUpdate, afA); break;
	case kOhplA2dHelpersDraw: p->RunHelperMessage(eUpdateableMessage_OnDraw, afA); break;
	case kOhplA2dHelpersLuxMsg: p->RunHelperLuxMessage(alI == 0 ? eLuxUpdateableMessage_OnMapEnter : eLuxUpdateableMessage_OnMapLeave, apP); break;
	case kOhplA2dCollisionCb: p->CheckCollisionCallback("Player", gpBase->mpMapHandler->GetCurrentMap()); break;
	case kOhplA2dBodyY: return p->mpCharBody->GetPosition().y;
	case kOhplA2dBodySpeed: return p->mpCharBody->GetVelocity(afA).Length();
	case kOhplA2dClearFocusText: p->msFocusText = _W(""); p->msLastFocusText = _W(""); break;
	case kOhplA2dNullFocus: p->mpEntityInFocus = NULL; p->mpBodyInFocus = NULL; break;
	case kOhplA2dDestroyCb: p->DestroyCollideCallbacks(); break;
	case kOhplA2dClearTerror: p->m_setTerrorEnemies.clear(); break;
	case kOhplA2dCamMapLeave:
		p->mpCamera->SetRoll(0.0f);
		p->mpCamera->SetFOV(p->mfFOV*p->mfFOVMul);
		p->mpCamera->SetAspect(p->mfAspect*p->mfAspectMul);
		break;
	case kOhplA2dPauseActive: return gpBase->mpMessageHandler->IsPauseMessageActive();
	case kOhplA2dFocusCanInteract:
		return p->mpEntityInFocus && p->mpEntityInFocus->CanInteract(p->mpBodyInFocus) &&
			p->mfCurrentFocusDistance <= p->mpEntityInFocus->GetMaxFocusDistance();
	case kOhplA2dDrawCrosshair:
	{
		cGuiGfxElement *pCrossGfx = p->mvStates[p->mD.mlState]->GetCrosshair();
		if(pCrossGfx)
		{
			cVector2f vGfxSize = pCrossGfx->GetImageSize();
			cVector2f vPos = (gpBase->mvHudVirtualCenterSize - vGfxSize)/2.0f;
			cVector3f vFinalPos = cVector3f(vPos.x, vPos.y,1);
			if(p->mvStates[p->mD.mlState]->OnDrawCrossHair(pCrossGfx,vFinalPos, vGfxSize))
				gpBase->mpGameHudSet->DrawGfx(pCrossGfx,vFinalPos,vGfxSize,cColor(1,1));
		}
		break;
	}
	case kOhplA2dFocusTextState: return p->msFocusText != _W("");
	case kOhplA2dDrawFocusText:
	{
		tWStringVec vRows;
		p->mpFocusFont->GetWordWrapRows(500, 22, 22, alI ? p->msLastFocusText : p->msFocusText, &vRows);
		for(size_t i=0; i<vRows.size(); ++i)
			gpBase->mpGameHudSet->DrawFont(vRows[i],p->mpFocusFont,cVector3f(400, 450+i*24,1),22,cColor(1,afA), eFontAlign_Center);
		break;
	}
	case kOhplA2dFreecamCamera: p->mpCharBody->SetCamera(alI ? NULL : p->mpCamera); break;
	case kOhplA2dShowCrosshair: return p->mbShowCrossHair;
	}
	return 0;
}

void cLuxPlayer::A2HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD)
{
	cLuxPlayer *p = (cLuxPlayer*)apCtx;
	switch(alKind)
	{
	case kOhplGa2DamageSplash: p->mpHudEffect->AddDamageSplash((eLuxDamageType)alI); break;
	case kOhplGa2HitFlash: p->mpHudEffect->Flash(cColor(0.6f,0,0, 0.5f),eGuiMaterial_Alpha,0,0.25f); break;
	case kOhplGa2DeathStart: p->mpDeath->Start(); break;
	case kOhplGa2CollapseStart: p->mpInsanityCollapse->Start(); break;
	case kOhplGa2SanityLost: p->mpSanity->SetSanityLost(); break;
	case kOhplGa2SanityGainFlash: gpBase->mpEffectHandler->GetSanityGainFlash()->Start(); break;
	case kOhplGa2SetFov: p->mpCamera->SetFOV(afA); break;
	case kOhplGa2SetAspect: p->mpCamera->SetAspect(afA); break;
	case kOhplGa2SetPitch: p->mpCamera->SetPitch(afA); break;
	case kOhplGa2SetRoll: p->mpCamera->SetRoll(afA); break;
	case kOhplA2HeadSpinCam:
		p->mpCamera->AddPitch(afA);
		p->mpCamera->AddYaw(afB);
		p->mpCharBody->SetYaw(p->mpCamera->GetYaw());
		break;
	case kOhplA2TerrorSound: p->UpdateTerrorSound(); break;
	}
}

float cLuxPlayer::A2HostQuery(void *apCtx, uint32_t alKind)
{
	cLuxPlayer *p = (cLuxPlayer*)apCtx;
	switch(alKind)
	{
	case kOhplA2QCamPitch: return p->mpCamera->GetPitch();
	case kOhplA2QPitchMin: return p->mpCamera->GetPitchMinLimit();
	case kOhplA2QPitchMax: return p->mpCamera->GetPitchMaxLimit();
	case kOhplA2QFreeCam: return p->mbFreeCameraActive ? 1.0f : 0.0f;
	case kOhplA2QHardMode: return gpBase->mbHardMode ? 1.0f : 0.0f;
	case kOhplA2QTerrorEnemies: return !(p->m_setTerrorEnemies.empty() || p->IsDead()) ? 1.0f : 0.0f;
	case kOhplA2QFocusHide: return (gpBase->mpMessageHandler->IsPauseMessageActive() || p->msFocusText == _W("")) ? 1.0f : 0.0f;
	case kOhplA2QRandYaw: return cMath::RandRectf(-1,1);
	case kOhplA2QRandPitch: return cMath::RandRectf(0,0.5f);
	}
	return 0;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SpinHead(float afSpeed)
{
	OhplA2Host h = {this, A2HostOp, A2HostQuery};
	ohpl_a2_spin_head(&mCore, &h, afSpeed);
}

//-----------------------------------------------------------------------

void cLuxPlayer::CreateCharacterBody(iPhysicsWorld *apPhysicsWorld)
{
	mpCharBody = apPhysicsWorld->CreateCharacterBody("Player",mvBodySize);
	
	//TODO: Use config here.
	mpCharBody->SetCustomGravity(	gpBase->mpGameCfg->GetVector3f("Player_Body","GravityForce",0) );
	mpCharBody->SetCustomGravityActive(true);
	mpCharBody->SetMass(mfDefaultMass);

	mpCharBody->SetAccurateClimbing(	gpBase->mpGameCfg->GetBool("Player_Body","AccurateClimbing",false) );
	mpCharBody->SetMaxNoSlideSlopeAngle(cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_Body","MaxNoSlideSlopeAngle",0) ) );
	mpCharBody->SetMaxPushMass(			gpBase->mpGameCfg->GetFloat("Player_Body","MaxPushMass",0) );
	mpCharBody->SetPushForce(			gpBase->mpGameCfg->GetFloat("Player_Body","PushForce",0) );
	mpCharBody->SetCharacterMaxPushMass(gpBase->mpGameCfg->GetFloat("Player_Body","CharacterMaxPushMass",0) );
	mpCharBody->SetCharacterPushForce(	gpBase->mpGameCfg->GetFloat("Player_Body","CharacterPushForce",0) );
	mpCharBody->SetMaxStepSize(			gpBase->mpGameCfg->GetFloat("Player_Body","MaxStepSize",0) );
	mpCharBody->SetMaxStepSizeInAir(	gpBase->mpGameCfg->GetFloat("Player_Body","MaxStepSizeInAir",0) );
	mpCharBody->SetStepClimbSpeed(		gpBase->mpGameCfg->GetFloat("Player_Body","StepClimbSpeed",0) );
	
	mpCharBody->SetCamera(mpCamera);
	mpCharBody->SetCameraPosAdd(mvCameraPosAdd);
	mpCharBody->SetCameraSmoothPosNum(	gpBase->mpGameCfg->GetInt("Player_Body","CameraSmoothPosNum",0) );

	//Create crouch extra
	mpCharBody->AddExtraSize(mvBodyCrouchSize);

	//temp placement!
	mpCharBody->SetFeetPosition(cVector3f(8.5f, 5.2f, 9.25f));
	mpCharBody->Update(0.001f);

	//mpCharBody->SetActive(false);
}


//-----------------------------------------------------------------------



