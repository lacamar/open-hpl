#include "LuxA2cDump.h"

#include "LuxPlayer.h"
#include "LuxPlayerHands.h"
#include "LuxPlayerHelpers.h"
#include "LuxInventory.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// OHPL_A2C_DUMP: scripted player inputs inside the real cLuxPlayer::Update, one state line per update
static FILE *gpOut = NULL;
static int glTick = 0;
static float gfStep = 0;

static void F(const char *asName, float afX)
{
	unsigned u;
	memcpy(&u, &afX, 4);
	fprintf(gpOut, " %s=%08x", asName, u);
}

static void V3(const char *asName, const cVector3f &avX)
{
	F(asName, avX.x);
	F("", avX.y);
	F("", avX.z);
}

static FILE *Out()
{
	static bool bInit = false;
	if(bInit == false)
	{
		bInit = true;
		const char *sPath = getenv("OHPL_A2C_DUMP");
		if(sPath && *sPath) gpOut = fopen(sPath, "w");
	}
	return gpOut;
}

static void Script(cLuxPlayer *p, int f)
{
	if(f == 2) p->SetLampOil(80);
	if(f >= 10 && f < 60) p->Move(eCharDir_Forward, 1.0f);
	if(f >= 15 && f < 45) { p->AddYaw(0.01f); p->AddPitch(0.004f); }
	if(f >= 45 && f < 70) { p->AddYaw(-0.015f); p->AddPitch(-0.008f); }
	if(f == 70) p->Run(true);
	if(f >= 70 && f < 120) p->Move(eCharDir_Forward, 1.0f);
	if(f == 120) p->Run(false);
	if(f == 125) p->Crouch(true);
	if(f >= 125 && f < 170) p->Move(eCharDir_Right, 1.0f);
	if(f == 170) p->Crouch(false);
	if(f == 180) p->Jump(true);
	if(f == 183) p->Jump(false);
	if(f >= 190 && f < 215) p->SetLean(1.0f);
	if(f >= 215 && f < 240) p->SetLean(-1.0f);
	if(f == 240) p->SetLean(0.0f);
	if(f == 242) p->GetHelperLantern()->SetActive(true, true, true, true);
	if(f == 245) p->DoAction(eLuxPlayerAction_Lantern, true);
	if(f == 262) p->DoAction(eLuxPlayerAction_Lantern, true);
	if(f == 246) p->DoAction(eLuxPlayerAction_Lantern, false);
	if(f == 300) { bool bRemove = false; gpBase->mpInventory->AddItem("a2c_potion", eLuxItemType_Health, "health", "agrippa_head.tga", 25, "", "", &bRemove); p->AddCoins(3); }
	if(f == 310) p->GiveDamage(12, 1, eLuxDamageType_Claws, true, false);
	if(f == 330) p->AddHealth(10);
	if(f == 340) p->LowerSanity(10, true);
	if(f == 345) p->SetFreeCamActive(true);
	if(f >= 345 && f < 355) { p->Move(eCharDir_Forward, 1.0f); p->AddYaw(0.02f); }
	if(f == 355) p->SetFreeCamActive(false);
	if(f == 360) p->DoAction(eLuxPlayerAction_Lantern, true);
	if(f == 361) p->DoAction(eLuxPlayerAction_Lantern, false);
	if(f >= 365 && f < 380) p->FadeRollTo(0.2f, 1.0f, 1.0f);
	if(f == 380) p->FadeFOVMulTo(0.8f, 1.0f);
	if(f == 400) p->FadeRollTo(0.0f, 1.0f, 1.0f);
	if(f == 410) p->FadePitchTo(0.1f, 1.0f, 1.0f);
}

void OhplA2cPre(cLuxPlayer *p, float afTimeStep)
{
	if(Out() == NULL) return;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glTick = 0; return; }
	++glTick;
	gfStep = afTimeStep;
	srand((unsigned)(glTick * 2654435761u));
	Script(p, glTick);
}

void OhplA2cPost(cLuxPlayer *p)
{
	if(Out() == NULL || glTick == 0) return;
	iCharacterBody *pBody = p->GetCharacterBody();
	cCamera *pCam = p->GetCamera();
	fprintf(gpOut, "f %d", glTick);
	F("dt", gfStep);
	V3("pos", pBody->GetPosition());
	V3("vel", pBody->GetVelocity(1.0f/60.0f));
	F("yaw", pCam->GetYaw());
	F("pitch", pCam->GetPitch());
	F("roll", pCam->GetRoll());
	F("fov", pCam->GetFOV());
	V3("cpos", pCam->GetPosition());
	F("hp", p->GetHealth());
	F("san", p->GetSanity());
	F("oil", p->GetLampOil());
	fprintf(gpOut, " lant=%d st=%d ms=%d", p->GetHelperLantern()->IsActive() ? 1 : 0, (int)p->GetCurrentState(), (int)p->GetCurrentMoveState());
	iLuxEntity *pEnt = p->GetEntityInFocus();
	fprintf(gpOut, " focus=%s", pEnt ? pEnt->GetName().c_str() : "-");
	cLuxPlayerHands *pH = p->GetHands();
	fprintf(gpOut, " hands=%d/%s coins=%d avg=", (int)pH->GetState(), pH->GetCurrentAnimation().c_str(), p->GetCoins());
	F("", p->GetAvgSpeed());
	fputc('\n', gpOut);
	fflush(gpOut);
	if(glTick == 450) { fprintf(gpOut, "END\n"); fflush(gpOut); }
}
