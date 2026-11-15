#include "RustLuxUiA11b.h"

#include "LuxBase.h"
#include "LuxHelpFuncs.h"
#include "LuxInputHandler.h"
#include "LuxMapHandler.h"
#include "LuxA11bDump.h"

#include <map>

static_assert(eFontAlign_Left == 0 && eFontAlign_Center == 2, "A11b font align");
static_assert(eGuiMaterial_Alpha == 1 && eGuiMaterial_FontNormal == 2 && eGuiMaterial_Additive == 3 && eGuiMaterial_LastEnum == 6, "A11b gui material");

static tString Str(OhplA11bBytes aB)
{
	return tString((const char*)aB.p, aB.n);
}

static tWString WStr(const unsigned *apP, size_t alN)
{
	return tWString((const wchar_t*)apP, alN);
}

static void Put(OhplA11bSink32 apSink, void *apCtx, const tWString &asX)
{
	apSink(apCtx, (const unsigned*)asX.data(), asX.size());
}

static std::map<size_t, tString> gGfxNames;

static cGuiSet *Set(size_t alSet)
{
	return alSet ? (cGuiSet*)alSet : gpBase->mpGameHudSet;
}

static void MenuStr(OhplA11bBytes aSec, OhplA11bBytes aKey, OhplA11bBytes aDef, OhplA11bSink apSink, void *apCtx)
{
	tString s = gpBase->mpMenuCfg->GetString(Str(aSec), Str(aKey), Str(aDef));
	OhplA11Ans("menu_str", OhplA11Esc(Str(aSec))+"|"+OhplA11Esc(Str(aKey))+"|"+OhplA11Esc(Str(aDef)), OhplA11Esc(s));
	apSink(apCtx, (const unsigned char*)s.data(), s.size());
}

static float MenuF32(OhplA11bBytes aSec, OhplA11bBytes aKey, float afDef)
{
	float f = gpBase->mpMenuCfg->GetFloat(Str(aSec), Str(aKey), afDef);
	OhplA11Ans("menu_f32", OhplA11Esc(Str(aSec))+"|"+OhplA11Esc(Str(aKey))+"|"+OhplA11Num(afDef), OhplA11Num(f));
	return f;
}

static void MenuVec2(OhplA11bBytes aSec, OhplA11bBytes aKey, float afDef, float *apOut)
{
	cVector2f v = gpBase->mpMenuCfg->GetVector2f(Str(aSec), Str(aKey), cVector2f(afDef));
	apOut[0] = v.x;
	apOut[1] = v.y;
	OhplA11Ans("menu_vec2", OhplA11Esc(Str(aSec))+"|"+OhplA11Esc(Str(aKey))+"|"+OhplA11Num(afDef), OhplA11Num(v.x)+"|"+OhplA11Num(v.y));
}

static int UserBool(OhplA11bBytes aSec, OhplA11bBytes aKey, int alDef)
{
	int b = gpBase->mpUserConfig->GetBool(Str(aSec), Str(aKey), alDef != 0) ? 1 : 0;
	OhplA11Ans("user_bool", OhplA11Esc(Str(aSec))+"|"+OhplA11Esc(Str(aKey))+"|"+(alDef ? "1" : "0"), b ? "1" : "0");
	return b;
}

static void SetUserBool(OhplA11bBytes aSec, OhplA11bBytes aKey, int alX)
{
	gpBase->mpUserConfig->SetBool(Str(aSec), Str(aKey), alX != 0);
}

static size_t GfxRect(const float *apCol)
{
	size_t l = (size_t)gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(apCol[0], apCol[1], apCol[2], apCol[3]), eGuiMaterial_Alpha);
	gGfxNames[l] = "rect";
	return l;
}

static size_t GfxImage(OhplA11bBytes aFile)
{
	size_t l = (size_t)gpBase->mpEngine->GetGui()->CreateGfxImage(Str(aFile), eGuiMaterial_Alpha);
	gGfxNames[l] = Str(aFile);
	return l;
}

