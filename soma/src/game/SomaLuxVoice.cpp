#include "SomaLuxVoice.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

#include <fstream>
#include <sstream>

cSomaLuxVoiceHandler *cSomaLuxVoiceHandler::mpInstance = NULL;

void SomaMapScriptCall(const tString &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && pMap->GetScript() && cSomaScriptRuntime::Get())
		cSomaScriptRuntime::Get()->Call(pMap->GetScript(), asDecl, aSetArgs);
}

// .voice and some .lang files are UTF-16 with a BOM
static bool ReadTextFile(const tWString &asPath, tString &asOut)
{
	std::ifstream file(cString::To8Char(asPath).c_str(), std::ios::binary);
	if (!file)
		return false;
	std::stringstream ss;
	ss << file.rdbuf();
	std::string s = ss.str();
	if (s.size() >= 2 && (unsigned char)s[0] == 0xff && (unsigned char)s[1] == 0xfe)
	{
		tWString w;
		for (size_t i = 2; i + 1 < s.size(); i += 2)
			w += (wchar_t)((unsigned char)s[i] | ((unsigned char)s[i + 1] << 8));
		asOut = cString::To8Char(w);
	}
	else if (s.size() >= 3 && (unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
		asOut = s.substr(3);
	else
		asOut = s;
	size_t lEnc = asOut.find("encoding=\"utf-16\"");
	if (lEnc != tString::npos)
		asOut.replace(lEnc, 17, "encoding=\"utf-8\"");
	return true;
}

static iXmlDocument *ParseXml(cEngine *apEngine, const tString &asFile)
{
	tWString sPath = apEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile);
	tString sData;
	if (sPath == _W("") || ReadTextFile(sPath, sData) == false)
		return NULL;
	iXmlDocument *pDoc = apEngine->GetResources()->GetLowLevel()->CreateXmlDocument();
	if (pDoc->CreateFromString(sData) == false)
	{
		hplDelete(pDoc);
		return NULL;
	}
	return pDoc;
}

static std::vector<cXmlElement *> Children(iXmlNode *apNode, const tString &asName)
{
	std::vector<cXmlElement *> v;
	if (apNode == NULL)
		return v;
	cXmlNodeListIterator it = apNode->GetChildIterator();
	while (it.HasNext())
		if (cXmlElement *p = it.Next()->ToElement())
			if (asName.empty() || p->GetValue() == asName)
				v.push_back(p);
	return v;
}

//---------------------------------------

cSomaLuxVoiceHandler::cSomaLuxVoiceHandler(cEngine *apEngine) : iUpdateable("SomaLuxVoice"), mpEngine(apEngine)
{
	mpInstance = this;
	cGui *pGui = mpEngine->GetGui();
	mpGuiSet = pGui->CreateSet("SomaVoiceSubtitles", pGui->CreateSkin("gui_default.skin"));
	cViewport *pViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	pViewport->AddGuiSet(mpGuiSet);
	mpFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");
	mpFadeGfx = pGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	LoadVoiceFile("global_voice.voice", "global");
	LoadLangFile("global_voice.lang");
}

cSomaLuxVoiceHandler::~cSomaLuxVoiceHandler()
{
	if (mpInstance == this)
		mpInstance = NULL;
}

void cSomaLuxVoiceHandler::LoadLangFile(const tString &asFile)
{
	iXmlDocument *pDoc = ParseXml(mpEngine, asFile);
	if (pDoc == NULL)
		return;
	for (cXmlElement *pCat : Children(pDoc, "CATEGORY"))
		if (cString::ToLowerCase(pCat->GetAttributeString("Name", "")).find("voices_") == 0)
			for (cXmlElement *pEntry : Children(pCat, "Entry"))
				mmapText[cString::ToLowerCase(pEntry->GetAttributeString("Name", ""))] = pEntry->GetAttributeString("_Text", "");
	hplDelete(pDoc);
}

bool cSomaLuxVoiceHandler::LoadVoiceFile(const tString &asFile, const tString &asSet)
{
	iXmlDocument *pDoc = ParseXml(mpEngine, asFile);
	if (pDoc == NULL)
	{
		Warning("SOMA voice: could not load '%s'\n", asFile.c_str());
		return false;
	}
	std::map<int, cLine> mapChars;
	for (cXmlElement *p : Children(pDoc->GetFirstElement("Characters"), "Character"))
	{
		cLine &c = mapChars[p->GetAttributeInt("ID", -1)];
		c.msCharacter = p->GetAttributeString("Name", "");
		c.msDisplayName = p->GetAttributeString("DisplayName", c.msCharacter);
		c.mfCharVolume = p->GetAttributeFloat("Volume", 1);
	}
	std::map<int, tString> mapScenes;
	for (cXmlElement *p : Children(pDoc->GetFirstElement("Scenes"), "Scene"))
		mapScenes[p->GetAttributeInt("ID", -1)] = p->GetAttributeString("Name", "");

	int lCount = 0;
	for (cXmlElement *pSubj : Children(pDoc->GetFirstElement("Subjects"), "Subject"))
	{
		std::unique_ptr<cSubject> pSubject(new cSubject());
		pSubject->msName = pSubj->GetAttributeString("Name", "");
		pSubject->msScene = mapScenes[pSubj->GetAttributeInt("SceneId", -1)];
		pSubject->msSet = asSet;
		pSubject->mbSingleRandomLine = pSubj->GetAttributeBool("UseSingleRandomLine", false);
		for (cXmlElement *pLine : Children(pSubj, "Line"))
		{
			cLine line = mapChars[pLine->GetAttributeInt("CharacterId", -1)];
			for (cXmlElement *pSound : Children(pLine, "Sound"))
			{
				cSound sound;
				sound.msText = pSound->GetAttributeString("Text", "");
				sound.mfVoiceOffset = pSound->GetAttributeFloat("VoiceOffset", 0);
				sound.mfTextOffset = pSound->GetAttributeFloat("TextOffset", 0);
				sound.mfEndPadding = pSound->GetAttributeFloat("EndPadding", 0);
				sound.mfVolume = pSound->GetAttributeFloat("Volume", 1);
				line.mvSounds.push_back(sound);
			}
			pSubject->mvLines.push_back(line);
		}
		mmapSubjects[pSubject->msName] = std::move(pSubject);
		++lCount;
	}
	hplDelete(pDoc);
	Log("SOMA voice: %d subjects from '%s'\n", lCount, asFile.c_str());
	return true;
}

void cSomaLuxVoiceHandler::LoadMapFile(const tString &asHpmPath, const tString &asMapName)
{
	for (const tString &s : mvLoadedSets)
		if (s == asMapName)
			return;
	mvLoadedSets.push_back(asMapName);
	LoadVoiceFile(asMapName + ".voice", asMapName);
	LoadLangFile(asMapName + ".lang");
}

void cSomaLuxVoiceHandler::Reset()
{
	StopAll();
}

tString cSomaLuxVoiceHandler::SoundKey(cSubject *apSubject, size_t alLine, size_t alSound)
{
	char vNum[32];
	snprintf(vNum, sizeof(vNum), "_%03d_", (int)alLine + 1);
	tString sKey = apSubject->msScene + "_" + apSubject->msName + vNum + apSubject->mvLines[alLine].msCharacter;
	snprintf(vNum, sizeof(vNum), "_%03d", (int)alSound + 1);
	return sKey + vNum;
}

bool cSomaLuxVoiceHandler::Play(const tString &asSubject, int alLine, const tString &asCallback, int alPrio, const std::function<void()> &aOnDone)
{
	auto it = mmapSubjects.find(asSubject);
	if (it == mmapSubjects.end())
	{
		Error("Voice subject '%s' could not be found in voicehandler\n", asSubject.c_str());
		return false;
	}
	cSubject *pSubject = it->second.get();
	Stop(pSubject->msScene);

	cPlaying p;
	p.mpSubject = pSubject;
	p.msCallback = asCallback;
	p.mOnDone = aOnDone;
	if (alLine >= 0 && alLine < (int)pSubject->mvLines.size())
		p.mvLines.push_back(alLine);
	else if (pSubject->mbSingleRandomLine && pSubject->mvLines.empty() == false)
		p.mvLines.push_back(cMath::RandRectl(0, (int)pSubject->mvLines.size() - 1));
	else
		for (size_t i = 0; i < pSubject->mvLines.size(); ++i)
			p.mvLines.push_back((int)i);
	mvPlaying.push_back(p);
	return true;
}

void cSomaLuxVoiceHandler::StartSound(cPlaying &aP)
{
	cSubject *pSubject = aP.mpSubject;
	const cLine &line = pSubject->mvLines[aP.mvLines[aP.mlLine]];
	const cSound &sound = line.mvSounds[aP.mlSound];
	tString sKey = SoundKey(pSubject, aP.mvLines[aP.mlLine], aP.mlSound);
	auto itText = mmapText.find(cString::ToLowerCase(sKey));
	tString sText = itText != mmapText.end() && itText->second != "" ? itText->second : sound.msText;
	aP.msSubtitle = sText.empty() ? "" : line.msDisplayName + ": " + sText;

	tString sFile = "voices/" + pSubject->msSet + "/" + sKey + ".ogg";
	aP.mpEntry = NULL;
	if (mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sFile) != _W(""))
		aP.mpEntry = mpEngine->GetSound()->GetSoundHandler()->PlayGuiStream(sFile, false, sound.mfVolume * line.mfCharVolume);
	aP.mlEntryId = aP.mpEntry ? aP.mpEntry->GetId() : -1;
	Log("SOMA voice: %s%s\n", sKey.c_str(), aP.mpEntry ? "" : " (no audio)");
	// Missing audio still shows its subtitle for a reading time
	aP.mfFallback = 1.0f + 0.06f * (float)sText.size();
}

