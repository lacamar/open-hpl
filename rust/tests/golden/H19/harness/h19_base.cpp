// H19 differential harness for the iWidget base: probe widgets in a seeded tree, scripted hierarchy,
// geometry, flag, message, callback, focus and draw operations; every observable is printed.
// Run with OHPL_IMPL=cpp and =rust and diff stdout. usage: H19_SEED=<n> h19_base
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#define private public
#define protected public
#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/Widget.h"
#include "graphics/Graphics.h"
#undef private
#undef protected
#include "impl/RustBackends.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

static unsigned long long gRng, gCb;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static bool Dec(unsigned pct)
{
	gCb = gCb * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gCb >> 33) % 100) < pct;
}

static unsigned Bits(float f)
{
	unsigned u;
	memcpy(&u, &f, 4);
	return u;
}

static cGuiSet *gSet;
static std::vector<iWidget*> gW;
static std::vector<int> gText;
static int gN;

static int Idx(iWidget *w)
{
	for(size_t i = 0; i < gW.size(); ++i)
		if(gW[i] == w) return (int)i;
	return -1;
}

static void PrintClip(cGuiClipRegion *c)
{
	if(!c) { printf("null"); return; }
	printf("%08x,%08x,%08x,%08x", Bits(c->mRect.x), Bits(c->mRect.y), Bits(c->mRect.w), Bits(c->mRect.h));
}

static void PrintData(const cGuiMessageData &d)
{
	if(d.mMessage == eGuiMessage_OnDraw || d.mMessage == eGuiMessage_OnUpdate || d.mMessage == eGuiMessage_TextChange)
	{
		printf(" tagged=%d", (int)d.mMessage);
		return;
	}
	printf(" pos=%08x,%08x val=%d key=%d,%d", Bits(d.mvPos.x), Bits(d.mvPos.y), d.mlVal, (int)d.mKeyPress.mKey, d.mKeyPress.mlModifier);
}

static bool gMore[16];

static bool Callback(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	int msg = (int)(intptr_t)apObj;
	bool ret = Dec(30);
	printf("cb w%d m%d", Idx(apWidget), msg);
	if(msg <= 8 || msg == 14 || msg == 15 || (msg >= 17 && msg <= 21)) PrintData(aData);
	if(msg == eGuiMessage_OnUpdate) printf(" dt=%08x", Bits(aData.mfVal));
	printf(" tag=%d ret=%d\n", (int)aData.mMessage, ret ? 1 : 0);
	int i = Idx(apWidget);
	if(msg == eGuiMessage_ButtonPressed && i >= 0 && !gMore[i])
	{
		gMore[i] = true;
		apWidget->AddCallback(eGuiMessage_ButtonPressed, (void*)(intptr_t)(eGuiMessage_ButtonPressed), Callback);
	}
	return ret;
}

class cProbe : public iWidget
{
public:
	cProbe(cGuiSet *apSet) : iWidget(eWidgetType_User, apSet, NULL), mbBasePoint(false) {}

	bool mbBasePoint;

	void L(const char *name) { printf("ev w%d %s", Idx(this), name); }

	bool V(const char *name, const cGuiMessageData &d, unsigned pct)
	{
		L(name);
		PrintData(d);
		bool r = Dec(pct);
		printf(" ret=%d\n", r ? 1 : 0);
		return r;
	}

	bool OnMessage(eGuiMessage m, const cGuiMessageData &d)
	{
		printf("ev w%d onmsg m%d", Idx(this), (int)m);
		PrintData(d);
		bool r = Dec(8);
		printf(" tag=%d ret=%d\n", (int)d.mMessage, r ? 1 : 0);
		return r;
	}
	bool OnMouseMove(const cGuiMessageData &d) { return V("move", d, 35); }
	bool OnMouseDown(const cGuiMessageData &d) { return V("down", d, 35); }
	bool OnMouseUp(const cGuiMessageData &d) { return V("up", d, 35); }
	bool OnMouseDoubleClick(const cGuiMessageData &d) { return V("dbl", d, 35); }
	bool OnMouseEnter(const cGuiMessageData &d) { return V("enter", d, 35); }
	bool OnMouseLeave(const cGuiMessageData &d) { return V("leave", d, 35); }
	bool OnKeyPress(const cGuiMessageData &d) { return V("key", d, 35); }
	bool OnLostFocus(const cGuiMessageData &d) { return V("lost", d, 35); }
	bool OnUIArrowPress(const cGuiMessageData &d) { return V("arrowp", d, 35); }
	bool OnUIArrowRelease(const cGuiMessageData &d) { return V("arrowr", d, 35); }
	bool OnUIButtonPress(const cGuiMessageData &d) { return V("btnp", d, 35); }
	bool OnUIButtonRelease(const cGuiMessageData &d) { return V("btnr", d, 35); }
	bool OnUIButtonDoublePress(const cGuiMessageData &d) { return V("btnd", d, 35); }
	bool OnGetUINavFocus(const cGuiMessageData &d) { return V("navget", d, 35); }
	bool OnLoseUINavFocus(const cGuiMessageData &d) { return V("navlose", d, 35); }

