// H21 differential harness for the five GUI popups (message box, file picker save/load, colour picker, UI
// keyboard) over a skin parsed from real game data: seeded mouse/key/gamepad/UI input, direct callback messages
// and widget pokes through cGuiSet, with user callbacks, destroy callbacks, set focus/attention, per-widget state
// after every step, then the RecordingLowLevelGraphics trace of three Draw/Render frames.
// Run with OHPL_IMPL=cpp and =rust and diff stdout.
// usage: H21_SEED=<n> H21_SKIN=<.skin> H21_FPDIR=<dir from make-fpdir.sh> [H21_KIND=msgbox|color|save|load|kb]
//        [H21_DIGEST=1: trace as line count + FNV-1a] [H21_LOG=<file>: Rust host cassette (OHPL_IMPL=rust)] h21_popups
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
#include "gui/GuiGfxElement.h"
#include "gui/Widget.h"
#include "gui/WidgetWindow.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetTextBox.h"
#include "gui/WidgetComboBox.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/GuiPopUp.h"
#include "gui/GuiPopUpMessageBox.h"
#include "gui/GuiPopUpFilePicker.h"
#include "gui/GuiPopUpColorPicker.h"
#include "gui/GuiPopUpUIKeyboard.h"
#include "gui/RustGuiSkin.h"
#include "gui/RustGuiH21.h"
#include "graphics/Graphics.h"
#include "graphics/FontData.h"
#include "resources/Resources.h"
#undef private
#undef protected
#include "graphics/Bitmap.h"
#include "impl/RustBackends.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

// cGuiRenderObjectCompare orders ties by clip/material/texture address, so draw order depends on heap
// layout. A bump allocator makes address order equal allocation order in both implementations.
#include <new>
#include <sys/mman.h>
static char *gArena, *gArenaPos;
static void *Bump(size_t n)
{
	if(!gArena)
	{
		gArena = (char*)mmap(NULL, 1ull << 33, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
		gArenaPos = gArena;
	}
	n = (n + 15) & ~(size_t)15;
	void *p = gArenaPos;
	gArenaPos += n ? n : 16;
	return p;
}
void *operator new(size_t n) { return Bump(n); }
void *operator new[](size_t n) { return Bump(n); }
void operator delete(void*) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*, size_t) noexcept {}



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

class cStubFont : public iFontData
{
public:
	cStubFont(cGui *apGui, iLowLevelGraphics *apLlg) : iFontData("stub", apLlg)
	{
		mfHeight = 14;
		mvSizeRatio = cVector2f(1, 1);
		mlFirstChar = 32;
		mlLastChar = 126;
		for(int c = 32; c <= 126; ++c)
		{
			cGuiGfxElement *g = apGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_FontNormal, false);
			AddGlyph(new cGlyph(g, cVector2f(0, 0), cVector2f(7, 14), (float)(7 + c % 3)));
		}
	}
	bool CreateFromFontFile(const tWString &, int, unsigned short, unsigned short) { return true; }
	bool CreateFromBitmapFile(const tWString &) { return true; }
};



static cGuiSet *gSet;
static std::vector<iWidget*> gSeen;
static std::vector<std::string> gLast;
static cWidgetTextBox *gTarget;
static cWidgetButton *gHostBtn;

static int Idx(iWidget *w)
{
	if(!w) return -1;
	for(size_t i = 0; i < gSeen.size(); ++i)
		if(gSeen[i] == w) return (int)i;
	gSeen.push_back(w);
	return (int)gSeen.size() - 1;
}

static std::string Str(const tWString &s)
{
	std::string o;
	for(size_t i = 0; i < s.size(); ++i)
	{
		wchar_t c = s[i];
		if(c >= 32 && c < 127 && c != '"') o += (char)c;
		else
		{
			char b[16];
			snprintf(b, sizeof(b), "\\u%x", (unsigned)c);
			o += b;
		}
	}
	return o;
}

static void Emit(int i, const std::string &line)
{
	if((int)gLast.size() <= i) gLast.resize(i + 1);
	if(gLast[i] == line) return;
	gLast[i] = line;
	printf("%s\n", line.c_str());
}

