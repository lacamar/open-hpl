// Engine side of rust/src/luxfx/host.rs: sound entries, GUI, configs, player, map and post effects.
// Each call is logged in the A12a dump format (LuxA12aDump.cpp) when the dump is on.

#include "LuxBase.h"
#include "RustLuxFxA12a.h"
#include "LuxA12aDump.h"
#include "LuxEffectHandler.h"
#include "LuxEffectRenderer.h"
#include "LuxPostEffects.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxCommentaryIcon.h"

#include <string.h>

namespace
{

tString Str(OhplA12aBytes b)
{
	return tString((const char*)b.p, b.n);
}

cSoundHandler* Snd()
{
	return gpBase->mpEngine->GetSound()->GetSoundHandler();
}

bool RenderLog()
{
	return OhplA12Enabled() && OhplA12Frame() >= 10;
}

std::string Floats(const float *apV, int alN)
{
	std::string s;
	for(int i=0; i<alN; ++i) s += (i ? " " : "") + OhplA12F(apV[i]);
	return s;
}

int SndValid(size_t aE, int alId)
{
	bool b = Snd()->IsValid((cSoundEntry*)aE, alId);
	OhplA12Log("snd_valid %d => %d", aE ? 1 : 0, b ? 1 : 0);
	return b;
}

size_t SndPlayGui(OhplA12aBytes aFile, int *apId)
{
	tString sFile = Str(aFile);
	cSoundEntry *e = Snd()->PlayGuiStream(sFile, false, 1.0f);
	*apId = e ? e->GetId() : -1;
	OhplA12Log("snd_play_gui %s => %d", OhplA12S(sFile).c_str(), e ? 1 : 0);
	return (size_t)e;
}

size_t SndPlay3d(OhplA12aBytes aFile, const float *apPos, float afMin, float afMax, int *apId)
{
	tString sFile = Str(aFile);
	cSoundEntry *e = Snd()->Play(sFile, false, 1.0f, cVector3f(apPos[0], apPos[1], apPos[2]), afMin, afMax, eSoundEntryType_Gui, false, true, 0, true);
	*apId = e ? e->GetId() : -1;
	OhplA12Log("snd_play_3d %s %s %s %s => %d", OhplA12S(sFile).c_str(), Floats(apPos, 3).c_str(), OhplA12F(afMin).c_str(), OhplA12F(afMax).c_str(), e ? 1 : 0);
	return (size_t)e;
}

void SndFadeOut(size_t aE, float afSpeed)
{
	OhplA12Log("snd_fade_out 1 %s", OhplA12F(afSpeed).c_str());
	((cSoundEntry*)aE)->FadeOut(afSpeed);
}

void SndStop(size_t aE)
{
	OhplA12Log("snd_stop 1");
	((cSoundEntry*)aE)->Stop();
}

void SndPause(size_t aE, int alOn)
{
	OhplA12Log("snd_pause 1 %d", alOn ? 1 : 0);
	((cSoundEntry*)aE)->SetPaused(alOn != 0);
}

void SndVolMul(size_t aE, float afX)
{
	OhplA12Log("snd_volmul 1 %s", OhplA12F(afX).c_str());
	((cSoundEntry*)aE)->SetVolumeMul(afX);
}

double SndElapsed(size_t aE)
{
	double d = ((cSoundEntry*)aE)->GetChannel()->GetElapsedTime();
	OhplA12Log("snd_elapsed 1 => %s", OhplA12D(d).c_str());
	return d;
}

void SndSetElapsed(size_t aE, double afX)
{
	OhplA12Log("snd_set_elapsed 1 %s", OhplA12D(afX).c_str());
	((cSoundEntry*)aE)->GetChannel()->SetElapsedTime(afX);
}

void SndCommentary(int alMode)
{
	OhplA12Log("snd_commentary %d", alMode);
	cSoundHandler *pSound = Snd();
	cMusicHandler *pMusic = gpBase->mpEngine->GetSound()->GetMusicHandler();
	if(alMode == 0)
	{
		pSound->SetGlobalVolume(1.0f, eSoundEntryType_World, eLuxGlobalVolumeType_Commentary);
		pMusic->SetVolumeMul(1.0f);
	}
	else
	{
		float fVol = alMode == 1 ? 0.15f : 1.0f;
		pSound->FadeGlobalVolume(fVol, 0.5f, eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
		pMusic->FadeVolumeMul(fVol, 0.5f);
	}
}

void SndResetWorld()
{
	OhplA12Log("snd_reset_world");
	for(size_t i=0; i<eLuxGlobalVolumeType_LastEnum; ++i)
	{
		Snd()->SetGlobalSpeed(1, eSoundEntryType_World, i);
		Snd()->SetGlobalVolume(1, eSoundEntryType_World, i);
	}
}

void GuiSound(OhplA12aBytes aName)
{
	tString sName = Str(aName);
	OhplA12Log("gui_sound %s", OhplA12S(sName).c_str());
	gpBase->mpHelpFuncs->PlayGuiSoundData(sName, eSoundEntryType_Gui);
}

void Translate(OhplA12aBytes aCat, OhplA12aBytes aEntry, OhplA12aSink32 aSink, void *apCtx)
{
	tString sCat = Str(aCat), sEntry = Str(aEntry);
	tWString sText = kTranslate(sCat, sEntry);
	OhplA12Log("translate %s %s => %s", OhplA12S(sCat).c_str(), OhplA12S(sEntry).c_str(), OhplA12S(sText).c_str());
	aSink(apCtx, (const unsigned*)sText.data(), sText.size());
}

void Wrap(size_t aFont, float afWidth, float afLine, const float *apSize, const unsigned *apText, size_t alLen, OhplA12aSink32 aSink, void *apCtx)
{
	iFontData *pFont = aFont ? (iFontData*)aFont : gpBase->mpDefaultFont;
	tWString sText((const wchar_t*)apText, alLen);
	cVector2f vSize(apSize[0], apSize[1]);
	tWStringVec vRows;
	pFont->GetWordWrapRows(afWidth, afLine, vSize, sText, &vRows);
	std::string sRows;
	for(size_t i=0; i<vRows.size(); ++i) { if(i) sRows += "|"; sRows += OhplA12S(vRows[i]); }
	OhplA12Log("wrap %d %s %s %s %s %s => %s", aFont ? 1 : 0, OhplA12F(afWidth).c_str(), OhplA12F(afLine).c_str(), OhplA12F(vSize.x).c_str(), OhplA12F(vSize.y).c_str(),
				OhplA12S(sText).c_str(), sRows.c_str());
	for(size_t i=0; i<vRows.size(); ++i)
		aSink(apCtx, (const unsigned*)vRows[i].data(), vRows[i].size());
}

size_t FontLoad()
{
	iFontData *pFont = gpBase->mpEngine->GetResources()->GetFontManager()->CreateFontData("game_default.fnt");
	OhplA12Log("font_load => %d", pFont ? 1 : 0);
	return (size_t)pFont;
}

void FontDestroy(size_t aFont)
{
	OhplA12Log("font_destroy");
	gpBase->mpEngine->GetResources()->GetFontManager()->Destroy((iFontData*)aFont);
}

size_t GfxRect(int alAdditive)
{
	size_t p = (size_t)gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), alAdditive ? eGuiMaterial_Additive : eGuiMaterial_Modulative);
	OhplA12Log("gfx_rect %s => 1", alAdditive ? "add" : "mod");
	return p;
}

