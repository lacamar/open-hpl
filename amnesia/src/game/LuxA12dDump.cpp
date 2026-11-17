#include "LuxA12dDump.h"
#include "LuxDebugHandler.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxEntity.h"
#include "LuxInsanityHandler.h"
#include "LuxConfigHandler.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <map>
#include <vector>

// OHPL_A12D_DUMP: turns the debug menu on and drives the real cLuxDebugHandler on a real map
// (window, widgets, overlays, inspection, reload paths); every observable effect is logged.

static FILE *gpDump = NULL;
static bool gbInit = false;

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A12D_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplA12dEnabled() { return Dump() != NULL; }

void OhplA12dLog(const char *fmt, ...)
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

//-----------------------------------------------------------------------

static bool gbCapture = false;

// timing-dependent values are masked; sound rows vary with what happens to be playing: tag dn
void OhplA12dDrawLog(const cVector3f& avPos, const wchar_t *asText)
{
	if(gbCapture == false) return;
	std::string s = cString::To8Char(tWString(asText));
	const char *sTag = "d";
	if(s.compare(0, 10, "FrameTime:") == 0) s = "FrameTime: *";
	else if(s.compare(0, 13, "ExtLightLevel") == 0) s = "ExtLightLevel: *";
	else if(s.compare(0, 6, "Music:") == 0) { size_t p = s.find(" vol:"); if(p != std::string::npos) s.erase(p); sTag = "dn"; }
	else if(s.compare(0, 14, "Num of sounds:") == 0) { s = "Num of sounds: *"; sTag = "dn"; }
	else if(s.find(")(") != std::string::npos && s.size() && s[s.size() - 1] == ')') { s.erase(s.find('(')); sTag = "dn"; }
	size_t p = s.find("AvgSpeed: ");
	if(p != std::string::npos) s.erase(p + 10);
	OhplA12dLog("%s %.9g %.9g %.9g %s", sTag, avPos.x, avPos.y, avPos.z, s.c_str());
}

static std::vector<std::string> gvLog;

static void LogCb(eLogOutputType aType, const char *asMessage)
{
	if(gbCapture) gvLog.push_back(asMessage);
	if(aType == eLogOutputType_Normal) return;
	gpBase->mpDebugHandler->AddErrorOrWarningMessage(cString::To16Char(asMessage));
}

// node ids are pointers and the dynamic tree's boxes and shape vary with timing: log what is stable
static void FlushLog()
{
	std::vector<std::string> vObj;
	int lNodes = 0, lObjs = 0, lOther = 0;
	for(size_t i = 0; i < gvLog.size(); ++i)
	{
		const std::string &s = gvLog[i];
		if(s.find("-- Node ") != std::string::npos) ++lNodes;
		else if(s.find("AABB:") != std::string::npos && s.find(" (") != std::string::npos)
		{
			++lObjs;
			size_t b = s.find_first_not_of("\t ");
			vObj.push_back(s.substr(b, s.find(" AABB:") - b));
		}
		else
		{
			++lOther;
			std::string t = s;
			while(t.size() && t[t.size() - 1] == '\n') t.erase(t.size() - 1);
			size_t p = t.find_first_not_of("\t ");
			if(p != std::string::npos && t.compare(p, 2, "--") != 0 && t.compare(p, 7, "Objects") != 0) OhplA12dLog("l %s", t.c_str());
		}
	}
	std::sort(vObj.begin(), vObj.end());
	OhplA12dLog("dn l lines=%d nodes=%d other=%d", (int)gvLog.size(), lNodes, lOther);
	OhplA12dLog("l objs=%d", lObjs);
	for(size_t i = 0; i < vObj.size(); ++i) OhplA12dLog("l o %s", vObj[i].c_str());
	gvLog.clear();
}

//-----------------------------------------------------------------------

static void WalkWidget(iWidget *apW, int alDepth)
{
	const char *vType[] = { "root", "window", "button", "frame", "label", "slider", "textbox", "checkbox", "image", "listbox", "combobox",
		"contextmenu", "mainmenu", "menuitem", "tab", "tablabel", "tabframe", "group", "user" };
	int lType = (int)apW->GetType();
	cVector3f vP = apW->GetLocalPosition();
	cVector2f vS = apW->GetSize();
	cVector2f vF = apW->GetDefaultFontSize();
	char sExtra[512] = "";
	if(apW->GetType() == eWidgetType_CheckBox) snprintf(sExtra, sizeof(sExtra), " checked=%d", ((cWidgetCheckBox*)apW)->IsChecked() ? 1 : 0);
	if(apW->GetType() == eWidgetType_Slider) snprintf(sExtra, sizeof(sExtra), " value=%d", ((cWidgetSlider*)apW)->GetValue());
	if(apW->GetType() == eWidgetType_ComboBox)
	{
		cWidgetComboBox *pC = (cWidgetComboBox*)apW;
		std::string s;
		for(int i = 0; i < pC->GetItemNum(); ++i) s += (i ? "|" : "") + cString::To8Char(pC->GetItem(i)->GetText());
		snprintf(sExtra, sizeof(sExtra), " sel=%d items=%d [%s]", pC->GetSelectedItem(), pC->GetItemNum(), s.c_str());
	}
	OhplA12dLog("w %*s%s '%s' pos=%.9g,%.9g,%.9g size=%.9g,%.9g font=%.9g,%.9g en=%d vis=%d uv=%d%s", alDepth * 2, "", lType >= 0 && lType < 19 ? vType[lType] : "?",
		cString::To8Char(apW->GetText()).c_str(), vP.x, vP.y, vP.z, vS.x, vS.y, vF.x, vF.y, apW->IsEnabled() ? 1 : 0, apW->IsVisible() ? 1 : 0, apW->GetUserValue(), sExtra);
	tWidgetList l = apW->GetChildren();
	for(tWidgetListIt it = l.begin(); it != l.end(); ++it) WalkWidget(*it, alDepth + 1);
}