static tWString gSaveName;
static tWStringVec gLoadList;
static cColor gDestColor(0.25f, 0.5f, 0.75f, 0.5f);
static int gPopups;

static void Dests()
{
	char buf[1024];
	std::string load;
	for(size_t i = 0; i < gLoadList.size(); ++i) load += Str(gLoadList[i]) + ";";
	snprintf(buf, sizeof(buf), "dest save=\"%s\" load=\"%s\" color=%08x,%08x,%08x,%08x", Str(gSaveName).c_str(), load.c_str(),
			 Bits(gDestColor.r), Bits(gDestColor.g), Bits(gDestColor.b), Bits(gDestColor.a));
	Emit(1, buf);
}

static void State()
{
	char buf[2048];
	snprintf(buf, sizeof(buf), "set foc=%d att=%d def=%d z=%08x pops=%d pend=%d mouse=%08x,%08x", Idx(gSet->mS.focused), Idx(gSet->mS.attention),
			 Idx(gSet->mS.defaultNav), Bits(gSet->mfLastPopUpZ), gSet->mlPopupCount, (int)gSet->mlstPopUps.size(),
			 Bits(gSet->mS.mousePos.x), Bits(gSet->mS.mousePos.y));
	Emit(0, buf);
	Dests();
	std::vector<iWidget*> live(gSet->mlstWidgets.begin(), gSet->mlstWidgets.end());
	for(size_t k = 0; k < live.size(); ++k) Idx(live[k]);
	for(size_t i = 0; i < gSeen.size(); ++i)
	{
		iWidget *w = gSeen[i];
		bool alive = false;
		for(size_t k = 0; k < live.size(); ++k) alive = alive || live[k] == w;
		if(!alive)
		{
			snprintf(buf, sizeof(buf), "w%zu gone", i);
			Emit((int)i + 2, buf);
			continue;
		}
		cVector3f g = w->GetGlobalPosition();
		int n = snprintf(buf, sizeof(buf), "w%zu t%d vis=%d en=%d over=%d pos=%08x,%08x,%08x sz=%08x,%08x nav=%d,%d,%d,%d text=\"%s\"", (size_t)i, (int)w->GetType(),
						 w->IsVisible(), w->IsEnabled(), w->GetMouseIsOver(), Bits(g.x), Bits(g.y), Bits(g.z), Bits(w->GetSize().x), Bits(w->GetSize().y),
						 Idx(w->GetFocusNavigation(eUIArrow_Up)), Idx(w->GetFocusNavigation(eUIArrow_Right)), Idx(w->GetFocusNavigation(eUIArrow_Down)),
						 Idx(w->GetFocusNavigation(eUIArrow_Left)), Str(w->GetText()).c_str());
		if(cWidgetSlider *sl = dynamic_cast<cWidgetSlider*>(w))
			snprintf(buf + n, sizeof(buf) - n, " value=%d max=%d", sl->GetValue(), sl->GetMaxValue());
		else if(cWidgetTextBox *tb = dynamic_cast<cWidgetTextBox*>(w))
			snprintf(buf + n, sizeof(buf) - n, " num=%08x kb=%d", Bits(tb->GetNumericValue()), (int)tb->mbUIKeyboardOpen);
		else if(cWidgetComboBox *cb = dynamic_cast<cWidgetComboBox*>(w))
			snprintf(buf + n, sizeof(buf) - n, " items=%d sel=%d", cb->GetItemNum(), cb->GetSelectedItem());
		else if(cWidgetMultiPropertyListBox *ml = dynamic_cast<cWidgetMultiPropertyListBox*>(w))
			snprintf(buf + n, sizeof(buf) - n, " items=%d sel=%d", ml->GetItemNum(), ml->GetSelectedItem());
		else if(cWidgetWindow *x = dynamic_cast<cWidgetWindow*>(w))
			snprintf(buf + n, sizeof(buf) - n, " focused=%d esc=%d", x->mbFocused, x->mbCloseOnEscapeKey);
		Emit((int)i + 2, buf);
		if(cWidgetComboBox *cb = dynamic_cast<cWidgetComboBox*>(w))
		{
			std::string items;
			for(int k = 0; k < cb->GetItemNum(); ++k) items += Str(cb->GetItemText(k)) + "|";
			snprintf(buf, sizeof(buf), "w%zu items %s", i, items.c_str());
			Emit((int)(gSeen.size() + 2 + i), buf);
		}
		else if(cWidgetMultiPropertyListBox *ml = dynamic_cast<cWidgetMultiPropertyListBox*>(w))
		{
			std::string items;
			for(int k = 0; k < ml->GetItemNum(); ++k)
			{
				cWidgetItem *it = (cWidgetItem*)ml->GetItem(k);
				// sizes and dates of anything outside the fixed tree (system root, cwd log) vary between runs
				static const char *fixed[] = { "a.txt", "b.txt", "c.lua", "notes.md", "saved.txt", "big.txt", "sub", "other", "personal", "deeper", "x.txt", "y.lua", ".hidden.txt" };
				bool stable = false;
				for(size_t f = 0; it && f < sizeof(fixed) / sizeof(fixed[0]); ++f)
					stable = stable || (it->GetProperty(1) && Str(it->GetProperty(1)->GetText()) == fixed[f]);
				for(int p = 0; it && p < it->GetPropertyNum(); ++p)
					if(p < 2 || stable) items += it->GetProperty(p) ? Str(it->GetProperty(p)->GetText()) + "," : std::string("-,");
				items += "|";
			}
			snprintf(buf, sizeof(buf), "w%zu rows %s", i, items.c_str());
			Emit((int)(gSeen.size() + 2 + i), buf);
		}
	}
}