	bool OnGotFocus(const cGuiMessageData &d)
	{
		L("gotfocus");
		PrintData(d);
		bool r = iWidget::OnGotFocus(d);
		printf(" ret=%d\n", r ? 1 : 0);
		return r;
	}
	bool OnGotTabFocus(const cGuiMessageData &d)
	{
		L("gottab");
		PrintData(d);
		bool r = iWidget::OnGotTabFocus(d);
		printf(" ret=%d\n", r ? 1 : 0);
		return r;
	}
	void OnChildGotFocus(iWidget *c, const cGuiMessageData &d)
	{
		L("childfocus"); printf(" c=%d", Idx(c)); PrintData(d); printf("\n");
		iWidget::OnChildGotFocus(c, d);
	}
	void OnChildGotTabFocus(iWidget *c, const cGuiMessageData &d)
	{
		L("childtab"); printf(" c=%d", Idx(c)); PrintData(d); printf("\n");
		iWidget::OnChildGotTabFocus(c, d);
	}
	void OnChildLostFocus(iWidget *c, const cGuiMessageData &d)
	{
		L("childlost"); printf(" c=%d", Idx(c)); PrintData(d); printf("\n");
		iWidget::OnChildLostFocus(c, d);
	}

	void OnChangePosition() { L("chpos"); printf(" n=%d\n", mlPositionCount); }
	void OnChangeSize() { L("chsize"); printf("\n"); }
	void OnChangeText() { L("chtext"); printf(" changed=%d\n", mbTextChanged ? 1 : 0); }
	void OnChildUpdate(iWidget *c) { L("childupd"); printf(" c=%d\n", Idx(c)); }
	void OnAttachChild(iWidget *c) { L("attach"); printf(" c=%d\n", Idx(c)); }
	void OnRemoveChild(iWidget *c) { L("remove"); printf(" c=%d\n", Idx(c)); }
	void OnUpdate(float dt) { L("update"); printf(" dt=%08x\n", Bits(dt)); }
	void OnDraw(float dt, cGuiClipRegion *c)
	{
		L("draw"); printf(" dt=%08x clip=", Bits(dt)); PrintClip(c); printf(" cur="); PrintClip(mpSet->mpCurrentClipRegion); printf("\n");
	}
	void OnDrawAfterClip(float dt, cGuiClipRegion *c)
	{
		L("drawac"); printf(" dt=%08x clip=", Bits(dt)); PrintClip(c); printf(" cur="); PrintClip(mpSet->mpCurrentClipRegion); printf("\n");
	}

	bool PointIsInside(const cVector2f &p, bool onlyClipped)
	{
		L("inside");
		printf(" p=%08x,%08x oc=%d", Bits(p.x), Bits(p.y), onlyClipped ? 1 : 0);
		bool r = mbBasePoint ? iWidget::PointIsInside(p, onlyClipped) : Dec(50);
		printf(" ret=%d\n", r ? 1 : 0);
		return r;
	}
};

static const wchar_t *kTexts[4] = { L"", L"a", L"bb", L"ccc" };

static std::vector<std::string> gLastS, gLastG;

static void Emit(std::vector<std::string> &last, int i, const std::string &line)
{
	if((int)last.size() <= i) last.resize(i + 1);
	if(last[i] == line) return;
	last[i] = line;
	printf("%s\n", line.c_str());
}

static void State()
{
	char buf[1024];
	for(int i = 0; i < gN; ++i)
	{
		iWidget *w = gW[i];
		int n = snprintf(buf, sizeof(buf), "s %d par=%d pos=%08x,%08x,%08x co=%08x,%08x,%08x sc=%08x,%08x,%08x sz=%08x,%08x", i, Idx(w->GetParent()),
			   Bits(w->mvPosition.x), Bits(w->mvPosition.y), Bits(w->mvPosition.z), Bits(w->mvChildrenOffset.x),
			   Bits(w->mvChildrenOffset.y), Bits(w->mvChildrenOffset.z), Bits(w->mvScrollAmount.x), Bits(w->mvScrollAmount.y),
			   Bits(w->mvScrollAmount.z), Bits(w->mvSize.x), Bits(w->mvSize.y));
		n += snprintf(buf + n, sizeof(buf) - n, " vis=%d en=%d aff=%d clip=%d conn=%d cbd=%d over=%d upd=%d cnt=%d txt=%d kids=", w->mbVisible, w->mbEnabled,
			   w->mbAffectedByScroll, w->mbClipsGraphics, w->mbConnectedToChildren, w->mbCallbacksDisabled, w->mbMouseIsOver,
			   w->mbPositionIsUpdated, w->mlPositionCount, gText[i]);
		for(tWidgetListIt it = w->mlstChildren.begin(); it != w->mlstChildren.end(); ++it) n += snprintf(buf + n, sizeof(buf) - n, "%d,", Idx(*it));
		Emit(gLastS, i, buf);
	}
	for(int i = 0; i < gN; ++i)
	{
		cVector3f g = gW[i]->GetGlobalPosition();
		snprintf(buf, sizeof(buf), "g %d %08x,%08x,%08x", i, Bits(g.x), Bits(g.y), Bits(g.z));
		Emit(gLastG, i, buf);
	}
	printf("sort=%d\n", gSet->mbSortWidgets ? 1 : 0);
	gSet->mbSortWidgets = false;
}