void cSomaLuxVoiceHandler::StopSound(cPlaying &aP)
{
	cSoundHandler *pHandler = mpEngine->GetSound()->GetSoundHandler();
	if (aP.mpEntry && pHandler->IsValid(aP.mpEntry, aP.mlEntryId))
		aP.mpEntry->Stop();
	aP.mpEntry = NULL;
}

void cSomaLuxVoiceHandler::Finish(size_t alIdx)
{
	cPlaying p = mvPlaying[alIdx];
	mvPlaying.erase(mvPlaying.begin() + alIdx);
	if (p.msCallback != "")
		SomaMapScriptCall("void " + p.msCallback + "(const tString&in, const tString&in)", [&](asIScriptContext *c) {
			c->SetArgObject(0, &p.mpSubject->msScene);
			c->SetArgObject(1, &p.mpSubject->msName);
		});
	if (p.mOnDone)
		p.mOnDone();
}

void cSomaLuxVoiceHandler::FadeTo(float afGoal, float afTime)
{
	mfFadeGoal = afGoal;
	mfFadeSpeed = afTime > 0 ? 1.0f / afTime : 0;
	if (afTime <= 0)
		mfFadeAlpha = afGoal;
}

void cSomaLuxVoiceHandler::UpdateVoices(float afTimeStep)
{
	if (mfFadeAlpha < mfFadeGoal)
		mfFadeAlpha = std::min(mfFadeAlpha + mfFadeSpeed * afTimeStep, mfFadeGoal);
	else if (mfFadeAlpha > mfFadeGoal)
		mfFadeAlpha = std::max(mfFadeAlpha - mfFadeSpeed * afTimeStep, mfFadeGoal);

	cSoundHandler *pHandler = mpEngine->GetSound()->GetSoundHandler();
	for (size_t i = 0; i < mvPlaying.size();)
	{
		cPlaying &p = mvPlaying[i];
		if (p.mbPaused)
		{
			++i;
			continue;
		}
		if (p.mlLine >= p.mvLines.size())
		{
			Finish(i);
			continue;
		}
		const cLine &line = p.mpSubject->mvLines[p.mvLines[p.mlLine]];
		if (p.mlSound >= line.mvSounds.size())
		{
			++p.mlLine;
			p.mlSound = 0;
			p.mlStep = 0;
			p.mfTime = 0;
			continue;
		}
		const cSound &sound = line.mvSounds[p.mlSound];
		p.mfTime += afTimeStep;
		if (p.mlStep == 0 && p.mfTime >= sound.mfVoiceOffset)
		{
			StartSound(p);
			p.mlStep = 1;
			p.mfTime = 0;
		}
		else if (p.mlStep == 1)
		{
			bool bPlaying = p.mpEntry ? pHandler->IsValid(p.mpEntry, p.mlEntryId) : p.mfTime < p.mfFallback;
			if (bPlaying == false)
			{
				p.mpEntry = NULL;
				p.mlStep = 2;
				p.mfTime = 0;
			}
		}
		else if (p.mlStep == 2 && p.mfTime >= std::max(sound.mfEndPadding, 0.0f))
		{
			p.msSubtitle = "";
			++p.mlSound;
			p.mlStep = 0;
			p.mfTime = 0;
		}
		++i;
	}
}

