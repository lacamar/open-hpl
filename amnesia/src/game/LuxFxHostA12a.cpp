// Engine side of rust/src/luxfx/host.rs: sound entries, GUI, configs, player, map and post effects.
// The A12a dump (rust/src/luxfx/dump.rs) logs each call on the Rust side.

#include "LuxBase.h"
#include "RustLuxFxA12a.h"
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

int SndValid(size_t aE, int alId)
{
	return Snd()->IsValid((cSoundEntry*)aE, alId);
}

size_t SndPlayGui(OhplA12aBytes aFile, int *apId)
{
	cSoundEntry *e = Snd()->PlayGuiStream(Str(aFile), false, 1.0f);
	*apId = e ? e->GetId() : -1;
	return (size_t)e;
}

size_t SndPlay3d(OhplA12aBytes aFile, const float *apPos, float afMin, float afMax, int *apId)
{
	cSoundEntry *e = Snd()->Play(Str(aFile), false, 1.0f, cVector3f(apPos[0], apPos[1], apPos[2]), afMin, afMax, eSoundEntryType_Gui, false, true, 0, true);
	*apId = e ? e->GetId() : -1;
	return (size_t)e;
}

void SndFadeOut(size_t aE, float afSpeed)
{
	((cSoundEntry*)aE)->FadeOut(afSpeed);
}

void SndStop(size_t aE)
{
	((cSoundEntry*)aE)->Stop();
}

void SndPause(size_t aE, int alOn)
{
	((cSoundEntry*)aE)->SetPaused(alOn != 0);
}

void SndVolMul(size_t aE, float afX)
{
	((cSoundEntry*)aE)->SetVolumeMul(afX);
}

double SndElapsed(size_t aE)
{
	return ohpl_sndch_read(((cSoundEntry*)aE)->GetChannel(), OhplSndCh_Elapsed);
}

void SndSetElapsed(size_t aE, double afX)
{
	ohpl_sndch_set_elapsed(((cSoundEntry*)aE)->GetChannel(), afX);
}

iRenderable* FirstObject()
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap == NULL) return NULL;
	cMeshEntityIterator it = pMap->GetWorld()->GetStaticMeshEntityIterator();
	while(it.HasNext())
	{
		cMeshEntity *pMesh = it.Next();
		if(pMesh->GetSubMeshEntityNum() > 0) return pMesh->GetSubMeshEntity(0);
	}
	return NULL;
}

void SndCommentary(int alMode)
{
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
	for(size_t i=0; i<eLuxGlobalVolumeType_LastEnum; ++i)
	{
		Snd()->SetGlobalSpeed(1, eSoundEntryType_World, i);
		Snd()->SetGlobalVolume(1, eSoundEntryType_World, i);
	}
}

void GuiSound(OhplA12aBytes aName)
{
	gpBase->mpHelpFuncs->PlayGuiSoundData(Str(aName), eSoundEntryType_Gui);
}

void Translate(OhplA12aBytes aCat, OhplA12aBytes aEntry, OhplA12aSink32 aSink, void *apCtx)
{
	tWString sText = kTranslate(Str(aCat), Str(aEntry));
	aSink(apCtx, (const unsigned*)sText.data(), sText.size());
}

void Wrap(size_t aFont, float afWidth, float afLine, const float *apSize, const unsigned *apText, size_t alLen, OhplA12aSink32 aSink, void *apCtx)
{
	iFontData *pFont = aFont ? (iFontData*)aFont : gpBase->mpDefaultFont;
	tWString sText((const wchar_t*)apText, alLen);
	tWStringVec vRows;
	pFont->GetWordWrapRows(afWidth, afLine, cVector2f(apSize[0], apSize[1]), sText, &vRows);
	for(size_t i=0; i<vRows.size(); ++i)
		aSink(apCtx, (const unsigned*)vRows[i].data(), vRows[i].size());
}

size_t FontLoad()
{
	return (size_t)gpBase->mpEngine->GetResources()->GetFontManager()->CreateFontData("game_default.fnt");
}

void FontDestroy(size_t aFont)
{
	gpBase->mpEngine->GetResources()->GetFontManager()->Destroy((iFontData*)aFont);
}

void Hud(float *apOut)
{
	apOut[0] = gpBase->mvHudVirtualStartPos.x; apOut[1] = gpBase->mvHudVirtualStartPos.y; apOut[2] = gpBase->mvHudVirtualStartPos.z;
	apOut[3] = gpBase->mvHudVirtualSize.x; apOut[4] = gpBase->mvHudVirtualSize.y;
}

void DrawFont(size_t aFont, const float *apPos, const float *apSize, const float *apCol, int alCenter, const unsigned *apText, size_t alLen)
{
	tWString sText((const wchar_t*)apText, alLen);
	gpBase->mpGameHudSet->DrawFont(sText, aFont ? (iFontData*)aFont : gpBase->mpDefaultFont, cVector3f(apPos[0], apPos[1], apPos[2]), cVector2f(apSize[0], apSize[1]),
									cColor(apCol[0], apCol[1], apCol[2], apCol[3]), alCenter ? eFontAlign_Center : eFontAlign_Left);
}

