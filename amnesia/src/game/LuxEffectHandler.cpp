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

#include "LuxEffectHandler.h"
#include "RustLuxFx.h"

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxCommentaryIcon.h"
#include "LuxA12aDump.h"

static bool A12Valid(cSoundHandler *apH, cSoundEntry *apE, int alId)
{
	bool b = apH->IsValid(apE, alId);
	OhplA12Log("snd_valid %d => %d", apE ? 1 : 0, b ? 1 : 0);
	return b;
}

static cSoundEntry* A12PlayGui(cSoundHandler *apH, const tString &asFile)
{
	cSoundEntry *e = apH->PlayGuiStream(asFile, false, 1.0f);
	OhplA12Log("snd_play_gui %s => %d", OhplA12S(asFile).c_str(), e ? 1 : 0);
	return e;
}

static cSoundEntry* A12Play3d(cSoundHandler *apH, const tString &asFile, const cVector3f &avPos, float afMin, float afMax)
{
	cSoundEntry *e = apH->Play(asFile,false, 1.0f, avPos, afMin, afMax, eSoundEntryType_Gui,false,true,0, true);
	OhplA12Log("snd_play_3d %s %s %s %s %s %s => %d", OhplA12S(asFile).c_str(), OhplA12F(avPos.x).c_str(), OhplA12F(avPos.y).c_str(), OhplA12F(avPos.z).c_str(),
				OhplA12F(afMin).c_str(), OhplA12F(afMax).c_str(), e ? 1 : 0);
	return e;
}

static void A12FadeOut(cSoundEntry *apE, float afSpeed)
{
	OhplA12Log("snd_fade_out 1 %s", OhplA12F(afSpeed).c_str());
	apE->FadeOut(afSpeed);
}

static void A12Stop(cSoundEntry *apE)
{
	OhplA12Log("snd_stop 1");
	apE->Stop();
}

static void A12Pause(cSoundEntry *apE, bool abX)
{
	OhplA12Log("snd_pause 1 %d", abX ? 1 : 0);
	apE->SetPaused(abX);
}

static void A12Vol(cSoundEntry *apE, float afX)
{
	OhplA12Log("snd_volmul 1 %s", OhplA12F(afX).c_str());
	apE->SetVolumeMul(afX);
}

static double A12Elapsed(cSoundEntry *apE)
{
	double d = apE->GetChannel()->GetElapsedTime();
	OhplA12Log("snd_elapsed 1 => %s", OhplA12D(d).c_str());
	return d;
}

static void A12SetElapsed(cSoundEntry *apE, double afX)
{
	OhplA12Log("snd_set_elapsed 1 %s", OhplA12D(afX).c_str());
	apE->GetChannel()->SetElapsedTime(afX);
}

static void A12Rows(int alFont, float afWidth, float afLine, const cVector2f &avSize, const tWString &asText, tWStringVec *apRows, iFontData *apFont)
{
	apFont->GetWordWrapRows(afWidth, afLine, avSize, asText, apRows);
	std::string sRows;
	for(size_t i=0; i<apRows->size(); ++i) { if(i) sRows += "|"; sRows += OhplA12S((*apRows)[i]); }
	OhplA12Log("wrap %d %s %s %s %s %s => %s", alFont, OhplA12F(afWidth).c_str(), OhplA12F(afLine).c_str(), OhplA12F(avSize.x).c_str(), OhplA12F(avSize.y).c_str(),
				OhplA12S(asText).c_str(), sRows.c_str());
}

static void A12Hud()
{
	OhplA12Log("hud => %s %s %s %s %s", OhplA12F(gpBase->mvHudVirtualStartPos.x).c_str(), OhplA12F(gpBase->mvHudVirtualStartPos.y).c_str(),
				OhplA12F(gpBase->mvHudVirtualStartPos.z).c_str(), OhplA12F(gpBase->mvHudVirtualSize.x).c_str(), OhplA12F(gpBase->mvHudVirtualSize.y).c_str());
}

static void A12DrawGfx(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol)
{
	OhplA12Log("draw_gfx 1 %s %s %s %s %s %s %s %s %s", OhplA12F(avPos.x).c_str(), OhplA12F(avPos.y).c_str(), OhplA12F(avPos.z).c_str(),
				OhplA12F(avSize.x).c_str(), OhplA12F(avSize.y).c_str(), OhplA12F(aCol.r).c_str(), OhplA12F(aCol.g).c_str(), OhplA12F(aCol.b).c_str(), OhplA12F(aCol.a).c_str());
}

static tWString A12Tr(const tString &asCat, const tString &asEntry)
{
	tWString s = kTranslate(asCat, asEntry);
	OhplA12Log("translate %s %s => %s", OhplA12S(asCat).c_str(), OhplA12S(asEntry).c_str(), OhplA12S(s).c_str());
	return s;
}

static double A12Time()
{
	double d = gpBase->mpEngine->GetGameTime();
	OhplA12Log("game_time");
	return d;
}

static void A12Script(const tString &asFunc)
{
	OhplA12Log("map_script %s", OhplA12S(asFunc).c_str());
	gpBase->mpMapHandler->GetCurrentMap()->RunScript(asFunc);
}

static void A12Post(int alKind, const char *asOp, float afA, float afB)
{
	OhplA12Log("post %d %s %s %s", alKind, asOp, OhplA12F(afA).c_str(), OhplA12F(afB).c_str());
}


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffectHandler::cLuxEffectHandler() : iLuxUpdateable("LuxEffectHandler")
{
	mpFade = hplNew( cLuxEffect_Fade, () );
	mvEffects.push_back(mpFade);

	mpFlash = hplNew( cLuxEffect_Flash, () );
	mvEffects.push_back(mpFlash);

	mpSanityGainFlash = hplNew( cLuxEffect_SanityGainFlash, () );
	mvEffects.push_back(mpSanityGainFlash);

	mpPlayVoice = hplNew( cLuxEffect_PlayVoice, () );
	mvEffects.push_back(mpPlayVoice);

	mpImageTrail = hplNew( cLuxEffect_ImageTrail, () );
	mvEffects.push_back(mpImageTrail);

	mpScreenShake = hplNew( cLuxEffect_ShakeScreen, () );
	mvEffects.push_back(mpScreenShake);

	mpSepiaColor = hplNew( cLuxEffect_SepiaColor, () );
	mvEffects.push_back(mpSepiaColor);

	mpRadialBlur = hplNew( cLuxEffect_RadialBlur, () );
	mvEffects.push_back(mpRadialBlur);

	mpEmotionFlash = hplNew( cLuxEffect_EmotionFlash, () );
	mvEffects.push_back(mpEmotionFlash);

	mpPlayCommentary = hplNew( cLuxEffect_PlayCommentary, () );
	mvEffects.push_back(mpPlayCommentary);
}

