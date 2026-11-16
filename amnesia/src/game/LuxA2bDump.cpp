#include "LuxA2bDump.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct OhplA2bDump
{
	static FILE *mpOut;
	static int mlFrame;

	static void F(const char *asName, float afX)
	{
		unsigned u;
		memcpy(&u, &afX, 4);
		fprintf(mpOut, " %s=%08x", asName, u);
	}
	static void I(const char *asName, int alX) { fprintf(mpOut, " %s=%d", asName, alX); }
	static void Words(const char *asTag, const void *apData, size_t alBytes, unsigned alSkip=0)
	{
		fprintf(mpOut, "%s", asTag);
		const unsigned char *p = (const unsigned char*)apData;
		for(size_t i=0; i+4<=alBytes; i+=4)
		{
			unsigned u;
			memcpy(&u, p+i, 4);
			if((alSkip >> (i/4)) & 1) fprintf(mpOut, " --");
			else fprintf(mpOut, " %08x", u);
		}
		fputc('\n', mpOut);
	}

	static void Script(cLuxPlayer *p, int f)
	{
		if(f == 3) p->GetHelperLantern()->SetActive(true, true, true, true);
		if(f == 10) p->SetLampOil(60);
		if(f == 20) p->AddLampOil(-10);
		if(f == 30) p->GetHelperLookAt()->SetTarget(p->GetCamera()->GetPosition() + cVector3f(1,0.2f,1), 1.0f, 2.0f, "");
		if(f == 70) p->GetHelperLookAt()->SetActive(false);
		if(f == 40) p->LowerSanity(20, true);
		if(f == 60) p->GiveSanityDamage(10);
		if(f == 75) p->SetSanity(15);
		if(f == 90) p->GetHelperHudEffect()->Flash(cColor(1,0,0,0.8f), eGuiMaterial_Alpha, 0.2f, 0.5f);
		if(f == 100) p->LowerSanity(100, true);
		if(f == 130) p->AddSanity(30, true);
		if(f == 140) p->GiveDamage(15, 1, eLuxDamageType_BloodSplat, true, false);
		if(f == 160) p->GiveDamage(10, 1, eLuxDamageType_Claws, false, false);
		if(f == 175) p->GiveDamage(10, 1, eLuxDamageType_Slash, true, false);
		if(f == 190) p->AddHealth(20);
		if(f >= 200 && f < 230) p->SetLean(1.0f);
		if(f >= 235 && f < 250) p->AddLean(-0.1f);
		if(f == 255) p->GetHelperLantern()->SetDisabled(true);
		if(f == 270) p->GetHelperLantern()->SetDisabled(false);
		if(f == 275) p->GetHelperFlashback()->Start("flashbacks/CH01L03_MapRoom", "");
		if(f == 280) p->GetHelperSpawnPS()->Start("misc/torture_fog.sps");
		if(f == 295) p->GetHelperSpawnPS()->Stop();
		if(f == 300) p->GiveDamage(1000, 1, eLuxDamageType_BloodSplat, true, true);
	}

	static void State(cLuxPlayer *p)
	{
		fprintf(mpOut, "f %d", mlFrame);
		F("health", p->GetHealth());
		F("sanity", p->GetSanity());
		F("oil", p->GetLampOil());
		F("terror", p->GetTerror());
		fputc('\n', mpOut);

		cLuxPlayerInsanityCollapse *pCol = p->GetInsanityCollapse();
		Words("col", &pCol->mS, sizeof(pCol->mS), 1<<5);
		Words("cam", &p->GetCamDirEffects()->mS.k, sizeof(p->GetCamDirEffects()->mS.k));
		Words("hurt", &p->mpHurt->mS, sizeof(p->mpHurt->mS));
		Words("look", &p->GetHelperLookAt()->mS, sizeof(p->GetHelperLookAt()->mS));
		cLuxPlayerFlashback *fb = p->GetHelperFlashback();
		fprintf(mpOut, "flash");
		F("run", fb->mfRunSpeedMul); F("delay", fb->mfFlashDelay); I("act", fb->mbActive); F("start", fb->mbActive ? fb->mfFlashbackStartCount : 0);
		fputc('\n', mpOut);
		Words("lant", &p->GetHelperLantern()->mS, sizeof(p->GetHelperLantern()->mS));
		Words("light", &p->GetHelperLightLevel()->mS, sizeof(p->GetHelperLightLevel()->mS), 3);
		Words("dark", &p->GetHelperInDarkness()->mS, sizeof(p->GetHelperInDarkness()->mS));

		cLuxPlayerSanity *s = p->GetHelperSanity();
		fprintf(mpOut, "san");
		F("hitA", s->mfHitAlpha); I("hitAct", s->mbHitActive); F("lost", s->mfSanityLostCount); F("low", s->mfAtLowSanityCount);
		I("hitUp", s->mbHitIsUpdated); I("lostUp", s->mbSanityLostIsUpdated); F("pant", s->mfPantCount);
		F("chk", s->mfCheckEnemySeenCount); I("effUp", s->mbSanityEffectUpdated); F("t", s->mfT); F("wave", s->mfInsaneWaveAlpha);
		F("drain", s->mfSanityDrainCount); F("drainV", s->mfSanityDrainVolume); F("beat", s->mfSanityHeartbeatCount);
		F("seenC", s->mfSeenEnemyCount); I("seen", s->mbEnemyIsSeen); F("hint", s->mfShowHintTimer);
		fputc('\n', mpOut);

		cLuxPlayerDeath *d = p->GetHelperDeath();
		fprintf(mpOut, "death");
		I("act", d->mbActive); I("state", d->mlState); F("hAdd", d->mfHeightAdd); F("roll", d->mfRoll); F("t", d->mfT);
		F("minH", d->mfMinHeightAdd); F("fade", d->mfFadeAlpha); F("t1", d->mfTextAlpha1); F("t2", d->mfTextAlpha2);
		F("onScr", d->mfTextOnScreenCount); F("white", d->mfWhiteCount); I("menu", d->mbToMainMenu);
		fputc('\n', mpOut);

		cLuxPlayerLean *l = p->mpLean;
		fprintf(mpOut, "lean");
		F("dir", l->mfDir); F("add", l->mfDirAdd); F("mov", l->mfMovement); F("rot", l->mfRotation); I("press", l->mbPressed);
		fputc('\n', mpOut);

		cLuxPlayerHudEffect *h = p->GetHelperHudEffect();
		fprintf(mpOut, "hud");
		I("splashes", (int)h->mlstSplashes.size());
		for(cLuxPlayerHudEffect_SplashListIt it = h->mlstSplashes.begin(); it != h->mlstSplashes.end(); ++it)
			F("a", it->mfAlpha);
		F("fa", h->mfFlashAlpha); I("fact", h->mbFlashActive); F("fsp", h->mfFlashAlphaSpeed);
		fputc('\n', mpOut);

		cLuxPlayerSpawnPS *sp = p->GetHelperSpawnPS();
		fprintf(mpOut, "sps");
		I("act", sp->mbActive); I("n", (int)sp->mvSpawnPos.size());
		fputc('\n', mpOut);
		fflush(mpOut);
	}
};

FILE *OhplA2bDump::mpOut = NULL;
int OhplA2bDump::mlFrame = 0;

bool OhplA2bTick(cLuxPlayer *apPlayer)
{
	static bool bInit = false;
	if(bInit == false)
	{
		bInit = true;
		const char *sPath = getenv("OHPL_A2B_DUMP");
		if(sPath && *sPath) OhplA2bDump::mpOut = fopen(sPath, "w");
	}
	if(OhplA2bDump::mpOut == NULL) return false;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { OhplA2bDump::mlFrame = 0; return true; }

	int f = ++OhplA2bDump::mlFrame;
	OhplA2bDump::Script(apPlayer, f);
	apPlayer->RunHelperMessage(eUpdateableMessage_Update, 1.0f/60.0f);
	OhplA2bDump::State(apPlayer);
	return true;
}
