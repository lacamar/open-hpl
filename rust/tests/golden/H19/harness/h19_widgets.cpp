// H19 differential harness for the nine simple widgets (Window, Frame, Button, CheckBox, Label, Slider,
// Image, Group, Dummy) over a skin parsed from real game data: seeded mouse/key/gamepad/UI input through
// cGuiSet::SendMessage, programmatic setters and updates, widget state and callback order after every
// step, then the RecordingLowLevelGraphics trace of three Draw/Render frames.
// Run with OHPL_IMPL=cpp and =rust and diff stdout.
// usage: H19_SEED=<n> H19_SKIN=<path/to/gui_default.skin> [H19_DIGEST=1: print the trace's line count and FNV-1a hash] h19_widgets
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
#include "gui/WidgetCheckBox.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetImage.h"
#include "gui/WidgetGroup.h"
#include "gui/WidgetDummy.h"
#include "gui/RustGuiSkin.h"
#include "graphics/Graphics.h"
#include "graphics/FontData.h"
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
static std::vector<iWidget*> gW;
static std::vector<const char*> gName;
static std::vector<std::string> gLast;

static int Idx(iWidget *w)
{
	for(size_t i = 0; i < gW.size(); ++i)
		if(gW[i] == w) return (int)i;
	return -1;
}

static bool Callback(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	int msg = (int)(intptr_t)apObj;
	bool ret = Dec(25);
	int i = Idx(apWidget);
	printf("cb %s m%d", i >= 0 ? gName[i] : "?", msg);
	if(msg <= 8) printf(" pos=%08x,%08x", Bits(aData.mvPos.x), Bits(aData.mvPos.y));
	if(msg <= 5 || (msg >= 17 && msg <= 21)) printf(" val=%d", aData.mlVal);
	if(msg == 14 || msg == 15) printf(" key=%d,%d", (int)aData.mKeyPress.mKey, aData.mKeyPress.mlModifier);
	if(msg == eGuiMessage_CheckChange || msg == eGuiMessage_SliderMove || msg == eGuiMessage_SliderRelease) printf(" val=%d", aData.mlVal);
	printf(" ret=%d\n", ret ? 1 : 0);
	return ret;
}

static const int kCbMsgs[] = { 0, 1, 2, 3, 4, 5, 6, 8, 11, 12, 13, 14, 17, 18, 19, 20, 21, 24, 25, 31 };

static void Add(iWidget *w, const char *name)
{
	gW.push_back(w);
	gName.push_back(name);
	for(size_t k = 0; k < sizeof(kCbMsgs) / sizeof(kCbMsgs[0]); ++k)
		if(Rnd(3) != 0) w->AddCallback((eGuiMessage)kCbMsgs[k], (void*)(intptr_t)kCbMsgs[k], Callback);
}

static void Emit(int i, const std::string &line)
{
	if((int)gLast.size() <= i) gLast.resize(i + 1);
	if(gLast[i] == line) return;
	gLast[i] = line;
	printf("%s\n", line.c_str());
}

