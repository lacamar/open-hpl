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
	void Emit(OhplA9Sink apSink, void *apCtx, const tWString &asText) { apSink(apCtx, (const uint32_t*)asText.data(), asText.size()); }
	void EmitBytes(OhplA9Sink apSink, void *apCtx, const tString &asText)
	{
		std::vector<uint32_t> vText(asText.size());
		for(size_t i = 0; i < asText.size(); ++i) vText[i] = (unsigned char)asText[i];
		apSink(apCtx, vText.data(), vText.size());
	}

}

//-----------------------------------------------------------------------

namespace
{

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
