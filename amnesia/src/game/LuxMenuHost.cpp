#include "LuxMainMenu.h"
#include "LuxInputHandler.h"
#include "LuxMapHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxSaveHandler.h"
#include "RustLuxMenuA9.h"
#include "RustMenuHost.h"
#include <cstring>

static_assert(sizeof(wchar_t) == sizeof(uint32_t), "wchar_t layout");
static_assert(sizeof(cDate) == 8 * sizeof(int32_t), "cDate layout");

namespace
{
	tWString W(const uint32_t *apText, size_t alLen) { return tWString((const wchar_t*)apText, alLen); }
	tString Bytes(const uint32_t *apText, size_t alLen)
	{
		tString sOut;
		for(size_t i = 0; i < alLen; ++i) sOut += (char)apText[i];
		return sOut;
	}
	void Emit(OhplA9Sink apSink, void *apCtx, const tWString &asText) { apSink(apCtx, (const uint32_t*)asText.data(), asText.size()); }
	void EmitBytes(OhplA9Sink apSink, void *apCtx, const tString &asText)
	{
		std::vector<uint32_t> vText(asText.size());
		for(size_t i = 0; i < asText.size(); ++i) vText[i] = (unsigned char)asText[i];
		apSink(apCtx, vText.data(), vText.size());
	}

