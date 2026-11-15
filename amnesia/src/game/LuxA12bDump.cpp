#include "LuxA12bDump.h"

#include "LuxMusicHandler.h"
#include "LuxInsanityHandler.h"
#include "LuxInputHandler.h"
#include "LuxConfigHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEnemy.h"
#include "LuxSavedGameTypes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *gpDump = NULL;
static bool gbInit = false;

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A12B_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplA12bEnabled()
{
	return Dump() != NULL;
}

void OhplA12bLog(const char *fmt, ...)
{
	FILE *f = Dump();
	if(f == NULL) return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fflush(f);
}

void OhplA12bLogCall(const char *asTag, uint32_t alOp, const OhplGa9Arg *apArgs, size_t alN)
{
	FILE *f = Dump();
	if(f == NULL) return;
	fprintf(f, "%s op=%u", asTag, alOp);
	for(size_t i=0; i<alN; ++i)
	{
		fprintf(f, " [%lld %.9g ", (long long)apArgs[i].i, apArgs[i].f);
		if(apArgs[i].p)
			for(size_t j=0; j<apArgs[i].n && j<200; ++j)
			{
				unsigned c = apArgs[i].p[j];
				if(c >= 32 && c < 127 && c != ']') fputc((int)c, f);
				else fprintf(f, "\\x%02x", c);
			}
		fputc(']', f);
	}
	fputc('\n', f);
	fflush(f);
}

static void LogMusicState(const char *asWhy)
{
	cLuxMusicHandler *pM = gpBase->mpMusicHandler;
	cMusicHandler *pEng = gpBase->mpEngine->GetSound()->GetMusicHandler();
	OhplA12bLog("m %s cur=%d song=%s vol=%.9g", asWhy, pM->GetCurrentMaxPrio(), pEng->GetCurrentSongName().c_str(), pEng->GetCurrentSong() ? pEng->GetCurrentSongVolume() : -1.0f);
	for(int i=0; i<=pM->GetMaxPrio(); ++i)
	{
		cLuxMusic m = pM->GetMusic(i);
		if(m.msFile == "") continue;
		OhplA12bLog("m slot %d %s loop=%d resume=%d vol=%.9g special=%d", i, m.msFile.c_str(), m.mbLoop ? 1 : 0, m.mbResume ? 1 : 0, m.mfVolume, m.mbSpecialEffect ? 1 : 0);
	}
}

static void MusicStep(int alN)
{
	cLuxMusicHandler *pM = gpBase->mpMusicHandler;
	for(int i=0; i<alN; ++i) pM->UpdateA12(OhplA12bDt());
}