void cSomaLuxVoiceHandler::OnDraw(float afFrameTime)
{
	if (mfFadeAlpha > 0 && cSomaLuxMap::GetCurrent())
		mpGuiSet->DrawGfx(mpFadeGfx, cVector3f(0, 0, 1), mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat(), cColor(0, mfFadeAlpha));
	if (mpFont == NULL || (gpSomaBase && gpSomaBase->GetConfig()->mbShowSubtitles == false))
		return;
	cVector2f vScreen = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	// game.cfg <Voice FontSize="26" TextY="700" MaxTextWidth="860"> on a 768 line canvas
	float fScale = vScreen.y / 768.0f;
	float fY = 700 * fScale;
	cVector2f vSize(26 * fScale, 26 * fScale);
	for (auto it = mvPlaying.rbegin(); it != mvPlaying.rend(); ++it)
	{
		if (it->msSubtitle.empty() || it->mlStep == 0)
			continue;
		tWStringVec vRows;
		mpFont->GetWordWrapRows(860 * fScale, vSize.y, vSize, cString::To16Char(it->msSubtitle), &vRows);
		fY -= vSize.y * (float)(vRows.size() - 1);
		for (size_t r = 0; r < vRows.size(); ++r)
		{
			cVector3f vPos(vScreen.x * 0.5f, fY + vSize.y * (float)r, 5);
			mpGuiSet->DrawFont(vRows[r], mpFont, vPos + cVector3f(fScale, fScale, -0.1f), vSize, cColor(0, 1), eFontAlign_Center);
			mpGuiSet->DrawFont(vRows[r], mpFont, vPos, vSize, cColor(1, 1), eFontAlign_Center);
		}
		fY -= vSize.y * 1.3f;
	}
}

