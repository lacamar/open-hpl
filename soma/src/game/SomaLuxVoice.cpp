#include "SomaLuxVoice.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaImGui.h"
#include "SomaLuxEntity.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "SomaSound.h"

#include "impl/scriptarray.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vorbis/vorbisfile.h>
#include <sstream>

cSomaImGui *SomaHudImGui();

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
		tString sType = p->GetAttributeString("EntryType", "GUIWorld");
		c.mlEntryType = sType == "World" ? 1 : sType == "WorldClean" ? 2 : sType == "GUI" ? 4 : 8;
		c.msSource = p->GetAttributeString("EntitySource", "");
		c.mfMinDist = p->GetAttributeFloat("MinDist", 0);
		c.mfMaxDist = p->GetAttributeFloat("MaxDist", 0);
		c.mbWorldSpace = p->GetAttributeBool("WorldSpace", false);
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
			line.msCallback = pLine->GetAttributeString("Callback", "");
			line.mlPrio = pLine->GetAttributeInt("Prio", 0);
			line.mbPlayOnce = pLine->GetAttributeBool("PlayOnce", false);
			if (pLine->GetAttributeBool("ChangeSource", false))
			{
				line.msSource = pLine->GetAttributeString("EntitySource", "");
				line.mfMinDist = pLine->GetAttributeFloat("MinDist", 0);
				line.mfMaxDist = pLine->GetAttributeFloat("MaxDist", 0);
				line.mbWorldSpace = line.mbChangeSource = true;
			}
			for (cXmlElement *pSound : Children(pLine, "Sound"))
			{
				cSound sound;
				sound.msText = pSound->GetAttributeString("Text", "");
				sound.msFile = pSound->GetAttributeString("FileName", "");
				sound.mfVoiceOffset = pSound->GetAttributeFloat("VoiceOffset", 0);
				sound.mfEndPadding = pSound->GetAttributeFloat("EndPadding", 0);
				sound.mfVolume = pSound->GetAttributeFloat("Volume", 1);
				sound.msEffect = pSound->GetAttributeString("ExtraEffectFile", "");
				sound.mbEndsAfterEffect = pSound->GetAttributeBool("EndsAfterExtraEffect", false);
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
	mmapSources.clear();
	msetLipEntities.clear();
	mmapSceneVolumes.clear();
	mmapSpeakingCallbacks.clear();
	msetSpeaking.clear();
}