static unsigned short Le16(const unsigned char *p) { return (unsigned short)(p[0] | (p[1] << 8)); }

static cGuiSkin *LoadSkin(cGui *gui, cGraphics *gfx, cRecordingLowLevelGraphics *llg, const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if(!f) { fprintf(stderr, "no skin %s\n", path.c_str()); _exit(2); }
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<char> data(len);
	if(fread(data.data(), len, 1, f) != 1) _exit(2);
	fclose(f);
	std::string dir = path.substr(0, path.rfind('/') + 1);
	int status = 0;
	void *parsed = ohpl_gui_skin_parse(data.data(), data.size(), true, &status);
	if(!parsed) { fprintf(stderr, "skin parse failed\n"); _exit(2); }
	const OhplSkinView *view = ohpl_gui_skin_view(parsed);
	cGuiSkin *skin = new cGuiSkin("h19", gui);
	for(size_t i = 0; i < view->attributeCount; ++i)
	{
		eGuiSkinAttribute t = gui->GetSkinAttributeFromString(view->attributes[i].type);
		if(t != eGuiSkinAttribute_LastEnum)
			skin->mvAttributes[t] = cVector3f(view->attributes[i].value[0], view->attributes[i].value[1], view->attributes[i].value[2]);
	}
	cStubFont *font = new cStubFont(gui, llg);
	for(size_t i = 0; i < view->fontCount; ++i)
	{
		eGuiSkinFont t = gui->GetSkinFontFromString(view->fonts[i].type);
		if(t == eGuiSkinFont_LastEnum) continue;
		cGuiSkinFont *sf = new cGuiSkinFont(gui);
		sf->mpFont = font;
		sf->mvSize = cVector2f(view->fonts[i].size[0], view->fonts[i].size[1]);
		sf->mColor = cColor(view->fonts[i].color[0], view->fonts[i].color[1], view->fonts[i].color[2], view->fonts[i].color[3]);
		skin->mvFonts[t] = sf;
	}
	for(int i = 0; i < eGuiSkinFont_LastEnum; ++i)
		if(!skin->mvFonts[i])
		{
			cGuiSkinFont *sf = new cGuiSkinFont(gui);
			sf->mpFont = font;
			sf->mvSize = cVector2f(14, 14);
			sf->mColor = cColor(1, 1);
			skin->mvFonts[i] = sf;
		}
	int loaded = 0, filled = 0;
	for(size_t i = 0; i < view->gfxCount; ++i)
	{
		const OhplSkinGfx &g = view->gfx[i];
		eGuiSkinGfx t = gui->GetSkinGfxFromString(g.type);
		if(t == eGuiSkinGfx_LastEnum) continue;
		cColor color(g.color[0], g.color[1], g.color[2], g.color[3]);
		cGuiGfxElement *e = NULL;
		if(g.file && g.file[0])
		{
			FILE *tf = fopen((dir + g.file).c_str(), "rb");
			unsigned char h[18];
			if(tf && fread(h, 18, 1, tf) == 1)
			{
				int w = Le16(h + 12), hh = Le16(h + 14);
				iTexture *tex = llg->CreateTexture(g.file, eTextureType_2D, eTextureUsage_Normal);
				cBitmap *bmp = new cBitmap();
				bmp->CreateData(cVector3l(w, hh, 1), ePixelFormat_RGBA, 0, 0);
				cBitmapData *d = bmp->GetData(0, 0);
				for(int k = 0; k < d->mlSize; ++k) d->mpData[k] = (unsigned char)(k * 7 + (int)i);
				tex->CreateFromBitmap(bmp);
				e = gui->CreateGfxTexture(tex, false, (eGuiMaterial)g.material, color);
				++loaded;
			}
			if(tf) fclose(tf);
		}
		if(!e)
		{
			e = gui->CreateGfxFilledRect(color, (eGuiMaterial)g.material);
			++filled;
		}
		if(g.hasActiveSize) e->SetActiveSize(cVector2f(g.activeSize[0], g.activeSize[1]));
		e->SetOffset(cVector3f(g.offset[0], g.offset[1], 0));
		skin->mvGfxElements[t] = e;
	}
	for(int i = 0; i < eGuiSkinGfx_LastEnum; ++i)
		if(!skin->mvGfxElements[i]) skin->mvGfxElements[i] = gui->CreateGfxFilledRect(cColor(0.5f, 0.5f, 0.5f, 1), eGuiMaterial_Alpha);
	fprintf(stderr, "skin %s: %d images, %d filled\n", path.c_str(), loaded, filled);
	ohpl_gui_skin_free(parsed);
	return skin;
}



