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

//----------------------------------------------

class cLuxPlayerInsanityCollapse : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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

	float mfRollAwakeSpeed;

	
    

	float mfSleepSpeedMul;
	
	tString msStartSound;
	tString msAwakenSound;
	tString msSleepLoopSound;
	float mfSleepLoopSoundVolume;

	tString msSleepRandomSound;
	
	float &mfHeightAdd = mS.mfHeightAdd;
	float &mfRoll = mS.mfRoll;

	float &mfT = mS.mfT;

	cSoundEntry *mpLoopSound;
	int mlLoopSoundID;
};

//----------------------------------------------

class cLuxPlayerCamDirEffects : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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





};

//----------------------------------------------

class cLuxPlayerSpawnPS : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
public:
	cLuxPlayerSpawnPS (cLuxPlayer *apPlayer);
	~cLuxPlayerSpawnPS();

	void Reset();

	void Start(const tString& asFileName);
	void Stop();

	void RespawnAll();

	bool IsActive(){ return ohpl_a2c_sps_active(mpS);}
	const tString& GetFileName(){ return msFileName;}

	void Update(float afTimeStep);
private:
	static size_t HostCreate(void *apCtx, const cVector3f *apPos);
	static bool HostExists(void *apCtx, size_t alPS);
	static void HostKill(void *apCtx, size_t alPS);
	static void HostDestroy(void *apCtx, size_t alPS);
	static float HostRand(void *apCtx, float afMin, float afMax);
	static void HostFeet(void *apCtx, cVector3f *apOut);

	bool LoadSpawnPSFile(const tString& asFileName, OhplA2cSpsCfg &aCfg);

	OhplA2cSps *mpS;
	tString msFileName;
	
	tString msParticleSystem;
	cColor mPSColor;
	bool mbFadePS;
	float mfPSMinFadeStart;
	float mfPSMinFadeEnd;
	float mfPSMaxFadeStart;
	float mfPSMaxFadeEnd;
};

//----------------------------------------------

class cLuxPlayerHurt : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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


	float mfNoiseAlpha;
	cColor mNoiseColor;
	
	float &mfAlpha = mS.k.mfAlpha;

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
friend struct OhplA2bDump;
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
	float mfWorldSoundVolume;
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
friend struct OhplA2bDump;
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
friend struct OhplA2bDump;
friend class cLuxPlayer_SaveData;
public:
	cLuxPlayerSanity(cLuxPlayer *apPlayer);
	~cLuxPlayerSanity();

	void Reset();

	void StartHit();
	void SetSanityLost();

	void Update(float afTimeStep);

	float GetAtLowSanityCount(){ return mS.mfAtLowSanityCount;}

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	bool ScanForEnemies();

	OhplGa2Sanity mS;
	float &mfAtLowSanityCount = mS.mfAtLowSanityCount;
	float mfNearCritterDecrease;
};

//----------------------------------------------

class cLuxPlayerLantern : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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
	
	uint32_t &mbDisabled = mS.mbDisabled;
	uint32_t &mbActive = mS.k.mbActive;
	float &mfAlpha = mS.k.mfAlpha;
	cLightPoint *mpLight;
	
};

//----------------------------------------------

class cLuxPlayerDeath : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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

	float GetFadeAlpha(){ return mS.mfFadeAlpha;}

	void DisableStartSound(){ mbSkipStartSound = true; }

	bool ShowHint(){ return mS.mbShowHint != 0;}
	void SetShowHint(bool abX){ mS.mbShowHint=abX;}

	void SetHint(const tString& asCat, const tString& asEntry);
	const tString& GetHintCat(){ return msHintCat; }
	const tString& GetHintEntry(){ return msHintEntry; }



private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	void ResetGame();

	OhplGa2Death mS;
	bool mbToMainMenu;
	bool mbSkipStartSound;

	tString msStartSound;
	tString msAwakenSound;

	tString msHintCat;
	tString msHintEntry;
	tWString msCurrentHintText;

	cGuiGfxElement *mpWhiteModGfx;
	iFontData *mpFont;

	cSoundEntry *mpVoiceEntry;
	int mlVoiceEntryId;
};


//----------------------------------------------

class cLuxPlayerLean : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);

	iCollideShape *mpHeadShape;
	OhplA2cLean mS;
};

//----------------------------------------------

class cLuxPlayerDamageData
{
public:
	std::vector<cGuiGfxElement*> mvImages;
};


class cLuxPlayerHudEffect : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
public:
	cLuxPlayerHudEffect(cLuxPlayer *apPlayer);
	~cLuxPlayerHudEffect();

	void AddDamageSplash(eLuxDamageType aType);
	void Flash(const cColor& aColor, eGuiMaterial aFlashMaterial, float afInTime, float afOutTime);

	void OnDraw(float afFrameTime);
	void Update(float afTimeStep);
	void Reset();
	
private:
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	void DrawSplashes(float afFrameTime);
	void DrawFlash(float afFrameTime);

	void LoadDamageData(cLuxPlayerDamageData *apData, const tString& asName);

	OhplA2cHud mS;
	std::vector<cLuxPlayerDamageData> mvDamageTypes;

	cGuiGfxElement *mpFlashGfx;
	cColor mFlashColor;
	eGuiMaterial mFlashMaterial;
};


//----------------------------------------------

class cLuxPlayerLightLevel : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
public:	
	cLuxPlayerLightLevel(cLuxPlayer *apPlayer);
	~cLuxPlayerLightLevel();
	
	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);

	float GetExtendedLightLevel(){ return mS.mfExtendedLightLevel;}
	float GetNormalLightLevel(){ return mS.mfNormalLightLevel;}

private:
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	OhplA2bLight mS;
	void SampleLevels(const cVector3f *apPos, float *apExt, float *apNormal);

};

//----------------------------------------------

class cLuxPlayerInDarkness : public iLuxPlayerHelper
{
friend struct OhplA2bDump;
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


	void SetActive(bool abX);

private:
	static void HostOp(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, float afC, float afD);
	static float HostQuery(void *apCtx, uint32_t alKind, float afA, float afB);
	static void HostFill(void *apCtx, uint32_t alKind, float *apBuf);
	OhplA2bDark mS;
	cSoundHandler *mpSoundHandler;

	
	float mfAmbientLightRadius;
	float mfAmbientLightIntensity;
	float mfAmbientLightFadeInTime;
	float mfAmbientLightFadeOutTime;
	cColor mAmbientLightColor;

	tString msLoopSoundFile;
	float mfLoopSoundVolume;
	float mfLoopSoundFadeInSpeed;
	float mfLoopSoundFadeOutSpeed;



	cSoundEntry *mpLoopSound;

	cLightPoint *mpAmbientLight;

	uint32_t &mbActive = mS.mbActive;
};

//----------------------------------------------


#endif // LUX_MAP_HANDLER_H
