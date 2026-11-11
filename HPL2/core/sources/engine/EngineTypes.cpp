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

#include "engine/EngineTypes.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "engine/RustEngine.h"

#include <cstddef>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// MULTIPLE SETTINGS HANDLER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	static_assert(sizeof(cMultipleSettingsHandler::cGSEntry) == sizeof(OhplEnMsEntry), "cGSEntry layout");
	static_assert(eMultipleSettingsCalcType_Mul == 0 && eMultipleSettingsCalcType_Min == 1 &&
				  eMultipleSettingsCalcType_Max == 2 && eMultipleSettingsCalcType_Avg == 3, "calc types");

	void cMultipleSettingsHandler::cGSEntry::FadeTo(float afDest, float afSpeed, bool abDestroyAtDest)
	{
		ohpl_en_ms_fade_to(reinterpret_cast<OhplEnMsEntry*>(this), afDest, afSpeed, abDestroyAtDest);
	}

	void cMultipleSettingsHandler::cGSEntry::SetValAndDest(float afVal)
	{
		ohpl_en_ms_set_val_and_dest(reinterpret_cast<OhplEnMsEntry*>(this), afVal);
	}

	//-----------------------------------------------------------------------

	cMultipleSettingsHandler::cMultipleSettingsHandler()
	{
		static_assert(offsetof(cGSEntry, mfVal) == 0 && offsetof(cGSEntry, mfDest) == 4 &&
					  offsetof(cGSEntry, mfSpeed) == 8 && offsetof(cGSEntry, mTypes) == 12 &&
					  offsetof(cGSEntry, mbDestroyAtDest) == 16, "cGSEntry layout");
	}

	cMultipleSettingsHandler::~cMultipleSettingsHandler()
	{
		for(size_t i=0; i<mvEntries.size(); ++i)
		{
			if(mvEntries[i]) hplDelete(mvEntries[i]);
		}
	}

	//-----------------------------------------------------------------------

	cMultipleSettingsHandler::cGSEntry* cMultipleSettingsHandler::GetEntry(int alIdx, bool abForceCreation)
	{
		bool bOccupied = alIdx >= 0 && alIdx < (int)mvEntries.size() && mvEntries[alIdx] != NULL;
		switch(ohpl_en_ms_slot_action(alIdx, mvEntries.size(), bOccupied, abForceCreation))
		{
		case 1:
			return mvEntries[alIdx];
		case 2:
			mvEntries[alIdx] = hplNew( cMultipleSettingsHandler::cGSEntry, () );
			return mvEntries[alIdx];
		case 3:
			mvEntries.resize(alIdx+1, NULL);
			mvEntries[alIdx] = hplNew( cMultipleSettingsHandler::cGSEntry, () );
			return mvEntries[alIdx];
		}
		return NULL;
	}

	//-----------------------------------------------------------------------

	int cMultipleSettingsHandler::CreateEntry()
	{
		size_t lIdx = ohpl_en_ms_create_slot(reinterpret_cast<const OhplEnMsEntry* const*>(mvEntries.data()), mvEntries.size());
		if(lIdx < mvEntries.size())
			mvEntries[lIdx] = hplNew( cMultipleSettingsHandler::cGSEntry, () );
		else
			mvEntries.push_back( hplNew( cMultipleSettingsHandler::cGSEntry, () ) );
		return (int)lIdx;
	}

	void cMultipleSettingsHandler::DestroyEntry(int alIdx)
	{
		if(alIdx >= 0 && alIdx < (int)mvEntries.size() && mvEntries[alIdx])
		{
			hplDelete( mvEntries[alIdx] );
			mvEntries[alIdx] = NULL;
		}
	}

	//-----------------------------------------------------------------------

	float cMultipleSettingsHandler::CalcResults(tFlag aTypes, eMultipleSettingsCalcType aCalcType, float afDefault)
	{
		return ohpl_en_ms_calc_results(reinterpret_cast<const OhplEnMsEntry* const*>(mvEntries.data()), mvEntries.size(), aTypes, aCalcType, afDefault);
	}

	//-----------------------------------------------------------------------

	void cMultipleSettingsHandler::Update(float afTimeStep)
	{
		for(size_t i=0; i<mvEntries.size();++i)
		{
			cMultipleSettingsHandler::cGSEntry *pEntry = mvEntries[i];
			if(pEntry==NULL) continue;

			if(ohpl_en_ms_update(reinterpret_cast<OhplEnMsEntry*>(pEntry), afTimeStep))
			{
				hplDelete(pEntry);
				mvEntries[i] = NULL;
			}
		}
	}

	//-----------------------------------------------------------------------



	//////////////////////////////////////////////////////////////////////////
	// SCRIPT VAR
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cScriptVar::cScriptVar()
	{
		
	}

	//-----------------------------------------------------------------------
	
	kBeginSerializeBase(cScriptVar)
	kSerializeVar(msName, eSerializeType_String)
	kSerializeVar(mlVal, eSerializeType_Int32)
	kEndSerialize()

	//-----------------------------------------------------------------------
}
