// H18 differential harness: drives a real cGuiSet (dummy widgets, null skin) with a seeded script of
// input and focus operations and prints every observable. Run with OHPL_IMPL=cpp and =rust and diff
// stdout. usage: H18_SEED=<n> h18_set
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "impl/RustBackends.h"
#include <vector>

#define private public
#define protected public
#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/Widget.h"
#include "gui/WidgetDummy.h"
#include "graphics/Graphics.h"
#undef private
#undef protected
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

static unsigned long long gRng, gCb;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static cGuiSet *gSet;
static std::vector<iWidget*> gW;
static int gN;

static unsigned Bits(float f)
{
	unsigned u;
	memcpy(&u, &f, 4);
	return u;
}

static int Idx(iWidget *w)
{
	for(size_t i = 0; i < gW.size(); ++i)
		if(gW[i] == w) return (int)i;
	return -1;
}

static const int kProb[24] = { 30, 20, 20, 20, 40, 20, 50, 40, 30, 0, 0, 0, 0, 0, 25, 25, 25, 30, 30, 30, 30, 30, 40, 40 };

static bool Callback(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	int msg = (int)(intptr_t)apObj;
	gCb = gCb * 6364136223846793005ull + 1442695040888963407ull;
	unsigned r = (unsigned)((gCb >> 33) % 100);
	bool ret = (int)r < kProb[msg];
	printf("ev w%d m%d", Idx(apWidget), msg);
	if(msg != eGuiMessage_GetUINavFocus && msg != eGuiMessage_LoseUINavFocus)
		printf(" pos=%08x,%08x", Bits(aData.mvPos.x), Bits(aData.mvPos.y));
	bool val = msg <= eGuiMessage_MouseLeave || msg == eGuiMessage_GamepadInput || (msg >= eGuiMessage_UIArrowPress && msg <= eGuiMessage_UIButtonDoublePress);
	if(val) printf(" val=%d", aData.mlVal);
	if(msg == eGuiMessage_KeyPress || msg == eGuiMessage_KeyRelease) printf(" key=%d,%d", (int)aData.mKeyPress.mKey, aData.mKeyPress.mlModifier);
	printf(" ret=%d\n", ret ? 1 : 0);
	return ret;
}

static int Ref(iWidget *w)
{
	return w ? Idx(w) : -1;
}

static void State(int ret)
{
	printf("= ret=%d foc=%d att=%d def=%d tab=%d tip=%d tmr=%08x mouse=%08x,%08x cur=%d", ret, Ref(gSet->mS.focused),
		   Ref(gSet->mS.attention), Ref(gSet->mS.defaultNav), Ref(gSet->mS.tabWidget), Ref(gSet->mS.tooltipWidget),
		   Bits(gSet->mS.tooltipTimer), Bits(gSet->mS.mousePos.x), Bits(gSet->mS.mousePos.y),
		   (int)(((uintptr_t)gSet->mpGfxCurrentPointer) >> 4));
	printf(" over=");
	for(int i = 0; i < gN; ++i) printf("%d", gW[i]->GetMouseIsOver() ? 1 : 0);
	printf(" down=");
	for(int i = 0; i < 9; ++i) printf("%d", gSet->mS.mouseDown[i] ? 1 : 0);
	printf(" ord=");
	for(tWidgetListIt it = gSet->mlstWidgets.begin(); it != gSet->mlstWidgets.end(); ++it) printf("%d,", Idx(*it));
	printf(" gz=");
	for(int i = 0; i < gN; ++i) printf("%08x,", Bits(gW[i]->GetGlobalPosition().z));
	printf("\n");
}