tString cSomaLuxVoiceHandler::SoundKey(cSubject *apSubject, size_t alLine, size_t alSound)
{
	const tString &sFile = apSubject->mvLines[alLine].mvSounds[alSound].msFile;
	if (sFile != "")
		return sFile;
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
	// cLuxVoiceSceneInstance::Play: a busy scene only yields to a higher prio
	for (const cPlaying &q : mvPlaying)
		if (q.mpSubject->msScene == pSubject->msScene && q.mlPrio >= alPrio)
			return false;
	Stop(pSubject->msScene);

	cPlaying p;
	p.mpSubject = pSubject;
	p.mlPrio = alPrio;
	p.msCallback = asCallback;
	p.mOnDone = aOnDone;
	const std::vector<cLine> &vLines = pSubject->mvLines;
	if (vLines.empty())
		return false;
	auto Key = [&](int i) { return pSubject->msName + "#" + cString::ToString(i); };
	auto Played = [&](int i) { return msetPlayedLines.count(Key(i)) > 0; };
	if (alLine >= 0)
	{
		if (alLine >= (int)vLines.size())
			return false;
		p.mvLines.push_back(alLine);
	}
	else if (pSubject->mbSingleRandomLine)
	{
		// highest Prio among unplayed lines, not the previous pick when there is a choice
		std::vector<int> vCand;
		for (int i = 0; i < (int)vLines.size(); ++i)
		{
			if (vLines[i].mbPlayOnce && Played(i))
				continue;
			if (vCand.empty() || vLines[i].mlPrio > vLines[vCand[0]].mlPrio)
				vCand = {i};
			else if (vLines[i].mlPrio == vLines[vCand[0]].mlPrio)
				vCand.push_back(i);
		}
		if (vCand.empty())
			return false;
		int r = cMath::RandRectl(0, (int)vCand.size() - 1);
		if (vCand.size() > 2 && vCand[r] == pSubject->mlLastLine)
			r = (r + 1) % (int)vCand.size();
		pSubject->mlLastLine = vCand[r];
		p.mvLines.push_back(vCand[r]);
	}
	else
	{
		p.mvLines.push_back(0);
		// the engine tests the first line's PlayOnce for every queued line
		for (int i = 1; i < (int)vLines.size(); ++i)
			if (vLines[0].mbPlayOnce == false || msetPlayedLines.insert(Key(i)).second)
				p.mvLines.push_back(i);
	}
	if (vLines[p.mvLines[0]].mbPlayOnce && msetPlayedLines.insert(Key(p.mvLines[0])).second == false)
		return false;
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
	aP.msSubtitle = sText.empty() || line.msDisplayName.empty() ? sText : line.msDisplayName + ": " + sText;

	tString sFile = "voices/" + pSubject->msSet + "/" + sKey + ".ogg";
	aP.msFile = sFile;
	aP.mpEntry = NULL;
	aP.msSourceEntity = "";
	float fVolume = sound.mfVolume * line.mfCharVolume * mmapSceneVolumes[pSubject->msScene].mfVolume;
	cSource src = {line.msSource, line.mfMinDist, line.mfMaxDist, line.mbWorldSpace && line.msSource != ""};
	auto itSource = mmapSources.find(line.msCharacter);
	if (itSource != mmapSources.end() && !line.mbChangeSource)
		src = itSource->second;
	cSomaLuxEntity *pSource = src.mbUse3D && cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(src.msEntity) : NULL;
	if (mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sFile) == _W(""))
		;
	else if (pSource)
	{
		aP.mpEntry = mpEngine->GetSound()->GetSoundHandler()->Play3D(sFile, false, fVolume, pSource->GetPosition(), src.mfMinDist, src.mfMaxDist,
																	  (eSoundEntryType)line.mlEntryType, false, 0, true);
		aP.msSourceEntity = src.msEntity;
	}
	else
		aP.mpEntry = mpEngine->GetSound()->GetSoundHandler()->PlayGuiStream(sFile, false, fVolume, cVector3f(0, 0, 1), (eSoundEntryType)line.mlEntryType);
	aP.mlEntryId = aP.mpEntry ? aP.mpEntry->GetId() : -1;
	aP.msLipEntity = itSource != mmapSources.end() && !line.mbChangeSource ? src.msEntity : line.msSource;
	aP.mpLipsync = aP.mpEntry && aP.msLipEntity != "" ? LoadLipsync("voices/" + pSubject->msSet + "/lipsync/" + sKey + ".anno") : nullptr;
	Log("SOMA voice: %s%s\n", sKey.c_str(), aP.mpEntry ? "" : " (no audio)");
	// Missing audio still shows its subtitle for a reading time
	aP.mfFallback = 0.5f + 0.09f * (float)sText.size();
}

