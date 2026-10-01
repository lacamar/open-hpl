#include "SomaSoundscape.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptBind.h"
#include "SomaSound.h"

#include "impl/OpenALSoundEnvironment.h"

#include <algorithm>
#include <cstring>

// cSoundReverbProperties: EnvDiffusion, Room, RoomHF, RoomLF (mB), DecayTime, DecayHFRatio, DecayLFRatio,
// Reflections (mB), ReflectionsDelay, Reverb (mB), ReverbDelay, ModTime, ModDepth, HFRef, LFRef, Diffusion, Density
static const float kReverbPresets[42][17] = {
    {1.0f, -10000.0f, -10000.0f, 0.0f, 1.0f, 1.0f, 1.0f, -2602.0f, 0.007f, 200.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 0.0f, 0.0f}, // 0 Off
    {1.0f, -1000.0f, -100.0f, 0.0f, 1.49f, 0.83f, 1.0f, -2602.0f, 0.007f, 200.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 1 Generic
    {1.0f, -1000.0f, -6000.0f, 0.0f, 0.17f, 0.1f, 1.0f, -1204.0f, 0.001f, 207.0f, 0.002f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 2 PaddedCell
    {1.0f, -1000.0f, -454.0f, 0.0f, 0.4f, 0.83f, 1.0f, -1646.0f, 0.002f, 53.0f, 0.003f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 3 Room
    {1.0f, -1000.0f, -1200.0f, 0.0f, 1.49f, 0.54f, 1.0f, -370.0f, 0.007f, 1030.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 60.0f}, // 4 BathRoom
    {1.0f, -1000.0f, -6000.0f, 0.0f, 0.5f, 0.1f, 1.0f, -1376.0f, 0.003f, -1104.0f, 0.004f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 5 LivingRoom
    {1.0f, -1000.0f, -300.0f, 0.0f, 2.31f, 0.64f, 1.0f, -711.0f, 0.012f, 83.0f, 0.017f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 6 StoneRoom
    {1.0f, -1000.0f, -476.0f, 0.0f, 4.32f, 0.59f, 1.0f, -789.0f, 0.02f, -289.0f, 0.03f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 7 Auditorium
    {1.0f, -1000.0f, -500.0f, 0.0f, 3.92f, 0.7f, 1.0f, -1230.0f, 0.02f, -2.0f, 0.029f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 8 ConcertHall
    {1.0f, -1000.0f, 0.0f, 0.0f, 2.91f, 1.3f, 1.0f, -602.0f, 0.015f, -302.0f, 0.022f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 9 Cave
    {1.0f, -1000.0f, -698.0f, 0.0f, 7.24f, 0.33f, 1.0f, -1166.0f, 0.02f, 16.0f, 0.03f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 10 Arena
    {1.0f, -1000.0f, -1000.0f, 0.0f, 10.05f, 0.23f, 1.0f, -602.0f, 0.02f, 198.0f, 0.03f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 11 Hangar
    {1.0f, -1000.0f, -4000.0f, 0.0f, 0.3f, 0.1f, 1.0f, -1831.0f, 0.002f, -1630.0f, 0.03f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 12 CarpettedHallway
    {1.0f, -1000.0f, -300.0f, 0.0f, 1.49f, 0.59f, 1.0f, -1219.0f, 0.007f, 441.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 13 Hallway
    {1.0f, -1000.0f, -237.0f, 0.0f, 2.7f, 0.79f, 1.0f, -1214.0f, 0.013f, 395.0f, 0.02f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 14 StoneCorridor
    {0.3f, -1000.0f, -270.0f, 0.0f, 1.49f, 0.86f, 1.0f, -1204.0f, 0.007f, -4.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 15 Alley
    {0.3f, -1000.0f, -3300.0f, 0.0f, 1.49f, 0.54f, 1.0f, -2560.0f, 0.162f, -229.0f, 0.088f, 0.25f, 0.0f, 5000.0f, 250.0f, 79.0f, 100.0f}, // 16 Forest
    {0.5f, -1000.0f, -800.0f, 0.0f, 1.49f, 0.67f, 1.0f, -2273.0f, 0.007f, -1691.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 50.0f, 100.0f}, // 17 City
    {0.27f, -1000.0f, -2500.0f, 0.0f, 1.49f, 0.21f, 1.0f, -2780.0f, 0.3f, -1434.0f, 0.1f, 0.25f, 0.0f, 5000.0f, 250.0f, 27.0f, 100.0f}, // 18 Mountains
    {1.0f, -1000.0f, -1000.0f, 0.0f, 1.49f, 0.83f, 1.0f, -10000.0f, 0.061f, 500.0f, 0.025f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 19 Quarry
    {0.21f, -1000.0f, -2000.0f, 0.0f, 1.49f, 0.5f, 1.0f, -2466.0f, 0.179f, -1926.0f, 0.1f, 0.25f, 0.0f, 5000.0f, 250.0f, 21.0f, 100.0f}, // 20 Plain
    {1.0f, -1000.0f, 0.0f, 0.0f, 1.65f, 1.5f, 1.0f, -1363.0f, 0.008f, -1153.0f, 0.012f, 0.25f, 0.0f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 21 ParkingLot
    {0.8f, -1000.0f, -1000.0f, 0.0f, 2.81f, 0.14f, 1.0f, 429.0f, 0.014f, 1023.0f, 0.021f, 0.25f, 0.0f, 5000.0f, 250.0f, 80.0f, 60.0f}, // 22 SewerPipe
    {1.0f, -1000.0f, -4000.0f, 0.0f, 1.49f, 0.1f, 1.0f, -449.0f, 0.007f, 1700.0f, 0.011f, 1.18f, 0.348f, 5000.0f, 250.0f, 100.0f, 100.0f}, // 23 Underwater
    {1.0f, -10000.0f, -10000.0f, 0.0f, 1.0f, 1.0f, 1.0f, -2602.0f, 0.007f, 200.0f, 0.011f, 0.25f, 0.0f, 5000.0f, 250.0f, 0.0f, 0.0f}, // 24 unused
    {1.0f, -400.0f, -900.0f, -1700.0f, 0.65f, 0.83f, 1.0f, -1900.0f, 0.0f, 200.0f, 0.01f, 0.25f, 0.0f, 4000.0f, 900.0f, 90.0f, 100.0f}, // 25 ExtraRoomSmall
    {1.0f, -200.0f, -2000.0f, -2000.0f, 0.78f, 0.83f, 1.0f, -2000.0f, 0.0f, 250.0f, 0.01f, 0.25f, 0.0f, 3000.0f, 900.0f, 95.0f, 100.0f}, // 26 ExtraRoomMedium
    {1.0f, -150.0f, -1700.0f, -2000.0f, 1.5f, 0.83f, 1.0f, -2000.0f, 0.03f, 120.0f, 0.04f, 0.25f, 0.0f, 2600.0f, 870.0f, 90.0f, 98.0f}, // 27 ExtraRoomLarge
    {1.0f, -400.0f, -300.0f, 0.0f, 2.8f, 0.64f, 1.0f, -761.0f, 0.03f, 350.0f, 0.06f, 0.25f, 0.0f, 4950.0f, 627.0f, 100.0f, 100.0f}, // 28 ExtraMarbleLarge
    {1.0f, 0.0f, -2800.0f, -5000.0f, 1.49f, 0.5f, 1.0f, -1400.0f, 0.24f, -500.0f, 0.8f, 0.25f, 0.0f, 2300.0f, 1290.0f, 21.0f, 100.0f}, // 29 ExtraOpenAreas
    {1.0f, 200.0f, 0.0f, 0.0f, 2.9f, 0.14f, 1.0f, 429.0f, 0.01f, 1323.0f, 0.02f, 0.25f, 0.0f, 5000.0f, 500.0f, 80.0f, 75.0f}, // 30 ExtraSewersBunkers
    {1.0f, -50.0f, -25.0f, -100.0f, 3.5f, 1.5f, 1.0f, 50.0f, 0.02f, -50.0f, 0.1f, 0.25f, 0.0f, 8000.0f, 1200.0f, 45.0f, 55.0f}, // 31 ExtraDeath
    {1.0f, -300.0f, 0.0f, -550.0f, 2.78f, 0.73f, 1.0f, -250.0f, 0.06f, 500.0f, 0.08f, 0.25f, 0.0f, 2500.0f, 650.0f, 58.0f, 80.0f}, // 32 ExtraBridge
    {1.0f, 200.0f, -500.0f, -100.0f, 9.0f, 0.23f, 1.0f, 550.0f, 0.2f, 500.0f, 0.27f, 0.25f, 0.0f, 5300.0f, 320.0f, 90.0f, 90.0f}, // 33 ExtraTunnelLarge
    {1.0f, 150.0f, -350.0f, 0.0f, 6.0f, 0.23f, 1.0f, 410.0f, 0.17f, 300.0f, 0.23f, 0.25f, 0.0f, 5800.0f, 580.0f, 90.0f, 99.0f}, // 34 ExtraTunnelMedium
    {1.0f, 200.0f, -300.0f, -500.0f, 4.5f, 0.23f, 1.0f, 400.0f, 0.01f, 300.0f, 0.87f, 0.25f, 0.0f, 5000.0f, 700.0f, 53.0f, 90.0f}, // 35 ExtraTunnelSmall
    {1.0f, -320.0f, -300.0f, -2800.0f, 1.6f, 2.2f, 1.0f, 150.0f, 0.1f, -300.0f, 0.26f, 0.25f, 0.0f, 1480.0f, 710.0f, 75.0f, 70.0f}, // 36 ExtraAlley
    {1.0f, 0.0f, -300.0f, -3200.0f, 0.8f, 2.2f, 1.0f, -1000.0f, 0.2f, -2000.0f, 0.2f, 0.25f, 0.0f, 2000.0f, 800.0f, 60.0f, 80.0f}, // 37 ExtraCarparks
    {1.0f, -900.0f, 1000.0f, 0.0f, 1.6f, 0.8f, 1.0f, -100.0f, 0.05f, 300.0f, 0.08f, 0.25f, 0.0f, 8000.0f, 1050.0f, 100.0f, 100.0f}, // 38 ExtraReflectiveInt
    {1.0f, -390.0f, -430.0f, -570.0f, 4.5f, 0.23f, 1.0f, -300.0f, 0.1f, -90.0f, 0.2f, 0.25f, 0.0f, 3000.0f, 650.0f, 90.0f, 99.0f}, // 39 ExtraWarehouse
    {1.0f, -300.0f, -350.0f, 0.0f, 12.0f, 0.23f, 1.0f, -500.0f, 0.2f, 200.0f, 0.37f, 0.25f, 0.0f, 5800.0f, 580.0f, 95.0f, 90.0f}, // 40 ExtraWarehouseMassive
    {1.0f, 0.0f, -300.0f, -3200.0f, 0.8f, 2.2f, 1.0f, -850.0f, 0.2f, -1800.0f, 0.2f, 0.25f, 0.0f, 2000.0f, 800.0f, 60.0f, 80.0f}, // 41 ExtraCarpark
};

static const char *kReverbNames[42] = {"Off", "Generic", "PaddedCell", "Room", "BathRoom", "LivingRoom", "StoneRoom", "Auditorium", "ConcertHall", "Cave", "Arena", "Hangar", "CarpettedHallway", "Hallway", "StoneCorridor", "Alley", "Forest", "City", "Mountains", "Quarry", "Plain", "ParkingLot", "SewerPipe", "Underwater", "", "ExtraRoomSmall", "ExtraRoomMedium", "ExtraRoomLarge", "ExtraMarbleLarge", "ExtraOpenAreas", "ExtraSewersBunkers", "ExtraDeath", "ExtraBridge", "ExtraTunnelLarge", "ExtraTunnelMedium", "ExtraTunnelSmall", "ExtraAlley", "ExtraCarparks", "ExtraReflectiveInt", "ExtraWarehouse", "ExtraWarehouseMassive", "ExtraCarpark"};

static int ReverbFromString(const tString &asName)
{
	for (int i = 0; i < 42; ++i)
		if (asName == kReverbNames[i])
			return i;
	return 0;
}

namespace
{
int LevelFromString(const tString &asLevel)
{
	if (asLevel.size() >= 2 && asLevel[0] >= '0' && asLevel[0] <= '3' && asLevel[1] == ':')
		return asLevel[0] - '0';
	return 2;
}

float FadeSpeed(float afTime) { return afTime > 0 ? 1.0f / afTime : 1000.0f; }

bool Contains(const std::vector<tString> &avX, const tString &asX) { return std::find(avX.begin(), avX.end(), asX) != avX.end(); }

char gHandlerTag;
}

cSomaSoundscape *cSomaSoundscape::Get()
{
	static cSomaSoundscape gInstance;
	return &gInstance;
}

void cSomaSoundscape::Load(cSomaLuxMap *apMap)
{
	mpMap = apMap;
	mpWorld = apMap ? apMap->GetWorld() : NULL;
	mvAreas.clear();
	mvActive.clear();
	for (cLevel &l : mLevels)
	{
		l.mpArea = NULL;
		l.mvIn.clear();
		l.mvOut.clear();
	}
	mpRevArea = NULL;
	mlRevType = -2;
	mlDefaultReverb = 0;
	mfDefaultReverbFade = 1;
	if (apMap == NULL)
		return;
	for (cSomaLuxEntity *pEnt : apMap->GetEntities())
	{
		if (pEnt->meType != eSomaLuxEntityType_Area || pEnt->msClassName != "Soundscape")
			continue;
		cResourceVarsObject &v = pEnt->mInstanceVars;
		cArea a;
		a.mpEnt = pEnt;
		a.mlLevel = LevelFromString(v.GetVarString("Level", ""));
		a.mlPrio = v.GetVarInt("Prio", 0);
		a.msBG = v.GetVarString("BG_Sound", "");
		a.mfBGVolume = v.GetVarFloat("BG_Volume", 1);
		a.mfBGFadeIn = v.GetVarFloat("BG_FadeInTime", 1);
		a.mfBGTransSpeed = v.GetVarFloat("BG_FadeTransitionSpeed", 1);
		a.mfParentVolMul = v.GetVarFloat("BG_ParentVolMul", 1);
		a.mvIn = cString::GetStringVec(v.GetVarString("SoundEnt_InNames", ""), a.mvIn, NULL);
		a.mvOut = cString::GetStringVec(v.GetVarString("SoundEnt_OutNames", ""), a.mvOut, NULL);
		a.mfEntFadeIn = v.GetVarFloat("SoundEnt_FadeInTime", 1);
		a.mfEntFadeOut = v.GetVarFloat("SoundEnt_FadeOutTime", 1);
		a.msPrefix = v.GetVarString("SoundPrefix", "");
		if (a.msPrefix == "[None]")
			a.msPrefix = "";
		a.mlPrefixPrio = v.GetVarInt("SoundPrefixPrio", 0);
		a.mbUseReverb = v.GetVarBool("UseReverb", false);
		a.mlReverbPrio = v.GetVarInt("ReverbPrio", 0);
		a.mlReverbType = ReverbFromString(v.GetVarString("ReverbType", "Off"));
		a.mfReverbAmount = v.GetVarFloat("ReverbAmount", 1);
		a.mfReverbFade = v.GetVarFloat("ReverbFadeTime", 1);
		mvAreas.push_back(a);
	}
	Log("SOMA soundscape: %d areas\n", (int)mvAreas.size());
}

bool cSomaSoundscape::Inside(cArea &aArea, const cVector3f &avPos)
{
	cVector3f vLocal = cMath::MatrixMul(cMath::MatrixInverse(aArea.mpEnt->GetMatrix()), avPos);
	const cVector3f &vSize = aArea.mpEnt->mvSize;
	for (int i = 0; i < 3; ++i)
		if (std::fabs(vLocal.v[i]) > vSize.v[i] * 0.5f)
			return false;
	return true;
}

bool cSomaSoundscape::BGLive(cLevel &aLevel)
{
	return aLevel.mpBG && cSomaSoundEvents::Get()->IsLive(aLevel.mpBG, aLevel.mlBGId);
}

void cSomaSoundscape::StopBG(cLevel &aLevel, float afParentProd)
{
	if (BGLive(aLevel))
		aLevel.mpBG->FadeOut(FadeSpeed(aLevel.mfBGFade));
	aLevel.mpBG = NULL;
	aLevel.msBG = "";
	aLevel.mfParentVolMul = 1;
}

// cLuxSoundscapeHandler: the old background fades out over the new area's fade-in time
void cSomaSoundscape::UpdateBG(int alLevel, cArea *apArea, float afParentProd)
{
	cLevel &l = mLevels[alLevel];
	if (apArea == NULL || apArea->msBG == "")
	{
		StopBG(l, afParentProd);
		return;
	}
	float fTarget = apArea->mfBGVolume * afParentProd;
	if (apArea->msBG == l.msBG && BGLive(l))
	{
		if (apArea->mfBGVolume == l.mfBGVolume)
			l.mpBG->FadeVolumeMulTo(fTarget, apArea->mfBGTransSpeed);
		else
			l.mpBG->FadeVolumeMulTo(fTarget, apArea->mfBGFadeIn > 0 ? fTarget / apArea->mfBGFadeIn : 10000);
	}
	else
	{
		if (BGLive(l))
			l.mpBG->FadeOut(FadeSpeed(apArea->mfBGFadeIn));
		l.mpBG = cSomaSoundEvents::Get()->PlayGui(apArea->msBG, 1.0f, eSoundEntryType_World, true);
		l.mlBGId = l.mpBG ? l.mpBG->GetId() : -1;
		if (l.mpBG)
		{
			l.mpBG->SetVolumeMul(fTarget);
			l.mpBG->FadeInTo(1, apArea->mfBGFadeIn > 0 ? 1.0f / apArea->mfBGFadeIn : 0);
		}
	}
	l.msBG = apArea->msBG;
	l.mfBGVolume = apArea->mfBGVolume;
	l.mfBGFade = apArea->mfBGFadeIn;
}

void cSomaSoundscape::FadeEntities(const std::vector<tString> &avA, const std::vector<tString> &avB, float afTime, bool abIn)
{
	if (mpWorld == NULL)
		return;
	for (const tString &sName : avA)
	{
		if (Contains(avB, sName))
			continue;
		cSoundEntity *pSound = mpWorld->GetSoundEntity(sName);
		if (pSound == NULL)
			continue;
		if (abIn)
			pSound->FadeIn(FadeSpeed(afTime));
		else
			pSound->FadeOut(FadeSpeed(afTime));
	}
}

void cSomaSoundscape::UpdateEntities(int alLevel, cArea *apArea)
{
	cLevel &l = mLevels[alLevel];
	if (apArea == NULL)
	{
		FadeEntities(l.mvOut, {}, l.mfEntFadeIn, true);
		FadeEntities(l.mvIn, {}, l.mfEntFadeOut, false);
		l.mvIn.clear();
		l.mvOut.clear();
		return;
	}
	FadeEntities(l.mvIn, apArea->mvIn, apArea->mfEntFadeOut, false);
	FadeEntities(apArea->mvIn, l.mvIn, apArea->mfEntFadeIn, true);
	FadeEntities(l.mvOut, apArea->mvOut, apArea->mfEntFadeIn, true);
	FadeEntities(apArea->mvOut, l.mvOut, apArea->mfEntFadeOut, false);
	l.mvIn = apArea->mvIn;
	l.mvOut = apArea->mvOut;
	l.mfEntFadeIn = apArea->mfEntFadeIn;
	l.mfEntFadeOut = apArea->mfEntFadeOut;
}

void cSomaSoundscape::Update(cSomaLuxMap *apMap, float afTimeStep)
{
	if (apMap != mpMap)
		Load(apMap);
	UpdateReverb(afTimeStep);
	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	cCamera *pCam = pPlayer ? pPlayer->GetCamera() : NULL;
	if (pCam == NULL)
		return;
	cVector3f vCam = pCam->GetPosition();
	iCharacterBody *pBody = pPlayer->GetCharacterBody();

	msPrefix = "";
	int lPrefixPrio = -9999999;
	for (cArea &a : mvAreas)
	{
		bool bInside = a.mpEnt->mbActive && Inside(a, vCam);
		if (bInside != a.mbInside)
		{
			a.mbInside = bInside;
			if (bInside)
				mvActive.push_back(&a);
			else
				mvActive.erase(std::find(mvActive.begin(), mvActive.end(), &a));
		}
		if (pBody && a.mpEnt->mbActive && a.mlPrefixPrio > lPrefixPrio && Inside(a, pBody->GetPosition()))
		{
			lPrefixPrio = a.mlPrefixPrio;
			msPrefix = a.msPrefix;
		}
	}

	// cLuxMapHandler::CalculateReverbSettings: first area taken, later ones only on strictly higher prio
	cArea *pRev = NULL;
	for (cArea *pArea : mvActive)
		if (pArea->mbUseReverb && (pRev == NULL || pArea->mlReverbPrio > pRev->mlReverbPrio))
			pRev = pArea;
	int lType = pRev ? pRev->mlReverbType : mlDefaultReverb;
	float fAmount = pRev ? pRev->mfReverbAmount : 1;
	if (lType != mlRevType || fAmount != mfRevAmount || pRev != mpRevArea)
	{
		bool bFirst = mlRevType == -2;
		mlRevType = lType;
		mfRevAmount = fAmount;
		mpRevArea = pRev;
		const float *pPreset = kReverbPresets[lType];
		for (int i = 0; i < kRevFields; ++i)
			mvRevTo[i] = pPreset[i];
		for (int i = 1; i <= 3; ++i)
			mvRevTo[i] = cMath::Clamp(-10000 + (pPreset[i] + 10000) * fAmount, -10000.0f, 0.0f);
		memcpy(mvRevFrom, mvRevCur, sizeof(mvRevCur));
		mfRevTime = bFirst ? 0 : pRev ? pRev->mfReverbFade : mfDefaultReverbFade;
		mfRevT = 0;
	}

	cArea *pBest[kLevels] = {};
	int lBestPrio[kLevels] = {-100000, -100000, -100000, -100000};
	for (cArea *pArea : mvActive)
		if (lBestPrio[pArea->mlLevel] < pArea->mlPrio)
		{
			lBestPrio[pArea->mlLevel] = pArea->mlPrio;
			pBest[pArea->mlLevel] = pArea;
		}
	bool bSame = true;
	for (int i = 0; i < kLevels; ++i)
		bSame = bSame && pBest[i] == mLevels[i].mpArea && (pBest[i] || BGLive(mLevels[i]) == false);
	if (bSame)
		return;

	for (int i = 0; i < kLevels; ++i)
		mLevels[i].mfParentVolMul = pBest[i] ? pBest[i]->mfParentVolMul : 1;
	for (int i = 0; i < kLevels; ++i)
	{
		float fParentProd = 1;
		for (int j = i + 1; j < kLevels; ++j)
			fParentProd *= mLevels[j].mfParentVolMul;
		UpdateBG(i, pBest[i], fParentProd);
	}
	for (int i = 0; i < kLevels; ++i)
	{
		UpdateEntities(i, pBest[i]);
		mLevels[i].mpArea = pBest[i];
	}
}

void cSomaSoundscape::SetDefaultReverb(int alPreset, float afFadeTime)
{
	mlDefaultReverb = cMath::Clamp(alPreset, 0, 41);
	mfDefaultReverbFade = afFadeTime;
}

void cSomaSoundscape::UpdateReverb(float afTimeStep)
{
	if (mfRevT >= 1)
		return;
	mfRevT = mfRevTime <= 0.0001f ? 1 : std::min(mfRevT + afTimeStep / mfRevTime, 1.0f);
	for (int i = 0; i < kRevFields; ++i)
		mvRevCur[i] = mvRevFrom[i] + (mvRevTo[i] - mvRevFrom[i]) * mfRevT;
	ApplyReverb();
}

// FMOD Ex reverb properties to EFX EAX reverb
void cSomaSoundscape::ApplyReverb()
{
	auto mB = [](float afX, float afMax) { return cMath::Clamp(powf(10, std::round(afX) / 2000), 0.0f, afMax); };
	const float *p = mvRevCur;
	static cOpenALSoundEnvironment env;
	env.SetGain(mB(p[1], 1));
	env.SetGainHF(mB(p[2], 1));
	env.SetGainLF(mB(p[3], 1));
	env.SetDecayTime(cMath::Clamp(p[4], 0.1f, 20.0f));
	env.SetDecayHFRatio(cMath::Clamp(p[5], 0.1f, 2.0f));
	env.SetDecayLFRatio(cMath::Clamp(p[6], 0.1f, 2.0f));
	env.SetReflectionsGain(mB(p[7], 3.16f));
	env.SetReflectionsDelay(cMath::Clamp(p[8], 0.0f, 0.3f));
	env.SetLateReverbGain(mB(p[9], 10));
	env.SetLateReverbDelay(cMath::Clamp(p[10], 0.0f, 0.1f));
	env.SetModulationTime(cMath::Clamp(p[11], 0.04f, 4.0f));
	env.SetModulationDepth(cMath::Clamp(p[12], 0.0f, 1.0f));
	env.SetHFReference(cMath::Clamp(p[13], 1000.0f, 20000.0f));
	env.SetLFReference(cMath::Clamp(p[14], 20.0f, 1000.0f));
	env.SetDiffusion(p[15] / 100);
	env.SetDensity(p[16] / 100);
	env.SetEchoTime(0.25f);
	env.SetEchoDepth(0);
	env.SetAirAbsorptionGainHF(0.994f);
	env.SetRoomRolloffFactor(0);
	env.SetDecayHFLimit(0);
	gpSomaBase->mpEngine->GetSound()->GetLowLevel()->SetSoundEnvironment(&env);
}

void cSomaSoundscape::SetPaused(bool abX)
{
	for (cLevel &l : mLevels)
		if (BGLive(l))
			l.mpBG->SetPaused(abX);
}

tString cSomaSoundscape::Describe()
{
	tString s = "prefix=" + msPrefix + " reverb=" + (mlRevType >= 0 ? kReverbNames[mlRevType] : "-") + "*" + cString::ToString(mfRevAmount) +
				(mpRevArea ? "(" + mpRevArea->mpEnt->msName + ")" : "") + " room=" + cString::ToString(mvRevCur[1]);
	for (int i = 0; i < kLevels; ++i)
		if (mLevels[i].mpArea || mLevels[i].mpBG)
			s += " L" + cString::ToString(i) + "=" + (mLevels[i].mpArea ? mLevels[i].mpArea->mpEnt->msName : tString("-")) + ":" + mLevels[i].msBG;
	return s;
}

void cSomaSoundscape::RegisterNatives(asIScriptEngine *e)
{
	SOMA_FUNC(e, "cLuxSoundscapeHandler@ cLux_GetSoundscapeHandler()", +[]() { return (void *)&gHandlerTag; });
	SOMA_FUNC(e, "void cLux_SetupDefaultGlobalReverb(eSoundReverbPreset aType, float afFadeTime)", +[](int alType, float afFade) { Get()->SetDefaultReverb(alType, afFade); });
	SOMA_METHOD(e, "cLuxSoundscapeHandler", "const tString& GetCurrentSoundPrefix()", +[](void *) -> const tString & { return Get()->GetPrefix(); });
}