static void Setup(int seed)
{
	gRng = (unsigned long long)seed * 7919 + 17;
	gCb = (unsigned long long)seed * 104729 + 3;
	cRecordingLowLevelGraphics *llg = new cRecordingLowLevelGraphics();
	cGraphics *gfx = new cGraphics(llg, NULL);
	cGui *gui = new cGui();
	gSet = new cGuiSet("h18", gui, NULL, NULL, gfx, NULL, NULL);

	gN = 6 + (int)Rnd(5);
	for(int i = 0; i < gN; ++i)
	{
		iWidget *parent = (i > 0 && Rnd(10) < 3) ? gW[Rnd(i)] : NULL;
		cWidgetDummy *w = gSet->CreateWidgetDummy(0, parent, "w");
		w->SetEncloseChildren(false);
		gW.push_back(w);
	}
	iWidget *stray = gSet->CreateWidgetDummy(0, NULL, "stray");
	gSet->mlstWidgets.remove(stray);
	gW.push_back(stray);

	gSet->mpSkin = new cGuiSkin("h18", gui);
	printf("seed %d\nn %d\n", seed, gN);
	for(int i = 0; i < gN; ++i)
	{
		iWidget *w = gW[i];
		w->SetSize(cVector2f((float)(20 + Rnd(100)), (float)(20 + Rnd(100))));
		w->SetGlobalPosition(cVector3f((float)Rnd(200), (float)Rnd(200), (float)Rnd(10) * 5.0f));
		w->mbVisible = Rnd(10) < 8;
		w->mbEnabled = Rnd(10) < 8;
		w->mbConnectedToChildren = Rnd(10) < 8;
		w->SetToolTipEnabled(Rnd(3) == 0);
		w->SetGlobalKeyPressListener(Rnd(4) == 0);
		w->SetGlobalUIInputListener(Rnd(3) == 0);
		w->mpPointerGfx = Rnd(3) == 0 ? (cGuiGfxElement*)(uintptr_t)(0x1000 + 0x10 * (1 + i)) : NULL;
		for(int d = 0; d < 4; ++d)
			if(Rnd(3) == 0) w->SetFocusNavigation((eUIArrow)(1 << d), gW[Rnd(gN)]);
		for(int m = 0; m < 24; ++m)
			if(kProb[m] > 0) w->AddCallback((eGuiMessage)m, (void*)(intptr_t)m, Callback);
	}
	gSet->mpGfxCurrentPointer = NULL;
	for(int i = 0; i < gN; ++i)
	{
		iWidget *w = gW[i];
		cVector3f p = w->GetGlobalPosition();
		iWidget *par = w->GetParent();
		printf("w %d par=%d vis=%d en=%d conn=%d tip=%d gkl=%d gul=%d ptr=%d nav=", i, par == gSet->mpWidgetRoot ? -1 : Idx(par),
			   w->mbVisible, w->mbEnabled, w->mbConnectedToChildren, w->mbToolTipEnabled, w->mbGlobalKeyPressListener,
			   w->mbGlobalUIInputListener, (int)(((uintptr_t)w->mpPointerGfx) >> 4));
		for(int d = 0; d < 4; ++d) printf("%d,", Ref(w->mvFocusNavWidgets[d]));
		printf(" pos=%08x,%08x,%08x size=%08x,%08x\n", Bits(p.x), Bits(p.y), Bits(p.z), Bits(w->GetSize().x), Bits(w->GetSize().y));
	}
	printf("ord=");
	for(tWidgetListIt it = gSet->mlstWidgets.begin(); it != gSet->mlstWidgets.end(); ++it) printf("%d,", Idx(*it));
	printf("\n");
}

static bool IsLeaf(int i)
{
	return gW[i]->mlstChildren.empty();
}

static std::vector<iWidget*> gTab;

static bool TabIsSafe()
{
	for(size_t i = 0; i < gTab.size(); ++i)
		if(gTab[i]->IsVisible() || gTab[i]->IsEnabled()) return true;
	return false;
}

