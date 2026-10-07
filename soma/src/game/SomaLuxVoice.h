#ifndef SOMA_LUX_VOICE_H
#define SOMA_LUX_VOICE_H

#include "hpl.h"

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

	// cLuxVoiceHandler::LoadFromFile: the map's .voice (and once, the global one)
	void LoadMapFile(const tString &asHpmPath, const tString &asMapName);
	void Reset();
	void UpdateVoices(float afTimeStep);
	void OnDraw(float afFrameTime) override;

	bool Play(const tString &asSubject, int alLine, const tString &asCallback, int alPrio,
			  const std::function<void()> &aOnDone = std::function<void()>());
	bool SubjectExists(const tString &asSubject);
	void Stop(const tString &asScene);
	void StopAll();
	void SkipCurrentLine(const tString &asScene);
	bool CharacterIsSpeaking(const tString &asName);
	void GetSpectrumFromSpeakingCharacter(const tString &asName, std::vector<float> &avOut, int alNum);
	bool SubjectIsPlaying(const tString &asName);
	bool SceneIsActive(const tString &asScene);
	bool SceneInvolvingCharacterIsActive(const tString &asName);
	bool SubjectInvolvesCharacter(const tString &asSubject, const tString &asName);
	bool AnySceneIsActive() { return mvPlaying.empty() == false; }
	int GetSubjectLineNumber(const tString &asSubject);
	const tString &GetSubjectSceneName(const tString &asSubject);
	void SetPaused(const tString &asScene, bool abX);
	void SetPausedAll(bool abX);
	void SetSource(const tString &asCharacter, const tString &asEntity, float afMinDist, float afMaxDist, bool abUse3D);
	void FadeSceneVolumeTo(const tString &asScene, float afVolume, float afTime);
	void SetSpeakingCallback(const tString &asCharacter, const tString &asFunc);

	std::set<tString> msetPlayedLines; // PlayOnce lines, "subject#index"

	static void RegisterNatives(asIScriptEngine *apEngine);

	// cLuxEffectHandler::FadeIn/FadeOut: black overlay under the subtitles
	void FadeTo(float afGoal, float afTime);
	bool IsFading() { return mfFadeAlpha != mfFadeGoal; }
	float GetFadeAlpha() { return mfFadeAlpha; }

	struct cSound
	{
		tString msText;
		tString msFile;
		float mfVoiceOffset = 0;
		float mfEndPadding = 0;
		float mfVolume = 1;
		tString msEffect;
		bool mbEndsAfterEffect = false;
	};
	struct cLine
	{
		tString msCharacter;
		tString msDisplayName;
		tString msCallback;
		float mfCharVolume = 1;
		int mlEntryType = 8;
		tString msSource;
		float mfMinDist = 0, mfMaxDist = 0;
		bool mbWorldSpace = false, mbChangeSource = false;
		int mlPrio = 0;
		bool mbPlayOnce = false;
		std::vector<cSound> mvSounds;
	};
	struct cSubject
	{
		tString msName;
		tString msScene;
		tString msSet; // map name or "global"
		bool mbSingleRandomLine = false;
		int mlLastLine = -1;
		std::vector<cLine> mvLines;
	};

private:
	typedef std::array<float, 17> tVisemes;
	struct cLipFrame
	{
		int mlStart, mlEnd;
		tVisemes mvW;
	};
	struct cPlaying
	{
		cSubject *mpSubject;
		int mlPrio = 0;
		tString msCallback;
		std::function<void()> mOnDone;
		std::vector<int> mvLines;
		size_t mlLine = 0;
		size_t mlSound = 0;
		float mfTime = 0;
		int mlStep = 0; // 0 before the sound, 1 playing, 2 end padding
		cSoundEntry *mpEntry = NULL;
		int mlEntryId = -1;
		float mfFallback = 0;
		tString msSubtitle;
		bool mbPaused = false;
		tString msSourceEntity;
		tString msFile;
		tString msLipEntity;
		std::shared_ptr<std::vector<cLipFrame>> mpLipsync;
		cSomaSoundInstance *mpEffect = NULL; // outlives its sound until the next one with an effect
		int mlEffectId = -1;
		std::vector<std::pair<cSoundEntry *, int>> mvOldVoices;
		std::vector<std::pair<cSomaSoundInstance *, int>> mvOldEffects;
	};
	struct cPcm
	{
		tString msFile;
		std::vector<float> mvData; // interleaved
		int mlChannels = 0, mlRate = 0;
	};
	bool LoadPcm(const tString &asFile);
	std::shared_ptr<std::vector<cLipFrame>> LoadLipsync(const tString &asFile);
	void UpdateLipsync();
	std::set<tString> msetLipEntities;
	bool LoadVoiceFile(const tString &asFile, const tString &asSet);
	void LoadLangFile(const tString &asFile);
	void StartSound(cPlaying &aP);
	void StopSound(cPlaying &aP);
	void Finish(size_t alIdx);
	void LineCallback(cSubject *apSubject, int alLine, bool abStart);
	tString SoundKey(cSubject *apSubject, size_t alLine, size_t alSound);

	static cSomaLuxVoiceHandler *mpInstance;

	cEngine *mpEngine;
	std::map<tString, std::unique_ptr<cSubject>> mmapSubjects;
	std::map<tString, tString> mmapText; // lowercase key -> text
	std::vector<tString> mvLoadedSets;
	std::vector<cPlaying> mvPlaying;
	cPcm mPcm; // ponytail: one decoded voice, two characters visualized at once would re-decode every call
	struct cSource
	{
		tString msEntity;
		float mfMinDist, mfMaxDist;
		bool mbUse3D;
	};
	std::map<tString, cSource> mmapSources;
	struct cSceneVolume
	{
		float mfVolume = 1, mfGoal = 1, mfSpeed = 0;
	};
	std::map<tString, cSceneVolume> mmapSceneVolumes;
	std::map<tString, tString> mmapSpeakingCallbacks;
	std::set<tString> msetSpeaking;
	void UpdateSpeakingCallbacks();

	float mfFadeAlpha = 0;
	float mfFadeGoal = 0;
	float mfFadeSpeed = 0;
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