void Hud(float *apOut)
{
	apOut[0] = gpBase->mvHudVirtualStartPos.x; apOut[1] = gpBase->mvHudVirtualStartPos.y; apOut[2] = gpBase->mvHudVirtualStartPos.z;
	apOut[3] = gpBase->mvHudVirtualSize.x; apOut[4] = gpBase->mvHudVirtualSize.y;
	OhplA12Log("hud => %s", Floats(apOut, 5).c_str());
}

void DrawGfx(size_t aSet, size_t aGfx, const float *apPos, const float *apSize, const float *apCol)
{
	OhplA12Log("draw_gfx 1 %s %s %s", Floats(apPos, 3).c_str(), Floats(apSize, 2).c_str(), Floats(apCol, 4).c_str());
	cGuiSet *pSet = aSet ? (cGuiSet*)aSet : gpBase->mpGameHudSet;
	pSet->DrawGfx((cGuiGfxElement*)aGfx, cVector3f(apPos[0], apPos[1], apPos[2]), cVector2f(apSize[0], apSize[1]), cColor(apCol[0], apCol[1], apCol[2], apCol[3]));
}

void DrawFont(size_t aFont, const float *apPos, const float *apSize, const float *apCol, int alCenter, const unsigned *apText, size_t alLen)
{
	tWString sText((const wchar_t*)apText, alLen);
	OhplA12Log("draw_font %d %s %s %s %d %s", aFont ? 1 : 0, Floats(apPos, 3).c_str(), Floats(apSize, 2).c_str(), Floats(apCol, 4).c_str(), alCenter ? 1 : 0, OhplA12S(sText).c_str());
	gpBase->mpGameHudSet->DrawFont(sText, aFont ? (iFontData*)aFont : gpBase->mpDefaultFont, cVector3f(apPos[0], apPos[1], apPos[2]), cVector2f(apSize[0], apSize[1]),
									cColor(apCol[0], apCol[1], apCol[2], apCol[3]), alCenter ? eFontAlign_Center : eFontAlign_Left);
}

