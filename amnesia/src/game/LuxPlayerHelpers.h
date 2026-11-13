/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LUX_PLAYER_HELPERS_H
#define LUX_PLAYER_HELPERS_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxPlayerA2.h"

//----------------------------------------------

class cLuxPlayer;
struct OhplGa2Ops;
struct OhplGa2Sanity;
struct OhplGa2Death;

//----------------------------------------------

class cLuxPlayerInsanityCollapse : public iLuxPlayerHelper
{
friend class cLuxPlayer_SaveData;
public:
	cLuxPlayerInsanityCollapse(cLuxPlayer *apPlayer);
	~cLuxPlayerInsanityCollapse();

	void Reset();

	void Start();
	void Stop();
	bool IsActive(){ return mbActive; }

	void Update(float afTimeStep);

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	OhplGa2Collapse mS;
	uint32_t &mbActive = mS.mbActive;

	int32_t &mlState = mS.mlState;

	float &mfHeightAddCollapseSpeed = mS.mfHeightAddCollapseSpeed;
	float &mfHeightAddAwakeSpeed = mS.mfHeightAddAwakeSpeed;
	float &mfRollCollapseSpeed = mS.mfRollCollapseSpeed;
	float mfRollAwakeSpeed;

	float &mfHeightAddGoal = mS.mfHeightAddGoal;
	
	float &mfAwakenSanity = mS.mfAwakenSanity;
    
	float &mfSleepTime = mS.mfSleepTime;

	float mfSleepSpeedMul;
	float &mfWakeUpSpeedMul = mS.mfWakeUpSpeedMul;
	
	tString msStartSound;
	tString msAwakenSound;
	tString msSleepLoopSound;
	float mfSleepLoopSoundVolume;

	tString msSleepRandomSound;
	float &mfSleepRandomMinTime = mS.mfSleepRandomMinTime;
	float &mfSleepRandomMaxTime = mS.mfSleepRandomMaxTime;
	
	float &mfHeightAdd = mS.mfHeightAdd;
	float &mfRoll = mS.mfRoll;

	float &mfT = mS.mfT;
	float &mfRandomCount = mS.mfRandomCount;

	cSoundEntry *mpLoopSound;
	int mlLoopSoundID;
};

//----------------------------------------------

class cLuxPlayerCamDirEffects : public iLuxPlayerHelper
{
public:
	cLuxPlayerCamDirEffects(cLuxPlayer *apPlayer);
	~cLuxPlayerCamDirEffects();

	void Reset();

	void Update(float afTimeStep);

	float AddAndGetYawAdd(float afX);
	float AddAndGetPitchAdd(float afX);

private:
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	OhplA2bCamDir mS;

	float &mfStartSwayMaxSanity = mS.k.mfStartSwayMaxSanity;

	uint32_t &mbSwayActive = mS.k.mbSwayActive;

	cVector2f &mvSwayAdd = mS.k.mvSwayAdd;
	int32_t &mlMaxPositions = mS.k.mlMaxPositions;

	float &mfSwayAlpha = mS.k.mfSwayAlpha;

	cVector2f &mvNextAdd = mS.k.mvNextAdd;
};

//----------------------------------------------

class cLuxPlayerSpawnPS_SpawnPos
{
public:
	cParticleSystem *mpPS;
	cVector3f mvPos;
	cVector3f mvLastLocalPos;
};

class cLuxPlayerSpawnPS : public iLuxPlayerHelper
{
public:
	cLuxPlayerSpawnPS (cLuxPlayer *apPlayer);
	~cLuxPlayerSpawnPS();

	void Reset();

	void Start(const tString& asFileName);
	void Stop();

	void RespawnAll();

	bool IsActive(){ return mbActive;}
	const tString& GetFileName(){ return msFileName;}

	void Update(float afTimeStep);
private:
	void GenerateAllSpawnPos();
	cParticleSystem* CreatePS(cLuxPlayerSpawnPS_SpawnPos *apSpawnPos);

	void DestroyAllSpawnPoints();

	bool LoadSpawnPSFile(const tString& asFileName);