// cLuxVoiceSceneInstance::FadeOutSoundsPlaying, game.cfg Voice*FadeOutSpeed = 2
void cSomaLuxVoiceHandler::StopSound(cPlaying &aP)
{
	cSoundHandler *pHandler = mpEngine->GetSound()->GetSoundHandler();
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	aP.mvOldVoices.push_back({aP.mpEntry, aP.mlEntryId});
	aP.mvOldEffects.push_back({aP.mpEffect, aP.mlEffectId});
	for (auto &v : aP.mvOldVoices)
		if (v.first && pHandler->IsValid(v.first, v.second))
			v.first->FadeOut(2);
	for (auto &e : aP.mvOldEffects)
		if (e.first && pEvents->IsLive(e.first, e.second))
			e.first->FadeOut(2);
	aP.mvOldVoices.clear();
	aP.mvOldEffects.clear();
	aP.mpEntry = NULL;
	aP.mpEffect = NULL;
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

void cSomaLuxVoiceHandler::LineCallback(cSubject *apSubject, int alLine, bool abStart)
{
	tString sFunc = apSubject->mvLines[alLine].msCallback;
	if (sFunc != "")
		SomaMapScriptCall("void " + sFunc + "(const tString&in, const tString&in, int, bool)", [&](asIScriptContext *c) {
			c->SetArgObject(0, &apSubject->msScene);
			c->SetArgObject(1, &apSubject->msName);
			c->SetArgDWord(2, alLine);
			c->SetArgByte(3, abStart);
		});
}

void cSomaLuxVoiceHandler::SetSource(const tString &asCharacter, const tString &asEntity, float afMinDist, float afMaxDist, bool abUse3D)
{
	mmapSources[asCharacter] = {asEntity, afMinDist, afMaxDist, abUse3D};
}

void cSomaLuxVoiceHandler::FadeSceneVolumeTo(const tString &asScene, float afVolume, float afTime)
{
	cSceneVolume &v = mmapSceneVolumes[asScene];
	v.mfGoal = afVolume;
	v.mfSpeed = afTime > 0 ? std::fabs(afVolume - v.mfVolume) / afTime : 0;
	if (afTime <= 0)
		v.mfVolume = afVolume;
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

	for (auto &it : mmapSceneVolumes)
	{
		cSceneVolume &v = it.second;
		if (v.mfVolume < v.mfGoal)
			v.mfVolume = std::min(v.mfVolume + v.mfSpeed * afTimeStep, v.mfGoal);
		else if (v.mfVolume > v.mfGoal)
			v.mfVolume = std::max(v.mfVolume - v.mfSpeed * afTimeStep, v.mfGoal);
	}
	cSoundHandler *pHandler = mpEngine->GetSound()->GetSoundHandler();
	cSomaSoundEvents *pEvents = cSomaSoundEvents::Get();
	for (cPlaying &p : mvPlaying)
	{
		auto itVol = mmapSceneVolumes.find(p.mpSubject->msScene);
		float fVol = itVol != mmapSceneVolumes.end() ? itVol->second.mfVolume : 1;
		p.mvOldVoices.erase(std::remove_if(p.mvOldVoices.begin(), p.mvOldVoices.end(), [pHandler](auto &v) { return pHandler->IsValid(v.first, v.second) == false; }),
							p.mvOldVoices.end());
		p.mvOldEffects.erase(std::remove_if(p.mvOldEffects.begin(), p.mvOldEffects.end(), [pEvents](auto &e) { return pEvents->IsLive(e.first, e.second) == false; }),
							 p.mvOldEffects.end());
		for (auto &v : p.mvOldVoices)
			v.first->SetVolumeMul(fVol);
		for (auto &e : p.mvOldEffects)
			e.first->SetVolumeMul(fVol);
		if (p.mpEffect && pEvents->IsLive(p.mpEffect, p.mlEffectId))
			p.mpEffect->SetVolumeMul(fVol);
		if (p.mpEntry == NULL || pHandler->IsValid(p.mpEntry, p.mlEntryId) == false)
			continue;
		p.mpEntry->SetVolumeMul(fVol);
		cSomaLuxEntity *pSource = p.msSourceEntity != "" && cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(p.msSourceEntity) : NULL;
		if (pSource)
			p.mpEntry->GetChannel()->SetPosition(pSource->GetPosition());
	}
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
			int lLine = p.mvLines[p.mlLine];
			cSubject *pSubject = p.mpSubject;
			++p.mlLine;
			p.mlSound = 0;
			p.mlStep = 0;
			p.mfTime = 0;
			LineCallback(pSubject, lLine, false);
			continue;
		}
		const cSound &sound = line.mvSounds[p.mlSound];
		// cLuxVoiceSceneInstance::PlaySound: ExtraEffectOffset is 0 throughout the data
		if (p.mlStep == 0 && p.mfTime == 0 && afTimeStep > 0 && sound.msEffect != "")
		{
			p.mvOldEffects.push_back({p.mpEffect, p.mlEffectId});
			p.mpEffect = pEvents->PlayGui(sound.msEffect, sound.mfVolume, line.mlEntryType, false, true);
			p.mlEffectId = p.mpEffect ? p.mpEffect->GetId() : -1;
			auto itVol = mmapSceneVolumes.find(p.mpSubject->msScene);
			if (p.mpEffect && itVol != mmapSceneVolumes.end())
				p.mpEffect->SetVolumeMul(itVol->second.mfVolume);
		}
		p.mfTime += afTimeStep;
		if (p.mlStep == 0 && p.mfTime >= sound.mfVoiceOffset)
		{
			StartSound(p);
			p.mlStep = 1;
			p.mfTime = 0;
			if (p.mlSound == 0)
			{
				LineCallback(p.mpSubject, p.mvLines[p.mlLine], true);
				++i;
				continue;
			}
		}
		else if (p.mlStep == 1)
		{
			// positive EndPadding hands over early and lets the sound play out; negative is a gap after it
			bool bPlaying;
			if (sound.mbEndsAfterEffect)
				bPlaying = p.mpEffect && pEvents->IsLive(p.mpEffect, p.mlEffectId) && p.mpEffect->IsPlaying() &&
						   p.mpEffect->GetTotalTime() - p.mpEffect->GetElapsedTime() >= sound.mfEndPadding;
			else if (p.mpEntry)
				bPlaying = pHandler->IsValid(p.mpEntry, p.mlEntryId) &&
						   p.mpEntry->GetChannel()->GetTotalTime() - p.mpEntry->GetChannel()->GetElapsedTime() >= sound.mfEndPadding;
			else
				bPlaying = p.mfTime < p.mfFallback;
			if (bPlaying == false)
			{
				p.mvOldVoices.push_back({p.mpEntry, p.mlEntryId});
				p.mpEntry = NULL;
				p.mlStep = 2;
				p.mfTime = 0;
			}
		}
		else if (p.mlStep == 2 && p.mfTime >= std::max(-sound.mfEndPadding, 0.0f))
		{
			p.msSubtitle = "";
			++p.mlSound;
			p.mlStep = 0;
			p.mfTime = 0;
		}
		++i;
	}
	UpdateLipsync();
	UpdateSpeakingCallbacks();
}