static void State()
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "set foc=%d att=%d mouse=%08x,%08x tip=%d", Idx(gSet->mS.focused), Idx(gSet->mS.attention),
			 Bits(gSet->mS.mousePos.x), Bits(gSet->mS.mousePos.y), Idx(gSet->mS.tooltipWidget));
	Emit(0, buf);
	for(size_t i = 0; i < gW.size(); ++i)
	{
		iWidget *w = gW[i];
		cVector3f g = w->GetGlobalPosition();
		int n = snprintf(buf, sizeof(buf), "w %s vis=%d en=%d over=%d pos=%08x,%08x,%08x sz=%08x,%08x sc=%08x,%08x ch=%08x,%08x", gName[i], w->IsVisible(),
						 w->IsEnabled(), w->GetMouseIsOver(), Bits(g.x), Bits(g.y), Bits(g.z), Bits(w->GetSize().x), Bits(w->GetSize().y),
						 Bits(w->GetScrollAmount().x), Bits(w->GetScrollAmount().y), Bits(w->GetChildrenOffset().x), Bits(w->GetChildrenOffset().y));
		if(cWidgetButton *b = dynamic_cast<cWidgetButton*>(w))
			snprintf(buf + n, sizeof(buf) - n, " pressed=%d timer=%08x times=%d", b->mbPressed, Bits(b->mfTimer), b->mlRepeatTimes);
		else if(cWidgetCheckBox *c = dynamic_cast<cWidgetCheckBox*>(w))
			snprintf(buf + n, sizeof(buf) - n, " checked=%d pressed=%d", c->mbChecked, c->mbPressed);
		else if(cWidgetSlider *sl = dynamic_cast<cWidgetSlider*>(w))
			snprintf(buf + n, sizeof(buf) - n, " value=%d max=%d bar=%d", sl->GetValue(), sl->GetMaxValue(), sl->GetBarValueSize());
		else if(cWidgetLabel *l = dynamic_cast<cWidgetLabel*>(w))
			snprintf(buf + n, sizeof(buf) - n, " timer=%08x off=%08x down=%d rows=%08x", Bits(l->mfScrollTimer), Bits(l->mfWordWrapOffset), l->mbScrollingDown, Bits(l->mfWordWrapRowsHeight));
		else if(cWidgetWindow *x = dynamic_cast<cWidgetWindow*>(w))
			snprintf(buf + n, sizeof(buf) - n, " focused=%d moving=%d rel=%08x,%08x", x->mbFocused, x->mbMoving, Bits(x->mvRelMousePos.x), Bits(x->mvRelMousePos.y));
		else if(cWidgetFrame *f = dynamic_cast<cWidgetFrame*>(w))
			snprintf(buf + n, sizeof(buf) - n, " max=%08x,%08x upd=%d", Bits(f->mvMaxWidgetCoord.x), Bits(f->mvMaxWidgetCoord.y), f->mbScrollBarsNeedUpdate);
		Emit((int)i + 1, buf);
	}
}

static cGuiGfxElement *gSkinGfx[eGuiSkinGfx_LastEnum];

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

static void PointIn(iWidget *w, float *x, float *y)
{
	cVector3f g = w->GetGlobalPosition();
	cVector2f s = w->GetSize();
	*x = g.x + (Rnd(100) < 85 ? F((unsigned)(s.x + 1)) : F(400));
	*y = g.y + (Rnd(100) < 85 ? F((unsigned)(s.y + 1)) : F(300));
	if(w->GetType() == eWidgetType_Window && Rnd(100) < 40) *y = g.y + F(16);
}

static void Frame(int n)
{
	printf("frame %d\n", n);
	gSet->DrawAll(0.016f);
	gSet->Render(NULL);
	gSet->ClearRenderObjects();
}