static void Collect(iWidget *apW, int alType, std::vector<iWidget*> &avOut)
{
	if((int)apW->GetType() == alType) avOut.push_back(apW);
	tWidgetList l = apW->GetChildren();
	for(tWidgetListIt it = l.begin(); it != l.end(); ++it) Collect(*it, alType, avOut);
}

static void Tree(const char *asTag)
{
	OhplA12dLog("t %s", asTag);
	cLuxDebugHandler *pH = gpBase->mpDebugHandler;
	if(pH->DumpDebugRoot()) WalkWidget(pH->DumpDebugRoot(), 0);
	if(pH->DumpScriptRoot()) WalkWidget(pH->DumpScriptRoot(), 0);
}

static void State(const char *asTag)
{
	cLuxDebugHandler *pH = gpBase->mpDebugHandler;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	OhplA12dLog("s %s window=%d fps=%d pinfo=%d einfo=%d sdbg=%d noflash=%d quicksave=%d ff=%d inspect='%s'", asTag, pH->GetDebugWindowActive() ? 1 : 0,
		pH->GetShowFPS() ? 1 : 0, pH->GetShowPlayerInfo() ? 1 : 0, pH->GetShowEntityInfo() ? 1 : 0, pH->GetScriptDebugOn() ? 1 : 0,
		pH->GetDisableFlashBacks() ? 1 : 0, pH->GetAllowQuickSave() ? 1 : 0, pH->GetFastForward() ? 1 : 0, pH->DumpInspectName().c_str());
	OhplA12dLog("s %s occl=%d reslog=%d fastphys=%d", asTag, gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbUseOcclusionCulling ? 1 : 0,
		iResourceBase::GetLogCreateAndDelete() ? 1 : 0, gpBase->mpConfigHandler->mbFastPhysicsLoad ? 1 : 0);
	if(pMap == NULL) { OhplA12dLog("s %s nomap", asTag); return; }
	cCamera *pCam = gpBase->mpPlayer->GetCamera();
	cVector3f vPos = gpBase->mpPlayer->GetCharacterBody()->GetPosition();
	int lEnts = 0;
	cLuxEntityIterator it = pMap->GetEntityIterator();
	while(it.HasNext()) { it.Next(); ++lEnts; }
	OhplA12dLog("s %s map=%s pos=%.9g,%.9g,%.9g pitch=%.9g yaw=%.9g ents=%d starts=%d", asTag, pMap->GetFileName().c_str(), vPos.x, vPos.y, vPos.z,
		pCam->GetPitch(), pCam->GetYaw(), lEnts, pMap->GetPlayerStartNodeNum());
}

static cGuiMessageData Msg(int alVal = 0)
{
	cGuiMessageData d;
	d.mlVal = alVal;
	return d;
}

static void Press(const wchar_t *asText)
{
	std::vector<iWidget*> v;
	Collect(gpBase->mpDebugHandler->DumpDebugRoot(), eWidgetType_Button, v);
	if(gpBase->mpDebugHandler->DumpScriptRoot()) Collect(gpBase->mpDebugHandler->DumpScriptRoot(), eWidgetType_Button, v);
	for(size_t i = 0; i < v.size(); ++i)
	{
		if(v[i]->GetText() == asText)
		{
			OhplA12dLog("b press '%s'", cString::To8Char(v[i]->GetText()).c_str());
			v[i]->ProcessMessage(eGuiMessage_ButtonPressed, Msg(), true, true);
			return;
		}
	}
	OhplA12dLog("b missing");
}

static void ToggleAll()
{
	std::vector<iWidget*> v;
	Collect(gpBase->mpDebugHandler->DumpDebugRoot(), eWidgetType_CheckBox, v);
	for(size_t i = 0; i < v.size(); ++i)
	{
		cWidgetCheckBox *pC = (cWidgetCheckBox*)v[i];
		pC->SetChecked(!pC->IsChecked(), true);
		OhplA12dLog("c toggle '%s' uv=%d -> %d", cString::To8Char(pC->GetText()).c_str(), pC->GetUserValue(), pC->IsChecked() ? 1 : 0);
		State("tog");
	}
}

