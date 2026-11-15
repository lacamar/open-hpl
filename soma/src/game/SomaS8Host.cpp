#include "SomaS8Host.h"
#include "SomaBase.h"
#include "SomaImGui.h"
#include "system/HeadlessControl.h"
#include <SDL2/SDL.h>
#include <cstring>

static_assert(sizeof(wchar_t) == sizeof(uint32_t), "wchar_t layout");
static_assert(eKey_Return == 3 && eKey_Escape == 5 && eKey_Space == 6, "gamma screen continue keys");

cSomaImGui *SomaHudImGui();
void SomaRenderImGuis();

namespace
{
	template <class T> T *Ptr(const OhplS8Arg &a) { return (T*)a.v; }
	float F(const OhplS8Arg &a) { uint32_t b = (uint32_t)a.v; float f; memcpy(&f, &b, 4); return f; }
	int I(const OhplS8Arg &a) { return (int)a.v; }
	tString Str(const OhplS8Arg &a) { return tString((const char*)a.v, a.n); }
	tWString WStr(const OhplS8Arg &a) { return tWString((const wchar_t*)a.v, a.n); }
	void SetF(OhplS8Call *c, int i, float f) { uint32_t b; memcpy(&b, &f, 4); c->u[i] = b; }
	cVector3f V3(const OhplS8Arg *a) { return cVector3f(F(a[0]), F(a[1]), F(a[2])); }
	cVector2f V2(const OhplS8Arg *a) { return cVector2f(F(a[0]), F(a[1])); }