bool cSomaLuxVoiceHandler::SubjectExists(const tString &asSubject)
{
	return mmapSubjects.count(asSubject) > 0;
}

void cSomaLuxVoiceHandler::Stop(const tString &asScene)
{
	for (size_t i = 0; i < mvPlaying.size();)
		if (mvPlaying[i].mpSubject->msScene == asScene)
		{
			StopSound(mvPlaying[i]);
			mvPlaying.erase(mvPlaying.begin() + i);
		}
		else
			++i;
}

void cSomaLuxVoiceHandler::StopAll()
{
	for (cPlaying &p : mvPlaying)
		StopSound(p);
	mvPlaying.clear();
}

void cSomaLuxVoiceHandler::SkipCurrentLine(const tString &asScene)
{
	for (cPlaying &p : mvPlaying)
		if (p.mpSubject->msScene == asScene)
		{
			StopSound(p);
			p.msSubtitle = "";
			++p.mlLine;
			p.mlSound = 0;
			p.mlStep = 0;
			p.mfTime = 0;
		}
}

bool cSomaLuxVoiceHandler::CharacterIsSpeaking(const tString &asName)
{
	for (cPlaying &p : mvPlaying)
		if (p.mlLine < p.mvLines.size() && p.mlStep == 1 && p.mpSubject->mvLines[p.mvLines[p.mlLine]].msCharacter == asName)
			return true;
	return false;
}

bool cSomaLuxVoiceHandler::SubjectIsPlaying(const tString &asName)
{
	for (cPlaying &p : mvPlaying)
		if (p.mpSubject->msName == asName)
			return true;
	return false;
}

bool cSomaLuxVoiceHandler::SceneIsActive(const tString &asScene)
{
	for (cPlaying &p : mvPlaying)
		if (p.mpSubject->msScene == asScene)
			return true;
	return false;
}

bool cSomaLuxVoiceHandler::SceneInvolvingCharacterIsActive(const tString &asName)
{
	for (cPlaying &p : mvPlaying)
		for (const cLine &l : p.mpSubject->mvLines)
			if (l.msCharacter == asName)
				return true;
	return false;
}

int cSomaLuxVoiceHandler::GetSubjectLineNumber(const tString &asSubject)
{
	auto it = mmapSubjects.find(asSubject);
	return it == mmapSubjects.end() ? 0 : (int)it->second->mvLines.size();
}

