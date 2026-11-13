// H20 differential harness for TextBox (text and numeric), ComboBox, ListBox, MultiPropertyListBox, TabFrame,
// ContextMenu, MenuItem and MainMenu over a skin parsed from real game data: seeded mouse/key (typing,
// selection, cursor)/gamepad/UI input through cGuiSet::SendMessage, programmatic setters and updates, widget
// state and callback order after every step, then the RecordingLowLevelGraphics trace of three Draw/Render frames.
// Run with OHPL_IMPL=cpp and =rust and diff stdout.
// usage: H20_SEED=<n> H20_SKIN=<path/to/gui_default.skin> [H20_DIGEST=1: print the trace's line count and FNV-1a hash] h20_widgets
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
#include "gui/WidgetTextBox.h"
#include "gui/WidgetComboBox.h"
#include "gui/WidgetListBox.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/WidgetTabFrame.h"
#include "gui/WidgetMenuItem.h"
#include "gui/WidgetContextMenu.h"
#include "gui/WidgetMainMenu.h"
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
static cWidgetContextMenu *gCtx;
static cWidgetComboBox *gCombo;
static cWidgetListBox *gList;
static cWidgetMultiPropertyListBox *gMpl;
static cWidgetTabFrame *gTabs;
static cWidgetTextBox *gTb, *gNtb;
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
		if(cWidgetTextBox *t = dynamic_cast<cWidgetTextBox*>(w))
		{
			unsigned long long h = 1469598103934665603ull;
			for(size_t k = 0; k < t->msText.size(); ++k) h = (h ^ (unsigned)t->msText[k]) * 1099511628211ull;
			const OhplW2Text &x = t->mState;
			snprintf(buf + n, sizeof(buf) - n, " txt=%zu:%016llx mk=%d se=%d fv=%d vs=%d tms=%08x max=%d edit=%d num=%08x chg=%d prs=%d gf=%d", t->msText.size(), h,
					 x.marker, x.sel_end, x.first_visible, x.visible_size, Bits(x.text_max_size), x.max_chars, x.can_edit, Bits(x.numeric_value),
					 x.changed_since_enter, x.pressed, x.got_focus_recently);
		}
		else if(cWidgetComboBox *c = dynamic_cast<cWidgetComboBox*>(w))
		{
			const OhplW2Combo &x = c->mCombo;
			snprintf(buf + n, sizeof(buf) - n, " open=%d sel=%d first=%d over=%d shown=%d ui=%d mh=%08x n=%d txt=%zu", x.menu_open, x.selected, x.first_item,
					 x.mouse_over, x.items_shown, x.open_by_ui, Bits(x.menu_height), c->GetItemNum(), c->msText.size());
		}
		else if(iWidgetListBoxBase *l = dynamic_cast<iWidgetListBoxBase*>(w))
		{
			const OhplW2List &x = l->mList;
			n += snprintf(buf + n, sizeof(buf) - n, " first=%d cur=%d last=%d anc=%d clk=%d lock=%d n=%d sel=", x.first_item, x.cursor_pos, x.last_cursor_pos,
						  x.anchor_pos, x.num_clicks, x.locked, l->GetItemNum());
			for(int k = 0; k < l->GetMultiSelectionNum() && n < 900; ++k) n += snprintf(buf + n, sizeof(buf) - n, "%d,", l->GetMultiSelection(k));
		}
		else if(cWidgetTabFrame *t = dynamic_cast<cWidgetTabFrame*>(w))
		{
			n += snprintf(buf + n, sizeof(buf) - n, " top=%d n=%d fv=%d lv=%d tabs=", t->GetTabOnTopIndex(), t->GetTabNum(), t->mState.first_visible, t->mState.last_visible);
			for(int k = 0; k < t->GetTabNum() && n < 900; ++k)
			{
				cWidgetTabLabel *lab = t->GetTab(k)->mpTabLabel;
				n += snprintf(buf + n, sizeof(buf) - n, "%d%d%08x,", lab->IsVisible(), lab->IsEnabled(), Bits(lab->GetLocalPosition().x));
			}
		}
		else if(cWidgetMenuItem *m = dynamic_cast<cWidgetMenuItem*>(w))
			snprintf(buf + n, sizeof(buf) - n, " chk=%d def=%d tm=%08x open=%d prs=%d", m->mbChecked, m->mbDefault, Bits(m->mfTimer), m->IsMenuOpen(), m->mbPressed);
		else if(iWidgetMenu *m = dynamic_cast<iWidgetMenu*>(w))
			snprintf(buf + n, sizeof(buf) - n, " hl=%d must=%d sub=%d", Idx(m->mpHighlightedItem), m->mbMustHide, m->IsSubmenuOpen());
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
	cGuiSkin *skin = new cGuiSkin("h20", gui);
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

