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

#ifndef LUX_MUSIC_HANDLER_H
#define LUX_MUSIC_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxFxA12b.h"
#include "LuxA12bDump.h"

//----------------------------------------------

class cLuxMusic
{
public:
	cLuxMusic() : mbLoop(false), mbResume(false), mfVolume(0), mbSpecialEffect(false) {}

	tString msFile;
	bool mbLoop;
	bool mbResume;
	float mfVolume;
	bool mbSpecialEffect;	//If started by enemy close or similar.
};

//----------------------------------------

class cLuxMusicHandler : public iLuxUpdateable
{
public:	
	cLuxMusicHandler() : iLuxUpdateable("LuxMusicHandler")
	{
		OhplA12bInitHost();
		mlH = ohpl_a12b_music_new();
	}
	~cLuxMusicHandler() { ohpl_a12b_music_free(mlH); }
	
	void OnStart(){}
	void Update(float afTimeStep) { if(OhplA12bEnabled() == false) UpdateA12(afTimeStep); }
	void UpdateA12(float afTimeStep) { ohpl_a12b_music_update(mlH, afTimeStep); }
	void Reset() { ohpl_a12b_music_reset(mlH); }

	float GetVolumeMul(){ return State().volumeMul; }

	int GetMaxPrio(){ return State().maxPrio; }
	int GetCurrentMaxPrio(){ return State().curPrio; }

	cLuxMusic GetMusic(int alPrio)
	{
		cLuxMusic music;
		OhplA12bSlot slot;
		if(ohpl_a12b_music_slot(mlH, alPrio, &slot, PutFile, &music.msFile) == 0) return music;
		music.mbLoop = slot.loop != 0;
		music.mbResume = slot.resume != 0;
		music.mfVolume = slot.volume;
		music.mbSpecialEffect = slot.special != 0;
		return music;
	}
	void SetMusic(int alPrio, const cLuxMusic &aMusic)
	{
		OhplA12bSlot slot = { aMusic.mbLoop, aMusic.mbResume, aMusic.mbSpecialEffect, aMusic.mfVolume };
		ohpl_a12b_music_set_slot(mlH, alPrio, &slot, aMusic.msFile.data(), aMusic.msFile.size());
	}

	void OnMapEnter(cLuxMap *apMap) { ohpl_a12b_music_on_map(mlH); }
	void OnMapLeave(cLuxMap *apMap)
	{
		ohpl_a12b_music_on_map(mlH);
		for(int i=0; i<60; ++i) Update(0.1f);
	}

	void Play(const tString &asFile, bool abLoop,float afVolume, float afFadeTime, int alPrio, bool abResume, bool abSpecialEffect)
	{
		ohpl_a12b_music_play(mlH, asFile.data(), asFile.size(), abLoop, afVolume, afFadeTime, alPrio, abResume, abSpecialEffect);
	}
	void Stop(float afFadeTime, int alPrio) { ohpl_a12b_music_stop(mlH, afFadeTime, alPrio); }
	uint64_t GetHandle(){ return mlH; }

	void AddEnemy(eLuxEnemyMusic aType, iLuxEnemy *apEnemy) { ohpl_a12b_music_enemy(mlH, 0, aType, (size_t)apEnemy); }
	void RemoveEnemy(eLuxEnemyMusic aType, iLuxEnemy *apEnemy) { ohpl_a12b_music_enemy(mlH, 1, aType, (size_t)apEnemy); }
	bool EnemyExist(eLuxEnemyMusic aType, iLuxEnemy *apEnemy) { return ohpl_a12b_music_enemy(mlH, 2, aType, (size_t)apEnemy) != 0; }

	// Save game access
	cMusicHandler *Engine() { return gpBase->mpEngine->GetSound()->GetMusicHandler(); }
	void SetSaveState(int alCurrentMaxPrio, bool abClosePlaying, bool abAttackPlaying, bool abSearchPlaying)
	{
		ohpl_a12b_music_set_state(mlH, alCurrentMaxPrio, abClosePlaying, abAttackPlaying, abSearchPlaying);
	}
	bool EnemyPlaying(eLuxEnemyMusic aType) { return State().playing[aType] != 0; }
	bool EnemyClosePlaying() { return State().closePlaying != 0; }
	int GetSlotNum() { return State().slotCount; }
	void GetEnemies(eLuxEnemyMusic aType, std::vector<iLuxEnemy*> &avEnemies)
	{
		size_t lNum = ohpl_a12b_music_enemies(mlH, aType, NULL, 0);
		std::vector<size_t> vRaw(lNum);
		if(lNum > 0) ohpl_a12b_music_enemies(mlH, aType, &vRaw[0], lNum);
		for(size_t i=0; i<lNum; ++i) avEnemies.push_back((iLuxEnemy*)vRaw[i]);
	}

private:
	static void PutFile(void *apCtx, const unsigned char *apP, size_t alN) { ((tString*)apCtx)->assign((const char*)apP, alN); }
	OhplA12bMusicState State()
	{
		OhplA12bMusicState state = {};
		ohpl_a12b_music_state(mlH, &state);
		return state;
	}

	uint64_t mlH;
};

//----------------------------------------------


#endif // LUX_MUSIC_HANDLER_H