//-----------------------------------------------------------------------

cLuxEffectHandler::~cLuxEffectHandler()
{
	STLDeleteAll(mvEffects);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAY COMMENTARY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


cLuxEffect_PlayCommentary::cLuxEffect_PlayCommentary()
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	mpMusicHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();

	mpSoundEntry = NULL;
	mlSoundEntryID = -1;
	Reset();
}

cLuxEffect_PlayCommentary::~cLuxEffect_PlayCommentary()
{
	Reset();
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Start(const tString &asTalker,const tString &asTopic, const tString &asFile, int alIconId)
{
	if(mpSoundEntry)
	{
		Stop();

		//Set the icon as not playing
		OhplA12Log("map_icon %d", mlIconID);
		if(gpBase->mpMapHandler->GetCurrentMap())
		{
			iLuxEntity *pEntity = gpBase->mpMapHandler->GetCurrentMap()->GetEntityByID(mlIconID, eLuxEntityType_CommentaryIcon);
			if(pEntity)
			{
				cLuxCommentaryIcon *mpIcon = static_cast<cLuxCommentaryIcon*>(pEntity);
				mpIcon->SetPlayingSound(false);
			}
		}
	}
	
	msTalker = asTalker;
	msTopic  = asTopic;
	mlIconID = alIconId;

	mpSoundEntry = A12PlayGui(mpSoundHandler, asFile);
	if(mpSoundEntry)
	{
		mlSoundEntryID = mpSoundEntry->GetId();
		SetActive(true);
		
		OhplA12Log("snd_commentary 1");
		mpSoundHandler->FadeGlobalVolume(0.15f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
		mpMusicHandler->FadeVolumeMul(0.15f, 0.5f);
		gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(0.1f);
	}
	
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Stop()
{
	if(mbActive==false) return;

	if(A12Valid(mpSoundHandler, mpSoundEntry,mlSoundEntryID)) A12FadeOut(mpSoundEntry, 1);
	mpSoundEntry = NULL;

	OhplA12Log("snd_commentary 2");
	mpSoundHandler->FadeGlobalVolume(1.0f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
	mpMusicHandler->FadeVolumeMul(1.0f, 0.5f);
	gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(1.0f);

	mbActive = false;
}

//-----------------------------------------------------------------------


void cLuxEffect_PlayCommentary::Update(float afTimeStep)
{
	if(A12Valid(mpSoundHandler, mpSoundEntry, mlSoundEntryID)) return;

	//Set the icon as not playing
	OhplA12Log("map_icon %d", mlIconID);
	if(gpBase->mpMapHandler->GetCurrentMap())
	{
		iLuxEntity *pEntity = gpBase->mpMapHandler->GetCurrentMap()->GetEntityByID(mlIconID, eLuxEntityType_CommentaryIcon);
		if(pEntity)
		{
			cLuxCommentaryIcon *mpIcon = static_cast<cLuxCommentaryIcon*>(pEntity);
			mpIcon->SetPlayingSound(false);
		}
	}

	mpSoundEntry = NULL;

	OhplA12Log("snd_commentary 2");
	mpSoundHandler->FadeGlobalVolume(1.0f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
	mpMusicHandler->FadeVolumeMul(1.0f, 0.5f);
	gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(1.0f);
	
	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::OnDraw(float afFrameTime)
{
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Reset()
{
	if(mpSoundEntry != NULL && A12Valid(mpSoundHandler, mpSoundEntry,mlSoundEntryID)) A12Stop(mpSoundEntry);
	mpSoundEntry = NULL;

	OhplA12Log("snd_commentary 0");
	mpSoundHandler->SetGlobalVolume(1.0f,eSoundEntryType_World, eLuxGlobalVolumeType_Commentary);
	mpMusicHandler->SetVolumeMul(1.0f);
	
	mlIconID = -1;
	msTalker = "";
	msTopic = "";
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// EMOTION FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_EmotionFlash::cLuxEffect_EmotionFlash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);
	OhplA12Log("gfx_rect add => 1");

	mpFont = NULL;

	mvFontSize = 20;
}
cLuxEffect_EmotionFlash::~cLuxEffect_EmotionFlash()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::ClearFonts()
{
	if(mpFont)
	{
		OhplA12Log("font_destroy");
		gpBase->mpEngine->GetResources()->GetFontManager()->Destroy(mpFont);
	}

	mpFont = NULL;
}

void cLuxEffect_EmotionFlash::LoadFonts()
{
	tString sFontFile = "game_default.fnt";
	mpFont = gpBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(sFontFile);
	OhplA12Log("font_load => %d", mpFont ? 1 : 0);
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::Start(const tString &asTextCat, const tString &asTextEntry, const tString &asSound)
{
	mbActive = true;
	mlStep = 0;
	mfAlpha =0;

	OhplA12Log("gui_sound %s", OhplA12S(asSound).c_str());
	gpBase->mpHelpFuncs->PlayGuiSoundData(asSound, eSoundEntryType_Gui);

	tWString sText = kTranslate(asTextCat, asTextEntry);
	OhplA12Log("translate %s %s => %s", OhplA12S(asTextCat).c_str(), OhplA12S(asTextEntry).c_str(), OhplA12S(sText).c_str());
	mvTextRows.clear();
	A12Rows(1, 500, mvFontSize.y, mvFontSize, sText, &mvTextRows, mpFont);

	OhplGa9bEmotion s = ToRust();
	ohpl_ga9b_emotion_start(&s, sText.length());
	FromRust(s);

	gpBase->mpEffectHandler->SetPlayerIsPaused(true);
	OhplA12Log("player_fov %s %s", OhplA12F(0.5f).c_str(), OhplA12F(0.5f).c_str());
	gpBase->mpPlayer->FadeFOVMulTo(0.5f, 0.5f);

	gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0.15f, 3);
	gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(0.6f);

	OhplA12Log("map_enemy_reset");
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);
}

OhplGa9bEmotion cLuxEffect_EmotionFlash::ToRust()
{
	OhplGa9bEmotion s = { mfAlpha, mlStep, mfCount, mfTextAlpha, mfTextTime, mbActive };
	return s;
}

void cLuxEffect_EmotionFlash::FromRust(const OhplGa9bEmotion &s)
{
	mfAlpha = s.alpha;
	mlStep = s.step;
	mfCount = s.count;
	mfTextAlpha = s.textAlpha;
	mfTextTime = s.textTime;
	mbActive = s.active != 0;
}

void cLuxEffect_EmotionFlash::Reset()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::Update(float afTimeStep)
{
	OhplGa9bEmotion s = ToRust();
	OhplGa9bOps ops;
	ohpl_ga9b_emotion_update(&s, afTimeStep, &ops);
	FromRust(s);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		switch(ops.ops[i].kind)
		{
		case kOhplGa9bEmoFovOut:
			OhplA12Log("player_fov %s %s", OhplA12F(1.0f).c_str(), OhplA12F(0.33f).c_str());
			gpBase->mpPlayer->FadeFOVMulTo(1.0f, 0.33f);
			break;
		case kOhplGa9bEmoUnpause:
			gpBase->mpEffectHandler->SetPlayerIsPaused(false);
			break;
		case kOhplGa9bEmoRadialOut:
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, 1);
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::OnDraw(float afFrameTime)
{
	A12Hud();
	A12DrawGfx(gpBase->mvHudVirtualStartPos + cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));
	gpBase->mpGameHudSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos + cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));

	if(mfTextAlpha > 0)
	{
		size_t lRows = mvTextRows.size();
		std::vector<OhplGa9bRow> vRows(lRows ? lRows : 1);
		size_t lNum = ohpl_ga9b_emotion_rows(lRows, mvFontSize.y, &vRows[0]);
		for(size_t i=0; i<lNum; ++i)
		{
			const OhplGa9bRow &row = vRows[i];
			OhplA12Log("draw_font 1 %s %s %s %s %s %s %s %s %s %d %s", OhplA12F(row.x).c_str(), OhplA12F(row.y).c_str(), OhplA12F(row.z).c_str(), OhplA12F(mvFontSize.x).c_str(), OhplA12F(mvFontSize.y).c_str(),
						OhplA12F(0).c_str(), OhplA12F(0).c_str(), OhplA12F(0).c_str(), OhplA12F(mfTextAlpha).c_str(), row.align == kOhplGa9bAlignCenter ? 1 : 0, OhplA12S(mvTextRows[row.row]).c_str());
			gpBase->mpGameHudSet->DrawFont(mvTextRows[row.row], mpFont, cVector3f(row.x,row.y, row.z), mvFontSize, cColor(0, mfTextAlpha),
											row.align == kOhplGa9bAlignCenter ? eFontAlign_Center : eFontAlign_Left);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	OhplGa9bEmotion s = ToRust();
	ohpl_ga9b_emotion_action(&s, abPressed);
	FromRust(s);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// RADIAL BLUR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_RadialBlur::cLuxEffect_RadialBlur()
{
	mfSize =0;
	mfSizeGoal =0;
	mfFadeSpeed =0;
	mfBlurStartDist =0;
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::SetBlurStartDist(float afDist)
{
	mfBlurStartDist = afDist;

	A12Post(0, "params", mfSize, mfBlurStartDist);
	cPostEffectParams_RadialBlur radialBlurParams;
	radialBlurParams.mfSize = mfSize;
	radialBlurParams.mfBlurStartDist = mfBlurStartDist;
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetParams(&radialBlurParams);

}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::FadeTo(float afSize, float afSpeed)
{
	mfSizeGoal = afSize;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	A12Post(0, "active", 1, 0);
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(true);
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::Update(float afTimeStep)
{
	OhplGa9bAmount s = { mfSize, mfSizeGoal, mfFadeSpeed, mbActive };
	OhplGa9bOps ops;
	ohpl_ga9b_amount_update(&s, afTimeStep, &ops);
	mfSize = s.amount;
	mbActive = s.active != 0;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bAmountParams)
		{
			A12Post(0, "params", ops.ops[i].a, mfBlurStartDist);
			cPostEffectParams_RadialBlur radialBlurParams;
			radialBlurParams.mfSize = ops.ops[i].a;
			radialBlurParams.mfBlurStartDist = mfBlurStartDist;
			gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetParams(&radialBlurParams);
		}
		else
		{
			A12Post(0, "active", 0, 0);
			gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(false);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::Reset()
{
	mfSize =0;
	mfSizeGoal =0;
	mfBlurStartDist =0;

	A12Post(0, "reset", 0, 0);
	A12Post(0, "active", 0, 0);
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->Reset();
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SEPIA COLOR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_SepiaColor::cLuxEffect_SepiaColor()
{
	mfAmount =0;
	mfAmountGoal =0;
	mfFadeSpeed =0;
}

void cLuxEffect_SepiaColor::FadeTo(float afAmount, float afSpeed)
{
	mfAmountGoal = afAmount;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	A12Post(1, "active", 1, 0);
	gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(true);
}

void cLuxEffect_SepiaColor::Update(float afTimeStep)
{
	OhplGa9bAmount s = { mfAmount, mfAmountGoal, mfFadeSpeed, mbActive };
	OhplGa9bOps ops;
	ohpl_ga9b_amount_update(&s, afTimeStep, &ops);
	mfAmount = s.amount;
	mbActive = s.active != 0;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bAmountParams)
		{
			A12Post(1, "params", ops.ops[i].a, 0);
			cPostEffectParams_ColorConvTex sepiaParams;
			sepiaParams.mfFadeAlpha = ops.ops[i].a;
			gpBase->mpMapHandler->GetPostEffect_Sepia()->SetParams(&sepiaParams);
		}
		else
		{
			A12Post(1, "active", 0, 0);
			gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(false);
		}
	}
}

void cLuxEffect_SepiaColor::Reset()
{
	mfAmount =0;
	mfAmountGoal =0;
	A12Post(1, "reset", 0, 0);
	A12Post(1, "active", 0, 0);
	gpBase->mpMapHandler->GetPostEffect_Sepia()->Reset();
	gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SCREEN SHAKE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_ShakeScreen::cLuxEffect_ShakeScreen()
{
}

cLuxEffect_ShakeScreen::~cLuxEffect_ShakeScreen()
{
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Start(float afAmount, float afTime,float afFadeInTime,float afFadeOutTime)
{
	cLuxEffect_ShakeScreen_Shake shake;
	shake.mfSize = afAmount;
	shake.mfMaxSize = afAmount;
	shake.mfTime = afTime;
	shake.mfFadeInTime = afFadeInTime;
	shake.mfMaxFadeInTime = afFadeInTime;
	shake.mfFadeOutTime = afFadeOutTime;
	shake.mfMaxFadeOutTime = afFadeOutTime;

	mlstShakes.push_back(shake);

	SetActive(true);
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Update(float afTimeStep)
{
	std::vector<OhplGa9bShake> vShakes;
	vShakes.reserve(mlstShakes.size());
	for(std::list<cLuxEffect_ShakeScreen_Shake>::iterator it = mlstShakes.begin(); it != mlstShakes.end(); ++it)
	{
		OhplGa9bShake s = { it->mfMaxSize, it->mfSize, it->mfTime, it->mfFadeInTime, it->mfMaxFadeInTime, it->mfFadeOutTime, it->mfMaxFadeOutTime };
		vShakes.push_back(s);
	}

	uint32_t lNum = (uint32_t)vShakes.size();
	OhplGa9bOps ops;
	bool bActive = ohpl_ga9b_shake_update(vShakes.empty() ? NULL : &vShakes[0], &lNum, afTimeStep, &ops);

	mlstShakes.clear();
	for(uint32_t i=0; i<lNum; ++i)
	{
		cLuxEffect_ShakeScreen_Shake shake;
		shake.mfMaxSize = vShakes[i].maxSize;
		shake.mfSize = vShakes[i].size;
		shake.mfTime = vShakes[i].time;
		shake.mfFadeInTime = vShakes[i].fadeInTime;
		shake.mfMaxFadeInTime = vShakes[i].maxFadeInTime;
		shake.mfFadeOutTime = vShakes[i].fadeOutTime;
		shake.mfMaxFadeOutTime = vShakes[i].maxFadeOutTime;
		mlstShakes.push_back(shake);
	}
	if(bActive == false) SetActive(false);

	for(uint32_t i=0; i<ops.n; ++i)
	{
		OhplA12Log(ops.ops[i].kind == kOhplGa9bShakeEnd ? "player_shake_clear" : "player_shake");
		if(ops.ops[i].kind == kOhplGa9bShakeEnd)
			gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake,0);
		else
			gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, cVector3f(ops.ops[i].a, ops.ops[i].b, ops.ops[i].c));
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Reset()
{
	mlstShakes.clear();
}


//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// IMAGE TRAIL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_ImageTrail::cLuxEffect_ImageTrail()
{
	mfAmount =0;
	mfAmountGoal =0;
	mfFadeSpeed =0;
}

void cLuxEffect_ImageTrail::FadeTo(float afAmount, float afSpeed)
{
	mfAmountGoal = afAmount;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	A12Post(2, "active", 1, 0);
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(true);
}

void cLuxEffect_ImageTrail::Update(float afTimeStep)
{
	OhplGa9bAmount s = { mfAmount, mfAmountGoal, mfFadeSpeed, mbActive };
	OhplGa9bOps ops;
	ohpl_ga9b_amount_update(&s, afTimeStep, &ops);
	mfAmount = s.amount;
	mbActive = s.active != 0;
	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bAmountParams)
		{
			A12Post(2, "params", ops.ops[i].a, 0);
			cPostEffectParams_ImageTrail imageTrailParams;
			imageTrailParams.mfAmount = ops.ops[i].a;
			gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetParams(&imageTrailParams);
		}
		else
		{
			A12Post(2, "active", 0, 0);
			gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(false);
		}
	}
}

void cLuxEffect_ImageTrail::Reset()
{
	mfAmount =0;
	mfAmountGoal =0;
	A12Post(2, "reset", 0, 0);
	A12Post(2, "active", 0, 0);
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->Reset();
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// FADE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_Fade::cLuxEffect_Fade()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Modulative);
	OhplA12Log("gfx_rect mod => 1");

	Reset();
}

cLuxEffect_Fade::~cLuxEffect_Fade()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::FadeIn(float afTime)
{
	OhplGa9bFade s = ToRust();
	ohpl_ga9b_fade_in(&s, afTime);
	FromRust(s);
}

OhplGa9bFade cLuxEffect_Fade::ToRust()
{
	OhplGa9bFade s = { mfGoalAlpha, mfAlpha, mfFadeSpeed, mbActive };
	return s;
}

void cLuxEffect_Fade::FromRust(const OhplGa9bFade &s)
{
	mfGoalAlpha = s.goal;
	mfAlpha = s.alpha;
	mfFadeSpeed = s.speed;
	mbActive = s.active != 0;
}

void cLuxEffect_Fade::FadeOut(float afTime)
{
	OhplGa9bFade s = ToRust();
	ohpl_ga9b_fade_out(&s, afTime);
	FromRust(s);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::SetDirectAlpha(float afX)
{
	OhplGa9bFade s = ToRust();
	ohpl_ga9b_fade_direct(&s, afX);
	FromRust(s);
}

//-----------------------------------------------------------------------

bool cLuxEffect_Fade::IsFading()
{
	OhplGa9bFade s = ToRust();
	return ohpl_ga9b_fade_is_fading(&s);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::Update(float afTimeStep)
{
	OhplGa9bFade s = ToRust();
	ohpl_ga9b_fade_update(&s, afTimeStep);
	FromRust(s);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::OnDraw(float afFrameTime)
{
	if(mfAlpha <=0) return;

	A12Hud();
	A12DrawGfx(gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(1-mfAlpha, 1));
	gpBase->mpGameHudSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(1-mfAlpha, 1));
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::Reset()
{
	OhplGa9bFade s = ToRust();
	ohpl_ga9b_fade_reset(&s);
	FromRust(s);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SANITY GAIN FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_SanityGainFlash::cLuxEffect_SanityGainFlash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);
	OhplA12Log("gfx_rect add => 1");

	mColor = gpBase->mpGameCfg->GetColor("Player_General","SanityGain_Color", 0);
	OhplA12Log("cfg_color Player_General SanityGain_Color => %s %s %s %s", OhplA12F(mColor.r).c_str(), OhplA12F(mColor.g).c_str(), OhplA12F(mColor.b).c_str(), OhplA12F(mColor.a).c_str());
	msSound = gpBase->mpGameCfg->GetString("Player_General","SanityGain_Sound", "");
	OhplA12Log("cfg_str Player_General SanityGain_Sound => %s", OhplA12S(msSound).c_str());
	mfFadeInTime = gpBase->mpGameCfg->GetFloat("Player_General","SanityGain_FadeInTime", 0);
	OhplA12Log("cfg_f32 Player_General SanityGain_FadeInTime => %s", OhplA12F(mfFadeInTime).c_str());
	mfFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_General","SanityGain_FadeOutTime", 0);
	OhplA12Log("cfg_f32 Player_General SanityGain_FadeOutTime => %s", OhplA12F(mfFadeOutTime).c_str());

	Reset();
}
cLuxEffect_SanityGainFlash::~cLuxEffect_SanityGainFlash()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Reset()
{
	mfAlpha =0;
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Start()
{
	if(msSound != "")
		OhplA12Log("gui_sound %s", OhplA12S(msSound).c_str());
	if(msSound != "")
		gpBase->mpHelpFuncs->PlayGuiSoundData(msSound, eSoundEntryType_Gui);

	OhplGa9bFlash s = ToRust();
	ohpl_ga9b_sanity_flash_start(&s, mfFadeInTime, mfFadeOutTime);
	FromRust(s);
}

OhplGa9bFlash cLuxEffect_SanityGainFlash::ToRust()
{
	OhplGa9bFlash s = { mfAlpha, mlStep, mfCount, mfFadeInSpeed, mfWhiteSpeed, mfFadeOutSpeed, mbActive };
	return s;
}

void cLuxEffect_SanityGainFlash::FromRust(const OhplGa9bFlash &s)
{
	mfAlpha = s.alpha;
	mlStep = s.step;
	mfCount = s.count;
	mfFadeInSpeed = s.inSpeed;
	mfWhiteSpeed = s.whiteSpeed;
	mfFadeOutSpeed = s.outSpeed;
	mbActive = s.active != 0;
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Update(float afTimeStep)
{
	OhplGa9bFlash s = ToRust();
	ohpl_ga9b_flash_update(&s, afTimeStep);
	FromRust(s);
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::OnDraw(float afFrameTime)
{
	DrawFlash(gpBase->mpGameHudSet, afFrameTime);
}	

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::DrawFlash(cGuiSet *apSet ,float afTimeStep)
{
	A12Hud();
	A12DrawGfx(gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,mColor*mfAlpha);
	apSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,mColor*mfAlpha);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_Flash::cLuxEffect_Flash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);
	OhplA12Log("gfx_rect add => 1");
	Reset();
}
cLuxEffect_Flash::~cLuxEffect_Flash()
{
	
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Start(float afFadeIn, float afWhite, float afFadeOut)
{
	OhplGa9bFlash s = ToRust();
	ohpl_ga9b_flash_start(&s, afFadeIn, afWhite, afFadeOut);
	FromRust(s);
}

OhplGa9bFlash cLuxEffect_Flash::ToRust()
{
	OhplGa9bFlash s = { mfAlpha, mlStep, mfCount, mfFadeInSpeed, mfWhiteSpeed, mfFadeOutSpeed, mbActive };
	return s;
}

void cLuxEffect_Flash::FromRust(const OhplGa9bFlash &s)
{
	mfAlpha = s.alpha;
	mlStep = s.step;
	mfCount = s.count;
	mfFadeInSpeed = s.inSpeed;
	mfWhiteSpeed = s.whiteSpeed;
	mfFadeOutSpeed = s.outSpeed;
	mbActive = s.active != 0;
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Reset()
{
	OhplGa9bFlash s = ToRust();
	ohpl_ga9b_flash_reset(&s);
	FromRust(s);
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Update(float afTimeStep)
{
	OhplGa9bFlash s = ToRust();
	ohpl_ga9b_flash_update(&s, afTimeStep);
	FromRust(s);
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::OnDraw(float afFrameTime)
{
	A12Hud();
	A12DrawGfx(gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));
	gpBase->mpGameHudSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));
}	


//////////////////////////////////////////////////////////////////////////
// PLAY VOICE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_PlayVoice::cLuxEffect_PlayVoice()
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	mvFontSize = gpBase->mpGameCfg->GetVector2f("Effects","VoiceTextFontSize",1);
	OhplA12Log("cfg_vec2 Effects VoiceTextFontSize => %s %s", OhplA12F(mvFontSize.x).c_str(), OhplA12F(mvFontSize.y).c_str());
	mfRowWidth = gpBase->mpGameCfg->GetFloat("Effects","VoiceTextRowWidth",1);
	OhplA12Log("cfg_f32 Effects VoiceTextRowWidth => %s", OhplA12F(mfRowWidth).c_str());

	mpVoiceEntry = NULL;
	mlVoiceEntryID = -1;
	mpEffectEntry = NULL;
	mlEffectEntryID = -1;

	Reset();
}

cLuxEffect_PlayVoice::~cLuxEffect_PlayVoice()
{
	Reset();
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::StopVoices(float afFadeOutSpeed)
{
	if(A12Valid(mpSoundHandler, mpVoiceEntry,mlVoiceEntryID)) A12FadeOut(mpVoiceEntry, afFadeOutSpeed);
	mpVoiceEntry = NULL;

	if(A12Valid(mpSoundHandler, mpEffectEntry,mlEffectEntryID)) A12FadeOut(mpEffectEntry, afFadeOutSpeed);
	mpEffectEntry = NULL;
	
	mlstVoices.clear();

	mbActive = false;
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::AddVoice(const tString& asVoiceFile, const tString& asEffectFile,
									const tString& asTextCat, const tString& asTextEntry, bool abUsePostion, 
									const cVector3f& avPosition, float afMinDistance, float afMaxDistance, int alPriority, bool abRemoveInterrupted)
{
	AddMultiSubbedVoice(
		asVoiceFile, asEffectFile, asTextCat,
		asTextEntry, 0.0f,
		"", 0.0f,
		"", 0.0f,
		"", 0.0f,
		"", 0.0f,
		"", 0.0f,
		"", 0.0f,
		abUsePostion, avPosition, afMinDistance, afMaxDistance, alPriority, abRemoveInterrupted );
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::AddMultiSubbedVoice(
	const tString& asVoiceFile, const tString& asEffectFile, const tString& asTextCat,
	const tString& asTextEntry, float afTextDelay,
	const tString& asText2Entry, float afText2Delay,
	const tString& asText3Entry, float afText3Delay,
	const tString& asText4Entry, float afText4Delay,
	const tString& asText5Entry, float afText5Delay,
	const tString& asText6Entry, float afText6Delay,
	const tString& asText7Entry, float afText7Delay,
	bool abUsePostion, 
	const cVector3f& avPosition, float afMinDistance, float afMaxDistance, int alPriority, bool abRemoveInterrupted
	)
{
	cLuxVoiceData voiceData;

	//Log("Adding sounds: '%s' and '%s'\n", asVoiceFile.c_str(), asEffectFile.c_str());
	
    voiceData.msVoiceFile = asVoiceFile;
	voiceData.msEffectFile = asEffectFile;
	
	if(asTextCat != "" && asTextEntry != "")
	{
		voiceData.msText = A12Tr(asTextCat, asTextEntry);
		voiceData.mfTextDelay = afTextDelay;
	}
	else
	{
		voiceData.msText = _W("");
		voiceData.mfTextDelay = 0.0f;
	}

	if(asTextCat != "" && asText2Entry != "")
	{
		voiceData.msText2 = A12Tr(asTextCat, asText2Entry);
		voiceData.mfText2Delay = afText2Delay;
	}
	else
	{
		voiceData.msText2 = _W("");
		voiceData.mfText2Delay = 0.0f;
	}

	if(asTextCat != "" && asText3Entry != "")
	{
		voiceData.msText3 = A12Tr(asTextCat, asText3Entry);
		voiceData.mfText3Delay = afText3Delay;
	}
	else
	{
		voiceData.msText3 = _W("");
		voiceData.mfText3Delay = 0.0f;
	}

	if(asTextCat != "" && asText4Entry != "")
	{
		voiceData.msText4 = A12Tr(asTextCat, asText4Entry);
		voiceData.mfText4Delay = afText4Delay;
	}
	else
	{
		voiceData.msText4 = _W("");
		voiceData.mfText4Delay = 0.0f;
	}

	if(asTextCat != "" && asText5Entry != "")
	{
		voiceData.msText5 = A12Tr(asTextCat, asText5Entry);
		voiceData.mfText5Delay = afText5Delay;
	}
	else
	{
		voiceData.msText5 = _W("");
		voiceData.mfText5Delay = 0.0f;
	}

	if(asTextCat != "" && asText6Entry != "")
	{
		voiceData.msText6 = A12Tr(asTextCat, asText6Entry);
		voiceData.mfText6Delay = afText6Delay;
	}
	else
	{
		voiceData.msText6 = _W("");
		voiceData.mfText6Delay = 0.0f;
	}

	if(asTextCat != "" && asText7Entry != "")
	{
		voiceData.msText7 = A12Tr(asTextCat, asText7Entry);
		voiceData.mfText7Delay = afText7Delay;
	}
	else
	{
		voiceData.msText7 = _W("");
		voiceData.mfText7Delay = 0.0f;
	}

	voiceData.mbUsePosition = abUsePostion;
	voiceData.mvPosition = avPosition;
	voiceData.mfMinDistance = afMinDistance;
	voiceData.mfMaxDistance = afMaxDistance;
	voiceData.mlPriority = alPriority;
	voiceData.mfCurrentTime = 0;
	voiceData.mfInterruptedAt = -1;

	OhplGa9bVoiceAdd addIn;
	addIn.active = mbActive;
	addIn.listEmpty = mlstVoices.empty();
	addIn.frontPriority = mlstVoices.empty() ? 0 : mlstVoices.front().mlPriority;
	addIn.priority = alPriority;
	addIn.removeInterrupted = abRemoveInterrupted;
	addIn.voiceValid = A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID);
	addIn.effectValid = A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID);
	OhplGa9bOps ops;
	ohpl_ga9b_voice_add(&addIn, &ops);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		switch(ops.ops[i].kind)
		{
		case kOhplGa9bVoiceFadeVoice:
			A12FadeOut(mpVoiceEntry, 0.5f);
			mlstVoices.front().mfCurrentTime = A12Elapsed(mpVoiceEntry);
			voiceData.mfInterruptedAt = A12Time();
			break;
		case kOhplGa9bVoiceVoiceNull:
			mpVoiceEntry = NULL;
			break;
		case kOhplGa9bVoiceFadeEffect:
			A12FadeOut(mpEffectEntry, 0.5f);
			break;
		case kOhplGa9bVoiceEffectNull:
			mpEffectEntry = NULL;
			break;
		case kOhplGa9bVoicePopFront:
			mlstVoices.pop_front();
			break;
		case kOhplGa9bVoicePushFront:
			mlstVoices.push_front(voiceData);
			break;
		case kOhplGa9bVoicePushBack:
			mlstVoices.push_back(voiceData);
			break;
		}
	}

	mbActive = true;
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::PauseCurrentVoices()
{
	if(mbActive==false || mbPaused) return;

	mbPaused = true;
	
	//Voice
	if(mpVoiceEntry && A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID))
	{
		A12Pause(mpVoiceEntry, true);
	}

	//Effect
	if(mpEffectEntry && A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID))
	{
		A12Pause(mpEffectEntry, true);
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::UnpauseCurrentVoices()
{
	if(mbActive==false || mbPaused==false) return;
	
	mbPaused = false;

	//Voice
	if(mpVoiceEntry && A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID))
	{
		A12Pause(mpVoiceEntry, false);
	}

	//Effect
	if(mpEffectEntry && A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID))
	{
		A12Pause(mpEffectEntry, false);
	}
}

//-----------------------------------------------------------------------


static bool SortVoiceData(const cLuxVoiceData& aLhs, const cLuxVoiceData& aRhs)
{
	return aLhs.mlPriority > aRhs.mlPriority;
}

void cLuxEffect_PlayVoice::Update(float afTimeStep)
{
	////////////////////////////
	// Subtitle queue
	std::vector<float> vDelays(mvTextEntryQueue.size());
	for(size_t i=0; i<mvTextEntryQueue.size(); ++i)
		vDelays[i] = mvTextEntryQueue[i].mfDelay;
	size_t lDue = ohpl_ga9b_voice_text_tick(vDelays.empty() ? NULL : &vDelays[0], vDelays.size(), afTimeStep);
	for(size_t i=0; i<vDelays.size(); ++i)
		mvTextEntryQueue[i].mfDelay = vDelays[i];
	for(size_t i=0; i<lDue; ++i)
	{
		mvCurrentTextRows.clear();
		A12Rows(0, mfRowWidth,mvFontSize.y+2,mvFontSize, mvTextEntryQueue.front().msTextEntry, &mvCurrentTextRows, gpBase->mpDefaultFont);
		mvTextEntryQueue.pop_front();
	}

	OhplGa9bVoiceUpdate upd;
	upd.volumeMul = mfVolumeMul;
	upd.voiceNonNull = mpVoiceEntry != NULL;
	upd.voiceValid = A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID);
	upd.effectNonNull = mpEffectEntry != NULL;
	upd.effectValid = A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID);
	upd.listEmpty = mlstVoices.empty();
	OhplGa9bOps ops;
	int lAction = ohpl_ga9b_voice_update_a(&upd, &ops);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bVoiceVolVoice) A12Vol(mpVoiceEntry, ops.ops[i].a);
		else if(ops.ops[i].kind == kOhplGa9bVoiceVolEffect) A12Vol(mpEffectEntry, ops.ops[i].a);
	}
	if(lAction == kOhplGa9bVoiceReturn) return;

	if(lAction == kOhplGa9bVoiceFinalize)
	{
		tString sCallback = msOverCallback;
		float fPreVolMul = mfVolumeMul;
		Reset();
		mfVolumeMul = fPreVolMul;
		SetActive(false);

		if(sCallback!="")
			A12Script(sCallback+"()");

		return;
	}

	mlstVoices.sort(SortVoiceData);

    cLuxVoiceData& voiceData = mlstVoices.front();
	
	double fStartTime = ohpl_ga9b_voice_start_time(voiceData.mfCurrentTime, voiceData.mfInterruptedAt, A12Time());

	//////////////////////
	//GUI sound
	if(voiceData.mbUsePosition==false)	
	{
		mpVoiceEntry = A12PlayGui(mpSoundHandler, voiceData.msVoiceFile);
		if(mpVoiceEntry)
		{
			mlVoiceEntryID = mpVoiceEntry->GetId();
			if(fStartTime > 0) A12SetElapsed(mpVoiceEntry, fStartTime);
		}

		if(voiceData.msEffectFile!="")
		{
			mpEffectEntry = A12PlayGui(mpSoundHandler, voiceData.msEffectFile);
			if(mpEffectEntry) 
			{
				mlEffectEntryID = mpEffectEntry->GetId();
				if(fStartTime > 0) A12SetElapsed(mpEffectEntry, fStartTime);
			}
		}
	}
	//////////////////////
	//3D sound with position
	else
	{
		mpVoiceEntry = A12Play3d(mpSoundHandler, voiceData.msVoiceFile, voiceData.mvPosition,voiceData.mfMinDistance, voiceData.mfMaxDistance);
		if(mpVoiceEntry)
		{
			mlVoiceEntryID = mpVoiceEntry->GetId();
			if(fStartTime > 0) A12SetElapsed(mpVoiceEntry, fStartTime);
		}
		
		if(voiceData.msEffectFile!="")
		{
			mpEffectEntry = A12Play3d(mpSoundHandler, voiceData.msEffectFile, voiceData.mvPosition,voiceData.mfMinDistance, voiceData.mfMaxDistance);
			if(mpEffectEntry) 
			{
				mlEffectEntryID = mpEffectEntry->GetId();
				if(fStartTime > 0) A12SetElapsed(mpEffectEntry, fStartTime);
			}
		}
	}

	//////////////////////
	//Text
	OhplGa9bVoiceTexts texts;
	const tWString *vTexts[7] = { &voiceData.msText, &voiceData.msText2, &voiceData.msText3, &voiceData.msText4, &voiceData.msText5, &voiceData.msText6, &voiceData.msText7 };
	const float vDelaysIn[7] = { voiceData.mfTextDelay, voiceData.mfText2Delay, voiceData.mfText3Delay, voiceData.mfText4Delay, voiceData.mfText5Delay, voiceData.mfText6Delay, voiceData.mfText7Delay };
	texts.nonEmptyMask = 0;
	for(int i=0; i<7; ++i)
	{
		if(*vTexts[i] != _W("")) texts.nonEmptyMask |= 1u << i;
		texts.delays[i] = vDelaysIn[i];
	}
	texts.listSize = (uint32_t)mlstVoices.size();
	texts.voiceNull = mpVoiceEntry==NULL;
	texts.effectNull = mpEffectEntry==NULL;
	bool bOver = ohpl_ga9b_voice_update_b(&texts, &ops);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		switch(ops.ops[i].kind)
		{
		case kOhplGa9bVoiceClearText:
			mvCurrentTextRows.clear();
			mvTextEntryQueue.clear();
			break;
		case kOhplGa9bVoiceWrapNow:
			A12Rows(0, mfRowWidth,mvFontSize.y+2,mvFontSize, voiceData.msText, &mvCurrentTextRows, gpBase->mpDefaultFont);
			break;
		case kOhplGa9bVoiceQueue:
			{
				cTextQueueEntry entry;
				entry.mfDelay = ops.ops[i].a;
				entry.msTextEntry = *vTexts[ops.ops[i].i];
				mvTextEntryQueue.push_back( entry );
			}
			break;
		}
	}

	mlstVoices.pop_front();

	if(bOver)
	{
		Reset();
		SetActive(false);
		if(msOverCallback!="")
			A12Script(msOverCallback+"()");
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::OnDraw(float afFrameTime)
{
	bool bSubs = gpBase->mpMessageHandler->ShowSubtitles();
	OhplA12Log("subtitles => %d", bSubs ? 1 : 0);
	if(bSubs==false) return;
	if(mvCurrentTextRows.empty()) return;

	size_t lRows = mvCurrentTextRows.size();
	std::vector<OhplGa9bRow> vRows(lRows);
	size_t lNum = ohpl_ga9b_voice_rows(lRows, mfRowWidth, mvFontSize.y, &vRows[0]);
	for(size_t i=0; i<lNum; ++i)
	{
		const OhplGa9bRow &row = vRows[i];
		OhplA12Log("draw_font 0 %s %s %s %s %s %s %s %s %s 0 %s", OhplA12F(row.x).c_str(), OhplA12F(row.y).c_str(), OhplA12F(row.z).c_str(), OhplA12F(mvFontSize.x).c_str(), OhplA12F(mvFontSize.y).c_str(),
					OhplA12F(1).c_str(), OhplA12F(1).c_str(), OhplA12F(1).c_str(), OhplA12F(1).c_str(), OhplA12S(mvCurrentTextRows[row.row]).c_str());
		gpBase->mpGameHudSet->DrawFont(mvCurrentTextRows[row.row],gpBase->mpDefaultFont, cVector3f(row.x, row.y, row.z), mvFontSize,cColor(1,1));
	}
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::Reset()
{
	if(mpVoiceEntry != NULL && A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID)) A12Stop(mpVoiceEntry);
	mpVoiceEntry = NULL;

	if(mpEffectEntry != NULL && A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID)) A12Stop(mpEffectEntry);
	mpEffectEntry = NULL;
	
	mvCurrentTextRows.clear();
	mvTextEntryQueue.clear();

	mlstVoices.clear();
	
	msOverCallback = "";

	mbPaused = false;

	mfVolumeMul = 1.0f;
}

void cLuxEffect_PlayVoice::SetVolumeMul(float afMul)
{
	mfVolumeMul = afMul;

	if(mpVoiceEntry && A12Valid(mpSoundHandler, mpVoiceEntry, mlVoiceEntryID))
		A12Vol(mpVoiceEntry, mfVolumeMul);

	if(mpEffectEntry && A12Valid(mpSoundHandler, mpEffectEntry, mlEffectEntryID))
		A12Vol(mpEffectEntry, mfVolumeMul);
}

//-----------------------------------------------------------------------

bool cLuxEffect_PlayVoice::VoiceDonePlaying()
{
	if(mlstVoices.empty() && mpVoiceEntry==NULL)
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnClearFonts()
{
	mpEmotionFlash->ClearFonts();
}

void cLuxEffectHandler::LoadFonts()
{
	mpEmotionFlash->LoadFonts();
}

void cLuxEffectHandler::OnStart()
{
}

//-----------------------------------------------------------------------


void cLuxEffectHandler::Reset()
{
	/////////////////////////
	// Effects
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		pEffect->Reset();
		pEffect->SetActive(false);
	}

	/////////////////////////
	// World sound mul
	OhplA12Log("snd_reset_world");
    for(size_t i=0; i<eLuxGlobalVolumeType_LastEnum; ++i)
	{
		gpBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalSpeed(1, eSoundEntryType_World, i);
		gpBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalVolume(1, eSoundEntryType_World, i);
	}

	mbPlayerIsPaused = false;
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::Update(float afTimeStep)
{
	if(OhplA12Enabled() && OhplA12InTick() == false) return;
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->Update(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnMapEnter(cLuxMap *apMap)
{
	
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnMapLeave(cLuxMap *apMap)
{
	//////////////////
	// Reset some effects on map leave
	mpSepiaColor->FadeTo(0, 1);
	mpRadialBlur->FadeTo(0, 1);
	if(mpPlayCommentary->IsActive()) mpPlayCommentary->Stop();
}

//-----------------------------------------------------------------------



void cLuxEffectHandler::OnDraw(float afFrameTime)
{
	OhplA12Tick();
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->OnDraw(afFrameTime);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->DoAction(aAction, abPressed);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::SetPlayerIsPaused(bool abX)
{
	mbPlayerIsPaused = abX;
	OhplA12Log("player_active %d", abX ? 0 : 1);
	gpBase->mpPlayer->SetActive(!abX);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------