static void Slider(int alVal)
{
	std::vector<iWidget*> v;
	Collect(gpBase->mpDebugHandler->DumpDebugRoot(), eWidgetType_Slider, v);
	for(size_t i = 0; i < v.size(); ++i)
	{
		((cWidgetSlider*)v[i])->SetValue(alVal, true);
		OhplA12dLog("c slider uv=%d -> %d", v[i]->GetUserValue(), ((cWidgetSlider*)v[i])->GetValue());
		State("sld");
	}
}

static void Combo(int alUV, int alSel)
{
	std::vector<iWidget*> v;
	Collect(gpBase->mpDebugHandler->DumpDebugRoot(), eWidgetType_ComboBox, v);
	for(size_t i = 0; i < v.size(); ++i)
	{
		cWidgetComboBox *pC = (cWidgetComboBox*)v[i];
		if(alSel < pC->GetItemNum()) pC->SetSelectedItem(alSel, false, false);
		OhplA12dLog("c combo %d sel=%d", (int)i, pC->GetSelectedItem());
	}
}

static void Draw(const char *asTag)
{
	OhplA12dLog("d begin %s", asTag);
	gbCapture = true;
	gpBase->mpDebugHandler->OnDraw(0.016f);
	gbCapture = false;
	OhplA12dLog("d end %s", asTag);
}

static void Capture(bool abOn) { gbCapture = abOn; }

//-----------------------------------------------------------------------

static int glFrame = 0;
static int glStep = 0;
static int glWait = 0;
static bool gbRunning = false;

static tWString gsBatch;

void OhplA12dTick()
{
	if(OhplA12dEnabled() == false || gbRunning) return;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glFrame = 0; return; }
	if(glStep >= 100) return;
	if(++glFrame < 10) return;
	if(glWait > 0) { --glWait; return; }
	gbRunning = true;
	glWait = 3;
	cLuxDebugHandler *pH = gpBase->mpDebugHandler;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	int lStep = glStep++;
	switch(lStep)
	{
	case 0:
		SetLogMessageCallback(LogCb);
		OhplA12dLog("map %s", pMap->GetName().c_str());
		Tree("init");
		State("init");
		break;
	case 1:
		pH->SetDebugWindowActive(true);
		State("open");
		pH->SetDebugWindowActive(false);
		State("close");
		pH->SetDebugWindowActive(true);
		State("open2");
		pH->SetDebugWindowActive(false);
		break;
	case 2:
		pH->AddMessage(_W("dbg message one"), false);
		pH->AddMessage(_W("dbg message one"), true);
		pH->AddMessage(_W("dbg message %d 100%"), false);
		pH->AddErrorOrWarningMessage(_W("err one"));
		pH->AddErrorOrWarningMessage(_W("err one"));
		Draw("msgs-off");
		break;
	case 3:
		ToggleAll();
		Slider(37);
		Tree("toggled");
		break;
	case 4:
		pH->AddMessage(_W("dbg message one"), false);
		pH->AddMessage(_W("dbg message one"), true);
		pH->AddMessage(_W("dbg message %d 100%"), false);
		pH->AddErrorOrWarningMessage(_W("err one"));
		pH->AddErrorOrWarningMessage(_W("err one"));
		Draw("all-on-a");
		break;
	case 5:
		State("inspect");
		Draw("all-on-b");
		break;
	case 6:
		Combo(0, 0);
		Capture(true);
		Press(_W("Print Container Debug Info"));
		Press(_W("Rebuild Dyn Container"));
		Capture(false);
		FlushLog();
		Press(_W("Teleport to start pos"));
		State("teleport");
		Press(_W("Reload Insanity Effects"));
		Tree("insanity");
		break;
	case 7:
		Press(_W("Recompile Script and Lang"));
		Tree("recompiled");
		Press(_W("Close Window"));
		Tree("closed");
		break;
	case 8:
		pH->AddMessage(_W("late"), false);
		Draw("late");
		ToggleAll();
		Draw("all-off");
		break;
	case 9:
		pH->SetFastForward(true);
		State("ff");
		pH->SetFastForward(false);
		State("ffoff");
		pH->SetFastForward(false);
		break;
	case 10:
		Press(_W("Start Insanity Effect"));
		State("insanity-start");
		break;
	case 11:
		Press(_W("Reload Map"));
		State("reloaded");
		break;
	case 12:
		Press(_W("Quick Map Reload (F2)"));
		State("quick");
		break;
	case 13:
		Press(_W("Test Change Map Save"));
		State("changesave");
		break;
	case 14:
		pH->DumpPick(_W("/some/dir/") + cString::To16Char(pMap->GetFileName()), false);
		State("pick");
		break;
	case 15:
	{
		const char *sPath = getenv("OHPL_A12D_DUMP");
		std::string sFile = std::string(sPath) + ".batch";
		FILE *f = fopen(sFile.c_str(), "w");
		if(f) { fprintf(f, "%s\n", pMap->GetFileName().c_str()); fclose(f); }
		pH->DumpPick(cString::To16Char(sFile), true);
		State("batch");
		glWait = 60;
		break;
	}
	case 16:
		State("batch-done");
		OhplA12dLog("done");
		break;
	}
	gbRunning = false;
}
