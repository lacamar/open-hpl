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

#ifndef LUX_JOURNAL_H
#define LUX_JOURNAL_H

#include "LuxBase.h"
#include "RustLuxInventory.h"

enum eLuxJournalState
{
	eLuxJournalState_Main,
	eLuxJournalState_Notes,
	eLuxJournalState_Diaries,
	eLuxJournalState_QuestLog,
	eLuxJournalState_OpenNote,
	eLuxJournalState_OpenDiary,
	eLuxJournalState_OpenNarratedDiary,

	eLuxJournalState_LastEnum,
};

// The journal lives in Rust (rust/src/luxui/jrn.rs); this is the module shell and its public calls.
class cLuxJournal : public iLuxUpdateable
{
public:
	cLuxJournal();
	~cLuxJournal();

	void OnClearFonts(){}
	void LoadFonts(){ ohpl_jrn_load_fonts(); }
	void OnStart(){}
	void Reset(){ ohpl_jrn_reset(); }
	void OnGameStart(){}
	void Update(float afTimeStep){ ohpl_jrn_update(afTimeStep); }
	void OnEnterContainer(const tString& asOldContainer){ ohpl_jrn_on_enter(); }
	void OnLeaveContainer(const tString& asNewContainer){ ohpl_jrn_on_leave(); }
	void OnDraw(float afFrameTime){ ohpl_jrn_on_draw(afFrameTime); }

	cGuiSet* GetSet(){ return (cGuiSet*)ohpl_jrn_get_set(); }

	void ExitPressed(bool abInstantExit){ ohpl_jrn_exit_pressed(abInstantExit); }
	void Exit(){ ohpl_jrn_exit(); }
	void SetForceInstantExit(bool abX){ ohpl_jrn_set_force_instant_exit(abX); }

	void AddNote(const tString& asNameAndTextEntry, const tString& asImage){ ohpl_jrn_add_note(OhplA11aS(asNameAndTextEntry), OhplA11aS(asImage)); }
	void AddDiary(const tString& asNameAndTextEntry, const tString& asImage, int &alCurrentEntryIdx)
	{
		alCurrentEntryIdx = ohpl_jrn_add_diary(OhplA11aS(asNameAndTextEntry), OhplA11aS(asImage));
	}
	bool AddQuestNote(const tString& asName, const tString& asNameAndTextEntry){ return ohpl_jrn_add_quest(OhplA11aS(asName), OhplA11aS(asNameAndTextEntry)) != 0; }
	bool DisableQuestNote(const tString& asName){ return ohpl_jrn_disable_quest(OhplA11aS(asName)) != 0; }
	// -1 unknown, 0 disabled, 1 active
	int QuestState(const tString& asName){ return ohpl_jrn_quest_state(OhplA11aS(asName)); }

	void ChangeState(eLuxJournalState aState){ ohpl_jrn_change_state(aState); }
	void OpenNoteIdx(int alIdx, bool abNarration){ ohpl_jrn_open_note_idx(alIdx, abNarration); }
	void OpenDiaryIdx(int alCat, int alIdx, bool abNarration){ ohpl_jrn_open_diary_idx(alCat, alIdx, abNarration); }
	void OpenLastReadText(){ ohpl_jrn_open_last_read(); }
};

#endif // LUX_JOURNAL_H