	bool mbActive;
	tString msFileName;
	
	tString msParticleSystem;
	float mfHeightFromFeet;
	float mfHeightAddMin;
	float mfHeightAddMax;
	float mfDensity;
	float mfRadius;
	cColor mPSColor;
	bool mbFadePS;
	float mfPSMinFadeStart;
	float mfPSMinFadeEnd;
	float mfPSMaxFadeStart;
	float mfPSMaxFadeEnd;

	std::vector<cLuxPlayerSpawnPS_SpawnPos> mvSpawnPos;
};

//----------------------------------------------

class cLuxPlayerHurt : public iLuxPlayerHelper
{
public:
	cLuxPlayerHurt (cLuxPlayer *apPlayer);
	~cLuxPlayerHurt();

	void Reset();

	void Update(float afTimeStep);

	void OnDraw(float afFrameTime);

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	OhplA2bHurt mS;
	std::vector<cGuiGfxElement*> mvNoiseGfx;
	float &mfEffectStartHealth = mS.k.mfEffectStartHealth;
	float &mfMinMoveMul = mS.k.mfMinMoveMul;
	float &mfMaxPantCount = mS.k.mfMaxPantCount;
	float &mfPantSpeed = mS.k.mfPantSpeed;
	float &mfPantSize = mS.k.mfPantSize;

	int32_t &mlCurrentNoise = mS.k.mlCurrentNoise;
	float &mfNoiseUpdateCount = mS.k.mfNoiseUpdateCount;

	float mfNoiseAlpha;
	float &mfNoiseFreq = mS.k.mfNoiseFreq;
	cColor mNoiseColor;
	
	float &mfPantCount = mS.k.mfPantCount;
	float &mfPantPosAdd = mS.k.mfPantPosAdd;
	float &mfPantPosAddVel = mS.k.mfPantPosAddVel;
	float &mfPantPosAddDir = mS.k.mfPantPosAddDir;
	float &mfAlpha = mS.k.mfAlpha;

	float &mfHealthRegainSpeed = mS.mfHealthRegainSpeed;
	float &mfHealthRegainLimit = mS.mfHealthRegainLimit;
};

//----------------------------------------------

class cLuxFlashbackData
{
public:
	cLuxFlashbackData(){}
	cLuxFlashbackData(const tString &asFile, const tString &asCallback) : msFile(asFile), msCallback(asCallback) {}

	tString msFile;
	tString msCallback;
};

typedef std::list<cLuxFlashbackData> tLuxFlashbackDataList;
typedef tLuxFlashbackDataList::iterator tLuxFlashbackDataListIt;

//----------------------------------------------

class cLuxPlayerFlashback : public iLuxPlayerHelper
{
friend class cLuxPlayer_SaveData;
public:
	cLuxPlayerFlashback (cLuxPlayer *apPlayer);
	~cLuxPlayerFlashback ();

	void Reset();
	
	void Start(const tString &asFlashbackFile, const tString &asCallback);

	void Update(float afTimeStep);

	void OnDraw(float afFrameTime);

	bool IsActive(){ return mbActive; }

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	OhplGa2Flashback mS;
	void LoadAndPlayFlashbackFile(const tString& asFlashbackFile);

	//Data
	float &mfRadialBlurSize = mS.mfRadialBlurSize;
	float &mfRadialBlurStartDist = mS.mfRadialBlurStartDist;
	float mfWorldSoundVolume;
	float &mfMoveSpeedMul = mS.mfMoveSpeedMul;
	float &mfRunSpeedMul = mS.mfRunSpeedMul;

	//Variables
	float &mfFlashDelay = mS.mfFlashDelay;
	uint32_t &mbActive = mS.mbActive;
	float &mfFlashbackStartCount = mS.mfFlashbackStartCount;
	tString msFlashbackFile;
	tString msCallback;
	tLuxFlashbackDataList mlstFlashbackQueue;
	
};

//---------------------------------------------