void cSomaLuxVoiceHandler::SetSpeakingCallback(const tString &asCharacter, const tString &asFunc)
{
	if (asFunc == "")
		mmapSpeakingCallbacks.erase(asCharacter);
	else
		mmapSpeakingCallbacks[asCharacter] = asFunc;
}

void cSomaLuxVoiceHandler::UpdateSpeakingCallbacks()
{
	std::set<tString> setNow;
	for (cPlaying &p : mvPlaying)
		if (p.mlLine < p.mvLines.size() && p.mlStep == 1)
			setNow.insert(p.mpSubject->mvLines[p.mvLines[p.mlLine]].msCharacter);
	std::vector<std::pair<tString, bool>> vChanged;
	for (const tString &s : setNow)
		if (msetSpeaking.count(s) == 0)
			vChanged.push_back({s, true});
	for (const tString &s : msetSpeaking)
		if (setNow.count(s) == 0)
			vChanged.push_back({s, false});
	msetSpeaking = setNow;
	for (auto &it : vChanged)
	{
		auto itCb = mmapSpeakingCallbacks.find(it.first);
		if (itCb == mmapSpeakingCallbacks.end())
			continue;
		tString sFunc = itCb->second;
		bool bKeep = true;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (pMap && pMap->GetScript() && cSomaScriptRuntime::Get())
			cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "bool " + sFunc + "(const tString&in, bool)", [&](asIScriptContext *c) {
				c->SetArgObject(0, &it.first);
				c->SetArgByte(1, it.second);
			}, [&](asIScriptContext *c) { bKeep = c->GetReturnByte() != 0; });
		auto itNow = mmapSpeakingCallbacks.find(it.first);
		if (bKeep == false && itNow != mmapSpeakingCallbacks.end() && itNow->second == sFunc)
			mmapSpeakingCallbacks.erase(itNow);
	}
}