float CfgF32(OhplA12aBytes aSec, OhplA12aBytes aKey, float afDef)
{
	return gpBase->mpGameCfg->GetFloat(Str(aSec), Str(aKey), afDef);
}

void CfgVec2(OhplA12aBytes aSec, OhplA12aBytes aKey, float afDef, float *apOut)
{
	cVector2f v = gpBase->mpGameCfg->GetVector2f(Str(aSec), Str(aKey), cVector2f(afDef));
	apOut[0] = v.x; apOut[1] = v.y;
}

void CfgStr(OhplA12aBytes aSec, OhplA12aBytes aKey, OhplA12aBytes aDef, OhplA12aSink aSink, void *apCtx)
{
	tString s = gpBase->mpGameCfg->GetString(Str(aSec), Str(aKey), Str(aDef));
	aSink(apCtx, (const unsigned char*)s.data(), s.size());
}

void CfgColor(OhplA12aBytes aSec, OhplA12aBytes aKey, float *apOut)
{
	cColor c = gpBase->mpGameCfg->GetColor(Str(aSec), Str(aKey), 0);
	apOut[0] = c.r; apOut[1] = c.g; apOut[2] = c.b; apOut[3] = c.a;
}

int Subtitles()
{
	return gpBase->mpMessageHandler->ShowSubtitles();
}

double GameTime()
{
	return gpBase->mpEngine->GetGameTime();
}

void Player(int alOp, float afA, float afB, float afC)
{
	switch(alOp)
	{
	case 0:
		gpBase->mpPlayer->FadeFOVMulTo(afA, afB);
		break;
	case 1:
		gpBase->mpPlayer->SetActive(afA != 0);
		break;
	case 2:
		gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, cVector3f(afA, afB, afC));
		break;
	default:
		gpBase->mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, 0);
		break;
	}
}

void Map(int alOp, int alId, OhplA12aBytes aName)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(alOp == 0)
	{
		if(pMap)
		{
			iLuxEntity *pEntity = pMap->GetEntityByID(alId, eLuxEntityType_CommentaryIcon);
			if(pEntity) static_cast<cLuxCommentaryIcon*>(pEntity)->SetPlayingSound(false);
		}
	}
	else if(alOp == 1)
	{
		pMap->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);
	}
	else
	{
		tString sName = Str(aName);
		pMap->RunScript(sName);
	}
}

void Post(int alKind, int alOp, float afA, float afB)
{
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
	return ((cRendererCallbackFunctions*)aFns)->GetSettings()->mbIsReflection;
}

int RCollides(size_t aFns, size_t aObj)
{
	return ((iRenderable*)aObj)->CollidesWithFrustum(((cRendererCallbackFunctions*)aFns)->GetFrustum());
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
}

void RClip(size_t aFns, const float *apMin, const float *apMax, int *apRect)
{
	cBoundingVolume totalBV;
	totalBV.SetLocalMinMax(cVector3f(apMin[0], apMin[1], apMin[2]), cVector3f(apMax[0], apMax[1], apMax[2]));
	cVector2l vScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
	cRect2l clipRect;
	cMath::GetClipRectFromBV(clipRect, totalBV, ((cRendererCallbackFunctions*)aFns)->GetFrustum(), vScreenSize, -1);
	apRect[0] = clipRect.x; apRect[1] = clipRect.y; apRect[2] = clipRect.w; apRect[3] = clipRect.h;
}

void RReplay(size_t aFns, const size_t *apObjs, size_t alObjs, const OhplGa9bROp *apOps, size_t alOps, const int *apClip)
{
	gpBase->mpEffectRenderer->Replay((cRendererCallbackFunctions*)aFns, apObjs, apOps, alOps, cRect2l(apClip[0], apClip[1], apClip[2], apClip[3]));
}

void PReplay(size_t aEffect, size_t aInput, size_t aFb, const OhplGa9bPOp *apOps, size_t alOps)
{
	((cLuxPostEffect_Insanity*)aEffect)->Replay(apOps, alOps, (iTexture*)aInput, (iFrameBuffer*)aFb);
}

size_t Dump(int alOp, float afX)
{
	cLuxMapHandler *pMaps = gpBase->mpMapHandler;
	cLuxPostEffect_Insanity *pIns = gpBase->mpPostEffectHandler->GetInsanity();
	switch(alOp)
	{
	case 0: return pMaps != NULL && pMaps->GetCurrentMap() != NULL;
	case 1: return (size_t)FirstObject();
	case 2: return pIns->IsActive();
	default: pIns->SetActive(afX != 0); return 0;
	}
}

const OhplA12aHost gHost =
{
	SndValid, SndPlayGui, SndPlay3d, SndFadeOut, SndStop, SndPause, SndVolMul, SndElapsed, SndSetElapsed, SndCommentary, SndResetWorld,
	GuiSound, Translate, Wrap, FontLoad, FontDestroy, Hud, DrawFont,
	CfgF32, CfgVec2, CfgStr, CfgColor, Subtitles, GameTime, Player, Map, Post,
	RReflection, RCollides, ROutline, RClip, RReplay, PReplay, Dump,
};

}

void OhplA12aInitHost()
{
	static bool bDone = false;
	if(bDone) return;
	bDone = true;
	ohpl_a12a_set_host(&gHost);
}
