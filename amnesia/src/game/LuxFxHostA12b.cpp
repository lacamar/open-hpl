#include "RustLuxFxA12b.h"

#include "LuxBase.h"
#include "LuxA12bDump.h"
#include "LuxPlayer.h"
#include "LuxDebugHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxProgressLogHandler.h"
#include "LuxEffectHandler.h"
#include "LuxMessageHandler.h"
#include "LuxEnemy.h"

// Mirrors rust/src/luxfx/host12.rs.
struct OhplA12bArgs
{
	size_t p[4];
	float f[6];
	int i[4];
	const unsigned char *s[3];
	size_t n[3];
	float out[6];
	int outI[2];
	void (*sink)(void*, int, int, const unsigned char*, size_t);
	void *ctx;
};

enum
{
	kCfgF32 = 1, kCfgStr = 2, kLog = 3, kWarn = 4, kErr = 5,
	kMPlay = 10, kMStop = 11, kMSongNull = 12, kMResetResume = 13, kMPlayerDead = 14, kMHardMode = 15, kMHasMap = 16, kMDangerScan = 17, kMEnemyInfo = 18,
	kXOpen = 30, kXType = 31, kXStr = 32, kXF32 = 33, kXI32 = 34, kXBool = 35, kXColor = 36, kXClose = 37,
	kGImage = 40, kGRectWhite = 41, kGTexture = 42, kGDestroy = 43, kGBounds = 45, kDBug = 50, kDFade = 51, kDFont = 52,
	kSStream = 60, kSFadeIn = 61, kSFadeOut = 62, kSStop = 63, kSIsValid = 64,
	kPCreate = 70, kPSetCamera = 71, kPExists = 72, kPKill = 73, kEStepSound = 80,
	kPlActive = 90, kPlSanity = 91, kSubRows = 100, kSubsState = 101, kLogStart = 110, kLogProg = 111
};

enum { kTagStr = 0, kTagRow = 1, kTagDanger = 2 };

static tString Str(const OhplA12bArgs &a, int n)
{
	return tString((const char*)a.s[n], a.n[n]);
}

static void Sink(OhplA12bArgs &a, int alTag, int alVal, const tString &asX)
{
	if(a.sink) a.sink(a.ctx, alTag, alVal, (const unsigned char*)asX.data(), asX.size());
}

static iXmlDocument *gpXmlDoc = NULL;
static std::vector<cXmlElement*> gvXmlElems;

static cXmlElement *Elem(const OhplA12bArgs &a)
{
	return a.i[0] >= 0 && (size_t)a.i[0] < gvXmlElems.size() ? gvXmlElems[a.i[0]] : NULL;
}

static cWorld *World()
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	return pMap ? pMap->GetWorld() : NULL;
}

static size_t Music(int alCode, OhplA12bArgs &a)
{
	cMusicHandler *pEng = gpBase->mpEngine->GetSound()->GetMusicHandler();
	switch(alCode)
	{
	case kMPlay:
		{
			tString sFile = Str(a, 0);
			OhplA12Log("m play %s %.9g %.9g %d %d", sFile.c_str(), a.f[0], a.f[1], a.i[0], a.i[1]);
			pEng->Play(sFile, a.f[0], a.f[1], a.i[0] != 0, a.i[1] != 0);
			return 0;
		}
	case kMStop:
		OhplA12Log("m stop %.9g", a.f[0]);
		pEng->Stop(a.f[0]);
		return 0;
	case kMSongNull: return pEng->GetCurrentSong() == NULL;
	case kMResetResume:
		OhplA12Log("m reset_resume");
		pEng->ResetResumeData();
		return 0;
	case kMPlayerDead: return gpBase->mpPlayer->IsDead();
	case kMHardMode: return gpBase->mbHardMode == true;
	case kMHasMap: return gpBase->mpMapHandler->GetCurrentMap() != NULL;
	case kMDangerScan:
		{
			cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
			if(pMap == NULL) return 0;
			cLuxEnemyIterator enemyIt = pMap->GetEnemyIterator();
			while(enemyIt.HasNext())
			{
				iLuxEnemy *pEnemy = enemyIt.Next();
				if(pEnemy->GetHealth() <= 0 || pEnemy->IsActive()==false) continue;
				if(pEnemy->GetPlayerInRange()) Sink(a, kTagDanger, pEnemy->GetDangerMusicPrio(), pEnemy->GetDangerMusic());
			}
			return 0;
		}
	case kMEnemyInfo:
		{
			iLuxEnemy *pEnemy = (iLuxEnemy*)a.p[0];
			a.outI[0] = pEnemy->GetMusicPrio((eLuxEnemyMusic)a.i[0]);
			Sink(a, kTagStr, 0, pEnemy->GetMusic((eLuxEnemyMusic)a.i[0]));
			return 0;
		}
	}
	return 0;
}