const tString &cSomaLuxVoiceHandler::GetSubjectSceneName(const tString &asSubject)
{
	static tString sEmpty;
	auto it = mmapSubjects.find(asSubject);
	return it == mmapSubjects.end() ? sEmpty : it->second->msScene;
}

void cSomaLuxVoiceHandler::SetPaused(const tString &asScene, bool abX)
{
	for (cPlaying &p : mvPlaying)
		if (p.mpSubject->msScene == asScene)
			p.mbPaused = abX;
}

void cSomaLuxVoiceHandler::SetPausedAll(bool abX)
{
	for (cPlaying &p : mvPlaying)
		p.mbPaused = abX;
}

//---------------------------------------

cSomaLuxDialogHandler *cSomaLuxDialogHandler::Get()
{
	static cSomaLuxDialogHandler handler;
	return &handler;
}

void cSomaLuxDialogHandler::Begin(const tString &asName)
{
	mBuilding = cDialog();
	mBuilding.msName = asName;
	mBuilding.msCallback = msPendingCallback;
	msPendingCallback = "";
}

void cSomaLuxDialogHandler::AddBranch(const tString &asName, const tString &asNext)
{
	cBranch b;
	b.msName = asName;
	b.msNext = asNext;
	mBuilding.mvBranches.push_back(b);
}

void cSomaLuxDialogHandler::AddSubject(const tString &asSubject, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty())
		AddBranch("", "");
	mBuilding.mvBranches.back().mvItems.push_back(cItem{asSubject, -1, asCallback});
}

void cSomaLuxDialogHandler::AddPause(float afTime, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty())
		AddBranch("", "");
	mBuilding.mvBranches.back().mvItems.push_back(cItem{"", afTime, asCallback});
}

void cSomaLuxDialogHandler::AddResponseOption(const tString &asEntry, const tString &asBranch, int alId, const tString &asCallback)
{
	if (mBuilding.mvBranches.empty() == false)
		mBuilding.mvBranches.back().mvOptions.push_back(cOption{asEntry, asBranch, alId, asCallback});
}

cSomaLuxDialogHandler::cBranch *cSomaLuxDialogHandler::FindBranch(cDialog &aD, const tString &asName, int *apIdx)
{
	for (size_t i = 0; i < aD.mvBranches.size(); ++i)
		if (aD.mvBranches[i].msName == asName)
		{
			*apIdx = (int)i;
			return &aD.mvBranches[i];
		}
	return NULL;
}

void cSomaLuxDialogHandler::End(const tString &asStartBranch)
{
	std::shared_ptr<cDialog> pD(new cDialog(mBuilding));
	mBuilding = cDialog();
	if (pD->mvBranches.empty())
		return;
	pD->mlBranch = 0;
	if (asStartBranch != "")
		FindBranch(*pD, asStartBranch, &pD->mlBranch);
	pD->mlItem = -1;
	mvActive.push_back(pD);
	NextItem(*pD);
}

static void ItemCallback(const tString &asFunc, const tString &asSubject, bool abStart)
{
	if (asFunc == "")
		return;
	SomaMapScriptCall("void " + asFunc + "(const tString&in, bool)", [&](asIScriptContext *c) {
		c->SetArgObject(0, (void *)&asSubject);
		c->SetArgByte(1, abStart);
	});
}

void cSomaLuxDialogHandler::StartItem(cDialog &aD)
{
	cItem &item = aD.mvBranches[aD.mlBranch].mvItems[aD.mlItem];
	ItemCallback(item.msCallback, item.msSubject, true);
	if (item.mfPause >= 0)
	{
		aD.mfPauseLeft = item.mfPause;
		aD.mbWaiting = false;
		return;
	}
	aD.mbWaiting = true;
	std::weak_ptr<cDialog> wD;
	for (auto &p : mvActive)
		if (p.get() == &aD)
			wD = p;
	bool bOk = cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(item.msSubject, -1, "", 0, [this, wD]() {
		if (auto p = wD.lock())
			p->mbWaiting = false;
	});
	if (bOk == false)
		aD.mbWaiting = false;
}