static void MusicScenario(cLuxMap *apMap)
{
	cLuxMusicHandler *pM = gpBase->mpMusicHandler;
	OhplA12bLog("m begin max=%d mul=%.9g", pM->GetMaxPrio(), pM->GetVolumeMul());

	pM->Play("00_event_gallery.ogg", true, 1.0f, 1.0f, 2, false, false); LogMusicState("p2"); MusicStep(30);
	pM->Play("00_event_gallery.ogg", true, 0.5f, 1.0f, 2, false, false); LogMusicState("p2again");
	pM->Play("01_event_dust.ogg", false, 0.8f, 1.0f, 4, true, true); LogMusicState("p4"); MusicStep(30);
	pM->Play("02_puzzle.ogg", true, 1.0f, 1.0f, 1, true, false); LogMusicState("p1"); MusicStep(30);
	pM->Play("02_amb_safe.ogg", true, 0.6f, 0.0f, 3, true, false); LogMusicState("p3"); MusicStep(10);
	pM->Stop(1.0f, 4); LogMusicState("s4"); MusicStep(30);
	pM->Stop(1.0f, 3); LogMusicState("s3");
	pM->Stop(0.0f, 7); LogMusicState("s7");
	pM->Play("03_event_books.ogg", true, 1.0f, 1.0f, 99, true, false); LogMusicState("p99");
	pM->Play("03_event_tomb.ogg", true, 1.0f, 1.0f, -1, true, false); LogMusicState("pm1");
	pM->Stop(1.0f, 2); LogMusicState("s2");
	pM->Stop(1.0f, 1); LogMusicState("s1"); MusicStep(30);
	pM->Stop(1.0f, 0); LogMusicState("s0");

	pM->Play("01_event_dust.ogg", true, 1.0f, 1.0f, 5, true, true);
	{
		cLuxMusicHandler_SaveData sd;
		sd.FromMusicHandler(pM);
		OhplA12bLog("m save cur=%d song=%s vol=%.9g max=%.9g add=%.9g loop=%d close=%d att=%d sea=%d", sd.mlCurrentMaxPrio, sd.msCurrentMusic.c_str(),
			sd.mfCurrentMusicVolume, sd.mfCurrentMusicMaxVolume, sd.mfCurrentMusicVolumeAdd, sd.mbCurrentMusicLoop ? 1 : 0,
			sd.mbEnemyClosePlaying ? 1 : 0, sd.mbAttackPlaying ? 1 : 0, sd.mbSearchPlaying ? 1 : 0);
		for(size_t i=0; i<sd.mvMusic.Size(); ++i)
			if(sd.mvMusic[i].msFile != "")
				OhplA12bLog("m save slot %d %s loop=%d resume=%d vol=%.9g special=%d", (int)i, sd.mvMusic[i].msFile.c_str(), sd.mvMusic[i].mbLoop ? 1 : 0,
					sd.mvMusic[i].mbResume ? 1 : 0, sd.mvMusic[i].mfVolume, sd.mvMusic[i].mbSpecialEffect ? 1 : 0);
		pM->Reset(); LogMusicState("reset");
		sd.ToMusicHandler(apMap, pM); LogMusicState("restored");
	}
	pM->Reset();

	cLuxEnemyIterator it = apMap->GetEnemyIterator();
	iLuxEnemy *pEnemy = it.HasNext() ? it.Next() : NULL;
	OhplA12bLog("m enemy %d", pEnemy ? 1 : 0);
	if(pEnemy)
	{
		for(int t=0; t<2; ++t)
		{
			pM->AddEnemy((eLuxEnemyMusic)t, pEnemy);
			OhplA12bLog("m enemy exist %d %d", t, pM->EnemyExist((eLuxEnemyMusic)t, pEnemy) ? 1 : 0);
			MusicStep(200); LogMusicState("enemy");
			pM->RemoveEnemy((eLuxEnemyMusic)t, pEnemy);
			OhplA12bLog("m enemy exist %d %d", t, pM->EnemyExist((eLuxEnemyMusic)t, pEnemy) ? 1 : 0);
			MusicStep(400); LogMusicState("enemygone");
		}
	}
	pM->OnMapEnter(apMap); LogMusicState("enter");
	pM->OnMapLeave(apMap); LogMusicState("leave");
	pM->Reset();
}

static void LogInsanityState(const char *asWhy)
{
	cLuxInsanityHandler *pI = gpBase->mpInsanityHandler;
	OhplA12bLog("i %s cur=%d count=%.9g possible=%d", asWhy, pI->GetCurrentEvent(), pI->GetNewEventCount(), pI->NewEventIsPossible() ? 1 : 0);
}

