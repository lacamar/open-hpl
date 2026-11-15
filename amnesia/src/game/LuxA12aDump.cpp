#include "LuxA12aDump.h"

#include "LuxEffectHandler.h"
#include "LuxEffectRenderer.h"
#include "LuxPostEffects.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>

static FILE *gpDump = NULL;
static bool gbInit = false;
static bool gbInTick = false;
static int glFrame = 0;
static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A12A_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplA12Enabled()
{
	return Dump() != NULL;
}

float OhplA12Dt()
{
	return 1.0f / 60.0f;
}

void OhplA12Log(const char *fmt, ...)
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

std::string OhplA12F(float afX)
{
	unsigned u;
	memcpy(&u, &afX, 4);
	char b[16];
	snprintf(b, sizeof(b), "%08x", u);
	return b;
}

std::string OhplA12D(double afX)
{
	unsigned long long u;
	memcpy(&u, &afX, 8);
	char b[24];
	snprintf(b, sizeof(b), "%016llx", u);
	return b;
}

static std::string Esc(const unsigned *apText, size_t alLen)
{
	std::string s;
	char b[16];
	for(size_t i=0; i<alLen; ++i)
	{
		unsigned c = apText[i];
		if(c >= 33 && c < 127 && c != '\\' && c != '|') { s += (char)c; continue; }
		snprintf(b, sizeof(b), "\\u%04x", c);
		s += b;
	}
	return s;
}

std::string OhplA12S(const tString &asText)
{
	std::vector<unsigned> v(asText.size());
	for(size_t i=0; i<asText.size(); ++i) v[i] = (unsigned char)asText[i];
	return Esc(v.empty() ? NULL : &v[0], v.size());
}

std::string OhplA12S(const tWString &asText)
{
	std::vector<unsigned> v(asText.size());
	for(size_t i=0; i<asText.size(); ++i) v[i] = (unsigned)asText[i];
	return Esc(v.empty() ? NULL : &v[0], v.size());
}

int OhplA12Frame()
{
	return glFrame;
}

bool OhplA12InTick()
{
	return gbInTick;
}

//-----------------------------------------------------------------------

