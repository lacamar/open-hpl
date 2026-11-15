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

#ifndef LUX_INPUT_HANDLER_H
#define LUX_INPUT_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"
#include "LuxTypes.h"
#include "RustLuxBase.h"
#include "LuxInputHost.h"

//----------------------------------------------

class cLuxAction
{	
public:
	cLuxAction() : msName(""){}
	cLuxAction(const tString& asName, 
			   int alId, 
			   bool abConfigurable, 
			   eLuxActionCategory aCat) : msName(asName), mlId(alId), mbConfigurable(abConfigurable), mCat(aCat){}

	tString msName;
	int mlId;
	bool mbConfigurable;
	eLuxActionCategory mCat;
};

typedef std::vector<cLuxAction*> tLuxActionVec;
typedef tLuxActionVec::iterator tLuxActionVecIt;

//----------------------------------------------

class cLuxInput
{
public:
	cLuxInput() : msInputType(""){}
	cLuxInput(const tString& asInputType, int alValue, int alActionId) : 
				msInputType(asInputType), mlValue(alValue), mlActionId(alActionId){}

	
    tString msInputType;
	int mlValue;
	int mlActionId;
};

typedef std::vector<cLuxInput*> tLuxInputVec;
typedef tLuxInputVec::iterator	tLuxInputVecIt;

//----------------------------------------------

// The action table and the default bindings live in Rust; these copies hand out stable pointers.
struct cLuxInputTables
{
	tLuxActionVec mvActionPtrs;
	std::vector<cLuxAction> mvActions;
	std::vector<cLuxInput> mvInputs;

	cLuxInputTables()
	{
		tString sName;
		auto emit = [&](uint32_t tag, const void *p, size_t n) { sName = OhplGa9EmitS(p, n); };

		mvActions.reserve(ohpl_ga9_action_count());
		OhplGa9Action action;
		for(size_t i=0; ohpl_ga9_action(i, &action, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit); ++i)
			mvActions.push_back(cLuxAction(sName, action.id, action.configurable, (eLuxActionCategory)action.cat));

		mvInputs.reserve(ohpl_ga9_input_count());
		int lValue, lActionId;
		for(size_t i=0; ohpl_ga9_input(i, &lValue, &lActionId, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit); ++i)
			mvInputs.push_back(cLuxInput(sName, lValue, lActionId));
	}
};

inline cLuxInputTables &LuxInputTables()
{
	static cLuxInputTables tables;
	return tables;
}

//----------------------------------------------

class cLuxInputHandler : public iLuxUpdateable
{
public:	
	cLuxInputHandler() : iLuxUpdateable("LuxInputHandler")
	{
		ohpl_in_set_host(OhplInHostCall);
		mlH = ohpl_in_new();
	}
	~cLuxInputHandler() { ohpl_in_free(mlH); }

	void LoadUserConfig() { ohpl_in_load_user_config(mlH); }
	void SaveUserConfig() { ohpl_in_save_user_config(mlH); }
	
	void OnStart() {}
	void Update(float afTimeStep) { if(OhplInDumpEnabled() == false || OhplInScripted()) ohpl_in_update(mlH); }
	void Reset() {}
	void OnPostRender(float afFrameTime) { OhplInDumpTick(); ohpl_in_post_render(mlH); }

	tWString GetInputName(const tString& asActionName)
	{
		tWString sName;
		return OhplInInputName(asActionName, sName) ? sName : _W("InvalidAction");
	}

	void ChangeState(eLuxInputState aState) { ohpl_in_change_state(mlH, aState); }
	eLuxInputState GetState(){ return (eLuxInputState)ohpl_in_state(mlH); }

	bool GetInvertMouse(){ return ohpl_in_get(mlH, 0) != 0; }
	void SetInvertMouse(bool abX) { ohpl_in_set(mlH, 0, abX); }

	bool GetSmoothMouse() { return ohpl_in_get(mlH, 1) != 0; }
	void SetSmoothMouse(bool abX) { ohpl_in_set(mlH, 1, abX); }

	float GetMouseSensitivity() { return ohpl_in_get(mlH, 2); }
	void SetMouseSensitivity(float afX) { ohpl_in_set(mlH, 2, afX); }

#ifdef USE_GAMEPAD
	bool GetInvertGamepadLook() { return ohpl_in_get(mlH, 4) != 0; }
	void SetInvertGamepadLook(bool abX) { ohpl_in_set(mlH, 4, abX); }

	float GetGamepadLookSensitivity() { return ohpl_in_get(mlH, 3); }
	void SetGamepadLookSensitivity(float afX) { ohpl_in_set(mlH, 3, afX); }

	iGamepad* GetGamepad() { return OhplInPad(); }
	bool IsGamepadPresent() { return OhplInPad() != NULL; }
	void AppDeviceWasPlugged() { OhplInQ(eInQ_PadSetup); }
	void AppDeviceWasRemoved() { OhplInQ(eInQ_PadSetup); }
#endif

	cLuxAction* GetActionByName(const tString& asName)
	{
		int lIdx = ohpl_ga9_action_by_name((const unsigned char*)asName.data(), asName.size());
		return lIdx < 0 ? NULL : &LuxInputTables().mvActions[lIdx];
	}

	cLuxAction* GetActionById(int alId)
	{
		int lIdx = ohpl_ga9_action_by_id(alId);
		return lIdx < 0 ? NULL : &LuxInputTables().mvActions[lIdx];
	}

	tLuxActionVec GetActionsByCategory(eLuxActionCategory aCat)
	{
		cLuxInputTables &tables = LuxInputTables();
		std::vector<int> vIdx(tables.mvActions.size());
		size_t lNum = ohpl_ga9_actions_by_category(aCat, vIdx.data(), vIdx.size());

		tLuxActionVec vActions;
		for(size_t i=0; i<lNum; ++i)
			vActions.push_back(&tables.mvActions[vIdx[i]]);
		return vActions;
	}

	tLuxInputVec GetDefaultInputsByActionId(int alId)
	{
		cLuxInputTables &tables = LuxInputTables();
		std::vector<int> vIdx(tables.mvInputs.size());
		size_t lNum = ohpl_ga9_default_inputs(alId, vIdx.data(), vIdx.size());

		tLuxInputVec vInputs;
		for(size_t i=0; i<lNum; ++i)
			vInputs.push_back(&tables.mvInputs[vIdx[i]]);
		return vInputs;
	}

	void ResetSmoothMousePos() { ohpl_in_smooth_reset(mlH); }
	cVector2f GetSmoothMousePos(const cVector2f& avRelPosMouse)
	{
		float vOut[2];
		ohpl_in_smooth_push(mlH, avRelPosMouse.x, avRelPosMouse.y, vOut);
		return cVector2f(vOut[0], vOut[1]);
	}

private:
	uint64_t mlH;
};

//----------------------------------------------


#endif // LUX_INPUT_HANDLER_H
