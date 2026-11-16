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

#ifndef LUX_DEBUG_HANDLER_H
#define LUX_DEBUG_HANDLER_H

#include "LuxBase.h"
#include "RustLuxBase.h"

// Shell over rust/src/luxbase/debug_ui.rs; the engine side is LuxDebugHost.cpp.
class cLuxDebugHandler : public iLuxUpdateable
{
public:
	cLuxDebugHandler();
	~cLuxDebugHandler();

	void LoadUserConfig();
	void SaveUserConfig();

	void OnStart() { ohpl_dbg_on_start(mpRust); }
	void Update(float afTimeStep);
	void Reset() { ohpl_dbg_reset(mpRust); }

	void OnMapEnter(cLuxMap *apMap) { ohpl_dbg_on_map_enter(mpRust); }
	void OnMapLeave(cLuxMap *apMap) { ohpl_dbg_on_map_leave(mpRust); }

	void SetDebugWindowActive(bool abActive) { ohpl_dbg_set_window_active(mpRust, abActive); }
	bool GetDebugWindowActive(){ return mpF->window_active;}

	void OnDraw(float afFrameTime) { ohpl_dbg_on_draw(mpRust); }
	void RenderSolid(cRendererCallbackFunctions* apFunctions);

	void AddErrorOrWarningMessage(const tWString& asText) { ohpl_ga9_debug_add_error(mpRust, (const uint32_t*)asText.data(), asText.size()); }
	void AddMessage(const tWString& asText, bool abCheckForDuplicates) { ohpl_ga9_debug_add_message(mpRust, (const uint32_t*)asText.data(), asText.size(), abCheckForDuplicates); }

	bool GetShowPlayerInfo(){ return mpF->show_player_info;}
	bool GetShowEntityInfo(){ return mpF->show_entity_info;}
	bool GetScriptDebugOn(){ return mpF->script_debug_on;}
	bool GetDisableFlashBacks(){ return mpF->disable_flashbacks;}
	bool GetAllowQuickSave(){ return mpF->allow_quick_save;}

	bool GetShowFPS(){ return mpF->show_fps;}
	void SetShowFPS(bool abX){ mpF->show_fps = abX;}

	void SetFastForward(bool abX) { ohpl_dbg_set_fast_forward(mpRust, abX); }
	bool GetFastForward(){ return mpF->fast_forward;}

	void QuickReloadMap() { ohpl_dbg_quick_reload(mpRust); }

	// OHPL_A12D_DUMP drivers
	iWidget *DumpDebugRoot();
	iWidget *DumpScriptRoot();
	tString DumpInspectName();
	void DumpPick(const tWString& asPath, bool abBatch);

	OhplGa9Debug *mpRust;
	OhplGa9DebugFlags *mpF;
};

#endif // LUX_DEBUG_HANDLER_H
