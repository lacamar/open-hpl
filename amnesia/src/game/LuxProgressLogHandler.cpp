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

#include "LuxProgressLogHandler.h"
#include "RustLuxFx.h"

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProgressLogHandler::cLuxProgressLogHandler() : iLuxUpdateable("LuxProgressLogHandler")
{
	mpFile = NULL;
	mlFileNameCount = 0;
	mlCounter =0;

	mbActive = false;
}

//-----------------------------------------------------------------------

cLuxProgressLogHandler::~cLuxProgressLogHandler()
{
	if(mpFile) fclose(mpFile);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

void cLuxProgressLogHandler::LoadUserConfig()
{
	mbActive = gpBase->mpUserConfig->GetBool("Debug", "UseProgLog", false);
	Reset();
}

void cLuxProgressLogHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Debug", "UseProgLog", mbActive);
}

//-----------------------------------------------------------------------

void cLuxProgressLogHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxProgressLogHandler::Reset()
{
	
}

//-----------------------------------------------------------------------

void cLuxProgressLogHandler::CreateAndResetLogFile()
{
	if(mbActive==false) return;

	/////////////////////////////
	// Close previous log file
	if(mpFile)
	{
		fclose(mpFile);
	}

	/////////////////////////////
	// Get file name of log
	cDate currentDate =  cPlatform::GetDate();
	OhplGa9bDate date = { currentDate.year, currentDate.month, currentDate.month_day, currentDate.hours, currentDate.minutes, currentDate.seconds };

	std::vector<uint32_t> vProfile(gpBase->msProfileName.begin(), gpBase->msProfileName.end());
	const uint32_t *pProfile = vProfile.empty() ? NULL : &vProfile[0];
	size_t lNameLen = ohpl_ga9b_proglog_file_name(pProfile, vProfile.size(), &date, mlFileNameCount, NULL, 0);
	std::vector<uint32_t> vName(lNameLen);
	ohpl_ga9b_proglog_file_name(pProfile, vProfile.size(), &date, mlFileNameCount, lNameLen ? &vName[0] : NULL, lNameLen);
	tWString sFileName(vName.begin(), vName.end());

	/////////////////////////////
	// Open file
	tWString sPath = gpBase->msBaseStatePath + sFileName;

#ifdef WIN32
	mpFile = _wfopen(sPath.c_str(),_W("w"));
#else
	mpFile = fopen(cString::To8Char(sPath).c_str(),"w");
#endif

	if(mpFile==NULL)
		Error("Could not open progress log file '%s'!\n", cString::To8Char(sPath).c_str());
	else
	{
		tString sProfile = cString::To8Char(gpBase->msProfileName);
		const unsigned char *pProfile8 = (const unsigned char*)sProfile.data();
		size_t lHeaderLen = ohpl_ga9b_proglog_header(pProfile8, sProfile.size(), &date, NULL, 0);
		std::vector<unsigned char> vHeader(lHeaderLen);
		ohpl_ga9b_proglog_header(pProfile8, sProfile.size(), &date, lHeaderLen ? &vHeader[0] : NULL, lHeaderLen);
		if(lHeaderLen) fwrite(&vHeader[0], 1, lHeaderLen, mpFile);
	}

	mlCounter =0;
}

//-----------------------------------------------------------------------

void cLuxProgressLogHandler::Update(float afTimeStep)
{
	mlCounter = ohpl_ga9b_proglog_tick(mbActive, mlCounter);
}

//-----------------------------------------------------------------------

void cLuxProgressLogHandler::AddLog(eLuxProgressLogLevel aLevel, const tString& asMessage)
{
	if(mbActive==false || mpFile==NULL) return;

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	tString sMapName = pMap ? pMap->GetName() : "NoMap";

	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	OhplGa9bVitals vitals = { pPlayer->GetHealth(), pPlayer->GetSanity(), pPlayer->GetLampOil(), pPlayer->GetTinderboxes(), pPlayer->GetCoins() };

	const unsigned char *pMapName = (const unsigned char*)sMapName.data();
	const unsigned char *pMessage = (const unsigned char*)asMessage.data();
	size_t lLen = ohpl_ga9b_proglog_line((int)aLevel, mlCounter, pMapName, sMapName.size(), &vitals, pMessage, asMessage.size(), NULL, 0);
	std::vector<unsigned char> vLine(lLen);
	ohpl_ga9b_proglog_line((int)aLevel, mlCounter, pMapName, sMapName.size(), &vitals, pMessage, asMessage.size(), lLen ? &vLine[0] : NULL, lLen);

	if(lLen) fwrite(&vLine[0], 1, lLen, mpFile);
	fflush(mpFile);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------
