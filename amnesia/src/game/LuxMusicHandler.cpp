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

#include "LuxMusicHandler.h"
#include "RustLuxFx.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxEnemy.h"


//////////////////////////////////////////////////////////////////////////
// MUSIC
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMusic::cLuxMusic()
{
	Reset();
}

void cLuxMusic::Reset()
{
	msFile = "";
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMusicHandler::cLuxMusicHandler() : iLuxUpdateable("LuxMusicHandler")
{
	mpMusicHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();

	mlMaxPrio = 8 + eLuxEnemyMusic_LastEnum;
	mfVolumeMul = gpBase->mpGameCfg->GetFloat("Sound","InGameMusicVolume",1.0f);

	mvGameMusic.resize(mlMaxPrio+1);

}

//-----------------------------------------------------------------------

cLuxMusicHandler::~cLuxMusicHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMusicHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::Reset()
{
	for(size_t i=0; i< mvGameMusic.size(); ++i)
	{
		mvGameMusic[i].Reset();
	}

	mlCurrentMaxPrio = -1;

	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
	{
		m_setEnemies[i].clear();

		mbEnemyPlaying[i] = false;
		mfEnemyPlayCount[i] = 0;
		mfEnemyStopCount[i] = 0;
	}

	gpBase->mpEngine->GetSound()->GetMusicHandler()->ResetResumeData();

	mbEnemyClosePlaying = false;
	mfEnemyCloseCount =0;
	mfEnemyGoneCount =0;

	mfUpdateDangerCount = 0;
	mfUpdateAttackCount = 0;
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::Update(float afTimeStep)
{
	if(gpBase->mpPlayer->IsDead()) return;

	///////////////////////////////////
	//Check if close by music should be played.
	UpdateDangerMusic(afTimeStep);

	///////////////////////////////////
	//Check if attack music should be played.
	UpdateEnemyMusic(afTimeStep, eLuxEnemyMusic_Search);
	UpdateEnemyMusic(afTimeStep, eLuxEnemyMusic_Attack);

	///////////////////////////////////
	//Check if current song is over
	bool bNoSong = mlCurrentMaxPrio >= 0 && mpMusicHandler->GetCurrentSong()==NULL;
	if(ohpl_ga9b_music_song_over(mlCurrentMaxPrio, bNoSong))
	{
		mvGameMusic[mlCurrentMaxPrio].msFile = "";
		mlCurrentMaxPrio = -1;
		PlayHighestPriority();
	}
}


//-----------------------------------------------------------------------

void cLuxMusicHandler::Play(const tString &asFile, bool abLoop,float afVolume, float afFadeTime, int alPrio, bool abResume, bool abSpecialEffect)
{
	alPrio = ohpl_ga9b_music_prio(alPrio, mlMaxPrio);
	if(alPrio < 0) return;

	OhplGa9bPlayOut out;
	if(ohpl_ga9b_music_play(mlCurrentMaxPrio, alPrio, mvGameMusic[alPrio].msFile == asFile, afVolume, mfVolumeMul, afFadeTime, &out) == false) return;

	//If higher or equal to song playing, start playing
	if(out.play)
	{
		mpMusicHandler->Play(asFile,out.volume,out.fadeSpeed,abLoop, abResume);
	}
	mlCurrentMaxPrio = out.curPrio;

	//Set game music properites
	mvGameMusic[alPrio].mbSpecialEffect = abSpecialEffect;
	mvGameMusic[alPrio].mbLoop = abLoop;
	mvGameMusic[alPrio].mbResume = abResume;
	mvGameMusic[alPrio].mfVolume = out.volume;
	mvGameMusic[alPrio].msFile = asFile;
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::Stop(float afFadeTime, int alPrio)
{
	alPrio = ohpl_ga9b_music_prio(alPrio, mlMaxPrio);
	if(alPrio < 0) return;

	bool bHasFile = mvGameMusic[alPrio].msFile != "";
	OhplGa9bStopOut out = ohpl_ga9b_music_stop(mlCurrentMaxPrio, alPrio, bHasFile, bHasFile ? mvGameMusic[alPrio].mfVolume : 0.0f, afFadeTime);
	if(out.clear)
		mvGameMusic[alPrio].msFile = "";

	if(out.stop)
	{
		mpMusicHandler->Stop(out.fadeSpeed);

		//Play next song in priority if there is any.
		mlCurrentMaxPrio = -1;
		PlayHighestPriority();
	}
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::AddEnemy(eLuxEnemyMusic aType, iLuxEnemy *apEntity)
{
	m_setEnemies[aType].insert(apEntity);
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::RemoveEnemy(eLuxEnemyMusic aType, iLuxEnemy *apEntity)
{
	m_setEnemies[aType].erase(apEntity);
}

bool cLuxMusicHandler::EnemyExist(eLuxEnemyMusic aType, iLuxEnemy *apEntity)
{
	tLuxEnemySetIt it = m_setEnemies[aType].find(apEntity);
	if(it == m_setEnemies[aType].end()) return false;

	return true;
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::OnMapEnter(cLuxMap *apMap)
{
	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
	{
		m_setEnemies[i].clear();
		Stop(2.0f, 8+i);
	}

	//Update(2.0f);
}

void cLuxMusicHandler::OnMapLeave(cLuxMap *apMap)
{
	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
	{
		m_setEnemies[i].clear();
		Stop(2.0f, 8+i);
	}


	for(int i=0; i<60; ++i)Update(0.1f);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMusicHandler::UpdateDangerMusic(float afTimeStep)
{
	OhplGa9bEnemyMusic e = EnemyToRust();
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	int lMode = ohpl_ga9b_music_danger_begin(gpBase->mbHardMode == true, &e, pMap != NULL);
	if(lMode == kOhplGa9bDangerReturn) return;

	std::vector<uint32_t> vNonEmpty;
	std::vector<int> vPrios;
	std::vector<tString> vMusics;
	bool bFound = false;

	cLuxEnemyIterator enemyIt = pMap->GetEnemyIterator();
	while(enemyIt.HasNext())
	{
		iLuxEnemy *pEnemy = enemyIt.Next();
		if(pEnemy->GetHealth() <= 0 || pEnemy->IsActive()==false) continue;

		if(pEnemy->GetPlayerInRange())
		{
			bFound = true;
			vNonEmpty.push_back(pEnemy->GetDangerMusic()!= "");
			vPrios.push_back(pEnemy->GetDangerMusicPrio());
			vMusics.push_back(pEnemy->GetDangerMusic());
		}
	}

	OhplGa9bOps ops;
	int lIdx = -1;
	if(lMode == kOhplGa9bDangerScanIdle)
	{
		lIdx = ohpl_ga9b_music_pick_danger(vNonEmpty.empty() ? NULL : &vNonEmpty[0], vPrios.empty() ? NULL : &vPrios[0], vNonEmpty.size());
		ohpl_ga9b_music_danger_idle(&e, lIdx >= 0, afTimeStep, &ops);
	}
	else
	{
		ohpl_ga9b_music_danger_playing(&e, bFound, afTimeStep, &ops);
	}
	EnemyFromRust(e);

	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bMusicPlayDanger)
			Play(vMusics[lIdx],true,1.0f, 6.0f, 5, true, true);
		else if(ops.ops[i].kind == kOhplGa9bMusicStopDanger)
			Stop(6.0f, 5);
	}
}

OhplGa9bEnemyMusic cLuxMusicHandler::EnemyToRust()
{
	OhplGa9bEnemyMusic e;
	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
	{
		e.playing[i] = mbEnemyPlaying[i];
		e.playCount[i] = mfEnemyPlayCount[i];
		e.stopCount[i] = mfEnemyStopCount[i];
	}
	e.closePlaying = mbEnemyClosePlaying;
	e.closeCount = mfEnemyCloseCount;
	e.goneCount = mfEnemyGoneCount;
	return e;
}

void cLuxMusicHandler::EnemyFromRust(const OhplGa9bEnemyMusic &e)
{
	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
	{
		mbEnemyPlaying[i] = e.playing[i] != 0;
		mfEnemyPlayCount[i] = e.playCount[i];
		mfEnemyStopCount[i] = e.stopCount[i];
	}
	mbEnemyClosePlaying = e.closePlaying != 0;
	mfEnemyCloseCount = e.closeCount;
	mfEnemyGoneCount = e.goneCount;
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::UpdateEnemyMusic(float afTimeStep, eLuxEnemyMusic aType)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();

	OhplGa9bEnemyMusic e = EnemyToRust();
	OhplGa9bOps ops;
	int lMode = ohpl_ga9b_music_enemy_begin(&e, (int)aType, pMap != NULL, m_setEnemies[aType].empty(), afTimeStep, &ops);
	EnemyFromRust(e);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		if(ops.ops[i].kind == kOhplGa9bMusicStopEnemy)
			Stop(5.0f, ops.ops[i].i);
	}
	if(lMode != kOhplGa9bEnemyScan) return;

	//////////////////////
	// Go through attackers and get music with highest priority
	std::vector<uint32_t> vNonEmpty;
	std::vector<int> vPrios;
	std::vector<tString> vMusics;
	tLuxEnemySetIt it = m_setEnemies[aType].begin();
	for(; it != m_setEnemies[aType].end(); ++it)
	{
		iLuxEnemy *pEnemy = *it;
		vNonEmpty.push_back(pEnemy->GetMusic(aType) != "");
		vPrios.push_back(pEnemy->GetMusicPrio(aType));
		vMusics.push_back(pEnemy->GetMusic(aType));
	}

	int lIdx = ohpl_ga9b_music_pick_enemy(vNonEmpty.empty() ? NULL : &vNonEmpty[0], vPrios.empty() ? NULL : &vPrios[0], vNonEmpty.size());
	if(lIdx < 0) return;

	e = EnemyToRust();
	ohpl_ga9b_music_enemy_found(&e, (int)aType, &ops);
	EnemyFromRust(e);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		switch(ops.ops[i].kind)
		{
		case kOhplGa9bMusicStopClose:
			Stop(2.0f, 5);
			break;
		case kOhplGa9bMusicStopLower:
			Stop(2.0f, ops.ops[i].i);
			break;
		case kOhplGa9bMusicPlayEnemy:
			Play(vMusics[lIdx], true, 1.0f, 1.5f, ops.ops[i].i, true, true);
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMusicHandler::PlayHighestPriority()
{
	std::vector<OhplGa9bSlot> vSlots(mvGameMusic.size());
	for(size_t i=0; i<mvGameMusic.size(); ++i)
	{
		cLuxMusic &music = mvGameMusic[i];
		bool bHasFile = music.msFile != "";
		vSlots[i].hasFile = bHasFile;
		vSlots[i].loop = bHasFile ? music.mbLoop : false;
		vSlots[i].resume = bHasFile ? music.mbResume : false;
		vSlots[i].volume = bHasFile ? music.mfVolume : 0.0f;
	}

	OhplGa9bOps ops;
	ohpl_ga9b_music_play_highest(&vSlots[0], vSlots.size(), mlMaxPrio, &ops);
	for(uint32_t i=0; i<ops.n; ++i)
	{
		int lPrio = ops.ops[i].i;
		if(ops.ops[i].kind == kOhplGa9bMusicPlay)
		{
			//If looped, play
			mpMusicHandler->Play(	mvGameMusic[lPrio].msFile, mvGameMusic[lPrio].mfVolume,	0.3f,true, mvGameMusic[lPrio].mbResume);
			mlCurrentMaxPrio = lPrio;
		}
		else
		{
			//If not looped remove
			mvGameMusic[lPrio].msFile = "";
		}
	}
}

//-----------------------------------------------------------------------