static size_t Xml(int alCode, OhplA12bArgs &a)
{
	switch(alCode)
	{
	case kXOpen:
		{
			cResources *pResources = gpBase->mpEngine->GetResources();
			gpXmlDoc = pResources->LoadXmlDocument(Str(a, 0));
			if(gpXmlDoc == NULL) return (size_t)-1;
			gvXmlElems.clear();
			cXmlNodeListIterator it = gpXmlDoc->GetChildIterator();
			while(it.HasNext()) gvXmlElems.push_back(it.Next()->ToElement());
			return gvXmlElems.size();
		}
	case kXClose:
		gvXmlElems.clear();
		if(gpXmlDoc) gpBase->mpEngine->GetResources()->DestroyXmlDocument(gpXmlDoc);
		gpXmlDoc = NULL;
		return 0;
	case kXType:
		if(Elem(a)) Sink(a, kTagStr, 0, Elem(a)->GetValue());
		return 0;
	case kXStr:
		if(Elem(a)) Sink(a, kTagStr, 0, Elem(a)->GetAttributeString(Str(a, 0), Str(a, 1)));
		return 0;
	case kXF32:
		a.out[0] = Elem(a) ? Elem(a)->GetAttributeFloat(Str(a, 0), a.f[0]) : a.f[0];
		return 0;
	case kXI32:
		a.outI[0] = Elem(a) ? Elem(a)->GetAttributeInt(Str(a, 0), a.i[1]) : a.i[1];
		return 0;
	case kXBool: return Elem(a) ? Elem(a)->GetAttributeBool(Str(a, 0), a.i[1] != 0) : a.i[1] != 0;
	case kXColor:
		{
			cColor c = Elem(a) ? Elem(a)->GetAttributeColor(Str(a, 0), cColor(1)) : cColor(1);
			a.out[0] = c.r; a.out[1] = c.g; a.out[2] = c.b; a.out[3] = c.a;
			return 0;
		}
	}
	return 0;
}