static void Op(int step)
{
	(void)step;
	cGuiMessageData d;
	memset(&d, 0, sizeof(d));
	int w = (int)Rnd(gN + 2) - 1;
	if(w >= gN + 1) w = gN;
	iWidget *wp = w >= 0 ? gW[w] : NULL;
	int b = (int)Rnd(2) ? (int)Rnd(5) : 0;
	bool chk = Rnd(2) != 0, flag = Rnd(2) != 0;
	int kind = (int)Rnd(100);
	bool ret = false;

	if(kind < 22)
	{
		float x = (float)Rnd(260), y = (float)Rnd(260);
		printf("op move %08x %08x\n", Bits(x), Bits(y));
		d.mvPos = cVector2f(x, y);
		ret = gSet->SendMessage(eGuiMessage_MouseMove, d);
	}
	else if(kind < 30)
	{
		printf("op down %d\n", b);
		d.mlVal = 1 << b;
		ret = gSet->SendMessage(eGuiMessage_MouseDown, d);
	}
	else if(kind < 37)
	{
		printf("op up %d\n", b);
		d.mlVal = 1 << b;
		ret = gSet->SendMessage(eGuiMessage_MouseUp, d);
	}
	else if(kind < 40)
	{
		printf("op dbl %d\n", b);
		d.mlVal = 1 << b;
		ret = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d);
	}
	else if(kind < 50)
	{
		static const int keys[] = { 1, 88, 89, 90, 91, 5 };
		int key = keys[Rnd(6)];
		int mod = Rnd(3) == 0 ? 2 : 0;
		bool press = Rnd(3) != 0;
		if(press && key == 1 && !TabIsSafe() && !gTab.empty())
		{
			printf("op skip\n");
			State(0);
			return;
		}
		printf("op key%s %d %d\n", press ? "press" : "rel", key, mod);
		d.mKeyPress = cKeyPress((eKey)key, 0, mod);
		ret = gSet->SendMessage(press ? eGuiMessage_KeyPress : eGuiMessage_KeyRelease, d);
	}
	else if(kind < 53)
	{
		printf("op pad\n");
		ret = gSet->SendMessage(eGuiMessage_GamepadInput, d);
	}
	else if(kind < 60)
	{
		int dir = 1 << Rnd(4);
		int which = (int)Rnd(5);
		static const char *names[] = { "arrowpress", "arrowrel", "btnpress", "btnrel", "btndbl" };
		static const eGuiMessage msgs[] = { eGuiMessage_UIArrowPress, eGuiMessage_UIArrowRelease, eGuiMessage_UIButtonPress,
											eGuiMessage_UIButtonRelease, eGuiMessage_UIButtonDoublePress };
		printf("op %s %d\n", names[which], dir);
		d.mlVal = dir;
		ret = gSet->SendMessage(msgs[which], d);
	}
	else if(kind < 66) { printf("op focus %d %d\n", w, chk); gSet->SetFocusedWidget(wp, chk); }
	else if(kind < 71) { printf("op attn %d %d %d\n", w, flag, chk); gSet->SetAttentionWidget(wp, flag, chk); }
	else if(kind < 73) { printf("op pushf\n"); gSet->PushFocusedWidget(); }
	else if(kind < 75) { printf("op popf\n"); gSet->PopFocusedWidget(); }
	else if(kind < 77) { printf("op pusha\n"); gSet->PushAttentionWidget(); }
	else if(kind < 79) { printf("op popa %d\n", flag); gSet->PopAttentionWidget(flag); }
	else if(kind < 81) { printf("op def %d %d\n", w, chk); gSet->SetDefaultFocusNavWidget(wp, chk); }
	else if(kind < 82) { printf("op pushd\n"); gSet->PushDefaultFocusNavWidget(); }
	else if(kind < 83) { printf("op popd\n"); gSet->PopDefaultFocusNavWidget(); }
	else if(kind < 86) {
		printf("op tabadd %d\n", w);
		if(wp)
		{
			gSet->AddToTabOrder(wp);
			bool have = false;
			for(size_t i = 0; i < gTab.size(); ++i) have |= gTab[i] == wp;
			if(!have) gTab.push_back(wp);
		}
	}
	else if(kind < 87) { printf("op tabclr\n"); gSet->ClearTabOrder(); gTab.clear(); }
	else if(kind < 89)
	{
		if(wp && w < gN && IsLeaf(w))
		{
			printf("op winadd %d\n", w);
			if(OhplBackendRustRequested()) ohpl_h18_add_window(&gSet->mS, (OhplH18W)wp);
			else gSet->mlstWindows.push_back(wp);
		}
		else printf("op skip\n");
	}
	else if(kind < 91)
	{
		printf("op wintop %d\n", w);
		if(wp && w < gN && IsLeaf(w)) gSet->SetWindowOnTop((cWidgetWindow*)wp);
	}
	else if(kind < 92)
	{
		printf("op winrm %d\n", w);
		if(wp) gSet->RemoveWindow((cWidgetWindow*)wp);
	}
	else if(kind < 94) { printf("op vis %d %d\n", w, flag); if(wp && w < gN) wp->SetVisible(flag); }
	else if(kind < 96) { printf("op en %d %d\n", w, flag); if(wp && w < gN) wp->SetEnabled(flag); }
	else if(kind < 97)
	{
		float x = (float)Rnd(200), y = (float)Rnd(200), z = (float)Rnd(10) * 5.0f;
		if(wp && w < gN && IsLeaf(w))
		{
			printf("op pos %d %08x %08x %08x\n", w, Bits(x), Bits(y), Bits(z));
			wp->SetGlobalPosition(cVector3f(x, y, z));
		}
		else printf("op skip\n");
	}
	else if(kind < 98) { printf("op sort\n"); gSet->SetWidgetsNeedSorting(); }
	else if(kind < 99) { printf("op mm %d\n", flag); gSet->SetMouseMovementEnabled(flag); }
	else { printf("op over %d %d\n", w, flag); if(wp && w < gN) wp->SetMouseIsOver(flag); }
	State(ret ? 1 : 0);
}

int hplMain(const tString&)
{
	const char *s = getenv("H18_SEED");
	int seed = s ? atoi(s) : 1;
	Setup(seed);
	State(0);
	for(int i = 0; i < 80; ++i) Op(i);
	fflush(stdout);
	_exit(0);
}