class cLuxPlayerLookAt : public iLuxPlayerHelper
{
friend class cLuxPlayer_SaveData;
public:
	cLuxPlayerLookAt(cLuxPlayer *apPlayer);
	~cLuxPlayerLookAt();

	void Update(float afTimeStep);

	void Reset();

	void SetTarget(const cVector3f &avTargetPos, float afSpeedMul, float afMaxSpeed, const tString& asAtTargetCallback);
	
	void SetActive(bool abX);
	bool IsActive(){ return mbActive;}

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	OhplA2bLookAt mS;
	uint32_t &mbActive = mS.mbActive;
	cVector3f &mvTargetPos = mS.mvTargetPos;

	cVector3f &mvCurrentSpeed = mS.k.mvCurrentSpeed;
	float &mfSpeedMul = mS.k.mfSpeedMul;
	float &mfMaxSpeed = mS.k.mfMaxSpeed;

	tString msAtTargetCallback;

	float &mfDestFovMul = mS.mfDestFovMul;
	float &mfFov = mS.mfFov;
	float &mfFovSpeed = mS.mfFovSpeed;
	float &mfFovMaxSpeed = mS.mfFovMaxSpeed;
};

//----------------------------------------------

class cLuxPlayerSanity : public iLuxPlayerHelper
{
friend class cLuxPlayer_SaveData;
public:
	cLuxPlayerSanity(cLuxPlayer *apPlayer);
	~cLuxPlayerSanity();

	void Reset();

	void StartHit();
	void SetSanityLost();

	void Update(float afTimeStep);

	void OnDraw(float afFrameTime);

	float GetAtLowSanityCount(){ return mfAtLowSanityCount;}

private:
	float GetCurrentSizeMul();

	void ToRust(OhplGa2Sanity &s);
	void FromRust(const OhplGa2Sanity &s);
	void Replay(const OhplGa2Ops &ops);
	void UpdateInsaneEffects(float afTimeStep);
	void UpdateCheckEnemySeen(float afTimeStep);
	void UpdateHit(float afTimeStep);
	
	void UpdateEnemySeenEffect(float afTimeStep);
	
	float mfHitAlpha;
	bool mbHitActive;
	float mfSanityLostCount;

	float mfAtLowSanityCount;

	bool mbHitIsUpdated;
	bool mbSanityLostIsUpdated;

	float mfPantCount;

	float mfCheckEnemySeenCount;

	bool mbSanityEffectUpdated;

	float mfT;
	float mfInsaneWaveAlpha;

	float mfSanityDrainCount;
	float mfSanityDrainVolume;
	float mfSanityHeartbeatCount;

	float mfSeenEnemyCount;
	bool mbEnemyIsSeen;

	float mfShowHintTimer;

	//////////////
	// Data
	float mfHitZoomInSpeed;
	float mfHitZoomOutSpeed;
	float mfHitZoomInFOVMul;
	float mfHitZoomInAspectMul;

	float mfSanityRegainSpeed;
	float mfSanityRegainLimit;
	float mfSanityVeryLowLimit;
	float mfSanityEffectsStart;

	float mfSanityWaveAlphaMul;
	float mfSanityWaveSpeedMul;

	float mfSanityLowLimit;
	float mfSanityLowLimitMaxTime;
	float mfSanityLowNewSanityAmount;

	float mfCheckNearEnemyInterval;
	float mfNearEnemyDecrease;
	float mfNearCritterDecrease;
};

//----------------------------------------------

class cLuxPlayerLantern : public iLuxPlayerHelper
{
public:	
	cLuxPlayerLantern(cLuxPlayer *apPlayer);
	~cLuxPlayerLantern();

	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

	void SetActive(bool abX, bool abUseEffects, bool abCheckForOilAndItems=true, bool abCheckIfAllowed=true);
	bool IsActive(){ return mbActive;}

	void SetDisabled(bool abX);
	bool GetDisabled(){ return mbDisabled;}