float CfgF32(OhplA12aBytes aSec, OhplA12aBytes aKey, float afDef)
{
	tString sSec = Str(aSec), sKey = Str(aKey);
	float f = gpBase->mpGameCfg->GetFloat(sSec, sKey, afDef);
	OhplA12Log("cfg_f32 %s %s => %s", OhplA12S(sSec).c_str(), OhplA12S(sKey).c_str(), OhplA12F(f).c_str());
	return f;
}

void CfgVec2(OhplA12aBytes aSec, OhplA12aBytes aKey, float afDef, float *apOut)
{
	tString sSec = Str(aSec), sKey = Str(aKey);
	cVector2f v = gpBase->mpGameCfg->GetVector2f(sSec, sKey, cVector2f(afDef));
	apOut[0] = v.x; apOut[1] = v.y;
	OhplA12Log("cfg_vec2 %s %s => %s", OhplA12S(sSec).c_str(), OhplA12S(sKey).c_str(), Floats(apOut, 2).c_str());
}

void CfgStr(OhplA12aBytes aSec, OhplA12aBytes aKey, OhplA12aBytes aDef, OhplA12aSink aSink, void *apCtx)
{
	tString sSec = Str(aSec), sKey = Str(aKey);
	tString s = gpBase->mpGameCfg->GetString(sSec, sKey, Str(aDef));
	OhplA12Log("cfg_str %s %s => %s", OhplA12S(sSec).c_str(), OhplA12S(sKey).c_str(), OhplA12S(s).c_str());
	aSink(apCtx, (const unsigned char*)s.data(), s.size());
}

void CfgColor(OhplA12aBytes aSec, OhplA12aBytes aKey, float *apOut)
{
	tString sSec = Str(aSec), sKey = Str(aKey);
	cColor c = gpBase->mpGameCfg->GetColor(sSec, sKey, 0);
	apOut[0] = c.r; apOut[1] = c.g; apOut[2] = c.b; apOut[3] = c.a;
	OhplA12Log("cfg_color %s %s => %s", OhplA12S(sSec).c_str(), OhplA12S(sKey).c_str(), Floats(apOut, 4).c_str());
}

int Subtitles()
{
	bool b = gpBase->mpMessageHandler->ShowSubtitles();
	OhplA12Log("subtitles => %d", b ? 1 : 0);
	return b;
}

double GameTime()
{
	OhplA12Log("game_time");
	return gpBase->mpEngine->GetGameTime();
}

