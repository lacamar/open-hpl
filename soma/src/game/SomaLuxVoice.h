#ifndef SOMA_LUX_VOICE_H
#define SOMA_LUX_VOICE_H

#include "hpl.h"
#include "RustSomaSound.h"

#include <angelscript.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>

using namespace hpl;

class cSomaSoundInstance;

class cSomaLuxVoiceHandler : public iUpdateable
{
public:
	cSomaLuxVoiceHandler(cEngine *apEngine);
	~cSomaLuxVoiceHandler();

	static cSomaLuxVoiceHandler *Get() { return mpInstance; }
	cEngine *GetEngine() { return mpEngine; }
	std::vector<tWStringVec> mvRowCache; // word wrap results of the frame's subtitles, in drawing order
	iFontData *GetFont() { return mpFont; }

	// cLuxVoiceHandler::LoadFromFile: the map's .voice (and once, the global one)
	void LoadMapFile(const tString &asHpmPath, const tString &asMapName);
	void Reset();
	void UpdateVoices(float afTimeStep);
	void OnDraw(float afFrameTime) override;

	bool Play(const tString &asSubject, int alLine, const tString &asCallback, int alPrio,
			  const std::function<void()> &aOnDone = std::function<void()>(), bool abPlayWhilePaused = false);
	bool SubjectExists(const tString &asSubject);
	void Stop(const tString &asScene);
	void StopAll();
	void SkipCurrentLine(const tString &asScene);
	bool CharacterIsSpeaking(const tString &asName);
	void GetSpectrumFromSpeakingCharacter(const tString &asName, std::vector<float> &avOut, int alNum);
	void GetSpectrumFromScene(const tString &asScene, std::vector<float> &avOut, int alNum);
	bool SubjectIsPlaying(const tString &asName);
	bool SceneIsActive(const tString &asScene);
	bool SceneInvolvingCharacterIsActive(const tString &asName);
	bool SubjectInvolvesCharacter(const tString &asSubject, const tString &asName);
	bool AnySceneIsActive();
	int GetSubjectLineNumber(const tString &asSubject);
	float GetTimeLeftOnLastLineOfPlayingSubject(const tString &asSubject);
	const tString &GetSubjectSceneName(const tString &asSubject);
	void SetPaused(const tString &asScene, bool abX);
	void SetPausedAll(bool abX);
	void ReapplyPauses();
	void AddSubtitle(const std::vector<tString> &avRows, const tString &asName, const cColor &aColor, int alDrawnChars);
	void SetSource(const tString &asCharacter, const tString &asEntity, float afMinDist, float afMaxDist, bool abUse3D,
				   float afListenRange = -1, float afMinFreq = 22000, float afMaxFreq = 22000, int alFreqFlags = 0);
	void SetFocusScene(const tString &asScene);
	void FadeSceneVolumeTo(const tString &asScene, float afVolume, float afTime);
	void SetSpeakingCallback(const tString &asCharacter, const tString &asFunc);

	std::set<tString> msetPlayedLines; // PlayOnce lines, "subject#index"

	static void RegisterNatives(asIScriptEngine *apEngine);

	// cLuxEffectHandler::FadeIn/FadeOut: black overlay under the subtitles
	void FadeTo(float afGoal, float afTime);
	bool IsFading();
	float GetFadeAlpha();

private:
	struct cPcm
	{
		tString msFile;
		std::vector<float> mvData; // interleaved
		int mlChannels = 0, mlRate = 0;
	};
	bool LoadPcm(const tString &asFile);
	void GetSpectrum(bool abByScene, const tString &asKey, std::vector<float> &avOut, int alNum);
	void UpdateLipsync();
	bool LoadVoiceFile(const tString &asFile, const tString &asSet);
	void LoadLangFile(const tString &asFile);
	bool ReadText(const tString &asFile, tString &asOut);
	void StopScene(const tString &asScene);
	void UpdateSpeakingCallbacks();
	std::function<void()> TakeDone(uint64_t alToken);

	static cSomaLuxVoiceHandler *mpInstance;

	cEngine *mpEngine;
	OhplGs5Voices *mpV;
	std::map<uint64_t, std::function<void()>> mmapOnDone;
	uint64_t mlNextToken = 1;
	cPcm mPcm; // ponytail: one decoded voice, two characters visualized at once would re-decode every call
	std::set<tString> msetLipEntities;
	tString msSceneCache;

	cGuiGfxElement *mpFadeGfx = NULL;

	cGuiSet *mpGuiSet = NULL;
	iFontData *mpFont = NULL;
};

class cSomaLuxDialogHandler
{
public:
	static cSomaLuxDialogHandler *Get();

	void Begin(const tString &asName);
	void End(const tString &asStartBranch);
	void AddBranch(const tString &asName, const tString &asNext);
	void AddSubject(const tString &asSubject, const tString &asCallback);
	void AddPause(float afTime, const tString &asCallback);
	void AddResponseOption(const tString &asEntry, const tString &asBranch, int alId, const tString &asCallback);
	void AddBranchEvent(int alType, float afVar, const tString &asVar, const tString &asNewBranch, bool abOnlyEnd);
	void SetCallbackFunc(const tString &asFunc) { msPendingCallback = asFunc; }
	void Stop(const tString &asName);
	void StopAll();
	bool CharacterIsActive(const tString &asName);
	void Update(float afTimeStep);

	std::map<tString, int> mmapVars;

	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	struct cEvent
	{
		int mlType;
		int mlVal;
		tString msVar;
		tString msNewBranch;
		bool mbOnlyEnd;
	};
	struct cItem
	{
		tString msSubject;
		float mfPause = -1;
		tString msCallback;
		std::vector<cEvent> mvEvents;
	};
	struct cOption
	{
		tString msEntry;
		tString msBranch;
		int mlId;
		tString msCallback;
	};
	struct cBranch
	{
		tString msName;
		tString msNext;
		std::vector<cItem> mvItems;
		std::vector<cOption> mvOptions;
	};
	struct cDialog
	{
		tString msName;
		tString msCallback;
		std::vector<cBranch> mvBranches;
		int mlBranch = -1;
		int mlItem = -1;
		bool mbWaiting = false;
		float mfPauseLeft = 0;
		bool mbDone = false;
	};
	cBranch *FindBranch(cDialog &aD, const tString &asName, int *apIdx);
	void StartItem(cDialog &aD);
	void NextItem(cDialog &aD);
	void EndBranch(cDialog &aD);
	void Finish(cDialog &aD);
	bool CheckEvent(cDialog &aD, const cItem &aItem, const cEvent &aE);
	bool CheckEndEvents(cDialog &aD);

	cDialog mBuilding;
	tString msPendingCallback;
	std::vector<std::shared_ptr<cDialog>> mvActive;
};

void SomaMapScriptCall(const tString &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs);

#endif // SOMA_LUX_VOICE_H