	iLight* GetLight(){ return mpLight;}

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	OhplA2bLantern mS;
	cColor &mDefaultColor = reinterpret_cast<cColor&>(mS.k.mColor);
	float mfRadius;
	tString msGobo;
	bool mbCastShadows;
	cVector3f mvLocalOffset;
	tString msTurnOnSound;
	tString msTurnOffSound;
	tString msOutOfOilSound;
	tString msDisabledSound;
	float &mfLowerOilSpeed = mS.k.mfLowerOilSpeed;
	float &mfFadeLightOilAmount = mS.k.mfFadeLightOilAmount;
	
	uint32_t &mbDisabled = mS.mbDisabled;
	uint32_t &mbActive = mS.k.mbActive;
	float &mfAlpha = mS.k.mfAlpha;
	cLightPoint *mpLight;
	
};

//----------------------------------------------

class cLuxPlayerDeath : public iLuxPlayerHelper
{
public:
	cLuxPlayerDeath(cLuxPlayer *apPlayer);
	~cLuxPlayerDeath();

	void LoadFonts();
	void LoadUserConfig();
	void SaveUserConfig();

	void Reset();

	void Start();

	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);


	void OnDraw(float afFrameTime);

	void OnPressButton();

	float GetFadeAlpha(){ return mfFadeAlpha;}

	void DisableStartSound(){ mbSkipStartSound = true; }

	bool ShowHint(){ return mbShowHint;}
	void SetShowHint(bool abX){ mbShowHint=abX;}

	void SetHint(const tString& asCat, const tString& asEntry);
	const tString& GetHintCat(){ return msHintCat; }
	const tString& GetHintEntry(){ return msHintEntry; }



private:
	void ResetGame();
	void ToRust(OhplGa2Death &s);
	void FromRust(const OhplGa2Death &s);
	void Replay(const OhplGa2Ops &ops);

	bool mbToMainMenu;
	bool mbActive;

	bool mbShowHint;

	int mlState;

	tString msStartSound;
	tString msAwakenSound;

	bool mbSkipStartSound;

	float mfHeightAddSpeed;
	float mfRollSpeed;

	float mfHeightAddGoal;
	float mfHeightAddGoalCrouch;
	float mfFadeOutTime;

	float mfMaxSanityGain;
	float mfMaxHealthGain;
	float mfMaxOilGain;
	float mfMinSanityGain;
	float mfMinHealthGain;
	float mfMinOilGain;

	float mfHeightAdd;
	float mfRoll;

	float mfT;

	float mfMinHeightAdd;

	tString msHintCat;
	tString msHintEntry;

	tWString msCurrentHintText;

	cGuiGfxElement *mpWhiteModGfx;

	iFontData *mpFont;
    	
	float mfFadeAlpha;
	float mfTextAlpha1;
	float mfTextAlpha2;
	float mfTextOnScreenCount;
	float mfWhiteCount;
	
	cLinearOscillation mFlashOscill;

	cSoundEntry *mpVoiceEntry;
	int mlVoiceEntryId;
};


//----------------------------------------------

class cLuxPlayerLean : public iLuxPlayerHelper
{
public:	
	cLuxPlayerLean(cLuxPlayer *apPlayer);
	~cLuxPlayerLean();
	
	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

	void Update(float afTimeStep);

	void SetLean(float afMul);
	void AddLean(float afAdd);

	void Reset();
private:
	iCollideShape *mpHeadShape;

	float mfDir;
	float mfDirAdd;
	float mfMovement;
	float mfRotation;

	float mfMaxMovement;
	float mfMaxRotation;
	float mfMaxTime;

	float mfMoveSpeed;

	bool mbIntersect;

	bool mbPressed;
};

//----------------------------------------------

class cLuxPlayerDamageData
{
public:
	std::vector<cGuiGfxElement*> mvImages;
};

class cLuxPlayerHudEffect_Splash
{
public:
	cGuiGfxElement *mpImage;	
	cVector3f mvPos;
	cVector2f mvSize;
	float mfAlpha;
	float mfAlphaMul;

	cVector3f mvPosVel;
	cVector2f mvSizeVel;
	float mfAlphaVel;
	float mfAlphaMoveStart;
};