static size_t Gui(int alCode, OhplA12bArgs &a)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	cGuiSet *pSet = gpBase->mpGameHudSet;
	switch(alCode)
	{
	case kGImage:
		OhplA12Log("i gfx_image %s", Str(a, 0).c_str());
		return (size_t)pGui->CreateGfxImage(Str(a, 0), eGuiMaterial_Alpha);
	case kGRectWhite:
		OhplA12Log("i gfx_rect white");
		return (size_t)pGui->CreateGfxFilledRect(cColor(1), eGuiMaterial_Modulative);
	case kGTexture:
		OhplA12Log("i gfx_texture %s", Str(a, 0).c_str());
		return (size_t)pGui->CreateGfxTexture(Str(a, 0), eGuiMaterial_Alpha, eTextureType_Rect);
	case kGDestroy:
		OhplA12Log("i gfx_destroy");
		pGui->DestroyGfx((cGuiGfxElement*)a.p[0]);
		return 0;
	case kGBounds:
		{
			cVector2f vGuiSetCenterSize = cVector2f(800, 600);
			cVector2f vGuiSetSize, vGuiSetOffset;
			LuxCalcGuiSetScreenOffset(vGuiSetCenterSize, vGuiSetSize, vGuiSetOffset);
			cVector2f vGuiSetMin = cVector2f(-vGuiSetOffset.x,-vGuiSetOffset.y);
			cVector2f vGuiSetMax = cVector2f(800.0f+vGuiSetOffset.x, 600.0f+vGuiSetOffset.y);
			cGuiGfxElement *pImage = (cGuiGfxElement*)a.p[0];
			cVector2f vActive = pImage ? pImage->GetActiveSize() : cVector2f(0);
			OhplA12Log("i gui_bounds %.9g %.9g %.9g %.9g active %.9g %.9g", vGuiSetMin.x, vGuiSetMin.y, vGuiSetMax.x, vGuiSetMax.y, vActive.x, vActive.y);
			a.out[0] = vGuiSetMin.x; a.out[1] = vGuiSetMin.y; a.out[2] = vGuiSetMax.x; a.out[3] = vGuiSetMax.y;
			return 0;
		}
	case kDBug:
		{
			cGuiGfxElement *pImage = (cGuiGfxElement*)a.p[0];
			cVector2f vPos(a.f[0], a.f[1]);
			cVector2f vSize = pImage->GetActiveSize()*a.f[2];
			OhplA12Log("i bug %d pos %.9g %.9g size %.9g %.9g col %.9g angle %.9g", a.i[0], vPos.x, vPos.y, vSize.x, vSize.y, a.f[3], a.f[4]);
			pSet->DrawGfx(pImage, vPos, vSize, cColor(1,a.f[3]), eGuiMaterial_LastEnum, a.f[4]);
			return 0;
		}
	case kDFade:
		{
			cColor fadeColor(a.f[0], a.f[1], a.f[2], a.f[3]);
			float fAlpha = a.f[4];
			bool bImage = a.p[0] != 0;
			cColor c = bImage ? fadeColor * fAlpha : fadeColor*fAlpha + cColor(1,1)*(1-fAlpha);
			OhplA12Log("i fade img=%d col %.9g %.9g %.9g %.9g", bImage ? 1 : 0, c.r, c.g, c.b, c.a);
			pSet->DrawGfx(bImage ? (cGuiGfxElement*)a.p[0] : (cGuiGfxElement*)a.p[1], gpBase->mvHudVirtualStartPos, gpBase->mvHudVirtualSize, c);
			return 0;
		}
	case kDFont:
		{
			tWString sText((const wchar_t*)a.s[0], a.n[0] / sizeof(wchar_t));
			OhplA12Log("i font %d %.9g %.9g %.9g", a.i[0], a.f[0], a.f[1], a.f[2]);
			pSet->DrawFont(sText, gpBase->mpDefaultFont, cVector3f(a.f[0], a.f[1], a.f[2]), 18,cColor(1,1));
			return 0;
		}
	}
	return 0;
}

static size_t Sound(int alCode, OhplA12bArgs &a)
{
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	cSoundEntry *pEntry = (cSoundEntry*)a.p[0];
	switch(alCode)
	{
	case kSStream:
		{
			tString sFile = Str(a, 0);
			OhplA12Log("i stream %s %d %.9g", sFile.c_str(), a.i[0], a.f[0]);
			cSoundEntry *pNew = pSoundHandler->PlayGuiStream(sFile, a.i[0] != 0, a.f[0]);
			OhplA12Log("i stream result %d", pNew ? 1 : 0);
			if(pNew) a.outI[0] = pNew->GetId();
			return (size_t)pNew;
		}
	case kSFadeIn:
		OhplA12Log("i fadein %.9g %.9g", a.f[0], a.f[1]);
		pEntry->FadeIn(a.f[0], a.f[1]);
		return 0;
	case kSFadeOut:
		OhplA12Log("i fadeout %.9g", a.f[0]);
		pEntry->FadeOut(a.f[0]);
		return 0;
	case kSStop:
		OhplA12Log("i sound stop");
		pEntry->Stop();
		return 0;
	case kSIsValid: return pSoundHandler->IsValid(pEntry, a.i[0]);
	}
	return 0;
}