static bool IsAncestor(iWidget *a, iWidget *w)
{
	for(; w; w = w->GetParent())
		if(w == a) return true;
	return false;
}

static float F(unsigned n) { return (float)Rnd(n) - (float)(n / 4); }

static void Op()
{
	int a = (int)Rnd(gN), b = (int)Rnd(gN);
	iWidget *wa = gW[a], *wb = gW[b];
	bool flag = Rnd(2) != 0;
	int kind = (int)Rnd(100);
	int ret = -1;
	if(kind < 12)
	{
		if(a != b && !IsAncestor(wb, wa))
		{
			printf("op attach %d %d\n", a, b);
			wa->AttachChild(wb);
		}
		else printf("op skip\n");
	}
	else if(kind < 17)
	{
		printf("op remove %d %d\n", a, b);
		wa->RemoveChild(wb);
	}
	else if(kind < 25)
	{
		float x = F(80), y = F(80), z = (float)Rnd(4) * 5.0f;
		printf("op setpos %d %08x %08x %08x\n", a, Bits(x), Bits(y), Bits(z));
		wa->SetPosition(cVector3f(x, y, z));
	}
	else if(kind < 31)
	{
		float x = F(80), y = F(80), z = (float)Rnd(4) * 5.0f;
		printf("op setgpos %d %08x %08x %08x\n", a, Bits(x), Bits(y), Bits(z));
		wa->SetGlobalPosition(cVector3f(x, y, z));
	}
	else if(kind < 35) { printf("op vis %d %d\n", a, flag); wa->SetVisible(flag); }
	else if(kind < 39) { printf("op en %d %d\n", a, flag); wa->SetEnabled(flag); }
	else if(kind < 43)
	{
		int t = (int)Rnd(4);
		printf("op text %d %d\n", a, t);
		if(t != gText[a]) { gText[a] = t; }
		wa->SetText(kTexts[t]);
	}
	else if(kind < 47)
	{
		float x = (float)Rnd(120), y = (float)Rnd(120);
		printf("op size %d %08x %08x\n", a, Bits(x), Bits(y));
		wa->SetSize(cVector2f(x, y));
	}
	else if(kind < 51)
	{
		float x = F(20), y = F(20), z = (float)Rnd(2);
		if(Rnd(4) == 0) x = y = z = 0;
		printf("op choff %d %08x %08x %08x\n", a, Bits(x), Bits(y), Bits(z));
		wa->SetChildrenOffset(cVector3f(x, y, z));
	}
	else if(kind < 55)
	{
		float x = F(20), y = F(20), z = 0;
		if(Rnd(4) == 0) x = y = 0;
		printf("op scroll %d %08x %08x %08x\n", a, Bits(x), Bits(y), Bits(z));
		wa->SetScrollAmount(cVector3f(x, y, z));
	}
	else if(kind < 57) { printf("op aff %d %d\n", a, flag); wa->SetAffectedByScroll(flag); }
	else if(kind < 60) { printf("op clip %d %d\n", a, flag); wa->SetClipActive(flag); }
	else if(kind < 62) { bool d = Rnd(5) == 0; printf("op cbd %d %d\n", a, d); wa->SetCallbacksDisabled(d); }
	else if(kind < 64) { printf("op conn %d %d\n", a, flag); wa->SetConnectedToChildren(flag); }
	else if(kind < 76)
	{
		static const int msgs[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 12, 13, 9, 10, 24, 31 };
		int m = msgs[Rnd(26)];
		bool sv = Rnd(4) == 0, se = Rnd(4) == 0;
		cGuiMessageData d;
		memset(&d, 0, sizeof(d));
		d.mvPos = cVector2f((float)Rnd(150), (float)Rnd(150));
		d.mlVal = (int)Rnd(20);
		d.mKeyPress = cKeyPress((eKey)Rnd(100), 0, (int)Rnd(3));
		printf("op msg %d %d %d %d %08x %08x %d %d %d\n", a, m, sv, se, Bits(d.mvPos.x), Bits(d.mvPos.y), d.mlVal, (int)d.mKeyPress.mKey, d.mKeyPress.mlModifier);
		ret = wa->ProcessMessage((eGuiMessage)m, d, sv, se);
	}
	else if(kind < 80)
	{
		float dt = 0.016f * (float)(1 + Rnd(4));
		printf("op update %d %08x\n", a, Bits(dt));
		wa->Update(dt);
	}
	else if(kind < 86)
	{
		float dt = 0.016f * (float)(1 + Rnd(4));
		printf("op draw %d %08x\n", a, Bits(dt));
		gSet->mpCurrentClipRegion = &gSet->mBaseClipRegion;
		wa->Draw(dt, &gSet->mBaseClipRegion);
		printf("after cur=");
		PrintClip(gSet->mpCurrentClipRegion);
		printf("\n");
		gSet->mpCurrentClipRegion = &gSet->mBaseClipRegion;
		gSet->mBaseClipRegion.Clear();
	}
	else if(kind < 90)
	{
		float x = (float)Rnd(150), y = (float)Rnd(150);
		bool oc = Rnd(2) != 0;
		printf("op inside %d %08x %08x %d\n", a, Bits(x), Bits(y), oc);
		ret = wa->iWidget::PointIsInside(cVector2f(x, y), oc);
	}
	else if(kind < 92) { printf("op conns %d %d %d\n", a, b, flag); ret = wa->IsConnectedTo(wb, flag); }
	else if(kind < 94)
	{
		int d = 1 << Rnd(4);
		if(Rnd(8) == 0) d = (int)Rnd(14);
		bool nl = Rnd(5) == 0;
		printf("op setnav %d %d %d %d\n", a, d, b, nl ? 1 : 0);
		wa->SetFocusNavigation((eUIArrow)d, nl ? NULL : wb);
	}
	else if(kind < 95)
	{
		int d = 1 << Rnd(4);
		printf("op getnav %d %d\n", a, d);
		ret = Idx(wa->GetFocusNavigation((eUIArrow)d));
		ret = ret * 2 + (wa->HasFocusNavigation() ? 1 : 0);
	}
	else if(kind < 98)
	{
		cGuiMessageData d;
		memset(&d, 0, sizeof(d));
		d.mvPos = cVector2f((float)Rnd(150), (float)Rnd(150));
		int which = (int)Rnd(3);
		printf("op focus %d %d %08x %08x\n", a, which, Bits(d.mvPos.x), Bits(d.mvPos.y));
		if(which == 0) ret = wa->GetFocus(d);
		else if(which == 1) ret = wa->GetTabFocus(d);
		else ret = wa->LoseFocus(d);
	}
	else if(kind < 99) { printf("op center %d\n", a); wa->CenterGlobalPositionInSet(); }
	else
	{
		printf("op chain %d\n", a);
		ret = (wa->IsVisible() ? 1 : 0) | (wa->IsEnabled() ? 2 : 0) | (wa->ClipsGraphics() ? 4 : 0);
	}
	printf("= ret=%d\n", ret);
	State();
}