typedef std::list<cLuxPlayerHudEffect_Splash> cLuxPlayerHudEffect_SplashList;
typedef cLuxPlayerHudEffect_SplashList::iterator cLuxPlayerHudEffect_SplashListIt;

class cLuxPlayerHudEffect : public iLuxPlayerHelper
{
public:
	cLuxPlayerHudEffect(cLuxPlayer *apPlayer);
	~cLuxPlayerHudEffect();

	void AddDamageSplash(eLuxDamageType aType);
	void Flash(const cColor& aColor, eGuiMaterial aFlashMaterial, float afInTime, float afOutTime);

	void OnDraw(float afFrameTime);
	void Update(float afTimeStep);
	void Reset();
	
private:
	void DrawSplashes(float afFrameTime);
	void UpdateSplashes(float afTimeStep);

	void DrawFlash(float afFrameTime);
	void UpdateFlash(float afTimeStep);

	void LoadDamageData(cLuxPlayerDamageData *apData, const tString& asName);

	std::vector<cLuxPlayerDamageData> mvDamageTypes;
	cLuxPlayerHudEffect_SplashList mlstSplashes;

	cGuiGfxElement *mpFlashGfx;
	cColor mFlashColor;
	eGuiMaterial mFlashMaterial;
	float mfFlashAlphaSpeed;
	float mfFlashAlphaOutSpeed;
	float mfFlashAlpha;
	bool mbFlashActive;
};


//----------------------------------------------

class cLuxPlayerLightLevel : public iLuxPlayerHelper
{
public:	
	cLuxPlayerLightLevel(cLuxPlayer *apPlayer);
	~cLuxPlayerLightLevel();
	
	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);

	float GetExtendedLightLevel(){ return mfExtendedLightLevel;}
	float GetNormalLightLevel(){ return mfNormalLightLevel;}

private:
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	OhplA2bLight mS;
	void SampleLevels(const cVector3f *apPos, float *apExt, float *apNormal);
	float &mfExtendedLightLevel = mS.mfExtendedLightLevel;
	float &mfNormalLightLevel = mS.mfNormalLightLevel;
	float &mfUpdateCount = mS.mfUpdateCount;

	float &mfRadiusAdd = mS.mfRadiusAdd;
};

//----------------------------------------------

class cLuxPlayerInDarkness : public iLuxPlayerHelper
{
friend class cLuxPlayer_SaveData;
public:	
	cLuxPlayerInDarkness(cLuxPlayer *apPlayer);
	~cLuxPlayerInDarkness();

	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

	cLightPoint* GetAmbientLight(){ return mpAmbientLight;}

	bool InDarkness();

	void SetActive(bool abX);

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	OhplA2bDark mS;
	cSoundHandler *mpSoundHandler;

	float &mfMinDarknessLightLevel = mS.k.mfMinDarknessLightLevel;
	
	float &mfAmbientLightMinLightLevel = mS.k.mfAmbientLightMinLightLevel;
	float mfAmbientLightRadius;
	float mfAmbientLightIntensity;
	float mfAmbientLightFadeInTime;
	float mfAmbientLightFadeOutTime;
	cColor mAmbientLightColor;

	tString msLoopSoundFile;
	float mfLoopSoundVolume;
	float &mfLoopSoundStartupTime = mS.k.mfLoopSoundStartupTime;
	float mfLoopSoundFadeInSpeed;
	float mfLoopSoundFadeOutSpeed;

	float &mfSanityLossPerSecond = mS.k.mfSanityLossPerSecond;
	float &mfSanityLossMul = mS.k.mfSanityLossMul;

	uint32_t &mbInDarkness = mS.k.mbInDarkness;
	float &mfShowHintTimer = mS.k.mfShowHintTimer;

	cSoundEntry *mpLoopSound;
	float &mfLoopSoundCount = mS.k.mfLoopSoundCount;

	cLightPoint *mpAmbientLight;
	uint32_t &mbAmbientLightIsOn = mS.k.mbAmbientLightIsOn;

	uint32_t &mbActive = mS.mbActive;
};

//----------------------------------------------


#endif // LUX_MAP_HANDLER_H