static float F(unsigned n) { return (float)Rnd(n); }

static void Frame(int n)
{
	printf("frame %d\n", n);
	gSet->DrawAll(0.016f);
	gSet->Render(NULL);
	gSet->ClearRenderObjects();
}

static const char *gKinds[] = { "msgbox", "color", "save", "load", "kb" };

static bool UserCb(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	bool ret = Dec(60);
	int kind = (int)(intptr_t)apObj - 1;
	printf("cb %s w=%d", gKinds[kind], Idx(apWidget));
	if(kind == 0 || kind == 2 || kind == 3) printf(" val=%d", aData.mlVal);
	printf(" ret=%d\n", ret ? 1 : 0);
	if(Dec(25) && gSeen.size())
	{
		iWidget *to = gSeen[Rnd((unsigned)gSeen.size())];
		bool alive = false;
		for(tWidgetListIt it = gSet->mlstWidgets.begin(); it != gSet->mlstWidgets.end(); ++it) alive = alive || *it == to;
		if(alive)
		{
			int what = (int)Rnd(3);
			printf("cb-act %d %d\n", what, Idx(to));
			if(what == 0) gSet->SetFocusedWidget(to);
			else if(what == 1) gSet->SetAttentionWidget(to);
			else gSet->SetDefaultFocusNavWidget(to);
		}
	}
	return ret;
}

static bool DestroyCb(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	printf("destroycb %d w=%d\n", (int)(intptr_t)apObj - 1, Idx(apWidget));
	return true;
}

static std::string gFpDir;