static void Click(iWidget *w, float fx, float fy, int mod, int button, bool dbl)
{
	cGuiMessageData d;
	memset(&d, 0, sizeof(d));
	cVector3f g = w->GetGlobalPosition();
	cVector2f sz = w->GetSize();
	float x = g.x + fx * sz.x, y = g.y + fy * sz.y;
	printf("op click %08x %08x %d %d %d\n", Bits(x), Bits(y), mod, button, dbl);
	d.mvPos = cVector2f(x, y);
	bool r = gSet->SendMessage(eGuiMessage_MouseMove, d);
	d.mlVal = button;
	d.mKeyPress = cKeyPress((eKey)0, 0, mod);
	r = gSet->SendMessage(eGuiMessage_MouseDown, d) || r;
	if(dbl) r = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d) || r;
	r = gSet->SendMessage(eGuiMessage_MouseUp, d) || r;
	printf("= click %d\n", r ? 1 : 0);
}

static void OpSpecial()
{
	int kind = (int)Rnd(100);
	bool ret = false;
	cGuiMessageData d;
	memset(&d, 0, sizeof(d));
	if(kind < 30 && Rnd(100) < 60)
	{
		// click through the interactive widgets, with modifiers, double clicks, wheel, nav keys
		static const int mods[] = { 0, 0, 0, 2, 1, 3 };
		int pick = (int)Rnd(8);
		int mod = mods[Rnd(6)], btn = Rnd(8) == 0 ? (Rnd(2) ? 4 : 8) : 1;
		bool dbl = Rnd(6) == 0;
		iWidget *w = pick == 0 ? (iWidget*)gCombo : pick == 1 ? (iWidget*)gList : pick == 2 ? (iWidget*)gMpl : pick == 3 ? (iWidget*)gTabs :
					 pick == 4 ? (iWidget*)gCtx : pick == 5 ? gW[13 + Rnd(2)] : pick == 6 ? (iWidget*)gTb : gW[9 + Rnd(4)];
		float fx = Rnd(3) == 0 ? 0.85f + (float)Rnd(15) / 100.0f : (float)Rnd(100) / 100.0f;
		float fy = pick == 3 && Rnd(2) ? (float)Rnd(15) / 100.0f - 0.07f : (float)Rnd(100) / 100.0f;
		if(pick == 0 && Rnd(2)) { fx = 0.9f; fy = 0.5f; }
		if(pick == 4 && !gCtx->IsVisible()) { gCtx->SavePrevAttentionWidget(); gCtx->SetVisible(true); gSet->SetAttentionWidget(gCtx); gSet->SetFocusedWidget(gCtx); }
		Click(w, fx, fy, mod, btn, dbl);
		State();
		return;
	}
	if(kind < 38 && Rnd(100) < 40)
	{
		static const int nav[] = { 88, 89, 6, 3, 90, 91 };
		static const int arrows[] = { 1, 2, 4, 8 };
		iWidget *w = Rnd(2) ? (iWidget*)gList : Rnd(2) ? (iWidget*)gMpl : (iWidget*)gCombo;
		gSet->SetFocusedWidget(w);
		int a = (int)Rnd(4);
		printf("op nav %d %d\n", a, (int)Rnd(1));
		if(Rnd(2)) { d.mKeyPress = cKeyPress((eKey)nav[Rnd(6)], 0, Rnd(3) == 0 ? 2 : Rnd(5) == 0 ? 1 : 0); ret = gSet->SendMessage(eGuiMessage_KeyPress, d); }
		else if(Rnd(2)) { d.mlVal = arrows[a]; ret = gSet->SendMessage(eGuiMessage_UIArrowPress, d); }
		else { d.mlVal = Rnd(3) == 0 ? 2 : 1; ret = gSet->SendMessage(Rnd(2) ? eGuiMessage_UIButtonPress : eGuiMessage_UIButtonRelease, d); }
		printf("= ret=%d\n", ret ? 1 : 0);
		State();
		return;
	}
	if(kind < 38)
	{
		static const int keys[] = { 0, 70, 91, 90, 93, 94, 44, 3, 86, 5 };
		cWidgetTextBox *t = Rnd(2) ? gTb : gNtb;
		gSet->SetFocusedWidget(t);
		int mod = Rnd(5) == 0 ? 2 : (Rnd(9) == 0 ? 1 : 0);
		if(Rnd(100) < 55)
		{
			static const wchar_t chars[] = L"abcXYZ0123456789.-+ _";
			int u = chars[Rnd(21)];
			printf("op type %s %d\n", t == gTb ? "tb" : "ntb", u);
			d.mKeyPress = cKeyPress((eKey)(Rnd(50) + 10), u, mod);
		}
		else
		{
			int key = keys[Rnd(10)];
			printf("op tkey %s %d %d\n", t == gTb ? "tb" : "ntb", key, mod);
			d.mKeyPress = cKeyPress((eKey)key, 0, mod);
		}
		ret = gSet->SendMessage(eGuiMessage_KeyPress, d);
	}
	else if(kind < 46)
	{
		cWidgetTextBox *t = Rnd(2) ? gTb : gNtb;
		float x, y;
		PointIn(t, &x, &y);
		int what = (int)Rnd(4);
		printf("op tmouse %s %d %08x %08x\n", t == gTb ? "tb" : "ntb", what, Bits(x), Bits(y));
		d.mvPos = cVector2f(x, y);
		d.mlVal = 1;
		if(what == 0) ret = gSet->SendMessage(eGuiMessage_MouseMove, d);
		else if(what == 1) ret = gSet->SendMessage(eGuiMessage_MouseDown, d);
		else if(what == 2) ret = gSet->SendMessage(eGuiMessage_MouseUp, d);
		else ret = gSet->SendMessage(eGuiMessage_MouseDoubleClick, d);
	}
	else if(kind < 54)
	{
		int w = (int)Rnd(7);
		cWidgetTextBox *t = Rnd(2) ? gTb : gNtb;
		int v = (int)Rnd(12);
		printf("op tset %s %d %d\n", t == gTb ? "tb" : "ntb", w, v);
		static const wchar_t *txt[] = { L"", L"abc", L"Hello there world", L"12.5", L"-3", L"a very long piece of text that does not fit in the box at all" };
		if(w == 0) t->SetText(txt[v % 6]);
		else if(w == 1) t->SetMaxTextLength(v - 2);
		else if(w == 2) t->SetSelectedText(v - 1, v % 5 - 1);
		else if(w == 3) t->SetNumericValue((float)v * 1.7f - 4);
		else if(w == 4) t->SetCanEdit(v & 1);
		else if(w == 5) { t->SetCallbackOnLostFocus(v & 1); t->SetForceCallBackOnEnter(v & 2); t->SetIllegalChars(v & 4 ? L"ab" : L""); }
		else { t->SetDecimals(v % 4 - 1); t->SetNumericAdd((float)(v % 3) * 0.5f + 0.5f); }
	}
	else if(kind < 66)
	{
		int w = (int)Rnd(8), v = (int)Rnd(16);
		printf("op list %d %d\n", w, v);
		iWidgetListBoxBase *l = w & 1 ? (iWidgetListBoxBase*)gList : (iWidgetListBoxBase*)gMpl;
		if(w < 2) l->SetSelectedItem(v - 1, Rnd(2), Rnd(2), (eListBoxSelectType)Rnd(4), Rnd(2));
		else if(w < 4) l->SetCursorPos(v - 2, Rnd(2), Rnd(2));
		else if(w == 4) { l->SetAnchorPos(v); l->SetAllowMultiSelection(Rnd(2)); }
		else if(w == 5) { wchar_t t[16]; swprintf(t, 16, L"New %d", v); gList->AddItem(std::wstring(t)); }
		else if(w == 6) { if(gList->GetItemNum() > 3) gList->RemoveItem(v % gList->GetItemNum()); }
		else { l->ClearSelection(); l->SetIsLocked(Rnd(2)); }
	}
	else if(kind < 76)
	{
		int w = (int)Rnd(5), v = (int)Rnd(9);
		printf("op combo %d %d\n", w, v);
		if(w == 0) gCombo->SetSelectedItem(v - 1, Rnd(2), Rnd(2));
		else if(w == 1) gCombo->SetMaxShownItems(v % 5 + 1);
		else if(w == 2) { wchar_t t[16]; swprintf(t, 16, L"More %d", v); gCombo->AddItem(std::wstring(t)); }
		else if(w == 3) { gCombo->SetCanEdit(v & 1); gCombo->SetText(v & 2 ? L"typed" : L""); }
		else { float x, y; PointIn(gCombo, &x, &y); d.mvPos = cVector2f(x, y); d.mlVal = 1; ret = gSet->SendMessage(v & 1 ? eGuiMessage_MouseDown : eGuiMessage_MouseUp, d); }
	}
	else if(kind < 88)
	{
		int w = (int)Rnd(6), v = (int)Rnd(8);
		printf("op tab %d %d\n", w, v);
		if(w < 2) gTabs->SetTabOnTopByIndex(v - 1);
		else if(w == 2) gTabs->AddTab(L"Added tab number");
		else if(w == 3) { gTabs->SetScrollButtonsHidden(v & 1); }
		else if(w == 4) { gTabs->SetSize(cVector2f(60 + F(300), 100 + F(100))); }
		else { gTabs->GetTab(v % gTabs->GetTabNum())->SetText(v & 1 ? L"Z" : L"Renamed long tab"); }
	}
	else
	{
		int w = (int)Rnd(6);
		printf("op menu %d\n", w);
		if(w == 0) { gCtx->SavePrevAttentionWidget(); gCtx->SetPosition(cVector3f(F(500), F(300), 0)); gCtx->SetVisible(true); gSet->SetAttentionWidget(gCtx); gSet->SetFocusedWidget(gCtx); }
		else if(w == 1) gCtx->Hide();
		else if(w == 2) { cWidgetMenuItem *mi = (cWidgetMenuItem*)gW[9 + Rnd(4)]; mi->SetChecked(Rnd(2)); mi->SetDefault(Rnd(2)); }
		else if(w == 3) { int i = 9 + (int)Rnd(8); gSet->SetFocusedWidget(gW[i]); }
		else if(w == 4) { gCtx->SetMustHide(Rnd(2)); }
		else { cWidgetMenuItem *mi = (cWidgetMenuItem*)gW[9 + Rnd(6)]; mi->AddMenuItem(L"Late"); }
	}
	printf("= ret=%d\n", ret ? 1 : 0);
	State();
}

