#include "LuxA11bDump.h"

#include "LuxHintHandler.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
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
		const char *sPath = getenv("OHPL_A11B_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplA11Enabled()
{
	return Dump() != NULL;
}

void OhplA11Log(const char *fmt, ...)
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

static void LogText(FILE *f, const tWString &asText)
{
	for(size_t i=0; i<asText.size(); ++i)
	{
		unsigned c = (unsigned)asText[i];
		if(c >= 32 && c < 127) fputc((int)c, f);
		else fprintf(f, "\\u%04x", c);
	}
}

void OhplA11LogFont(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alAlign, int alMat, const tWString &asText)
{
	FILE *f = Dump();
	if(f == NULL) return;
	fprintf(f, "F %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %d |", avPos.x, avPos.y, avPos.z, avSize.x, avSize.y,
		aCol.r, aCol.g, aCol.b, aCol.a, alAlign, alMat);
	LogText(f, asText);
	fputc('\n', f);
	fflush(f);
}

void OhplA11LogGfx(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alMat)
{
	OhplA11Log("G %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d", avPos.x, avPos.y, avPos.z, avSize.x, avSize.y,
		aCol.r, aCol.g, aCol.b, aCol.a, alMat);
}

class cOhplA11Callback : public iLuxMessageCallback
{
public:
	void OnPress(bool abYes) { OhplA11Log("cb %d", abYes ? 1 : 0); }
};

static cOhplA11Callback gCallback;
static int glFrame = 0;

static void LogState(int alFrame)
{
	OhplA11Log("tick %d hint=%d msg=%d pause=%d sub=%d esub=%d", alFrame, gpBase->mpHintHandler->IsActive() ? 1 : 0,
		gpBase->mpMessageHandler->IsMessageActive() ? 1 : 0, gpBase->mpMessageHandler->IsPauseMessageActive() ? 1 : 0,
		gpBase->mpMessageHandler->ShowSubtitles() ? 1 : 0, gpBase->mpMessageHandler->ShowEffectSubtitles() ? 1 : 0);
}

void OhplA11Tick()
{
	if(OhplA11Enabled() == false) return;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glFrame = 0; return; }

	int f = ++glFrame;
	cLuxHintHandler *pH = gpBase->mpHintHandler;
	cLuxMessageHandler *pM = gpBase->mpMessageHandler;
	switch(f)
	{
	case 3:
		OhplA11Log("add a %d", pH->Add("a11b_a", _W("Hint one"), 2.0f) ? 1 : 0);
		OhplA11Log("add a again %d", pH->Add("a11b_a", _W("Hint one"), 2.0f) ? 1 : 0);
		break;
	case 6:
		pM->SetMessage(_W("Game message one that is somewhat long and has to wrap over several rows of the screen"), 2.0f);
		break;
	case 9:
		pH->Block("a11b_b");
		OhplA11Log("add b blocked %d", pH->Add("a11b_b", _W("Blocked"), 1.0f) ? 1 : 0);
		pH->UnBlock("a11b_b");
		OhplA11Log("remove a %d", pH->Remove("a11b_a") ? 1 : 0);
		OhplA11Log("remove zz %d", pH->Remove("zz") ? 1 : 0);
		break;
	case 12:
		pM->StarQuestAddedMessage();
		break;
	case 15:
		pM->StartPauseMessage(_W("Pause message text that is long enough to wrap around the pause box more than once, really"), true, &gCallback);
		break;
	case 40:
		pM->DoAction(eLuxPlayerAction_Interact, true);
		break;
	case 41:
		pM->DoAction(eLuxPlayerAction_Interact, false);
		break;
	case 45:
		pM->StartPauseMessage(_W("Second pause"), false, NULL);
		break;
	case 50:
		pM->DoAction(eLuxPlayerAction_Attack, true);
		break;
	case 55:
		pM->StartPauseMessage(_W("Third pause"), true, &gCallback);
		break;
	case 60:
		pM->DoAction(eLuxPlayerAction_Attack, true);
		break;
	case 65:
		pM->SetPauseMessageActive(true);
		break;
	case 70:
		pM->SetPauseMessageActive(false);
		break;
	case 85:
		OhplA11Log("add c %d", pH->Add("a11b_c", _W("Press $GamepadButton.A$ and $GamepadButton.Guide$ and $GamepadHat.0.Up$ and $GamepadAxis.LeftX.Positive$ to go on with a long line of text"), 1.0f) ? 1 : 0);
		break;
	case 80:
		pH->SetActive(false);
		break;
	case 82:
		pH->SetActive(true);
		break;
	case 250:
		OhplA11Log("add d %d", pH->Add("a11b_d", _W("Duration derived from the string"), 0) ? 1 : 0);
		break;
	case 100:
	{
		cLuxHintHandler_SaveData sd;
		sd.FromHintHandler(pH);
		cContainerListIterator<cLuxHintHandler_Hint_SaveData> it = sd.mlstHintsGiven.GetIterator();
		while(it.HasNext()) OhplA11Log("given %s", it.Next().msName.c_str());
		it = sd.mlstHintsBlocked.GetIterator();
		while(it.HasNext()) OhplA11Log("blocked %s", it.Next().msName.c_str());
		sd.ToHintHandler(NULL, pH);
		pM->Reset();
		pH->Reset();
		break;
	}
	}
	LogState(f);
}
