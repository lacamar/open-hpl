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

#ifndef LUX_PROGRESS_LOG_HANDLER_H
#define LUX_PROGRESS_LOG_HANDLER_H

#include "LuxA10bHost.h"

class cLuxProgressLogHandler : public iLuxUpdateable
{
public:
	cLuxProgressLogHandler() : iLuxUpdateable("LuxProgressLogHandler"), mR(this, eOhplA10bKind_Other) { mR.mH = ohpl_a10b_log_new(); }
	~cLuxProgressLogHandler() { ohpl_a10b_log_delete(mR.mH); }

	void LoadUserConfig() { ohpl_a10b_log_load_user_config(&mR.mHost, mR.mH); }
	void SaveUserConfig() { ohpl_a10b_log_save_user_config(&mR.mHost, mR.mH); }
	void Update(float afTimeStep) { ohpl_a10b_log_update(mR.mH); }
	void CreateAndResetLogFile() { ohpl_a10b_log_create(&mR.mHost, mR.mH); }
	void AddLog(eLuxProgressLogLevel aLevel, const tString& asMessage)
	{
		ohpl_a10b_log_add(&mR.mHost, mR.mH, (int)aLevel, asMessage.data(), asMessage.size());
	}

private:
	cOhplA10bShell mR;
};

#endif