static void InsanityScenario(cLuxMap *apMap)
{
	cLuxInsanityHandler *pI = gpBase->mpInsanityHandler;
	int lNum = pI->GetEventNum();
	OhplA12bLog("i events %d", lNum);
	for(int i=0; i<lNum; ++i)
		OhplA12bLog("i event %d %s set=%s max=%.9g used=%d over=%d", i, pI->GetEventName(i).c_str(), pI->GetEventSet(i).c_str(), pI->GetEventMaxSanity(i),
			pI->EventIsUsed(i) ? 1 : 0, pI->EventIsOver(i) ? 1 : 0);

	for(int i=0; i<lNum; ++i)
	{
		pI->StartEvent(i);
		LogInsanityState("start");
		int lStep = 0;
		for(; lStep < 1500 && pI->GetCurrentEvent() >= 0; ++lStep)
		{
			pI->UpdateA12(OhplA12bDt());
			pI->OnDraw(OhplA12bDt());
			if(lStep % 100 == 0) LogInsanityState("step");
		}
		OhplA12bLog("i event %d ran %d steps", i, lStep);
		pI->StopCurrentEvent();
		LogInsanityState("stop");
	}

	pI->StartEvent(std::string("nonexistent")); LogInsanityState("noname");
	if(lNum > 0)
	{
		pI->StartEvent(pI->GetEventName(lNum - 1)); LogInsanityState("byname");
	}
	pI->StopCurrentEvent();

	pI->DisableSet("Default");
	for(int i=0; i<3; ++i) { pI->StartEvent(); LogInsanityState("rand_disabled"); pI->StopCurrentEvent(); }
	pI->EnableSet("Default");
	for(int i=0; i<12; ++i)
	{
		pI->StartEvent();
		LogInsanityState("rand");
		for(int s=0; s<20 && pI->GetCurrentEvent() >= 0; ++s) { pI->UpdateA12(OhplA12bDt()); pI->OnDraw(OhplA12bDt()); }
		pI->StopCurrentEvent();
	}

	pI->DisableSet("Other");
	cLuxInsanityHandler_SaveData sd;
	sd.FromInsanityHandler(pI);
	OhplA12bLog("i save count=%.9g events=%d sets=%d", sd.mfNewEventCount, (int)sd.mvEvents.Size(), (int)sd.mvDisabledSets.Size());
	for(size_t i=0; i<sd.mvEvents.Size(); ++i) OhplA12bLog("i save used %d %d", (int)i, sd.mvEvents[i].mbUsed ? 1 : 0);
	for(size_t i=0; i<sd.mvDisabledSets.Size(); ++i) OhplA12bLog("i save set %s", sd.mvDisabledSets[i].msName.c_str());
	pI->Reset();
	pI->EnableSet("Other");
	LogInsanityState("reset");
	sd.ToInsanityHandler(apMap, pI);
	LogInsanityState("restored");
	for(int i=0; i<lNum; ++i) OhplA12bLog("i used %d %d", i, pI->EventIsUsed(i) ? 1 : 0);
	pI->Reset();

	for(int i=0; i<300; ++i) pI->UpdateA12(OhplA12bDt());
	LogInsanityState("idle");
	pI->ReloadEvents();
	OhplA12bLog("i reloaded %d", pI->GetEventNum());
}

static void InputScenario()
{
	cLuxInputHandler *pH = gpBase->mpInputHandler;
	for(int c=0; c<eLuxActionCategory_LastEnum; ++c)
	{
		tLuxActionVec v = pH->GetActionsByCategory((eLuxActionCategory)c);
		for(size_t i=0; i<v.size(); ++i)
		{
			OhplA12bLog("a cat=%d %s id=%d conf=%d", c, v[i]->msName.c_str(), v[i]->mlId, v[i]->mbConfigurable ? 1 : 0);
			tLuxInputVec in = pH->GetDefaultInputsByActionId(v[i]->mlId);
			for(size_t j=0; j<in.size(); ++j) OhplA12bLog("a def %s %d %d", in[j]->msInputType.c_str(), in[j]->mlValue, in[j]->mlActionId);
			cLuxAction *pA = pH->GetActionByName(v[i]->msName);
			cLuxAction *pB = pH->GetActionById(v[i]->mlId);
			OhplA12bLog("a look %d %d", pA == NULL ? -1 : pA->mlId, pB == NULL ? -1 : pB->mlId);
		}
	}
	OhplA12bLog("a missing %d %d", pH->GetActionByName("no_such") == NULL ? 1 : 0, pH->GetActionById(-77) == NULL ? 1 : 0);
	OhplA12bLog("a settings invert=%d smooth=%d sens=%.9g", pH->GetInvertMouse() ? 1 : 0, pH->GetSmoothMouse() ? 1 : 0, pH->GetMouseSensitivity());
	pH->SetMouseSensitivity(2.5f);
	pH->SetInvertMouse(true);
	pH->SetSmoothMouse(true);
	OhplA12bLog("a settings2 invert=%d smooth=%d sens=%.9g", pH->GetInvertMouse() ? 1 : 0, pH->GetSmoothMouse() ? 1 : 0, pH->GetMouseSensitivity());
	pH->ResetSmoothMousePos();
	for(int i=0; i<12; ++i)
	{
		cVector2f v = pH->GetSmoothMousePos(cVector2f((float)(i * 3 - 7), (float)(5 - i)));
		OhplA12bLog("a smooth %.9g %.9g", v.x, v.y);
	}
	pH->SaveUserConfig();
	OhplA12bLog("a state %d", (int)pH->GetState());
	pH->ChangeState(eLuxInputState_Inventory);
	OhplA12bLog("a state %d", (int)pH->GetState());
	pH->ChangeState(eLuxInputState_Game);
}