	bool Tramp(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
	{
		cOhplS8Shell::Cb *pCb = (cOhplS8Shell::Cb*)apObject;
		return ohpl_s8_gamma_cb(&pCb->mpShell->mHost, pCb->mpShell->mH, pCb->mlTag);
	}

	cOhplS8Shell *gpTickShell;

	void Call(void *apCtx, OhplS8Call *c)
	{
		cOhplS8Shell *s = (cOhplS8Shell*)apCtx;
		const OhplS8Arg *a = c->a;
		cGui *pGui = s->mpEngine ? s->mpEngine->GetGui() : NULL;
		iLowLevelGraphics *pLow = s->mpEngine ? s->mpEngine->GetGraphics()->GetLowLevel() : NULL;
		switch (c->op)
		{
		case kS8_SetNew: c->u[0] = (uint64_t)pGui->CreateSet(Str(a[0]), pGui->CreateSkin("gui_default.skin")); break;
		case kS8_SetDrawMouse: Ptr<cGuiSet>(a[0])->SetDrawMouse(a[1].v != 0); break;
		case kS8_SetActive: Ptr<cGuiSet>(a[0])->SetActive(a[1].v != 0); break;
		case kS8_GuiFocus: pGui->SetFocus(Ptr<cGuiSet>(a[0])); break;
		case kS8_GuiFocusedIs: c->u[0] = pGui->GetFocusedSet() == Ptr<cGuiSet>(a[0]); break;
		case kS8_ViewportNew: c->u[0] = (uint64_t)s->mpEngine->GetScene()->CreateViewport(NULL, NULL, a[0].v != 0); break;
		case kS8_ViewportAddSet: Ptr<cViewport>(a[0])->AddGuiSet(Ptr<cGuiSet>(a[1])); break;
		case kS8_ViewportActive: Ptr<cViewport>(a[0])->SetActive(a[1].v != 0); break;
		case kS8_GfxRect: c->u[0] = (uint64_t)pGui->CreateGfxFilledRect(cColor(F(a[0]), F(a[1]), F(a[2]), F(a[3])), (eGuiMaterial)I(a[4])); break;
		case kS8_GfxTex: c->u[0] = (uint64_t)pGui->CreateGfxTexture(Str(a[0]), (eGuiMaterial)I(a[1]), eTextureType_2D); break;
		case kS8_GfxSize:
		{
			cVector2f vSize = Ptr<cGuiGfxElement>(a[0])->GetImageSize();
			SetF(c, 0, vSize.x);
			SetF(c, 1, vSize.y);
			break;
		}
		case kS8_GfxDestroy: pGui->DestroyGfx(Ptr<cGuiGfxElement>(a[0])); break;
		case kS8_DrawGfx:
			Ptr<cGuiSet>(a[0])->DrawGfx(Ptr<cGuiGfxElement>(a[1]), V3(a + 2), V2(a + 5), cColor(F(a[7]), F(a[8]), F(a[9]), F(a[10])));
			break;
		case kS8_DrawFont:
			Ptr<cGuiSet>(a[0])->DrawFont(WStr(a[10]), Ptr<iFontData>(a[1]), V3(a + 2), cVector2f(F(a[5]), F(a[5])),
										 cColor(F(a[6]), F(a[7]), F(a[8]), F(a[9])), eFontAlign_Left);
			break;
		case kS8_FontNew: c->u[0] = (uint64_t)s->mpEngine->GetResources()->GetFontManager()->CreateFontData(Str(a[0])); break;
		case kS8_FontWrap:
		{
			tWStringVec vRows;
			Ptr<iFontData>(a[0])->GetWordWrapRows(F(a[1]), F(a[2]), cVector2f(F(a[3]), F(a[3])), WStr(a[4]), &vRows);
			for (const tWString &sRow : vRows)
				c->sink(c->sinkCtx, (const uint32_t*)sRow.data(), sRow.size());
			break;
		}
		case kS8_ClipNew: c->u[0] = (uint64_t)hplNew(cGuiClipRegion, ()); break;
		case kS8_ClipRect: Ptr<cGuiClipRegion>(a[0])->mRect = cRect2f(F(a[1]), F(a[2]), F(a[3]), F(a[4])); break;
		case kS8_ClipGet: c->u[0] = (uint64_t)Ptr<cGuiSet>(a[0])->GetCurrentClipRegion(); break;
		case kS8_ClipSet: Ptr<cGuiSet>(a[0])->SetCurrentClipRegion(Ptr<cGuiClipRegion>(a[1])); break;
		case kS8_ClipDelete: hplDelete(Ptr<cGuiClipRegion>(a[0])); break;
		case kS8_SliderNew:
		{
			cWidgetSlider *pSlider = Ptr<cGuiSet>(a[0])->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, V3(a + 1), V2(a + 4), I(a[6]), NULL);
			pSlider->AddCallback(eGuiMessage_SliderMove, &s->mCb[I(a[7])], &Tramp);
			c->u[0] = (uint64_t)pSlider;
			break;
		}
		case kS8_ButtonNew:
		{
			cWidgetButton *pButton = Ptr<cGuiSet>(a[0])->CreateWidgetButton(V3(a + 1), V2(a + 4), WStr(a[6]), NULL);
			pButton->AddCallback(eGuiMessage_ButtonPressed, &s->mCb[I(a[7])], &Tramp);
			c->u[0] = (uint64_t)pButton;
			break;
		}
		case kS8_SliderSet: Ptr<cWidgetSlider>(a[0])->SetValue(I(a[1]), false); break;
		case kS8_SliderGet:
			c->u[0] = Ptr<cWidgetSlider>(a[0])->GetValue();
			c->u[1] = Ptr<cWidgetSlider>(a[0])->GetMaxValue();
			break;
		case kS8_DefaultFocusNav: Ptr<cGuiSet>(a[0])->SetDefaultFocusNavWidget(Ptr<iWidget>(a[1])); break;
		case kS8_FocusedWidget: Ptr<cGuiSet>(a[0])->SetFocusedWidget(Ptr<iWidget>(a[1])); break;
		case kS8_Music: s->mpEngine->GetSound()->GetMusicHandler()->Play(Str(a[0]), F(a[1]), F(a[2]), a[3].v != 0, a[4].v != 0); break;
		case kS8_Now: c->u[0] = cPlatform::GetApplicationTime(); break;
		case kS8_Screen:
		{
			cVector2f vScreen = pLow->GetScreenSizeFloat();
			SetF(c, 0, vScreen.x);
			SetF(c, 1, vScreen.y);
			break;
		}
		case kS8_LoadMenu:
			gpTickShell = s;
			iResourceManager::mpLoadTickCallback = [] { ohpl_s8_splash_load_draw(&gpTickShell->mHost, gpTickShell->mH, true); };
			s->mpBase->LoadScriptMainMenu();
			iResourceManager::mpLoadTickCallback = NULL;
			break;
		case kS8_HudImGui: SomaHudImGui(); break;
		case kS8_BaseSplashDone: s->mpBase->OnSplashFinished(); break;
		case kS8_BaseGammaDone: s->mpBase->OnGammaScreenFinished(); break;
		case kS8_SaveGamma:
		{
			s->mpBase->GetConfig()->mfGamma = F(a[0]);
			s->mpBase->GetConfig()->Save();
			if (cConfigFile *pUser = SomaUserConfig())
			{
				pUser->SetFloat("Graphics", "Brightness", F(a[0]));
				pUser->Save();
			}
			break;
		}
		case kS8_GammaGet: SetF(c, 0, pLow->GetGammaCorrection()); break;
		case kS8_GammaSet: pLow->SetGammaCorrection(F(a[0])); break;
		case kS8_FrameBegin:
			// Lets the compositor's fullscreen configure through (niri keeps the window tiled until acked)
			SDL_PumpEvents();
			pLow->SetCurrentFrameBuffer(NULL);
			pLow->SetClearColor(cColor(0, 1));
			pLow->ClearFrameBuffer(eClearFrameBufferFlag_Color);
			break;
		case kS8_ImGuiRender: SomaRenderImGuis(); break;
		case kS8_FrameEnd:
			Ptr<cGuiSet>(a[0])->Render(NULL);
			Ptr<cGuiSet>(a[0])->ClearRenderObjects();
			pLow->FlushRendering();
			if (s->mpEngine->GetHeadlessControl())
				s->mpEngine->GetHeadlessControl()->Update(true);
			pLow->SwapBuffers();
			break;
		case kS8_MousePoll:
			if (iMouse *pMouse = s->mpEngine->GetInput()->GetMouse())
			{
				c->u[0] = 1;
				c->u[1] = pMouse->GetAbsPosition().x;
				c->u[2] = pMouse->GetAbsPosition().y;
				c->u[3] = pMouse->GetRelPosition().x;
				c->u[4] = pMouse->GetRelPosition().y;
				c->u[5] = pMouse->ButtonIsDown(eMouseButton_Left);
			}
			break;
		case kS8_GuiMousePos: pGui->SendMousePos(cVector2l(I(a[0]), I(a[1])), cVector2l(I(a[2]), I(a[3]))); break;
		case kS8_Click:
			if (a[0].v)
				pGui->SendMouseClickDown(eGuiMouseButton_Left);
			else
				pGui->SendMouseClickUp(eGuiMouseButton_Left);
			break;
		case kS8_KeyPoll:
		{
			c->u[0] = (uint64_t)(int64_t)-1;
			cInput *pInput = s->mpEngine->GetInput();
			iKeyboard *pKeyboard = pInput ? pInput->GetKeyboard() : NULL;
			if (pKeyboard && pKeyboard->KeyIsPressed())
				c->u[0] = pKeyboard->GetKey().mKey;
			break;
		}
		case kS8_StateDir:
		{
			tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome) + _W("open-hpl/") + SomaGameIdW();
			c->sink(c->sinkCtx, (const uint32_t*)sDir.data(), sDir.size());
			break;
		}
		}
	}
}

cOhplS8Shell::cOhplS8Shell(cEngine *apEngine, cSomaBase *apBase) : mpEngine(apEngine), mpBase(apBase)
{
	mHost.ctx = this;
	mHost.call = &Call;
	mCb[0] = {this, 0};
	mCb[1] = {this, 1};
}