void Player(int alOp, float afA, float afB, float afC)
{
	switch(alOp)
	{
	case 0:
		OhplA12Log("player_fov %s %s", OhplA12F(afA).c_str(), OhplA12F(afB).c_str());
		gpBase->mpPlayer->FadeFOVMulTo(afA, afB);
		break;
	case 1:
		OhplA12Log("player_active %d", afA != 0 ? 1 : 0);
		gpBase->mpPlayer->SetActive(afA != 0);
		break;
	case 2:
		OhplA12Log("player_shake");
		gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, cVector3f(afA, afB, afC));
		break;
	default:
		OhplA12Log("player_shake_clear");
		gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, 0);
		break;
	}
}

void Map(int alOp, int alId, OhplA12aBytes aName)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(alOp == 0)
	{
		OhplA12Log("map_icon %d", alId);
		if(pMap)
		{
			iLuxEntity *pEntity = pMap->GetEntityByID(alId, eLuxEntityType_CommentaryIcon);
			if(pEntity) static_cast<cLuxCommentaryIcon*>(pEntity)->SetPlayingSound(false);
		}
	}
	else if(alOp == 1)
	{
		OhplA12Log("map_enemy_reset");
		pMap->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);
	}
	else
	{
		tString sName = Str(aName);
		OhplA12Log("map_script %s", OhplA12S(sName).c_str());
		pMap->RunScript(sName);
	}
}

void Post(int alKind, int alOp, float afA, float afB)
{
	static const char *vOps[3] = { "params", "active", "reset" };
	OhplA12Log("post %d %s %s %s", alKind, vOps[alOp], OhplA12F(afA).c_str(), OhplA12F(afB).c_str());
	cLuxMapHandler *pMaps = gpBase->mpMapHandler;
	iPostEffect *pEffect = alKind == eOhplA12aPost_RadialBlur ? pMaps->GetPostEffect_RadialBlur() :
							alKind == eOhplA12aPost_Sepia ? pMaps->GetPostEffect_Sepia() : pMaps->GetPostEffect_ImageTrail();
	if(alOp == 1) { pEffect->SetActive(afA != 0); return; }
	if(alOp == 2) { pEffect->Reset(); return; }
	if(alKind == eOhplA12aPost_RadialBlur)
	{
		cPostEffectParams_RadialBlur params;
		params.mfSize = afA;
		params.mfBlurStartDist = afB;
		pEffect->SetParams(&params);
	}
	else if(alKind == eOhplA12aPost_Sepia)
	{
		cPostEffectParams_ColorConvTex params;
		params.mfFadeAlpha = afA;
		pEffect->SetParams(&params);
	}
	else
	{
		cPostEffectParams_ImageTrail params;
		params.mfAmount = afA;
		pEffect->SetParams(&params);
	}
}

int RReflection(size_t aFns)
{
	bool b = ((cRendererCallbackFunctions*)aFns)->GetSettings()->mbIsReflection;
	if(RenderLog()) OhplA12Log("r_reflection => %d", b ? 1 : 0);
	return b;
}

int RCollides(size_t aFns, size_t aObj)
{
	bool b = ((iRenderable*)aObj)->CollidesWithFrustum(((cRendererCallbackFunctions*)aFns)->GetFrustum());
	if(RenderLog()) OhplA12Log("r_collides => %d", b ? 1 : 0);
	return b;
}

