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

#ifndef LUX_INSANITY_HANDLER_H
#define LUX_INSANITY_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxFxA12b.h"
#include "LuxA12bDump.h"

//----------------------------------------------

class cLuxInsanityHandler : public iLuxUpdateable
{
public:	
	cLuxInsanityHandler() : iLuxUpdateable("LuxInsanityHandler")
	{
		OhplA12bInitHost();
		mlH = ohpl_a12b_ins_new();
	}
	~cLuxInsanityHandler() { ohpl_a12b_ins_free(mlH); }
	
	void OnStart(){}
	void Reset() { ohpl_a12b_ins_reset(mlH); }
	void Update(float afTimeStep) { if(OhplA12Enabled() == false) UpdateA12(afTimeStep); }
	void UpdateA12(float afTimeStep) { ohpl_a12b_ins_update(mlH, afTimeStep); }
	void OnDraw(float afFrameTime)
	{
		OhplA12Tick();
		ohpl_a12b_ins_draw(mlH);
	}

	void StartEvent() { ohpl_a12b_ins_start(mlH, 0, 0, NULL, 0); }
	void StartEvent(int alIdx) { ohpl_a12b_ins_start(mlH, 1, alIdx, NULL, 0); }
	void StartEvent(const tString &asName) { ohpl_a12b_ins_start(mlH, 2, 0, asName.data(), asName.size()); }
	void StopCurrentEvent() { ohpl_a12b_ins_stop(mlH); }

	int GetEventNum(){ return ohpl_a12b_ins_count(mlH); }
	tString GetEventName(int alIdx) { return Event(alIdx, NULL, 0); }
	tString GetEventSet(int alIdx) { return Event(alIdx, NULL, 1); }
	float GetEventMaxSanity(int alIdx) { OhplA12bEventInfo info = {}; Event(alIdx, &info, 0); return info.maxSanity; }
	bool EventIsUsed(int alIdx) { OhplA12bEventInfo info = {}; Event(alIdx, &info, 0); return info.used != 0; }
	bool EventIsOver(int alIdx) { OhplA12bEventInfo info = {}; Event(alIdx, &info, 0); return info.over != 0; }
	void SetEventUsed(int alIdx, bool abX) { ohpl_a12b_ins_set_used(mlH, alIdx, abX); }

	void ReloadEvents() { ohpl_a12b_ins_reload(mlH); }

	void EnableSet(const tString &asSet) { ohpl_a12b_ins_set(mlH, 1, asSet.data(), asSet.size()); }
	void DisableSet(const tString &asSet) { ohpl_a12b_ins_set(mlH, 0, asSet.data(), asSet.size()); }
	void ClearDisabledSets() { ohpl_a12b_ins_set(mlH, 2, NULL, 0); }
	void GetDisabledSets(tStringVec &avSets) { ohpl_a12b_ins_sets(mlH, PushString, &avSets); }

	/////////////////
	// Properties
	bool NewEventIsPossible() { return ohpl_a12b_ins_possible(mlH) != 0; }
	float GetNewEventCount(){ return ohpl_a12b_ins_new_count(mlH); }
	void SetNewEventCount(float afX){ ohpl_a12b_ins_set_new_count(mlH, afX); }
	int GetCurrentEvent(){ return ohpl_a12b_ins_current(mlH); }

private:
	struct Strings { tString msName, msSet; int mlNum; Strings() : mlNum(0) {} };
	static void PutStrings(void *apCtx, const unsigned char *apP, size_t alN)
	{
		Strings *p = (Strings*)apCtx;
		(p->mlNum++ == 0 ? p->msName : p->msSet).assign((const char*)apP, alN);
	}
	static void PushString(void *apCtx, const unsigned char *apP, size_t alN) { ((tStringVec*)apCtx)->push_back(tString((const char*)apP, alN)); }
	tString Event(int alIdx, OhplA12bEventInfo *apInfo, int alWhich)
	{
		OhplA12bEventInfo info;
		Strings s;
		if(ohpl_a12b_ins_event(mlH, alIdx, apInfo ? apInfo : &info, PutStrings, &s) == 0) return "";
		return alWhich == 0 ? s.msName : s.msSet;
	}

	uint64_t mlH;
};

//----------------------------------------------


#endif // LUX_Insanity_HANDLER_H