static void ConfigScenario()
{
	cLuxConfigHandler *c = gpBase->mpConfigHandler;
	OhplA12bLog("c debug=%d first=%d ext=%s lang=%s cache=%d", c->mbLoadDebugMenu ? 1 : 0, c->mbFirstStart ? 1 : 0, c->ScreenShotExt().c_str(), c->LangFile().c_str(), c->mbForceCacheLoadingAndSkipSaving ? 1 : 0);
	OhplA12bLog("c screen=%d,%d display=%d full=%d vsync=%d avsync=%d", c->mvScreenSize.x, c->mvScreenSize.y, c->mlDisplay, c->mbFullscreen ? 1 : 0, c->mbVSync ? 1 : 0, c->mbAdaptiveVSync ? 1 : 0);
	OhplA12bLog("c tex=%d filt=%d aniso=%.9g shq=%d shres=%d shadows=%d gui=%d", c->mlTextureQuality, c->mlTextureFilter, c->mfTextureAnisotropy, c->mlShadowQuality, c->mlShadowRes, c->mbShadowsActive ? 1 : 0, c->mlGuiScale);
	OhplA12bLog("c renderer=%s color=%s pw=%.9g peak=%.9g", c->Renderer().c_str(), c->ColorOutput().c_str(), c->mfPaperWhite, c->mfPeakNits);
	OhplA12bLog("c ssao=%d,%d,%d par=%d,%d occ=%d edge=%d refl=%d refr=%d sm=%d", c->mbSSAOActive ? 1 : 0, c->mlSSAOSamples, c->mlSSAOResolution, c->mbParallaxEnabled ? 1 : 0, c->mlParallaxQuality,
		c->mbOcclusionTestLights ? 1 : 0, c->mbEdgeSmooth ? 1 : 0, c->mbWorldReflection ? 1 : 0, c->mbRefraction ? 1 : 0, c->mbForceShaderModel3And4Off ? 1 : 0);
	OhplA12bLog("c fast=%d,%d,%d snd=%d,%d,%d,%d", c->mbFastPhysicsLoad ? 1 : 0, c->mbFastStaticLoad ? 1 : 0, c->mbFastEntityLoad ? 1 : 0, c->mlSoundDevID, c->mlMaxSoundChannels, c->mlSoundStreamBuffers, c->mlSoundStreamBufferSize);
	c->SaveMainConfig();
	cGuiSet *pSet = NULL;
	OhplA12bLog("c restart warn(before)");
	c->SetGameNeedsRestart();
	c->SetGameNeedsRestart();
	(void)pSet;
}

static int glFrame = 0;
static bool gbDone = false;

void OhplA12bTick()
{
	if(OhplA12bEnabled() == false || gbDone) return;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glFrame = 0; return; }
	if(++glFrame < 10) return;
	gbDone = true;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	OhplA12bLog("map %s", pMap->GetName().c_str());
	cMath::Randomize(1234);
	ConfigScenario();
	MusicScenario(pMap);
	InsanityScenario(pMap);
	InputScenario();
	OhplA12bLog("done");
}