static void CreatePopup(int kind)
{
	iGuiPopUp *p = NULL;
	printf("create %s\n", gKinds[kind]);
	if(kind == 0)
	{
		static const wchar_t *labels[] = { L"Title", L"A longer message box title that is wide", L"" };
		static const wchar_t *texts[] = { L"Hello", L"Some text with more characters to measure the width of the box", L"x" };
		bool two = Rnd(2) != 0;
		const wchar_t *label = labels[Rnd(3)];
		const wchar_t *text = texts[Rnd(3)];
		bool cb = Rnd(4) != 0;
		p = gSet->CreatePopUpMessageBox(label, text, L"OK", two ? L"Cancel" : L"", cb ? (void*)(intptr_t)1 : NULL, cb ? UserCb : NULL);
	}
	else if(kind == 1)
	{
		bool hasDest = Rnd(4) != 0;
		bool cb = Rnd(4) != 0;
		p = gSet->CreatePopUpColorPicker(hasDest ? &gDestColor : NULL, cVector3f(0), (void*)(intptr_t)2, cb ? UserCb : NULL);
	}
	else if(kind == 2 || kind == 3)
	{
		tWString start = cString::To16Char(gFpDir + (Rnd(3) ? "/" : "/sub/"));
		cGuiPopUpFilePicker *fp;
		bool hidden = Rnd(2) != 0;
		bool cb = Rnd(5) != 0;
		if(kind == 2)
		{
			tWString startName = Rnd(2) ? L"" : L"saved.txt";
			fp = gSet->CreatePopUpSaveFilePicker(gSaveName, L"Text files", L"*.txt", start, hidden, (void*)(intptr_t)3, cb ? UserCb : NULL, startName);
		}
		else
		{
			bool all = Rnd(2) != 0;
			fp = gSet->CreatePopUpLoadFilePicker(gLoadList, all, start, hidden, (void*)(intptr_t)4, cb ? UserCb : NULL);
		}
		if(Rnd(2)) fp->AddCategory(L"Scripts", L"*.lua");
		if(Rnd(3) == 0) fp->AddFilter(0, L"*.md");
		p = fp;
	}
	else p = gSet->CreatePopUpUIKeyboard(gTarget);
	if(Rnd(3) == 0) p->AddOnDestroyCallback((void*)(intptr_t)(kind + 1), DestroyCb);
	if(Rnd(4) == 0) p->SetKillOnEscapeKey(Rnd(2) != 0);
	printf("killonesc %d\n", (int)p->GetKillOnEscapeKey());
	++gPopups;
}

static void PointIn(iWidget *w, float *x, float *y)
{
	cVector3f g = w->GetGlobalPosition();
	cVector2f s = w->GetSize();
	*x = g.x + (Rnd(100) < 85 ? F((unsigned)(s.x + 1)) : F(400));
	*y = g.y + (Rnd(100) < 85 ? F((unsigned)(s.y + 1)) : F(300));
}

static iWidget *Pick()
{
	std::vector<iWidget*> live(gSet->mlstWidgets.begin(), gSet->mlstWidgets.end());
	size_t n = live.size();
	size_t pool = n < 45 ? n : 45;
	if(Rnd(2))
	{
		std::vector<iWidget*> buttons;
		for(size_t i = n - pool; i < n; ++i)
			if(live[i]->GetType() == eWidgetType_Button) buttons.push_back(live[i]);
		if(!buttons.empty()) return buttons[Rnd((unsigned)buttons.size())];
	}
	return live[n - 1 - Rnd((unsigned)pool)];
}