static void Op(int step)
{
	cGuiMessageData d;
	memset(&d, 0, sizeof(d));
	int wi = (int)Rnd((unsigned)gW.size());
	iWidget *w = gW[wi];
	int kind = (int)Rnd(100);
	bool ret = false;
	static const int btns[] = { 1, 2, 4, 8, 16 };
	if(kind < 25)
	{
		float x, y;
		PointIn(w, &x, &y);
		printf("op move %08x %08x\n", Bits(x), Bits(y));
		d.mvPos = cVector2f(x, y);
		ret = gSet->SendMessage(eGuiMessage_MouseMove, d);
	}
	else if(kind < 40)
	{
		int b = btns[Rnd(10) < 7 ? 0 : Rnd(5)];
		printf("op down %d\n", b);
		d.mlVal = b;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseDown, d);
	}
	else if(kind < 55)
	{
		int b = btns[Rnd(10) < 7 ? 0 : Rnd(5)];
		printf("op up %d\n", b);
		d.mlVal = b;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseUp, d);
	}
	else if(kind < 59)
	{
		printf("op dbl\n");
		d.mlVal = 1;
		d.mvPos = gSet->mS.mousePos;
		ret = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d);
	}
	else if(kind < 67)
	{
		static const int keys[] = { 5, 1, 88, 89, 90, 91, 10, 20, 30, 40 };
		int key = keys[Rnd(10)], mod = Rnd(4) == 0 ? 2 : 0;
		bool press = Rnd(3) != 0;
		printf("op key%s %d %d\n", press ? "press" : "rel", key, mod);
		d.mKeyPress = cKeyPress((eKey)key, 0, mod);
		ret = gSet->SendMessage(press ? eGuiMessage_KeyPress : eGuiMessage_KeyRelease, d);
	}
	else if(kind < 70) { printf("op pad\n"); ret = gSet->SendMessage(eGuiMessage_GamepadInput, d); }
	else if(kind < 76)
	{
		static const int dirs[] = { 1, 2, 4, 8 };
		static const char *names[] = { "arrowpress", "arrowrel", "btnpress", "btnrel", "btndbl" };
		static const eGuiMessage msgs[] = { eGuiMessage_UIArrowPress, eGuiMessage_UIArrowRelease, eGuiMessage_UIButtonPress,
											eGuiMessage_UIButtonRelease, eGuiMessage_UIButtonDoublePress };
		int which = (int)Rnd(5);
		int val = which < 2 ? dirs[Rnd(4)] : (Rnd(3) == 0 ? 2 : 1);
		printf("op %s %d\n", names[which], val);
		d.mlVal = val;
		ret = gSet->SendMessage(msgs[which], d);
	}
	else if(kind < 84)
	{
		float dt = 0.016f * (float)(1 + Rnd(30));
		printf("op update %08x\n", Bits(dt));
		gSet->Update(dt);
	}
	else if(kind < 87) { bool f = Rnd(4) != 0; printf("op vis %s %d\n", gName[wi], f); w->SetVisible(f); }
	else if(kind < 90) { bool f = Rnd(4) != 0; printf("op en %s %d\n", gName[wi], f); w->SetEnabled(f); }
	else if(kind < 92)
	{
		int v = (int)Rnd(25);
		printf("op setvalue %d\n", v);
		for(size_t i = 0; i < gW.size(); ++i)
		{
			if(gW[i]->GetType() == eWidgetType_Slider && Rnd(2)) ((cWidgetSlider*)gW[i])->SetValue(v, Rnd(2) != 0);
			if(gW[i]->GetType() == eWidgetType_CheckBox) ((cWidgetCheckBox*)gW[i])->SetChecked(v & 1, v & 2);
			if(gW[i]->GetType() == eWidgetType_Button) ((cWidgetButton*)gW[i])->SetPressed(v & 1, v & 2);
		}
	}
	else if(kind < 94)
	{
		static const wchar_t *texts[] = { L"", L"Short", L"A much longer label text that must wrap across several rows of the label", L"x y z" };
		int t = (int)Rnd(4);
		printf("op text %s %d\n", gName[wi], t);
		w->SetText(texts[t]);
	}
	else if(kind < 96)
	{
		float x = F(150), y = F(120), z = (float)Rnd(3) * 4;
		printf("op setpos %s %08x %08x %08x\n", gName[wi], Bits(x), Bits(y), Bits(z));
		w->SetPosition(cVector3f(x, y, z));
	}
	else if(kind < 98)
	{
		float x = 30 + F(260), y = 30 + F(200);
		printf("op setsize %s %08x %08x\n", gName[wi], Bits(x), Bits(y));
		w->SetSize(cVector2f(x, y));
	}
	else
	{
		int m = kCbMsgs[Rnd(sizeof(kCbMsgs) / sizeof(kCbMsgs[0]))];
		d.mvPos = cVector2f(F(300), F(300));
		d.mlVal = (int)Rnd(20);
		printf("op msg %s %d %08x %08x %d\n", gName[wi], m, Bits(d.mvPos.x), Bits(d.mvPos.y), d.mlVal);
		ret = w->ProcessMessage((eGuiMessage)m, d, false, false);
	}
	printf("= ret=%d\n", ret ? 1 : 0);
	State();
	(void)step;
}