void ROutline(size_t aFns, size_t aObj, OhplGa9bOutline *apOut)
{
	iRenderable *pObject = (iRenderable*)aObj;
	memset(apOut, 0, sizeof(*apOut));
	apOut->collides = pObject->CollidesWithFrustum(((cRendererCallbackFunctions*)aFns)->GetFrustum());
	if(apOut->collides)
	{
		cBoundingVolume *pBV = pObject->GetBoundingVolume();
		apOut->hasAlpha = pObject->GetMaterial()->GetTexture(eMaterialTexture_Alpha) != NULL;
		const cVector3f vMin = pBV->GetMin(), vMax = pBV->GetMax();
		const cVector3f vLocalMin = pBV->GetLocalMin(), vLocalMax = pBV->GetLocalMax(), vLocalCenter = pBV->GetLocalCenter();
		apOut->min[0] = vMin.x; apOut->min[1] = vMin.y; apOut->min[2] = vMin.z;
		apOut->max[0] = vMax.x; apOut->max[1] = vMax.y; apOut->max[2] = vMax.z;
		apOut->localMin[0] = vLocalMin.x; apOut->localMin[1] = vLocalMin.y; apOut->localMin[2] = vLocalMin.z;
		apOut->localMax[0] = vLocalMax.x; apOut->localMax[1] = vLocalMax.y; apOut->localMax[2] = vLocalMax.z;
		apOut->localCenter[0] = vLocalCenter.x; apOut->localCenter[1] = vLocalCenter.y; apOut->localCenter[2] = vLocalCenter.z;
		const cMatrixf &mtxWorld = pObject->GetWorldMatrix();
		for(int r=0; r<4; ++r)
			for(int c=0; c<4; ++c)
				apOut->world[r*4+c] = mtxWorld.m[r][c];
	}
	if(RenderLog())
		OhplA12Log("r_outline => %u %u %s %s %s %s %s %s", apOut->collides, apOut->hasAlpha, Floats(apOut->min, 3).c_str(), Floats(apOut->max, 3).c_str(),
					Floats(apOut->localMin, 3).c_str(), Floats(apOut->localMax, 3).c_str(), Floats(apOut->localCenter, 3).c_str(), Floats(apOut->world, 16).c_str());
}

void RClip(size_t aFns, const float *apMin, const float *apMax, int *apRect)
{
	cBoundingVolume totalBV;
	totalBV.SetLocalMinMax(cVector3f(apMin[0], apMin[1], apMin[2]), cVector3f(apMax[0], apMax[1], apMax[2]));
	cVector2l vScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
	cRect2l clipRect;
	cMath::GetClipRectFromBV(clipRect, totalBV, ((cRendererCallbackFunctions*)aFns)->GetFrustum(), vScreenSize, -1);
	apRect[0] = clipRect.x; apRect[1] = clipRect.y; apRect[2] = clipRect.w; apRect[3] = clipRect.h;
	if(RenderLog()) OhplA12Log("r_clip => %d %d %d %d", apRect[0], apRect[1], apRect[2], apRect[3]);
}

void RReplay(size_t aFns, const size_t *apObjs, size_t alObjs, const OhplGa9bROp *apOps, size_t alOps, const int *apClip)
{
	gpBase->mpEffectRenderer->Replay((cRendererCallbackFunctions*)aFns, apObjs, apOps, alOps, cRect2l(apClip[0], apClip[1], apClip[2], apClip[3]));
}

void PReplay(size_t aEffect, size_t aInput, size_t aFb, const OhplGa9bPOp *apOps, size_t alOps)
{
	((cLuxPostEffect_Insanity*)aEffect)->Replay(apOps, alOps, (iTexture*)aInput, (iFrameBuffer*)aFb);
}

const OhplA12aHost gHost =
{
	SndValid, SndPlayGui, SndPlay3d, SndFadeOut, SndStop, SndPause, SndVolMul, SndElapsed, SndSetElapsed, SndCommentary, SndResetWorld,
	GuiSound, Translate, Wrap, FontLoad, FontDestroy, GfxRect, Hud, DrawGfx, DrawFont,
	CfgF32, CfgVec2, CfgStr, CfgColor, Subtitles, GameTime, Player, Map, Post,
	RReflection, RCollides, ROutline, RClip, RReplay, PReplay,
};

}

void OhplA12aInitHost()
{
	static bool bDone = false;
	if(bDone) return;
	bDone = true;
	ohpl_a12a_set_host(&gHost);
}

void OhplA12FillState(OhplA12aState *apState)
{
	memset(apState, 0, sizeof(*apState));
	ohpl_a12a_fx_get_state(gpBase->mpEffectHandler->GetHandle(), apState);
	cLuxPostEffect_Insanity *pIns = gpBase->mpPostEffectHandler->GetInsanity();
	float vClock[5];
	pIns->GetClock(vClock);
	apState->insT = vClock[0]; apState->insAnim = vClock[1]; apState->insWave = vClock[2]; apState->insZoom = vClock[3]; apState->insSpeed = vClock[4];
	apState->insActive = pIns->IsActive();
}