static size_t GfxTexture(OhplA11bBytes aFile)
{
	size_t l = (size_t)gpBase->mpEngine->GetGui()->CreateGfxTexture(Str(aFile), eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	gGfxNames[l] = Str(aFile);
	return l;
}

static void GfxSize(size_t alGfx, int alWhich, float *apOut)
{
	cGuiGfxElement *pGfx = (cGuiGfxElement*)alGfx;
	cVector2f v = alWhich == 0 ? pGfx->GetActiveSize() : pGfx->GetImageSize();
	apOut[0] = v.x;
	apOut[1] = v.y;
	OhplA11Ans("gfx_size", OhplA11Esc(gGfxNames[alGfx])+"|"+(alWhich ? "1" : "0"), OhplA11Num(v.x)+"|"+OhplA11Num(v.y));
}

static void WrapRows(size_t alFont, float afWidth, float afLine, const float *apSize, const unsigned *apText, size_t alN,
					OhplA11bSink32 apSink, void *apCtx)
{
	tWStringVec vRows;
	((iFontData*)alFont)->GetWordWrapRows(afWidth, afLine, cVector2f(apSize[0], apSize[1]), WStr(apText, alN), &vRows);
	for(size_t i=0; i<vRows.size(); ++i) Put(apSink, apCtx, vRows[i]);
	if(OhplA11Answers())
	{
		std::string sOut;
		for(size_t i=0; i<vRows.size(); ++i) sOut += (i ? "\\u0001" : "") + OhplA11Esc((const unsigned*)vRows[i].data(), vRows[i].size());
		OhplA11Ans("wrap_rows", OhplA11Num(afWidth)+"|"+OhplA11Num(afLine)+"|"+OhplA11Num(apSize[0])+"|"+OhplA11Num(apSize[1])+"|"+OhplA11Esc(apText, alN),
			std::to_string(vRows.size())+"|"+sOut);
	}
}

static float FontLen(size_t alFont, const float *apSize, const unsigned *apText, size_t alN)
{
	float f = ((iFontData*)alFont)->GetLength(cVector2f(apSize[0], apSize[1]), WStr(apText, alN).c_str());
	OhplA11Ans("font_len", OhplA11Num(apSize[0])+"|"+OhplA11Num(apSize[1])+"|"+OhplA11Esc(apText, alN), OhplA11Num(f));
	return f;
}

static void FontRange(size_t alFont, unsigned *apOut)
{
	apOut[0] = ((iFontData*)alFont)->GetFirstChar();
	apOut[1] = ((iFontData*)alFont)->GetLastChar();
	OhplA11Ans("font_range", "", std::to_string(apOut[0])+"|"+std::to_string(apOut[1]));
}

static int FontGlyph(size_t alFont, unsigned alIdx, float *apOut)
{
	cGlyph *pGlyph = ((iFontData*)alFont)->GetGlyph((int)alIdx);
	if(pGlyph == NULL) { OhplA11Ans("font_glyph", std::to_string(alIdx), "none"); return 0; }
	apOut[0] = pGlyph->mvSize.x;
	apOut[1] = pGlyph->mvSize.y;
	apOut[2] = pGlyph->mfAdvance;
	OhplA11Ans("font_glyph", std::to_string(alIdx), OhplA11Num(apOut[0])+"|"+OhplA11Num(apOut[1])+"|"+OhplA11Num(apOut[2]));
	return 1;
}

static void ParseString(const unsigned *apText, size_t alN, OhplA11bSink32 apSink, void *apCtx)
{
	tWString s = gpBase->mpHelpFuncs->ParseString(WStr(apText, alN));
	OhplA11Ans("parse_string", OhplA11Esc(apText, alN), OhplA11Esc((const unsigned*)s.data(), s.size()));
	Put(apSink, apCtx, s);
}

static float StringDuration(const unsigned *apText, size_t alN)
{
	float f = gpBase->mpHelpFuncs->GetStringDuration(WStr(apText, alN));
	OhplA11Ans("string_duration", OhplA11Esc(apText, alN), OhplA11Num(f));
	return f;
}

static void GuiSound(OhplA11bBytes aName)
{
	tString sName = Str(aName);
	OhplA11Log("snd %s", sName.c_str());
	gpBase->mpHelpFuncs->PlayGuiSoundData(sName, eSoundEntryType_Gui);
}

static void SetUpdateActive(int alX)
{
	OhplA11Log("upd %d", alX != 0 ? 1 : 0);
	gpBase->mpMapHandler->SetUpdateActive(alX != 0);
}

static void InputName(OhplA11bBytes aAction, OhplA11bSink32 apSink, void *apCtx)
{
	tWString s = gpBase->mpInputHandler->GetInputName(Str(aAction));
	OhplA11Ans("input_name", OhplA11Esc(Str(aAction)), OhplA11Esc((const unsigned*)s.data(), s.size()));
	Put(apSink, apCtx, s);
}

static void Hud(float *apOut)
{
	apOut[0] = gpBase->mvHudVirtualCenterSize.x;
	apOut[1] = gpBase->mvHudVirtualCenterSize.y;
	apOut[2] = gpBase->mvHudVirtualStartPos.x;
	apOut[3] = gpBase->mvHudVirtualStartPos.y;
	apOut[4] = gpBase->mvHudVirtualStartPos.z;
	OhplA11Ans("hud", "", OhplA11Num(apOut[0])+"|"+OhplA11Num(apOut[1])+"|"+OhplA11Num(apOut[2])+"|"+OhplA11Num(apOut[3])+"|"+OhplA11Num(apOut[4]));
}

static int PTest()
{
	OhplA11Ans("ptest", "", gpBase->mbPTestActivated ? "1" : "0");
	return gpBase->mbPTestActivated ? 1 : 0;
}

static void DrawGfx(size_t alSet, size_t alGfx, const float *apPos, const float *apSize, const float *apCol, int alMat)
{
	cVector3f vPos(apPos[0], apPos[1], apPos[2]);
	cVector2f vSize = apSize ? cVector2f(apSize[0], apSize[1]) : cVector2f(-1);
	cColor col(apCol[0], apCol[1], apCol[2], apCol[3]);
	OhplA11LogGfx(vPos, vSize, col, alMat);
	Set(alSet)->DrawGfx((cGuiGfxElement*)alGfx, vPos, vSize, col, (eGuiMaterial)alMat);
}

static void DrawFont(size_t alSet, size_t alFont, const float *apPos, const float *apSize, const float *apCol, int alAlign,
					int alMat, const unsigned *apText, size_t alN, int alPrefix)
{
	cVector3f vPos(apPos[0], apPos[1], apPos[2]);
	cVector2f vSize(apSize[0], apSize[1]);
	cColor col(apCol[0], apCol[1], apCol[2], apCol[3]);
	tWString sText = WStr(apText, alN);
	OhplA11LogFont(vPos, vSize, col, alAlign, alMat, alPrefix ? _W(" ") + sText : sText);
	if(alPrefix)
		Set(alSet)->DrawFont((iFontData*)alFont, vPos, vSize, col, (eFontAlign)alAlign, (eGuiMaterial)alMat, _W(" %ls"), sText.c_str());
	else
		Set(alSet)->DrawFont(sText, (iFontData*)alFont, vPos, vSize, col, (eFontAlign)alAlign, (eGuiMaterial)alMat);
}

static void Translate(OhplA11bBytes aCat, OhplA11bBytes aKey, OhplA11bSink32 apSink, void *apCtx)
{
	tWString s = kTranslate(Str(aCat), Str(aKey));
	OhplA11Ans("translate", OhplA11Esc(Str(aCat))+"|"+OhplA11Esc(Str(aKey)), OhplA11Esc((const unsigned*)s.data(), s.size()));
	Put(apSink, apCtx, s);
}

static int Chinese()
{
	OhplA11Ans("chinese", "", gpBase->msCurrentLanguage == "chinese.lang" ? "1" : "0");
	return gpBase->msCurrentLanguage == "chinese.lang" ? 1 : 0;
}

static void Callback(size_t alCb, int alYes)
{
	OhplA11Log("callback %d %d", alCb ? 1 : 0, alYes != 0 ? 1 : 0);
	if(alCb) ((iLuxMessageCallback*)alCb)->OnPress(alYes != 0);
}

void OhplA11bInitHost()
{
	static const OhplA11bHost host = {
		MenuStr, MenuF32, MenuVec2, UserBool, SetUserBool, GfxRect, GfxImage, GfxTexture, GfxSize, WrapRows, FontLen,
		FontRange, FontGlyph, ParseString, StringDuration, GuiSound, SetUpdateActive, InputName, Hud, PTest, DrawGfx,
		DrawFont, Translate, Chinese, Callback
	};
	ohpl_a11b_set_host(&host);
}