void cSomaLuxVoiceHandler::OnDraw(float afFrameTime)
{
	// cLuxFadeEffect::OnDraw: on the HUD set at z 0.9, so HUD ImGui (credits) draws over it
	if (mfFadeAlpha > 0 && cSomaLuxMap::GetCurrent())
	{
		cGuiSet *pHud = SomaHudImGui()->GetSet();
		cVector2f vSize = pHud->GetVirtualSize();
		cVector2f vPos = pHud->GetVirtualSizeOffset() * -1.0f - vSize * 0.125f;
		pHud->DrawGfx(mpFadeGfx, cVector3f(vPos.x, vPos.y, 0.9f), vSize * 1.25f, cColor(0, mfFadeAlpha));
	}
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

// .anno (Annosoft): TEA-ECB over the first ceil8(size/2) bytes, then "ONNA", version, frames of
// {start ms, end ms, n, n x {char[3] phoneme, f32 weight}}
std::shared_ptr<std::vector<cSomaLuxVoiceHandler::cLipFrame>> cSomaLuxVoiceHandler::LoadLipsync(const tString &asFile)
{
	std::ifstream file(cString::To8Char(mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile)).c_str(), std::ios::binary);
	if (!file)
		return nullptr;
	std::string d((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	static const uint32_t k[4] = {0x8f812a90, 0x71823fad, 0x0317fab7, 0xc3856aa5};
	size_t lEnc = std::min((d.size() / 2 + 7) / 8 * 8, d.size() / 8 * 8);
	for (size_t i = 0; i < lEnc; i += 8)
	{
		uint32_t v[2];
		memcpy(v, &d[i], 8);
		for (uint32_t r = 0, sum = 0xc6ef3720; r < 32; ++r, sum -= 0x9e3779b9)
		{
			v[1] -= ((v[0] << 4) + k[2]) ^ (v[0] + sum) ^ ((v[0] >> 5) + k[3]);
			v[0] -= ((v[1] << 4) + k[0]) ^ (v[1] + sum) ^ ((v[1] >> 5) + k[1]);
		}
		memcpy(&d[i], v, 8);
	}
	int32_t h[3];
	if (d.size() < 12 || d.compare(0, 4, "ONNA") != 0)
		return nullptr;
	memcpy(h, d.data(), 12);
	// Soma_NoSteam.exe 0x140568440 + 0x140358170 (17-viseme set)
	static const std::map<std::string, int> mapViseme = {
		{"AA", 1}, {"AH", 2}, {"h", 2}, {"AO", 3}, {"AW", 4}, {"OW", 4}, {"OY", 5}, {"UH", 5}, {"UW", 5}, {"AE", 6}, {"EH", 6},
		{"AY", 7}, {"IH", 7}, {"EY", 8}, {"IY", 9}, {"y", 9}, {"ER", 10}, {"r", 10}, {"l", 11}, {"w", 12}, {"b", 13}, {"m", 13},
		{"p", 13}, {"DH", 14}, {"NG", 14}, {"TH", 14}, {"ZH", 14}, {"d", 14}, {"g", 14}, {"k", 14}, {"n", 14}, {"s", 14},
		{"t", 14}, {"z", 14}, {"CH", 15}, {"SH", 15}, {"j", 15}, {"f", 16}, {"v", 16}, {"x", 17}};
	auto pOut = std::make_shared<std::vector<cLipFrame>>();
	size_t p = 12;
	for (int f = 0; f < h[2] && p + 12 <= d.size(); ++f)
	{
		cLipFrame fr = {};
		int32_t n;
		memcpy(&fr.mlStart, &d[p], 4);
		memcpy(&fr.mlEnd, &d[p + 4], 4);
		memcpy(&n, &d[p + 8], 4);
		p += 12;
		for (int i = 0; i < n && p + 7 <= d.size(); ++i, p += 7)
		{
			float w;
			memcpy(&w, &d[p + 3], 4);
			auto it = mapViseme.find(std::string(&d[p], strnlen(&d[p], 3)));
			if (it != mapViseme.end())
				fr.mvW[it->second - 1] += w;
		}
		pOut->push_back(fr);
	}
	return pOut;
}

// Visemes_17_N anim states as an unnormalized layer, sampled at the voice's playback time
void cSomaLuxVoiceHandler::UpdateLipsync()
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	std::map<tString, tVisemes> mapWeights;
	for (cPlaying &p : mvPlaying)
	{
		if (p.mpLipsync == NULL || p.mpLipsync->empty() || p.mlStep != 1 || p.mpEntry == NULL ||
			mpEngine->GetSound()->GetSoundHandler()->IsValid(p.mpEntry, p.mlEntryId) == false)
			continue;
		const std::vector<cLipFrame> &v = *p.mpLipsync;
		float fMs = p.mpEntry->GetChannel()->GetElapsedTime() * 1000.0f + 30; // cActorAnimController starts its timer at 0.03
		auto it = std::upper_bound(v.begin(), v.end(), fMs, [](float t, const cLipFrame &f) { return t < f.mlStart; });
		const cLipFrame &a = it == v.begin() ? v.front() : *(it - 1);
		const cLipFrame &b = it == v.end() ? a : *it;
		float fT = b.mlStart > a.mlStart ? cMath::Clamp((fMs - a.mlStart) / (b.mlStart - a.mlStart), 0.0f, 1.0f) : 0;
		tVisemes &w = mapWeights[p.msLipEntity];
		for (size_t i = 0; i < w.size(); ++i)
			w[i] = cMath::Clamp(a.mvW[i] + (b.mvW[i] - a.mvW[i]) * fT, 0.0f, 2.0f);
	}
	std::set<tString> setAll = msetLipEntities;
	for (auto &it : mapWeights)
		setAll.insert(it.first);
	msetLipEntities.clear();
	for (const tString &sName : setAll)
	{
		cSomaLuxEntity *pEnt = pMap ? pMap->GetEntity(sName) : NULL;
		if (pEnt == NULL || pEnt->mpMesh == NULL)
			continue;
		auto itW = mapWeights.find(sName);
		for (int i = 0; i < 17; ++i)
		{
			cAnimationState *pState = pEnt->mpMesh->GetAnimationStateFromName("Visemes_17_" + cString::ToString(i + 1));
			if (pState == NULL)
				continue;
			if (itW == mapWeights.end())
			{
				pState->FadeOut(0.1f);
				continue;
			}
			pState->SetLayer(true);
			pState->SetActive(true);
			pState->SetPaused(true);
			pState->SetFadeStep(0);
			pState->SetTimePosition(0);
			pState->SetWeight(itW->second[i]);
		}
		if (itW != mapWeights.end())
			msetLipEntities.insert(sName);
	}
}

bool cSomaLuxVoiceHandler::LoadPcm(const tString &asFile)
{
	if (mPcm.msFile == asFile)
		return mPcm.mlChannels > 0;
	mPcm = cPcm();
	mPcm.msFile = asFile;
	OggVorbis_File vf;
	if (ov_fopen(cString::To8Char(mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asFile)).c_str(), &vf) != 0)
		return false;
	mPcm.mlChannels = ov_info(&vf, -1)->channels;
	mPcm.mlRate = ov_info(&vf, -1)->rate;
	int lSection = 0;
	float **pBuf;
	for (long n; (n = ov_read_float(&vf, &pBuf, 4096, &lSection)) > 0;)
		for (long i = 0; i < n; ++i)
			for (int c = 0; c < mPcm.mlChannels; ++c)
				mPcm.mvData.push_back(pBuf[c][i]);
	ov_clear(&vf);
	return true;
}