static void Op(int step, int kindMode)
{
	cGuiMessageData d;
	memset(&d, 0, sizeof(d));
	iWidget *w = Pick();
	int wi = Idx(w);
	int kind = (int)Rnd(100);
	bool ret = false;
	static const int btns[] = { 1, 2, 4, 8, 16 };
	if(kind < 16)
	{
		float x, y;
		PointIn(w, &x, &y);
		printf("op move %08x %08x\n", Bits(x), Bits(y));
		d.mvPos = cVector2f(x, y);
		ret = gSet->SendMessage(eGuiMessage_MouseMove, d);
	}
	else if(kind < 24)
	{
		int b = btns[Rnd(10) < 8 ? 0 : Rnd(5)];
		printf("op down %d\n", b);
		d.mlVal = b;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseDown, d);
	}
	else if(kind < 32)
	{
		int b = btns[Rnd(10) < 8 ? 0 : Rnd(5)];
		printf("op up %d\n", b);
		d.mlVal = b;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseUp, d);
	}
	else if(kind < 46)
	{
		float x, y;
		cVector3f g = w->GetGlobalPosition();
		cVector2f sz = w->GetSize();
		x = g.x + sz.x * 0.5f;
		y = g.y + sz.y * 0.5f;
		printf("op click w%d %08x %08x\n", wi, Bits(x), Bits(y));
		d.mvPos = cVector2f(x, y);
		gSet->SendMessage(eGuiMessage_MouseMove, d);
		d.mlVal = 1;
		gSet->SendMessage(eGuiMessage_MouseDown, d);
		ret = gSet->SendMessage(eGuiMessage_MouseUp, d);
		if(Rnd(4) == 0) ret = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d);
	}
	else if(kind < 50)
	{
		printf("op dbl\n");
		d.mlVal = 1;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d);
	}
	else if(kind < 61)
	{
		static const int keys[] = { eKey_Escape, eKey_Return, 1, 88, 89, 90, 91, eKey_BackSpace, eKey_0 + 1, eKey_0 + 7, eKey_A + 2, eKey_Space };
		int key = keys[Rnd(12)], mod = Rnd(5) == 0 ? 2 : 0;
		int uni = key >= eKey_0 && key < eKey_0 + 10 ? '0' + key - eKey_0 : key >= eKey_A && key < eKey_A + 26 ? 'a' + key - eKey_A : key == eKey_Space ? ' ' : 0;
		bool press = Rnd(4) != 0;
		printf("op key%s %d %d %d\n", press ? "press" : "rel", key, uni, mod);
		d.mKeyPress = cKeyPress((eKey)key, uni, mod);
		ret = gSet->SendMessage(press ? eGuiMessage_KeyPress : eGuiMessage_KeyRelease, d);
	}
	else if(kind < 71)
	{
		static const int dirs[] = { 1, 2, 4, 8 };
		static const int bvals[] = { 1, 2, 4, 32, 64, 36, 5, 8 };
		static const char *names[] = { "arrowpress", "arrowrel", "btnpress", "btnrel" };
		static const eGuiMessage msgs[] = { eGuiMessage_UIArrowPress, eGuiMessage_UIArrowRelease, eGuiMessage_UIButtonPress, eGuiMessage_UIButtonRelease };
		int which = (int)Rnd(4);
		int val = which < 2 ? dirs[Rnd(4)] : bvals[Rnd(8)];
		printf("op %s %d\n", names[which], val);
		d.mlVal = val;
		ret = gSet->SendMessage(msgs[which], d);
	}
	else if(kind < 79)
	{
		float dt = 0.016f * (float)(1 + Rnd(30));
		printf("op update %08x\n", Bits(dt));
		gSet->Update(dt);
	}
	else if(kind < 86)
	{
		static const int msgs[] = { eGuiMessage_ButtonPressed, eGuiMessage_SliderMove, eGuiMessage_SelectionChange, eGuiMessage_SelectionDoubleClick,
									eGuiMessage_TextBoxEnter, eGuiMessage_UIButtonPress, eGuiMessage_UIButtonRelease, eGuiMessage_WindowClose };
		int m = msgs[Rnd(8)];
		d.mvPos = cVector2f(F(300), F(300));
		d.mlVal = (int)Rnd(70);
		printf("op msg w%d %d %08x %08x %d\n", wi, m, Bits(d.mvPos.x), Bits(d.mvPos.y), d.mlVal);
		ret = w->ProcessMessage((eGuiMessage)m, d, false, false);
	}
	else if(kind < 96)
	{
		int v = (int)Rnd(1200);
		bool trig = Rnd(2) != 0;
		if(cWidgetSlider *sl = dynamic_cast<cWidgetSlider*>(w)) { printf("op slider w%d %d %d\n", wi, v, trig); sl->SetValue(v, trig); }
		else if(cWidgetTextBox *tb = dynamic_cast<cWidgetTextBox*>(w))
		{
			static const wchar_t *texts[] = { L"0.5", L"2", L"-1", L"0.123", L"", L"abc", L"saved.txt", L"a.txt", L"missing.txt", L"b.lua" };
			int t = (int)Rnd(10);
			printf("op text w%d %d\n", wi, t);
			tb->SetText(texts[t]);
		}
		else if(cWidgetComboBox *cb = dynamic_cast<cWidgetComboBox*>(w))
		{
			int s = cb->GetItemNum() ? (int)Rnd((unsigned)cb->GetItemNum()) : 0;
			printf("op select w%d %d\n", wi, s);
			cb->SetSelectedItem(s);
		}
		else if(cWidgetMultiPropertyListBox *ml = dynamic_cast<cWidgetMultiPropertyListBox*>(w))
		{
			int s = ml->GetItemNum() ? (int)Rnd((unsigned)ml->GetItemNum()) : 0;
			printf("op lselect w%d %d\n", wi, s);
			ml->SetSelectedItem(s);
		}
		else printf("op none w%d\n", wi);
	}
	else
	{
		int k = kindMode >= 0 ? kindMode : (int)Rnd(5);
		if(gPopups < 8) CreatePopup(k);
		else printf("op none\n");
	}
	printf("= ret=%d\n", ret ? 1 : 0);
	State();
	(void)step;
}