void cSomaLuxDialogHandler::NextItem(cDialog &aD)
{
	while (aD.mbDone == false)
	{
		cBranch &b = aD.mvBranches[aD.mlBranch];
		if (aD.mlItem >= 0 && aD.mlItem < (int)b.mvItems.size())
		{
			cItem &prev = b.mvItems[aD.mlItem];
			ItemCallback(prev.msCallback, prev.msSubject, false);
		}
		++aD.mlItem;
		if (aD.mlItem < (int)b.mvItems.size())
		{
			StartItem(aD);
			return;
		}
		EndBranch(aD);
		if (aD.mbDone)
			return;
	}
}

void cSomaLuxDialogHandler::EndBranch(cDialog &aD)
{
	cBranch &b = aD.mvBranches[aD.mlBranch];
	tString sNext = b.msNext;
	if (b.mvOptions.empty() == false)
	{
		// No dialog UI yet: take the first option
		const cOption &opt = b.mvOptions[0];
		Log("SOMA dialog: auto-selected option '%s'\n", opt.msEntry.c_str());
		if (opt.msCallback != "")
			SomaMapScriptCall("void " + opt.msCallback + "(const tString&in, const tString&in, int)", [&](asIScriptContext *c) {
				c->SetArgObject(0, &b.msName);
				c->SetArgObject(1, (void *)&opt.msEntry);
				c->SetArgDWord(2, opt.mlId);
			});
		sNext = opt.msBranch;
	}
	int lIdx = -1;
	if (sNext != "" && FindBranch(aD, sNext, &lIdx))
	{
		aD.mlBranch = lIdx;
		aD.mlItem = -1;
		return;
	}
	aD.mbDone = true;
	if (aD.msCallback != "")
		SomaMapScriptCall("void " + aD.msCallback + "(const tString&in)", [&](asIScriptContext *c) { c->SetArgObject(0, &aD.msName); });
}

void cSomaLuxDialogHandler::Update(float afTimeStep)
{
	std::vector<std::shared_ptr<cDialog>> vActive = mvActive;
	for (auto &p : vActive)
	{
		if (p->mbDone)
			continue;
		if (p->mfPauseLeft > 0)
		{
			p->mfPauseLeft -= afTimeStep;
			if (p->mfPauseLeft <= 0)
				NextItem(*p);
		}
		else if (p->mbWaiting == false)
			NextItem(*p);
	}
	for (size_t i = 0; i < mvActive.size();)
		if (mvActive[i]->mbDone)
			mvActive.erase(mvActive.begin() + i);
		else
			++i;
}

void cSomaLuxDialogHandler::Stop(const tString &asName)
{
	for (auto &p : mvActive)
		if (p->msName == asName)
			p->mbDone = true;
}

void cSomaLuxDialogHandler::StopAll()
{
	for (auto &p : mvActive)
		p->mbDone = true;
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->StopAll();
}

bool cSomaLuxDialogHandler::CharacterIsActive(const tString &asName)
{
	return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneInvolvingCharacterIsActive(asName);
}

//---------------------------------------
// Natives: both handlers are singletons, any non-null pointer identifies them

static int gVoiceTag, gDialogTag;

void SomaRegisterFadeNatives(asIScriptEngine *e)
{
	const char *T = "cLuxEffectHandler";
	SOMA_METHOD(e, T, "void FadeIn(float afTime)", +[](void *, float t) { if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()->FadeTo(0, t); });
	SOMA_METHOD(e, T, "void FadeOut(float afTime)", +[](void *, float t) { if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()->FadeTo(1, t); });
	SOMA_METHOD(e, T, "bool IsFading()", +[](void *) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->IsFading(); });
	SOMA_METHOD(e, T, "float GetFadeAlpha()", +[](void *) { return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetFadeAlpha() : 0.0f; });
}

void cSomaLuxVoiceHandler::RegisterNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	auto V = []() { return cSomaLuxVoiceHandler::Get(); };
	(void)V;
	SOMA_FUNC(e, "cLuxVoiceHandler@ cLux_GetVoiceHandler()", +[]() { return (void *)&gVoiceTag; });
	SOMA_FUNC(e, "cLuxDialogHandler@ cLux_GetDialogHandler()", +[]() { return (void *)&gDialogTag; });
	SomaRegisterFadeNatives(e);
	const char *T = "cLuxVoiceHandler";
