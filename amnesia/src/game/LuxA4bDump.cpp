#include "LuxEnemy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <fstream>

#include "LuxMap.h"
#include "LuxSaveHandler.h"
#include "LuxBase.h"

static FILE *gpDump = NULL;
static bool gbInit = false;

// OHPL_A4B_DUMP: one line per enemy update; OHPL_A4B_SCRIPT also reseeds libc rand every map update
static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A4B_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

void iLuxEnemy::A4bDump()
{
	FILE *f = Dump();
	if(f == NULL) return;
	static int lCount = 0;
	if(++lCount > 40000) return;
	cVector3f vP = mpCharBody->GetPosition();
	cVector3f vV = mpCharBody->GetVelocity(1.0f/60.0f);
	cVector3f vG;
	ohpl_a4b_nav_goal(mNav, &vG.x);
	cAnimationState *pAnim = GetCurrentAnimation();
	OhplA4bMoverSave mv;
	ohpl_a4b_mover_get(mNav, &mv);
	fprintf(f, "%s st=%d/%d/%d pos=%.9g,%.9g,%.9g vel=%.9g,%.9g,%.9g ms=%d turn=%d tga=%.9g ts=%.9g tba=%.9g stuck=%.9g ovr=%d pf=%d goal=%.9g,%.9g,%.9g ids=",
		GetName().c_str(), (int)mCurrentState, (int)mNextState, (int)mPreviousState, vP.x, vP.y, vP.z, vV.x, vV.y, vV.z,
		mv.moveState, mv.turning ? 1 : 0, mv.turnGoalAngle, mv.turnSpeed, mv.turnBreakAcc,
		mv.stuckCounter, mv.overrideMoveState ? 1 : 0, ohpl_a4b_nav_moving(mNav) ? 1 : 0, vG.x, vG.y, vG.z);
	for(size_t i=0, lN = ohpl_a4b_nav_path_len(mNav); i<lN; ++i) fprintf(f, "%d,", ((cAINode*)ohpl_a4b_nav_path_at(mNav, i))->GetID());
	fprintf(f, " win=%d anim=%s@%.9g hp=%.9g\n", ohpl_a4b_nav_win_len(mNav), pAnim ? pAnim->GetName().c_str() : "-", pAnim ? pAnim->GetTimePosition() : -1.0f, mfHealth);
	fflush(f);
}

static int glTick = 0;
static int glLoadTick = -1;

// the first enemy restored by a load restarts the tick count and switches to the "L<tick>" script lines
void OhplA4bOnLoad()
{
	if(Dump() == NULL || glLoadTick == 0) return;
	glTick = 0;
	glLoadTick = 0;
	fprintf(Dump(), "LOADED\n");
	fflush(Dump());
}

// OHPL_A4B_SCRIPT: lines "<map update> <script line>" run at the start of that update of the current map, "L<n> ..." the same
// n updates after a load; "@SAVE <path>" saves the game, "END" ends the dump
void OhplA4bTick(cLuxMap *apMap)
{
	static const char *sPath = getenv("OHPL_A4B_SCRIPT");
	if(sPath == NULL || Dump() == NULL) return;
	static std::vector<std::pair<int, std::string> > vLines, vLoadLines;
	static bool bRead = false;
	if(bRead == false)
	{
		bRead = true;
		std::ifstream in(sPath);
		std::string sLine;
		while(std::getline(in, sLine))
		{
			size_t lSp = sLine.find(' ');
			if(lSp == std::string::npos) continue;
			bool bLoad = sLine[0] == 'L';
			std::pair<int, std::string> line(atoi(sLine.substr(bLoad ? 1 : 0, lSp).c_str()), sLine.substr(lSp + 1));
			(bLoad ? vLoadLines : vLines).push_back(line);
		}
	}
	++glTick;
	srand((unsigned)(glTick * 2654435761u + (glLoadTick == 0 ? 7 : 0)));
	std::vector<std::pair<int, std::string> > &vRun = glLoadTick == 0 ? vLoadLines : vLines;
	for(size_t i=0; i<vRun.size(); ++i)
	{
		if(vRun[i].first != glTick) continue;
		const std::string &sCmd = vRun[i].second;
		if(sCmd == "END") { fprintf(Dump(), "END\n"); fflush(Dump()); }
		else if(sCmd.compare(0, 6, "@SAVE ") == 0)
		{
			gpBase->mpSaveHandler->SaveGameToFile(cString::To16Char(sCmd.substr(6)), false);
			fprintf(Dump(), "SAVED\n");
			fflush(Dump());
		}
		else apMap->RunScript(sCmd);
	}
}