static void Op(int step)
{
	if(Rnd(100) < 45) { OpSpecial(); return; }
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
	const char *s = getenv("H20_SEED");
	int seed = s ? atoi(s) : 1;
	const char *skinPath = getenv("H20_SKIN");
	gRng = (unsigned long long)seed * 7919 + 17;
	gCb = (unsigned long long)seed * 104729 + 3;

	cRecordingLowLevelGraphics llg;
	cGraphics gfx(&llg, NULL);
	cGui *gui = new cGui();
	gui->Init(NULL, &gfx, NULL, NULL, NULL);
	cGuiSkin *skin = LoadSkin(gui, &gfx, &llg, skinPath ? skinPath : "");
	gSet = new cGuiSet("h20", gui, skin, NULL, &gfx, NULL, NULL);
	gSet->mpSkin = skin;
	gSet->SetVirtualSize(cVector2f(800, 600), -1000, 1000, cVector2f(0));

	cWidgetTextBox *tb = gSet->CreateWidgetTextBox(cVector3f(10, 10, 0), cVector2f(160, 0), L"Hello", NULL, eWidgetTextBoxInputType_Normal, 1, false, "tb");
	cWidgetTextBox *ntb = gSet->CreateWidgetTextBox(cVector3f(10, 50, 0), cVector2f(120, 0), L"", NULL, eWidgetTextBoxInputType_Numeric, 1.5f, true, "ntb");
	ntb->SetLowerBound(true, -5);
	ntb->SetUpperBound(true, 40);
	cWidgetComboBox *combo = gSet->CreateWidgetComboBox(cVector3f(200, 10, 0), cVector2f(150, 20), L"", NULL, "combo");
	for(int i = 0; i < 7; ++i) { wchar_t t[16]; swprintf(t, 16, L"Entry %d", i); combo->AddItem(std::wstring(t)); }
	combo->SetMaxShownItems(4);
	combo->SetSelectedItem(1, false, false);
	cWidgetListBox *list = gSet->CreateWidgetListBox(cVector3f(10, 100, 0), cVector2f(160, 100), NULL, "list");
	for(int i = 0; i < 14; ++i) { wchar_t t[16]; swprintf(t, 16, L"Item %d", i); list->AddItem(std::wstring(t)); }
	list->SetAllowMultiSelection(true);
	cWidgetMultiPropertyListBox *mpl = gSet->CreateWidgetMultiPropertyListBox(cVector3f(200, 100, 0), cVector2f(220, 120), NULL, "mpl");
	mpl->AddColumn("A", 0);
	mpl->AddColumn("B", 1, eFontAlign_Right);
	mpl->AddColumn("C", 2);
	mpl->SetColumnWidth(0, 60);
	mpl->SetColumnWidth(1, 50);
	for(int i = 0; i < 9; ++i)
	{
		cWidgetItem *it = mpl->AddItem();
		wchar_t t[16];
		swprintf(t, 16, L"r%d", i); it->AddProperty(std::wstring(t));
		swprintf(t, 16, L"c%d", i * 3); it->AddProperty(std::wstring(t));
		it->AddProperty(skin->GetGfx(eGuiSkinGfx_ButtonUpBackground));
	}
	mpl->SetAllowMultiSelection(true);
	cWidgetTabFrame *tabs = gSet->CreateWidgetTabFrame(cVector3f(10, 230, 0), cVector2f(300, 150), L"", NULL, true, true, "tabs");
	const wchar_t *tnames[] = { L"First", L"Second tab", L"Third", L"Fourth one", L"Fifth" };
	for(int i = 0; i < 5; ++i)
	{
		cWidgetTab *tab = tabs->AddTab(tnames[i]);
		gSet->CreateWidgetButton(cVector3f(10, 20 + i * 4, 0), cVector2f(70, 20), L"In tab", tab, false, "tbtn");
	}
	cWidgetContextMenu *ctx = gSet->CreateWidgetContextMenu(cVector3f(450, 40, 0), 0, L"", NULL, "ctx");
	static const char *iname[] = { "ci0", "ci1", "ci2", "ci3", "ci4", "cs0", "cs1", "cs2", "ms0", "ms1" };
	cWidgetMenuItem *ci[5];
	ci[0] = ctx->AddMenuItem(L"Open");
	ci[1] = ctx->AddMenuItem(L"Save as");
	ctx->AddSeparator();
	ci[2] = ctx->AddMenuItem(L"Sub menu");
	ci[3] = ctx->AddMenuItem(L"Exit");
	ci[1]->SetChecked(true);
	ci[3]->SetDefault(true);
	ci[2]->AddMenuItem(L"Sub one");
	ci[2]->AddMenuItem(L"Sub two");
	cWidgetMainMenu *mm = gSet->CreateWidgetMainMenu(NULL, "mm");
	cWidgetMenuItem *mi0 = mm->AddMenuItem(L"File");
	cWidgetMenuItem *mi1 = mm->AddMenuItem(L"Edit");
	mi0->AddMenuItem(L"New");
	mi0->AddMenuItem(L"Quit");
	mi1->AddMenuItem(L"Copy");
	cWidgetButton *out = gSet->CreateWidgetButton(cVector3f(600, 300, 0), cVector2f(90, 26), L"Outside", NULL, false, "out");
	std::vector<iWidget*> list_;
	std::vector<const char*> names_;
	iWidget *base[] = { tb, ntb, combo, list, mpl, tabs, ctx, mm, out };
	const char *bnames[] = { "tb", "ntb", "combo", "list", "mpl", "tabs", "ctx", "mm", "out" };
	for(size_t i = 0; i < sizeof(base) / sizeof(base[0]); ++i) { list_.push_back(base[i]); names_.push_back(bnames[i]); }
	for(int i = 0; i < 4; ++i) { list_.push_back(ci[i]); names_.push_back(iname[i]); }
	list_.push_back(mi0); names_.push_back("mm0");
	list_.push_back(mi1); names_.push_back("mm1");
	list_.push_back(ci[2]->GetChildMenu()); names_.push_back("csub");
	list_.push_back(mi0->GetChildMenu()); names_.push_back("msub");
	gCtx = ctx; gCombo = combo; gList = list; gMpl = mpl; gTabs = tabs; gTb = tb; gNtb = ntb;
	for(size_t i = 0; i < list_.size(); ++i) Add(list_[i], names_[i]);

	printf("seed %d skin %s\n", seed, skinPath ? strrchr(skinPath, '/') + 1 : "");
	State();
	for(int i = 0; i < 320; ++i)
	{
		if(i == 0 || i == 120 || i == 319) Frame(i);
		Op(i);
	}
	const std::string trace = llg.Trace().Text();
	if(getenv("H20_DIGEST"))
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