int hplMain(const tString&)
{
	const char *s = getenv("H19_SEED");
	int seed = s ? atoi(s) : 1;
	gRng = (unsigned long long)seed * 7919 + 17;
	gCb = (unsigned long long)seed * 104729 + 3;
	cRecordingLowLevelGraphics *llg = new cRecordingLowLevelGraphics();
	cGraphics *gfx = new cGraphics(llg, NULL);
	cGui *gui = new cGui();
	gSet = new cGuiSet("h19", gui, NULL, NULL, gfx, NULL, NULL);
	gSet->SetVirtualSize(cVector2f(640, 480), -1000, 1000, cVector2f(0));

	gN = 6 + (int)Rnd(5);
	printf("seed %d\nn %d\n", seed, gN);
	for(int i = 0; i < gN; ++i)
	{
		cProbe *p = new cProbe(gSet);
		p->mbBasePoint = Rnd(3) != 0;
		gW.push_back(p);
		gText.push_back(0);
		printf("w %d base=%d\n", i, p->mbBasePoint ? 1 : 0);
	}
	for(int i = 0; i < gN; ++i)
		for(int m = 0; m < 32; ++m)
			if(m != eGuiMessage_OnDraw && m != eGuiMessage_OnUpdate ? Rnd(4) == 0 : Rnd(2) == 0)
			{
				gW[i]->AddCallback((eGuiMessage)m, (void*)(intptr_t)m, Callback);
				printf("cb %d %d\n", i, m);
			}
	State();
	for(int i = 0; i < 150; ++i) Op();
	fflush(stdout);
	_exit(0);
}