int hplMain(const tString&)
{
	const char *s = getenv("H21_SEED");
	int seed = s ? atoi(s) : 1;
	const char *skinPath = getenv("H21_SKIN");
	const char *fpDir = getenv("H21_FPDIR");
	gFpDir = fpDir ? fpDir : "/nonexistent";
	if(chdir((gFpDir + "/sub").c_str())) {}  // the engine starts in the binary's directory; relative picker paths need a fixed one
	const char *kindEnv = getenv("H21_KIND");
	int kindMode = -1;
	if(kindEnv)
		for(int i = 0; i < 5; ++i)
			if(strcmp(kindEnv, gKinds[i]) == 0) kindMode = i;
	gRng = (unsigned long long)seed * 7919 + 17;
	gCb = (unsigned long long)seed * 104729 + 3;
	if(getenv("H21_LOG") && OhplBackendRustRequested()) ohpl_h21_log_start();

	cRecordingLowLevelGraphics llg;
	cGraphics gfx(&llg, NULL);
	cGui *gui = new cGui();
	gui->Init(NULL, &gfx, NULL, NULL, NULL);
	cGuiSkin *skin = LoadSkin(gui, &gfx, &llg, skinPath ? skinPath : "");
	cResources *res = new cResources(NULL, NULL);
	gSet = new cGuiSet("h21", gui, skin, res, &gfx, NULL, NULL);
	gSet->mpSkin = skin;
	gSet->SetVirtualSize(cVector2f(800, 600), -1000, 1000, cVector2f(0));
	if(seed & 1) iFileBrowser::msPersonalDir = cString::To16Char(gFpDir + "/personal/");
	if(seed & 2) iFileBrowser::msGameDir = cString::To16Char(gFpDir + "/");
	gSaveName = L"untouched";

	cWidgetWindow *host = gSet->CreateWidgetWindow(eWidgetWindowButtonFlag_ButtonClose, cVector3f(20, 20, 5), cVector2f(300, 120), L"Host", NULL, "host");
	gTarget = gSet->CreateWidgetTextBox(cVector3f(10, 30, 0), cVector2f(150, 20), L"initial", host, eWidgetTextBoxInputType_Normal, 1.0f, true, "target");
	gHostBtn = gSet->CreateWidgetButton(cVector3f(10, 60, 0), cVector2f(80, 24), L"Host button", host, false, "hostbtn");
	gTarget->SetUserData(gHostBtn);
	gSet->SetFocusedWidget(gTarget);
	gSet->SetDefaultFocusNavWidget(gHostBtn);
	Idx(host);
	Idx(gTarget);
	Idx(gHostBtn);

	printf("seed %d skin %s kind %d\n", seed, skinPath ? strrchr(skinPath, '/') + 1 : "", kindMode);
	State();
	CreatePopup(kindMode >= 0 ? kindMode : seed % 5);
	State();
	for(int i = 0; i < 90; ++i)
	{
		if(i == 0 || i == 45 || i == 89) Frame(i);
		Op(i, kindMode);
	}
	const std::string trace = llg.Trace().Text();
	if(getenv("H21_DIGEST"))
	{
		unsigned long long h = 1469598103934665603ull;
		size_t lines = 0;
		for(size_t i = 0; i < trace.size(); ++i)
		{
			h = (h ^ (unsigned char)trace[i]) * 1099511628211ull;
			if(trace[i] == '\n') ++lines;
		}
		printf("trace lines=%zu fnv1a64=%016llx\n", lines, h);
	}
	else fputs(trace.c_str(), stdout);
	fflush(stdout);
	if(getenv("H21_LOG") && OhplBackendRustRequested()) ohpl_h21_log_write(getenv("H21_LOG"));
	_exit(0);
}