// cLuxVoiceHandler::GetSpectrumFromScene -> FMOD Channel::getSpectrum: |FFT| of the last 2n samples
// at the 48 kHz mix rate, rect window, averaged over channels
void cSomaLuxVoiceHandler::GetSpectrum(cPlaying &p, std::vector<float> &avOut, int alNum)
{
	avOut.assign(alNum, 0.0f);
	if (p.mpEntry == NULL || mpEngine->GetSound()->GetSoundHandler()->IsValid(p.mpEntry, p.mlEntryId) == false || LoadPcm(p.msFile) == false)
		return;
	int lN = 2 * alNum, lCh = mPcm.mlChannels;
	double fStep = mPcm.mlRate / 48000.0;
	long lFrames = (long)mPcm.mvData.size() / lCh;
	long lEnd = (long)(p.mpEntry->GetChannel()->GetElapsedTime() * mPcm.mlRate);
	for (int c = 0; c < lCh; ++c)
		for (int k = 0; k < alNum; ++k)
		{
			float fRe = 0, fIm = 0;
			for (int n = 0; n < lN; ++n)
			{
				long i = lEnd - (long)((lN - n) * fStep);
				if (i < 0 || i >= lFrames)
					continue;
				float x = mPcm.mvData[i * lCh + c], a = k2Pif * k * n / lN;
				fRe += x * cosf(a);
				fIm -= x * sinf(a);
			}
			avOut[k] += sqrtf(fRe * fRe + fIm * fIm) * 2.0f / lN / lCh;
		}
}

void cSomaLuxVoiceHandler::GetSpectrumFromSpeakingCharacter(const tString &asName, std::vector<float> &avOut, int alNum)
{
	for (cPlaying &p : mvPlaying)
		if (p.mlLine < p.mvLines.size() && p.mpSubject->mvLines[p.mvLines[p.mlLine]].msCharacter == asName)
			return GetSpectrum(p, avOut, alNum);
}

