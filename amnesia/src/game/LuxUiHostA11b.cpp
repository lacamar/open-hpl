#include "RustLuxUiA11b.h"

#include "LuxBase.h"
#include "LuxHelpFuncs.h"
#include "LuxInputHandler.h"
#include "LuxMapHandler.h"
#include "LuxMessageHandler.h"

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

static cGuiSet *Set(size_t alSet)
{
	return alSet ? (cGuiSet*)alSet : gpBase->mpGameHudSet;
}

static void MenuStr(OhplA11bBytes aSec, OhplA11bBytes aKey, OhplA11bBytes aDef, OhplA11bSink apSink, void *apCtx)
{
	tString s = gpBase->mpMenuCfg->GetString(Str(aSec), Str(aKey), Str(aDef));
	apSink(apCtx, (const unsigned char*)s.data(), s.size());
}

static float MenuF32(OhplA11bBytes aSec, OhplA11bBytes aKey, float afDef)
{
	return gpBase->mpMenuCfg->GetFloat(Str(aSec), Str(aKey), afDef);
}

static void MenuVec2(OhplA11bBytes aSec, OhplA11bBytes aKey, float afDef, float *apOut)
{
	cVector2f v = gpBase->mpMenuCfg->GetVector2f(Str(aSec), Str(aKey), cVector2f(afDef));
	apOut[0] = v.x;
	apOut[1] = v.y;
}

static int UserBool(OhplA11bBytes aSec, OhplA11bBytes aKey, int alDef)
{
	return gpBase->mpUserConfig->GetBool(Str(aSec), Str(aKey), alDef != 0) ? 1 : 0;
}

static void SetUserBool(OhplA11bBytes aSec, OhplA11bBytes aKey, int alX)
{
	gpBase->mpUserConfig->SetBool(Str(aSec), Str(aKey), alX != 0);
}

static void FontRange(size_t alFont, unsigned *apOut)
{
	apOut[0] = ((iFontData*)alFont)->GetFirstChar();
	apOut[1] = ((iFontData*)alFont)->GetLastChar();
}

static int FontGlyph(size_t alFont, unsigned alIdx, float *apOut)
{
	cGlyph *pGlyph = ((iFontData*)alFont)->GetGlyph((int)alIdx);
	if(pGlyph == NULL) return 0;
	apOut[0] = pGlyph->GetSize().x;
	apOut[1] = pGlyph->GetSize().y;
	apOut[2] = pGlyph->GetAdvance();
	return 1;
}

static void ParseString(const unsigned *apText, size_t alN, OhplA11bSink32 apSink, void *apCtx)
{
	Put(apSink, apCtx, gpBase->mpHelpFuncs->ParseString(WStr(apText, alN)));
}

static float StringDuration(const unsigned *apText, size_t alN)
{
	return gpBase->mpHelpFuncs->GetStringDuration(WStr(apText, alN));
}

static void GuiSound(OhplA11bBytes aName)
{
	gpBase->mpHelpFuncs->PlayGuiSoundData(Str(aName), eSoundEntryType_Gui);
}

static void SetUpdateActive(int alX)
{
	gpBase->mpMapHandler->SetUpdateActive(alX != 0);
}

static void InputName(OhplA11bBytes aAction, OhplA11bSink32 apSink, void *apCtx)
{
	Put(apSink, apCtx, gpBase->mpInputHandler->GetInputName(Str(aAction)));
}

static void Hud(float *apOut)
{
	apOut[0] = gpBase->mvHudVirtualCenterSize.x;
	apOut[1] = gpBase->mvHudVirtualCenterSize.y;
	apOut[2] = gpBase->mvHudVirtualStartPos.x;
	apOut[3] = gpBase->mvHudVirtualStartPos.y;
	apOut[4] = gpBase->mvHudVirtualStartPos.z;
}

static int PTest()
{
	return gpBase->mbPTestActivated ? 1 : 0;
}

static void DrawFont(size_t alSet, size_t alFont, const float *apPos, const float *apSize, const float *apCol, int alAlign,
					int alMat, const unsigned *apText, size_t alN, int alPrefix)
{
	cVector3f vPos(apPos[0], apPos[1], apPos[2]);
	cVector2f vSize(apSize[0], apSize[1]);
	cColor col(apCol[0], apCol[1], apCol[2], apCol[3]);
	tWString sText = WStr(apText, alN);
	if(alPrefix)
		Set(alSet)->DrawFont((iFontData*)alFont, vPos, vSize, col, (eFontAlign)alAlign, (eGuiMaterial)alMat, _W(" %ls"), sText.c_str());
	else
		Set(alSet)->DrawFont(sText, (iFontData*)alFont, vPos, vSize, col, (eFontAlign)alAlign, (eGuiMaterial)alMat);
}

static void Translate(OhplA11bBytes aCat, OhplA11bBytes aKey, OhplA11bSink32 apSink, void *apCtx)
{
	Put(apSink, apCtx, kTranslate(Str(aCat), Str(aKey)));
}

static int Chinese()
{
	return gpBase->msCurrentLanguage == "chinese.lang" ? 1 : 0;
}

static void Callback(size_t alCb, int alYes)
{
	((iLuxMessageCallback*)alCb)->OnPress(alYes != 0);
}

static int MapLoaded()
{
	return gpBase->mpMapHandler != NULL && gpBase->mpMapHandler->GetCurrentMap() != NULL;
}

static int JournalActive()
{
	return gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() == "Journal";
}

static void EnterJournal()
{
	gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
}

void OhplA11bInitHost()
{
	static const OhplA11bHost host = {
		MenuStr, MenuF32, MenuVec2, UserBool, SetUserBool,
		FontRange, FontGlyph, ParseString, StringDuration, GuiSound, SetUpdateActive, InputName, Hud, PTest,
		DrawFont, Translate, Chinese, Callback, MapLoaded, JournalActive, EnterJournal
	};
	ohpl_a11b_set_host(&host);
}