int hplMain(const tString&)
{
	const char *s = getenv("H19_SEED");
	int seed = s ? atoi(s) : 1;
	const char *skinPath = getenv("H19_SKIN");
	gRng = (unsigned long long)seed * 7919 + 17;
	gCb = (unsigned long long)seed * 104729 + 3;

	cRecordingLowLevelGraphics llg;
	cGraphics gfx(&llg, NULL);
	cGui *gui = new cGui();
	gui->Init(NULL, &gfx, NULL, NULL, NULL);
	cGuiSkin *skin = LoadSkin(gui, &gfx, &llg, skinPath ? skinPath : "");
	gSet = new cGuiSet("h19", gui, skin, NULL, &gfx, NULL, NULL);
	gSet->mpSkin = skin;
	gSet->SetVirtualSize(cVector2f(800, 600), -1000, 1000, cVector2f(0));

	cWidgetWindow *win = gSet->CreateWidgetWindow(eWidgetWindowButtonFlag_ButtonClose, cVector3f(20, 20, 5), cVector2f(340, 280), L"Window", NULL, "win");
	cWidgetFrame *frame = gSet->CreateWidgetFrame(cVector3f(10, 10, 0), cVector2f(300, 220), true, win, true, true, "frame");
	cWidgetButton *btn = gSet->CreateWidgetButton(cVector3f(10, 10, 0), cVector2f(80, 24), L"Button", frame, false, "btn");
	cWidgetButton *tog = gSet->CreateWidgetButton(cVector3f(100, 10, 0), cVector2f(80, 24), L"Toggle", frame, true, "tog");
	cWidgetButton *rep = gSet->CreateWidgetButton(cVector3f(190, 10, 0), cVector2f(80, 24), L"Repeat", frame, false, "rep");
	rep->SetRepeatActive(true);
	rep->SetRepeatFreq(4.0f);
	cWidgetCheckBox *chk = gSet->CreateWidgetCheckBox(cVector3f(10, 45, 0), cVector2f(120, 20), L"Check", frame, "chk");
	cWidgetLabel *lbl = gSet->CreateWidgetLabel(cVector3f(10, 70, 0), cVector2f(140, 40), L"A label with some long wrapped text here", frame, "lbl");
	lbl->SetWordWrap(true);
	cWidgetSlider *sh = gSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, cVector3f(10, 120, 0), cVector2f(150, 16), 20, frame, "sh");
	cWidgetSlider *sv = gSet->CreateWidgetSlider(eWidgetSliderOrientation_Vertical, cVector3f(170, 45, 0), cVector2f(16, 100), 10, frame, "sv");
	cWidgetImage *img = gSet->CreateWidgetImage("", cVector3f(200, 45, 0), cVector2f(60, 40), eGuiMaterial_Alpha, false, frame, "img");
	img->SetImage(skin->GetGfx(eGuiSkinGfx_ButtonUpBackground));
	cWidgetGroup *grp = gSet->CreateWidgetGroup(cVector3f(10, 150, 0), cVector2f(140, 60), L"Group", frame, "grp");
	cWidgetButton *gbtn = gSet->CreateWidgetButton(cVector3f(8, 16, 0), cVector2f(60, 20), L"In group", grp, false, "gbtn");
	cWidgetDummy *dum = gSet->CreateWidgetDummy(cVector3f(220, 100, 0), frame, "dum");
	cWidgetButton *out = gSet->CreateWidgetButton(cVector3f(400, 40, 0), cVector2f(90, 26), L"Outside", NULL, false, "out");
	iWidget *list[] = { win, frame, btn, tog, rep, chk, lbl, sh, sv, img, grp, gbtn, dum, out };
	const char *names[] = { "win", "frame", "btn", "tog", "rep", "chk", "lbl", "sh", "sv", "img", "grp", "gbtn", "dum", "out" };
	for(size_t i = 0; i < sizeof(list) / sizeof(list[0]); ++i) Add(list[i], names[i]);

	printf("seed %d skin %s\n", seed, skinPath ? strrchr(skinPath, '/') + 1 : "");
	State();
	for(int i = 0; i < 110; ++i)
	{
		if(i == 0 || i == 40 || i == 109) Frame(i);
		Op(i);
	}
	const std::string trace = llg.Trace().Text();
	if(getenv("H19_DIGEST"))
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
	_exit(0);
}