static size_t WorldOps(int alCode, OhplA12bArgs &a)
{
	switch(alCode)
	{
	case kPCreate:
		{
			tString sName = Str(a, 0), sFile = Str(a, 1);
			cParticleSystem *pPS = World()->CreateParticleSystem(sName, sFile, 1);
			OhplA12Log("i ps create %s %s %d", sName.c_str(), sFile.c_str(), pPS ? 1 : 0);
			if(pPS) pPS->SetIsSaved(false);
			return (size_t)pPS;
		}
	case kPSetCamera:
		{
			cVector3f vCam = gpBase->mpPlayer->GetCamera()->GetPosition();
			OhplA12Log("i ps pos %.9g %.9g %.9g", vCam.x, vCam.y, vCam.z);
			((cParticleSystem*)a.p[0])->SetPosition(gpBase->mpPlayer->GetCamera()->GetPosition());
			return 0;
		}
	case kPExists:
		return World() && World()->ParticleSystemExists((cParticleSystem*)a.p[0]);
	case kPKill:
		OhplA12Log("i ps kill");
		((cParticleSystem*)a.p[0])->Kill();
		return 0;
	case kEStepSound:
		{
			tString sFile = Str(a, 0);
			cSoundEntity *pSound = World()->CreateSoundEntity("insanity_step", sFile, true);
			OhplA12Log("i step_sound %s %d", sFile.c_str(), pSound ? 1 : 0);
			if(pSound)
			{
				cVector3f vP = gpBase->mpPlayer->GetCharacterBody()->GetPosition() + cVector3f(a.f[0], a.f[1], a.f[2]);
				OhplA12Log("i step_pos %.9g %.9g %.9g", vP.x, vP.y, vP.z);
				pSound->SetPosition(gpBase->mpPlayer->GetCharacterBody()->GetPosition() + cVector3f(a.f[0], a.f[1], a.f[2]));
			}
			return 0;
		}
	}
	return 0;
}

static size_t Game(int alCode, OhplA12bArgs &a)
{
	switch(alCode)
	{
	case kPlActive:
		OhplA12Log("i player active %d", a.i[0]);
		gpBase->mpPlayer->SetActive(a.i[0] != 0);
		return 0;
	case kPlSanity:
		{
			float fSanity = gpBase->mpPlayer->GetSanity();
			OhplA12Log("i sanity %.9g", fSanity);
			a.out[0] = fSanity;
			return 0;
		}
	case kSubRows:
		{
			std::vector<tWString> vRows;
			gpBase->mpDefaultFont->GetWordWrapRows(600,20,18, kTranslate(Str(a, 0),Str(a, 1)), &vRows);
			for(size_t i=0; i<vRows.size(); ++i)
				a.sink(a.ctx, kTagRow, 0, (const unsigned char*)vRows[i].data(), vRows[i].size() * sizeof(wchar_t));
			return 0;
		}
	case kSubsState:
		return (gpBase->mpMessageHandler->ShowEffectSubtitles() ? 1 : 0) | (gpBase->mpEffectHandler->GetPlayVoice()->IsActive() ? 2 : 0);
	case kLogStart:
		{
			tString sName = Str(a, 0), sSet = Str(a, 1);
			OhplA12Log("i prog Starting Insanity event: '%s' set: '%s'", sName.c_str(), sSet.c_str());
			gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_Low, "Starting Insanity event: '"+sName+"'"+ " set: '"+sSet+"'");
			gpBase->mpDebugHandler->AddMessage(	_W("Starting Insanity event: '")+cString::To16Char(sName)+_W("'")+
												_W("set: '")+cString::To16Char(sSet)+_W("'") , false);
			return 0;
		}
	case kLogProg:
		ProgLog(eLuxProgressLogLevel_Low, Str(a, 0));
		return 0;
	}
	return 0;
}

static size_t Host(int alCode, OhplA12bArgs *apArgs)
{
	OhplA12bArgs &a = *apArgs;
	switch(alCode)
	{
	case kCfgF32:
		a.out[0] = gpBase->mpGameCfg->GetFloat(Str(a, 0), Str(a, 1), a.f[0]);
		return 0;
	case kCfgStr:
		Sink(a, kTagStr, 0, gpBase->mpGameCfg->GetString(Str(a, 0), Str(a, 1), Str(a, 2)));
		return 0;
	case kLog:
		OhplA12Log("%s", Str(a, 0).c_str());
		return 0;
	case kWarn:
		Warning("%s", Str(a, 0).c_str());
		return 0;
	case kErr:
		Error("%s", Str(a, 0).c_str());
		return 0;
	}
	if(alCode >= kMPlay && alCode <= kMEnemyInfo) return Music(alCode, a);
	if(alCode >= kXOpen && alCode <= kXClose) return Xml(alCode, a);
	if((alCode >= kGImage && alCode <= kGBounds) || (alCode >= kDBug && alCode <= kDFont)) return Gui(alCode, a);
	if(alCode >= kSStream && alCode <= kSIsValid) return Sound(alCode, a);
	if(alCode >= kPCreate && alCode <= kEStepSound) return WorldOps(alCode, a);
	return Game(alCode, a);
}

void OhplA12bInitHost()
{
	ohpl_a12b_set_host(Host, OhplA12Enabled() ? 1 : 0);
}