namespace
{

struct Cmd
{
	int mlFrom, mlTo;
	std::string msLine;
};

std::vector<Cmd> gvCmds;
bool gbLoaded = false;

void Load()
{
	gbLoaded = true;
	const char *sPath = getenv("OHPL_A12A_SCRIPT");
	if(sPath == NULL) return;
	std::ifstream in(sPath);
	std::string sLine;
	while(std::getline(in, sLine))
	{
		if(sLine.empty() || sLine[0] == '#') continue;
		std::istringstream ss(sLine);
		std::string sSpan;
		ss >> sSpan;
		Cmd c;
		size_t lDash = sSpan.find('-');
		c.mlFrom = atoi(sSpan.c_str());
		c.mlTo = lDash == std::string::npos ? c.mlFrom : atoi(sSpan.c_str() + lDash + 1);
		std::getline(ss, c.msLine);
		gvCmds.push_back(c);
	}
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

tString Str(std::istringstream &ss)
{
	std::string s;
	ss >> s;
	return s == "-" ? "" : s;
}

void Run(const std::string &asLine)
{
	std::istringstream ss(asLine);
	std::string sCmd;
	ss >> sCmd;
	OhplA12Log("cmd%s", asLine.c_str());

	cLuxEffectHandler *pE = gpBase->mpEffectHandler;
	float a=0, b=0, c=0, d=0;

	if(sCmd == "fade_in")			{ ss >> a; pE->GetFade()->FadeIn(a); }
	else if(sCmd == "fade_out")		{ ss >> a; pE->GetFade()->FadeOut(a); }
	else if(sCmd == "fade_direct")	{ ss >> a; pE->GetFade()->SetDirectAlpha(a); }
	else if(sCmd == "flash")		{ ss >> a >> b >> c; pE->GetFlash()->Start(a, b, c); }
	else if(sCmd == "sanity")		{ pE->GetSanityGainFlash()->Start(); }
	else if(sCmd == "emotion")		{ tString sCat = Str(ss), sEntry = Str(ss); pE->GetEmotionFlash()->Start(sCat, sEntry, ""); }
	else if(sCmd == "emotion_act")	{ int i=0; ss >> i; pE->GetEmotionFlash()->DoAction(eLuxPlayerAction_Interact, i != 0); }
	else if(sCmd == "radial")		{ ss >> a >> b; pE->GetRadialBlur()->FadeTo(a, b); }
	else if(sCmd == "radial_dist")	{ ss >> a; pE->GetRadialBlur()->SetBlurStartDist(a); }
	else if(sCmd == "sepia")		{ ss >> a >> b; pE->GetSepiaColor()->FadeTo(a, b); }
	else if(sCmd == "trail")		{ ss >> a >> b; pE->GetImageTrail()->FadeTo(a, b); }
	else if(sCmd == "shake")		{ ss >> a >> b >> c >> d; pE->GetScreenShake()->Start(a, b, c, d); }
	else if(sCmd == "voice" || sCmd == "multivoice")
	{
		tString sVoice = Str(ss), sEffect = Str(ss), sCat = Str(ss);
		tString vEntry[7];
		float vDelay[7] = {0,0,0,0,0,0,0};
		int lTexts = sCmd == "voice" ? 1 : 7;
		for(int i=0; i<lTexts; ++i) { vEntry[i] = Str(ss); ss >> vDelay[i]; }
		int lPos=0, lPrio=-1, lRemove=1;
		float x=0,y=0,z=0,fMin=0,fMax=0;
		ss >> lPos >> x >> y >> z >> fMin >> fMax >> lPrio >> lRemove;
		pE->GetPlayVoice()->AddMultiSubbedVoice(sVoice, sEffect, sCat,
			vEntry[0], vDelay[0], vEntry[1], vDelay[1], vEntry[2], vDelay[2], vEntry[3], vDelay[3],
			vEntry[4], vDelay[4], vEntry[5], vDelay[5], vEntry[6], vDelay[6],
			lPos != 0, cVector3f(x,y,z), fMin, fMax, lPrio, lRemove != 0);
	}
	else if(sCmd == "voice_stop")	{ ss >> a; pE->GetPlayVoice()->StopVoices(a); }
	else if(sCmd == "voice_pause")	{ pE->GetPlayVoice()->PauseCurrentVoices(); }
	else if(sCmd == "voice_unpause"){ pE->GetPlayVoice()->UnpauseCurrentVoices(); }
	else if(sCmd == "voice_vol")	{ ss >> a; pE->GetPlayVoice()->SetVolumeMul(a); }
	else if(sCmd == "voice_cb")		{ pE->GetPlayVoice()->SetOverCallback(Str(ss)); }
	else if(sCmd == "commentary")	{ tString sFile = Str(ss); int lIcon=-1; ss >> lIcon; pE->GetPlayCommentary()->Start("talker", "topic", sFile, lIcon); }
	else if(sCmd == "commentary_stop") { pE->GetPlayCommentary()->Stop(); }
	else if(sCmd == "map_leave")	{ pE->OnMapLeave(gpBase->mpMapHandler->GetCurrentMap()); }
	else if(sCmd == "reset")		{ pE->Reset(); }
	else if(sCmd == "player_paused"){ int i=0; ss >> i; pE->SetPlayerIsPaused(i != 0); }
	else if(sCmd == "insanity")
	{
		int lOn=0;
		ss >> lOn >> a >> b >> c;
		cLuxPostEffect_Insanity *pI = gpBase->mpPostEffectHandler->GetInsanity();
		pI->SetWaveAlpha(a);
		pI->SetZoomAlpha(b);
		pI->SetWaveSpeed(c);
		pI->SetActive(lOn != 0);
	}
	else if(sCmd == "rflash")		{ ss >> a; iRenderable *p = FirstObject(); if(p) gpBase->mpEffectRenderer->AddFlashObject(p, a); }
	else if(sCmd == "rglow")		{ ss >> a; iRenderable *p = FirstObject(); if(p) gpBase->mpEffectRenderer->AddEnemyGlow(p, a); }
	else if(sCmd == "routline")		{ iRenderable *p = FirstObject(); if(p) gpBase->mpEffectRenderer->AddOutlineObject(p); }
	else if(sCmd == "rclear_outline"){ gpBase->mpEffectRenderer->ClearOutlineObjects(); }
	else if(sCmd == "rreset")		{ gpBase->mpEffectRenderer->Reset(); }
}

void LogState()
{
	OhplA12aState s;
	OhplA12FillState(&s);
	std::string out;
	char b[512];
	snprintf(b, sizeof(b), "fade %s %s %s %u", OhplA12F(s.fade.goal).c_str(), OhplA12F(s.fade.alpha).c_str(), OhplA12F(s.fade.speed).c_str(), s.fade.active);
	OhplA12Log("%s", b);
	snprintf(b, sizeof(b), "flash a=%s act=%u", OhplA12F(s.flash.alpha).c_str(), s.flash.active);
	out = b;
	if(s.flash.active) out += " step=" + std::to_string(s.flash.step) + " c=" + (s.flash.step >= 1 ? OhplA12F(s.flash.count) : std::string("-")) + " " + OhplA12F(s.flash.inSpeed) + " " + OhplA12F(s.flash.whiteSpeed) + " " + OhplA12F(s.flash.outSpeed);
	OhplA12Log("%s", out.c_str());
	snprintf(b, sizeof(b), "sanity a=%s act=%u", OhplA12F(s.sanity.alpha).c_str(), s.sanity.active);
	out = b;
	if(s.sanity.active) out += " step=" + std::to_string(s.sanity.step) + " c=" + (s.sanity.step >= 1 ? OhplA12F(s.sanity.count) : std::string("-")) + " " + OhplA12F(s.sanity.inSpeed) + " " + OhplA12F(s.sanity.whiteSpeed) + " " + OhplA12F(s.sanity.outSpeed);
	OhplA12Log("%s", out.c_str());
	const OhplGa9bAmount *vAm[3] = { &s.trail, &s.sepia, &s.radial };
	const char *vAmName[3] = { "trail", "sepia", "radial" };
	for(int i=0; i<3; ++i)
	{
		out = std::string(vAmName[i]) + " " + OhplA12F(vAm[i]->amount) + " " + OhplA12F(vAm[i]->goal) + " " + OhplA12F(vAm[i]->speed) + " " + std::to_string(vAm[i]->active);
		if(i == 2) out += " " + OhplA12F(s.radialStart);
		OhplA12Log("%s", out.c_str());
	}
	out = "emotion act=" + std::to_string(s.emotion.active);
	if(s.emotion.active) out += " " + OhplA12F(s.emotion.alpha) + " " + std::to_string(s.emotion.step) + " " + (s.emotion.step >= 1 ? OhplA12F(s.emotion.count) : std::string("-")) + " " + OhplA12F(s.emotion.textAlpha) + " " + OhplA12F(s.emotion.textTime);
	OhplA12Log("%s", out.c_str());
	OhplA12Log("shake act=%u n=%u", s.shakeActive, s.shakeCount);
	OhplA12Log("voice act=%u pause=%u n=%u rows=%u queue=%u entries=%u vol=%s", s.voiceActive, s.voicePaused, s.voiceCount, s.voiceRows, s.voiceQueue, s.voiceEntries, OhplA12F(s.voiceVol).c_str());
	OhplA12Log("commentary act=%u icon=%d", s.commentaryActive, s.commentaryIcon);
	OhplA12Log("paused %u", s.playerPaused);
	if(glFrame >= 3)
		OhplA12Log("insanity act=%u %s %s %s", s.insActive, OhplA12F(s.insWave).c_str(), OhplA12F(s.insZoom).c_str(), OhplA12F(s.insSpeed).c_str());
}

}

void OhplA12Tick()
{
	if(OhplA12Enabled() == false) return;
	if(gbLoaded == false) Load();
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glFrame = 0; return; }

	int f = ++glFrame;
	for(size_t i=0; i<gvCmds.size(); ++i)
		if(f >= gvCmds[i].mlFrom && f <= gvCmds[i].mlTo) Run(gvCmds[i].msLine);

	gbInTick = true;
	gpBase->mpEffectHandler->Update(OhplA12Dt());
	gpBase->mpEffectRenderer->Update(OhplA12Dt());
	gpBase->mpPostEffectHandler->Update(OhplA12Dt());
	gbInTick = false;
	OhplA12Log("tick %d", f);
	LogState();
}