#define VH if (cSomaLuxVoiceHandler::Get()) cSomaLuxVoiceHandler::Get()
	SOMA_METHOD(e, T, "bool Play(const tString &in asSubject, int alSpecificLine, const tString &in asCallback, int alPrio)",
				+[](void *, S s, int l, S cb, int prio) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(s, l, cb, prio); });
	SOMA_METHOD(e, T, "int GetSubjectLineNumber(const tString &in asSubject)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectLineNumber(s) : 0; });
	SOMA_METHOD(e, T, "const tString& GetSubjectSceneName(const tString &in asSubject)", +[](void *, S s) -> const tString & {
		static tString sEmpty;
		return cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectSceneName(s) : sEmpty;
	});
	SOMA_METHOD(e, T, "bool SubjectExists(const tString &in asSubject)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SubjectExists(s); });
	SOMA_METHOD(e, T, "void StopAll()", +[](void *) { VH->StopAll(); });
	SOMA_METHOD(e, T, "void Stop(const tString&in asScene)", +[](void *, S s) { VH->Stop(s); });
	SOMA_METHOD(e, T, "void SkipCurrentLine(const tString&in asScene)", +[](void *, S s) { VH->SkipCurrentLine(s); });
	SOMA_METHOD(e, T, "void SkipCurrentSound(const tString&in asScene)", +[](void *, S s) { VH->SkipCurrentLine(s); });
	SOMA_METHOD(e, T, "void SetPaused(const tString&in asScene, bool abX)", +[](void *, S s, bool b) { VH->SetPaused(s, b); });
	SOMA_METHOD(e, T, "void SetPausedAll(bool abX)", +[](void *, bool b) { VH->SetPausedAll(b); });
	SOMA_METHOD(e, T, "bool CharacterIsSpeaking(const tString&in asName)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->CharacterIsSpeaking(s); });
	SOMA_METHOD(e, T, "bool SubjectIsPlaying(const tString&in asName)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SubjectIsPlaying(s); });
	SOMA_METHOD(e, T, "bool SceneIsActive(const tString&in asScene)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneIsActive(s); });
	SOMA_METHOD(e, T, "bool SceneInvolvingCharacterIsActive(const tString&in asCharacter)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneInvolvingCharacterIsActive(s); });
	SOMA_METHOD(e, T, "bool AnySceneIsActive()", +[](void *) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->AnySceneIsActive(); });
#undef VH
}

void cSomaLuxDialogHandler::RegisterNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	typedef cSomaLuxDialogHandler D;
	const char *T = "cLuxDialogHandler";
	SOMA_METHOD(e, T, "void Begin(const tString &in asName)", +[](void *, S s) { D::Get()->Begin(s); });
	SOMA_METHOD(e, T, "void End(const tString&in asStartBranch)", +[](void *, S s) { D::Get()->End(s); });
	SOMA_METHOD(e, T, "void SetCallbackFunc(const tString&in asFunc)", +[](void *, S s) { D::Get()->SetCallbackFunc(s); });
	SOMA_METHOD(e, T, "void AddBranch(const tString&in asName, const tString&in asNextBranch)", +[](void *, S n, S next) { D::Get()->AddBranch(n, next); });
	SOMA_METHOD(e, T, "void AddBranchSubject(const tString&in asSubject, const tString&in asCallback)", +[](void *, S s, S cb) { D::Get()->AddSubject(s, cb); });
	SOMA_METHOD(e, T, "void AddBranchPause(float afTime, const tString&in asCallback)", +[](void *, float t, S cb) { D::Get()->AddPause(t, cb); });
	SOMA_METHOD(e, T, "void AddResponseOption(const tString &in asEntry, const tString&in asBranch,int alId, const tString&in asCallback)",
				+[](void *, S entry, S branch, int id, S cb) { D::Get()->AddResponseOption(entry, branch, id, cb); });
	SOMA_METHOD(e, T, "bool CharacterIsActive(const tString&in asName)", +[](void *, S s) { return D::Get()->CharacterIsActive(s); });
	SOMA_METHOD(e, T, "void Stop(const tString &in asName)", +[](void *, S s) { D::Get()->Stop(s); });
	SOMA_METHOD(e, T, "void StopAll()", +[](void *) { D::Get()->StopAll(); });
	SOMA_METHOD(e, T, "void SetVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] = x; });
	SOMA_METHOD(e, T, "void IncVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] += x; });
	SOMA_METHOD(e, T, "int GetVar(const tString&in asName)", +[](void *, S s) { return D::Get()->mmapVars[s]; });
}