void cSomaLuxVoiceHandler::GetSpectrumFromScene(const tString &asScene, std::vector<float> &avOut, int alNum)
{
	for (cPlaying &p : mvPlaying)
		if (p.mpSubject->msScene == asScene)
			return GetSpectrum(p, avOut, alNum);
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

bool cSomaLuxVoiceHandler::SubjectInvolvesCharacter(const tString &asSubject, const tString &asName)
{
	auto it = mmapSubjects.find(asSubject);
	if (it == mmapSubjects.end())
		return false;
	for (const cLine &l : it->second->mvLines)
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

// cLuxDialogHandler::AddBranchEvent: attaches to the last added subject
void cSomaLuxDialogHandler::AddBranchEvent(int alType, float afVar, const tString &asVar, const tString &asNewBranch, bool abOnlyEnd)
{
	if (mBuilding.mvBranches.empty() || mBuilding.mvBranches.back().mvItems.empty())
		return;
	mBuilding.mvBranches.back().mvItems.back().mvEvents.push_back(cEvent{alType, (int)lroundf(afVar), asVar, asNewBranch, abOnlyEnd});
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
	bool bOk = cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->Play(item.msSubject, -1, "", 100, [this, wD]() {
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
			if (CheckEndEvents(aD))
				continue;
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
	Finish(aD);
}

void cSomaLuxDialogHandler::Finish(cDialog &aD)
{
	aD.mbDone = true;
	if (aD.msCallback != "")
		SomaMapScriptCall("void " + aD.msCallback + "(const tString&in)", [&](asIScriptContext *c) { c->SetArgObject(0, &aD.msName); });
}

// cLuxDialogInstance::CheckBranchEvent; OutOfRange/PlayerNotLooking are unused by the game scripts
bool cSomaLuxDialogHandler::CheckEvent(cDialog &aD, const cItem &aItem, const cEvent &aE)
{
	int lVar = mmapVars.count(aE.msVar) ? mmapVars[aE.msVar] : 0;
	switch (aE.mlType)
	{
	case 2: mmapVars[aE.msVar] = aE.mlVal; return false;
	case 3: mmapVars[aE.msVar] += aE.mlVal; return false;
	case 4: return lVar == aE.mlVal;
	case 5: return lVar < aE.mlVal;
	case 6: return lVar > aE.mlVal;
	case 7:
	{
		bool bRet = false;
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		if (aE.msVar == "" || pMap == NULL || pMap->GetScript() == NULL)
			return false;
		const tString &sBranch = aD.mvBranches[aD.mlBranch].msName;
		int lLine = cSomaLuxVoiceHandler::Get() ? cSomaLuxVoiceHandler::Get()->GetSubjectLineNumber(aItem.msSubject) - 1 : -1;
		cSomaScriptRuntime::Get()->Call(
			pMap->GetScript(), "bool " + aE.msVar + "(const tString&in, const tString&in, int, const tString&in)",
			[&](asIScriptContext *c) {
				c->SetArgObject(0, (void *)&sBranch);
				c->SetArgObject(1, (void *)&aItem.msSubject);
				c->SetArgDWord(2, lLine);
				c->SetArgObject(3, (void *)&aE.msNewBranch);
			},
			[&](asIScriptContext *c) { bRet = c->GetReturnByte() != 0; });
		return bRet;
	}
	}
	return false;
}

// cLuxDialogInstance::CheckEvents(true): a firing event jumps to its branch, or ends the dialog
bool cSomaLuxDialogHandler::CheckEndEvents(cDialog &aD)
{
	const cItem item = aD.mvBranches[aD.mlBranch].mvItems[aD.mlItem];
	for (const cEvent &e : item.mvEvents)
	{
		if (e.mbOnlyEnd == false || CheckEvent(aD, item, e) == false)
			continue;
		int lIdx = -1;
		if (e.msNewBranch != "" && FindBranch(aD, e.msNewBranch, &lIdx))
		{
			aD.mlBranch = lIdx;
			aD.mlItem = -1;
		}
		else
			Finish(aD);
		return true;
	}
	return false;
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
	cSomaLuxVoiceHandler *pVoice = cSomaLuxVoiceHandler::Get();
	if (pVoice == NULL)
		return false;
	if (pVoice->SceneInvolvingCharacterIsActive(asName))
		return true;
	// Pauses between subjects still count
	for (auto &p : mvActive)
		if (p->mbDone == false)
			for (const cBranch &b : p->mvBranches)
				for (const cItem &item : b.mvItems)
					if (item.msSubject != "" && pVoice->SubjectInvolvesCharacter(item.msSubject, asName))
						return true;
	return false;
}

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
	SOMA_METHOD(e, T, "void AddCharacterSpeakingCallback(const tString&in asCharacter, const tString&in asCallback)", +[](void *, S c, S f) { VH->SetSpeakingCallback(c, f); });
	SOMA_METHOD(e, T, "void GetSpectrumFromSpeakingCharacter(const tString&in asCharacter, array<float>&out aDestArray, int alNumSamples=64)",
				+[](void *, S s, CScriptArray &arr, int n) {
					std::vector<float> v;
					VH->GetSpectrumFromSpeakingCharacter(s, v, n);
					arr.Resize((asUINT)v.size());
					for (asUINT i = 0; i < v.size(); ++i)
						*(float *)arr.At(i) = v[i];
				});
	SOMA_METHOD(e, T, "void GetSpectrumFromScene(const tString&in asScene, array<float>&out aDestArray, int alNumSamples=64)",
				+[](void *, S s, CScriptArray &arr, int n) {
					std::vector<float> v;
					VH->GetSpectrumFromScene(s, v, n);
					arr.Resize((asUINT)v.size());
					for (asUINT i = 0; i < v.size(); ++i)
						*(float *)arr.At(i) = v[i];
				});
	SOMA_METHOD(e, T, "bool SubjectIsPlaying(const tString&in asName)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SubjectIsPlaying(s); });
	SOMA_METHOD(e, T, "bool SceneIsActive(const tString&in asScene)", +[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneIsActive(s); });
	SOMA_METHOD(e, T, "bool SceneInvolvingCharacterIsActive(const tString&in asCharacter)",
				+[](void *, S s) { return cSomaLuxVoiceHandler::Get() && cSomaLuxVoiceHandler::Get()->SceneInvolvingCharacterIsActive(s); });
	SOMA_METHOD(e, T, "void FadeSceneVolumeTo(const tString&in asScene, float afVolume, float afTime)", +[](void *, S s, float v, float t) { VH->FadeSceneVolumeTo(s, v, t); });
	SOMA_METHOD(e, T, "void SetFocusScene(const tString&in asScene)", +[](void *, S) {});
	SOMA_METHOD(e, "cLuxMap",
				"void SetVoiceSource(const tString &in asCharacter, const tString &in asEntityName, float afMinDistance, float afMaxDistance, bool abUse3D, float afMaxPlayerListeningRange, float afMinFreq = 22000, float afMaxFreq = 22000, uint aFrequencyFlags = 0)",
				+[](void *, S c, S ent, float fMin, float fMax, bool b3D, float, float, float, asUINT) { VH->SetSource(c, ent, fMin, fMax, b3D); });
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
	SOMA_METHOD(e, T, "void AddBranchEvent(eLuxDialogBranchEvent aType, float afVar, const tString&in asVar, const tString&in asNewBranch, bool abOnlyCheckEndOfSubject)",
				+[](void *, int t, float f, S var, S branch, bool b) { D::Get()->AddBranchEvent(t, f, var, branch, b); });
	SOMA_METHOD(e, T, "bool CharacterIsActive(const tString&in asName)", +[](void *, S s) { return D::Get()->CharacterIsActive(s); });
	SOMA_METHOD(e, T, "void Stop(const tString &in asName)", +[](void *, S s) { D::Get()->Stop(s); });
	SOMA_METHOD(e, T, "void StopAll()", +[](void *) { D::Get()->StopAll(); });
	SOMA_METHOD(e, T, "void SetVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] = x; });
	SOMA_METHOD(e, T, "void IncVar(const tString&in asName, int alX)", +[](void *, S s, int x) { D::Get()->mmapVars[s] += x; });
	SOMA_METHOD(e, T, "int GetVar(const tString&in asName)", +[](void *, S s) { return D::Get()->mmapVars[s]; });
}