	bool Tramp(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
	{
		return ohpl_a9_cb((size_t)apObject, apWidget, aData.mlVal);
	}

	const eGuiMessage kMsg[] = { eGuiMessage_OnUpdate, eGuiMessage_ButtonPressed, eGuiMessage_UIButtonPress,
		eGuiMessage_SelectionDoubleClick, eGuiMessage_CheckChange, eGuiMessage_GetUINavFocus, eGuiMessage_SelectionChange,
		eGuiMessage_WindowClose, eGuiMessage_TextBoxEnter };
	const eUIArrow kArrow[] = { eUIArrow_Up, eUIArrow_Right, eUIArrow_Down, eUIArrow_Left };

	iWidget *Wd(OhplA9W apWidget) { return (iWidget*)apWidget; }
	cWidgetListBox *List(OhplA9W apWidget) { return (cWidgetListBox*)apWidget; }
	cWidgetLabel *Label(OhplA9W apWidget) { return (cWidgetLabel*)apWidget; }
	cWidgetTextBox *Text(OhplA9W apWidget) { return (cWidgetTextBox*)apWidget; }
	cColor Color(const float *v) { return cColor(v[0], v[1], v[2], v[3]); }
}

//-----------------------------------------------------------------------

namespace
{

OhplA9W ohpl_a9h_new(OhplA9W apSet, int alKind, OhplA9W apParent, const float *apPos, const float *apSize, int alFlags,
					const uint32_t *apText, size_t alLen)
{
	cGuiSet *pSet = (cGuiSet*)apSet;
	iWidget *pParent = Wd(apParent);
	cVector3f vPos(apPos[0], apPos[1], apPos[2]);
	cVector2f vSize(apSize[0], apSize[1]);
	tWString sText = W(apText, alLen);

	switch(alKind)
	{
	case 0: return pSet->CreateWidgetWindow(alFlags ? eWidgetWindowButtonFlag_ButtonClose : eWidgetWindowButtonFlag_None, vPos, vSize, sText);
	case 1: return pSet->CreateWidgetButton(vPos, vSize, sText, pParent);
	case 2: return pSet->CreateWidgetLabel(vPos, vSize, sText, pParent);
	case 3: return pSet->CreateWidgetListBox(vPos, vSize, pParent);
	case 4: return pSet->CreateWidgetImage("", vPos, vSize, eGuiMaterial_Alpha, false, pParent);
	case 5: return pSet->CreateWidgetFrame(vPos, vSize, false, pParent, false, true);
	case 6: return pSet->CreateWidgetTextBox(vPos, vSize, sText, pParent);
	}
	return NULL;
}

OhplA9W ohpl_a9h_popup(OhplA9W apSet, const OhplA9Str *apStr, size_t alObj, int alKillOnEscape, bool abCopyFocus)
{
	cGuiSet *pSet = (cGuiSet*)apSet;
	cGuiPopUpMessageBox *pPopUp = pSet->CreatePopUpMessageBox(W(apStr[0].mpText, apStr[0].mlLen), W(apStr[1].mpText, apStr[1].mlLen),
								W(apStr[2].mpText, apStr[2].mlLen), W(apStr[3].mpText, apStr[3].mlLen),
								alObj ? (void*)alObj : NULL, alObj ? &Tramp : NULL);
	if(abCopyFocus) pPopUp->GetGuiSet()->SetDrawFocus(pSet->GetDrawFocus());
	if(alKillOnEscape >= 0) pPopUp->SetKillOnEscapeKey(alKillOnEscape != 0);
	return pPopUp;
}

size_t ohpl_a9h_set(OhplA9W apSet, int alOp, OhplA9W apWidget, size_t alArg)
{
	cGuiSet *pSet = (cGuiSet*)apSet;
	switch(alOp)
	{
	case 0: pSet->SetDrawFocus(alArg != 0); break;
	case 1: return pSet->GetDrawFocus();
	case 2: pSet->SetFocusedWidget(Wd(apWidget)); break;
	case 3: pSet->SetDefaultFocusNavWidget(Wd(apWidget)); break;
	case 4: return pSet->PopUpIsActive();
	case 5: pSet->PushAttentionWidget(); break;
	case 6: pSet->PushDefaultFocusNavWidget(); break;
	case 7: pSet->PushFocusedWidget(); break;
	case 8: pSet->PopAttentionWidget(); break;
	case 9: pSet->PopDefaultFocusNavWidget(); break;
	case 10: pSet->PopFocusedWidget(); break;
	case 11: pSet->SetAttentionWidget(Wd(apWidget)); break;
	case 12: return (size_t)pSet->GetAttentionWidget();
	}
	return 0;
}

size_t ohpl_a9h_w(OhplA9W apWidget, int alOp, size_t alArg)
{
	iWidget *pWidget = Wd(apWidget);
	switch(alOp)
	{
	case 0: pWidget->SetEnabled(alArg != 0); break;
	case 1: pWidget->SetVisible(alArg != 0); break;
	case 2: return pWidget->IsVisible();
	case 3: ((cWidgetWindow*)pWidget)->SetStatic(alArg != 0); break;
	case 4: pWidget->CenterGlobalPositionInSet(); break;
	case 5: pWidget->SetGlobalUIInputListener(alArg != 0); break;
	case 6: pWidget->SetUserData((void*)alArg); break;
	case 7: ((cWidgetWindow*)pWidget)->SetCloseButtonDisablesWindow(alArg != 0); break;
	case 8: Label(pWidget)->SetWordWrap(alArg != 0); break;
	case 9: pWidget->SetClipActive(alArg != 0); break;
	case 10: Label(pWidget)->SetDrawBackGround(alArg != 0); break;
	case 11: Label(pWidget)->SetAutogenerateSize(alArg != 0); break;
	case 12: Text(pWidget)->SetForceCallBackOnEnter(alArg != 0); break;
	case 13: Text(pWidget)->SetCallbackOnLostFocus(alArg != 0); break;
	case 14: Text(pWidget)->SetMaxTextLength((int)alArg); break;
	case 15: Text(pWidget)->SetSelectedText(); break;
	case 16: List(pWidget)->ClearItems(); break;
	case 17: return List(pWidget)->GetItemNum();
	case 18: return (size_t)(ssize_t)List(pWidget)->GetSelectedItem();
	case 19: List(pWidget)->SetSelectedItem((int)alArg); break;
	case 20: List(pWidget)->SetIsLocked(alArg != 0); break;
	case 21: return List(pWidget)->GetIsLocked();
	case 22: Label(pWidget)->SetTextAlign(eFontAlign_Center); break;
	case 23: return (size_t)List(pWidget)->GetItem((int)alArg)->GetUserData();
	}
	return 0;
}

void ohpl_a9h_w_set_f(OhplA9W apWidget, int alOp, const float *apVal)
{
	iWidget *pWidget = Wd(apWidget);
	switch(alOp)
	{
	case 0: pWidget->SetPosition(cVector3f(apVal[0], apVal[1], apVal[2])); break;
	case 1: pWidget->SetSize(cVector2f(apVal[0], apVal[1])); break;
	case 2: pWidget->SetDefaultFontColor(Color(apVal)); break;
	case 3: pWidget->SetColorMul(Color(apVal)); break;
	case 4: Label(pWidget)->SetBackGroundColor(Color(apVal)); break;
	case 5: Label(pWidget)->SetScrollSpeedMul(apVal[0]); break;
	case 6: pWidget->SetDefaultFontSize(cVector2f(apVal[0], apVal[1])); break;
	}
}

void ohpl_a9h_w_get_f(OhplA9W apWidget, int alOp, float *apOut)
{
	iWidget *pWidget = Wd(apWidget);
	if(alOp == 0)
	{
		cVector3f v = pWidget->GetLocalPosition();
		apOut[0] = v.x; apOut[1] = v.y; apOut[2] = v.z;
	}
	else
	{
		cVector2f v = pWidget->GetSize();
		apOut[0] = v.x; apOut[1] = v.y;
	}
}

size_t ohpl_a9h_w_str(OhplA9W apWidget, int alOp, const uint32_t *apText, size_t alLen, size_t alArg)
{
	iWidget *pWidget = Wd(apWidget);
	switch(alOp)
	{
	case 0: pWidget->SetText(W(apText, alLen)); break;
	case 1: Text(pWidget)->SetIllegalChars(W(apText, alLen)); break;
	case 2:
		{
			cWidgetItem *pItem = List(pWidget)->AddItem(W(apText, alLen));
			if(alArg) pItem->SetUserData((void*)alArg);
		}
		break;
	}
	return 0;
}

void ohpl_a9h_w_get_str(OhplA9W apWidget, int alOp, int alIdx, OhplA9Sink apSink, void *apCtx)
{
	if(alOp == 0) Emit(apSink, apCtx, Wd(apWidget)->GetText());
	else Emit(apSink, apCtx, List(apWidget)->GetItem(alIdx)->GetText());
}

void ohpl_a9h_w_cb(OhplA9W apWidget, int alMsg, size_t alObj)
{
	Wd(apWidget)->AddCallback(kMsg[alMsg], (void*)alObj, &Tramp);
}

void ohpl_a9h_w_nav(OhplA9W apWidget, int alDir, OhplA9W apTarget)
{
	Wd(apWidget)->SetFocusNavigation(kArrow[alDir], Wd(apTarget));
}

float ohpl_a9h_text_width(OhplA9W apWidget, const uint32_t *apText, size_t alLen)
{
	iWidget *pWidget = Wd(apWidget);
	tWString sText = W(apText, alLen);
	return pWidget->GetDefaultFontType()->GetLength(pWidget->GetDefaultFontSize(), sText.c_str());
}

void ohpl_a9h_image_file(OhplA9W apWidget, const uint32_t *apText, size_t alLen)
{
	cWidgetImage *pImage = (cWidgetImage*)apWidget;
	cGui *pGui = gpBase->mpEngine->GetGui();

	cGuiGfxElement *pGfx = pImage->GetImage();
	if(pGfx)
	{
		pGui->DestroyGfx(pGfx);
		pGfx = NULL;
	}

	iTexture *pTex = gpBase->mpEngine->GetResources()->GetTextureManager()->Create2D(Bytes(apText, alLen), false, eTextureType_Rect);
	if(pTex)
		pGfx = pGui->CreateGfxTexture(pTex, true, eGuiMaterial_Alpha);

	pImage->SetImage(pGfx);
}

size_t ohpl_a9h_lux(int alOp, size_t alArg)
{
	switch(alOp)
	{
	case 0: return gpBase->mbHardMode;
	case 1: gpBase->mbHardMode = alArg != 0; break;
	case 2: return gpBase->mpCustomStory != NULL;
	case 3: gpBase->SetCustomStory(NULL); break;
	case 4: return gpBase->mpInputHandler->IsGamepadPresent();
	case 5: gpBase->mpInputHandler->ChangeState(eLuxInputState_Game); break;
	case 6:
		gpBase->mpEngine->GetUpdater()->SetContainer("Default");
		gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
		break;
	case 7: gpBase->mpProgressLogHandler->CreateAndResetLogFile(); break;
	case 8: return gpBase->mpSaveHandler->SaveFileExists();
	case 9: gpBase->mpSaveHandler->AutoLoad(true); break;
	case 10: return gpBase->mpUserConfig != NULL;
	case 11: gpBase->InitUserConfig(); break;
	case 12: gpBase->StartCustomStory(); break;
	case 13: gpBase->mpEngine->Exit(); break;
	case 18:
#ifdef USERDIR_RESOURCES
		return 1;
#else
		return 0;
#endif
	}
	return 0;
}

void ohpl_a9h_lux_str(int alOp, OhplA9Sink apSink, void *apCtx)
{
	switch(alOp)
	{
	case 0: Emit(apSink, apCtx, gpBase->msBaseSavePath); break;
	case 1: Emit(apSink, apCtx, gpBase->msProfileSavePath); break;
	case 2: Emit(apSink, apCtx, gpBase->msMainProfileSavePath); break;
	case 3: Emit(apSink, apCtx, gpBase->msBaseConfigPath); break;
	case 4: Emit(apSink, apCtx, gpBase->msDefaultProfileName); break;
	case 5: Emit(apSink, apCtx, cString::AddSlashAtEndW(cString::To16Char(gpBase->msCustomStoryPath))); break;
	case 6: Emit(apSink, apCtx, cString::AddSlashAtEndW(gpBase->msUserResourceDir + cString::To16Char(gpBase->msCustomStoryPath))); break;
	}
}

size_t ohpl_a9h_lux_with(int alOp, const uint32_t *apText, size_t alLen)
{
	tWString sText = W(apText, alLen);
	switch(alOp)
	{
	case 0: return gpBase->CreateProfile(sText);
	case 1: gpBase->SetProfile(sText); break;
	case 2: gpBase->mpSaveHandler->LoadGameFromFile(sText); break;
	case 3: return gpBase->msProfileName == sText;
	}
	return 0;
}

void ohpl_a9h_lux_with_str(int alOp, const uint32_t *apText, size_t alLen, OhplA9Sink apSink, void *apCtx)
{
	Emit(apSink, apCtx, gpBase->mpSaveHandler->GetProperSaveName(W(apText, alLen)));
}

void ohpl_a9h_plat_list(int alKind, const uint32_t *apDir, size_t alDirLen, const uint32_t *apMask, size_t alMaskLen,
						OhplA9Sink apSink, void *apCtx)
{
	tWStringList lstNames;
	if(alKind == 0) cPlatform::FindFilesInDir(lstNames, W(apDir, alDirLen), W(apMask, alMaskLen));
	else cPlatform::FindFoldersInDir(lstNames, W(apDir, alDirLen), false, false);
	for(tWStringListIt it = lstNames.begin(); it != lstNames.end(); ++it) Emit(apSink, apCtx, *it);
}

void ohpl_a9h_plat_date(int alKind, const uint32_t *apPath, size_t alLen, int32_t *apOut)
{
	cDate date = alKind == 0 ? cPlatform::FileModifiedDate(W(apPath, alLen)) : cPlatform::FileCreationDate(W(apPath, alLen));
	memcpy(apOut, &date, sizeof(date));
}

size_t ohpl_a9h_plat_folder(int alOp, const uint32_t *apPath, size_t alLen)
{
	tWString sPath = W(apPath, alLen);
	switch(alOp)
	{
	case 0: return cPlatform::FolderExists(sPath);
	case 1: return cPlatform::CreateFolder(sPath);
	case 2: return cPlatform::RemoveFolder(sPath, true, true);
	}
	return 0;
}

size_t ohpl_a9h_cfg(int alOp, const uint8_t *apSection, size_t alSecLen, const uint8_t *apKey, size_t alKeyLen, float *apOut)
{
	tString sSection((const char*)apSection, alSecLen), sKey((const char*)apKey, alKeyLen);
	if(alOp == 0)
	{
		apOut[0] = gpBase->mpMenuCfg->GetFloat(sSection, sKey, 0);
	}
	else
	{
		cVector2f v = gpBase->mpMenuCfg->GetVector2f(sSection, sKey, 0);
		apOut[0] = v.x; apOut[1] = v.y;
	}
	return 1;
}

void ohpl_a9h_tr(const uint8_t *apCat, size_t alCatLen, const uint8_t *apKey, size_t alKeyLen, OhplA9Sink apSink, void *apCtx)
{
	Emit(apSink, apCtx, gpBase->mpEngine->GetResources()->Translate(tString((const char*)apCat, alCatLen), tString((const char*)apKey, alKeyLen)));
}

OhplA9W ohpl_a9h_story_new(const uint32_t *apPath, size_t alLen)
{
	cLuxCustomStorySettings *pStory = hplNew(cLuxCustomStorySettings, ());
	if(pStory->CreateFromPath(W(apPath, alLen))) return pStory;
	hplDelete(pStory);
	return NULL;
}

void ohpl_a9h_story_str(OhplA9W apStory, int alOp, OhplA9Sink apSink, void *apCtx)
{
	cLuxCustomStorySettings *pStory = (cLuxCustomStorySettings*)apStory;
	switch(alOp)
	{
	case 0: Emit(apSink, apCtx, pStory->msName); break;
	case 1: Emit(apSink, apCtx, pStory->msAuthor); break;
	case 2: EmitBytes(apSink, apCtx, pStory->msImgFile); break;
	}
}

void ohpl_a9h_story_activate(OhplA9W apStory) { ((cLuxCustomStorySettings*)apStory)->SetActive(); }
void ohpl_a9h_story_free(OhplA9W apStory) { hplDelete((cLuxCustomStorySettings*)apStory); }

}

const OhplA9Host gHost = {
	&ohpl_a9h_new,
	&ohpl_a9h_popup,
	&ohpl_a9h_set,
	&ohpl_a9h_w,
	&ohpl_a9h_w_set_f,
	&ohpl_a9h_w_get_f,
	&ohpl_a9h_w_str,
	&ohpl_a9h_w_get_str,
	&ohpl_a9h_w_cb,
	&ohpl_a9h_w_nav,
	&ohpl_a9h_text_width,
	&ohpl_a9h_image_file,
	&ohpl_a9h_lux,
	&ohpl_a9h_lux_str,
	&ohpl_a9h_lux_with,
	&ohpl_a9h_lux_with_str,
	&ohpl_a9h_plat_list,
	&ohpl_a9h_plat_date,
	&ohpl_a9h_plat_folder,
	&ohpl_a9h_cfg,
	&ohpl_a9h_tr,
	&ohpl_a9h_story_new,
	&ohpl_a9h_story_str,
	&ohpl_a9h_story_activate,
	&ohpl_a9h_story_free
};

void OhplA9RegisterHost() { ohpl_a9_set_host(&gHost); }
