#include "SomaAgentDump.h"
#include "SomaAgent.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaSave.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace
{
FILE *Dump()
{
	static FILE *spFile = NULL;
	static bool sbInit = false;
	if (sbInit == false)
	{
		sbInit = true;
		const char *sPath = getenv("OHPL_S4B_DUMP");
		if (sPath && *sPath)
			spFile = fopen(sPath, "a");
	}
	return spFile;
}

int glTick = 0;
bool gbEnded = false;
std::set<cSomaLuxEntity *> gSeen;
std::vector<cSomaLuxEntity *> gPrev;
}

bool OhplS4bOn() { return Dump() != NULL; }

void OhplS4bLog(const char *fmt, ...)
{
	FILE *f = Dump();
	if (f == NULL || gbEnded)
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
}

// OHPL_S4B_SCRIPT lines "<tick> <cmd>": wake <ent> | player x y z | state <ent> n | msg <ent> n | save <file> | load <file> | dumpsave | END
void OhplS4bBegin(cSomaLuxEntity *apEnt)
{
	FILE *f = Dump();
	if (f == NULL || gbEnded)
		return;
	if (gSeen.insert(apEnt).second == false)
	{
		gPrev.assign(gSeen.begin(), gSeen.end());
		std::sort(gPrev.begin(), gPrev.end(), [](cSomaLuxEntity *a, cSomaLuxEntity *b) { return a->msName < b->msName; });
		gSeen.clear();
		gSeen.insert(apEnt);
	}
	if (gSeen.size() == 1)
	{
		if (++glTick == 1)
			fprintf(f, "M %s\n", cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetName().c_str() : "?");
		static std::vector<std::pair<int, std::string>> vLines;
		static bool bRead = false;
		if (bRead == false)
		{
			bRead = true;
			if (const char *sPath = getenv("OHPL_S4B_SCRIPT"))
			{
				std::ifstream in(sPath);
				std::string sLine;
				while (std::getline(in, sLine))
					if (size_t lSp = sLine.find(' ') + 1)
						vLines.push_back({atoi(sLine.c_str()), sLine.substr(lSp)});
			}
		}
		fprintf(f, "T %d\n", glTick);
		for (auto &l : vLines)
		{
			if (l.first != glTick)
				continue;
			char sName[256] = "";
			float x, y, z;
			int n;
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			if (l.second == "END")
			{
				fputs("END\n", f);
				fflush(f);
				gbEnded = true;
			}
			else if (sscanf(l.second.c_str(), "wake %255s", sName) == 1)
			{
				if (cSomaLuxEntity *p = pMap ? pMap->GetEntity(tString(sName)) : NULL)
					p->SetActive(true);
			}
			else if (sscanf(l.second.c_str(), "player %f %f %f", &x, &y, &z) == 3)
			{
				if (cSomaLuxPlayer::Get() && cSomaLuxPlayer::Get()->GetCharacterBody())
					cSomaLuxPlayer::Get()->GetCharacterBody()->SetFeetPosition(cVector3f(x, y, z));
			}
			else if (sscanf(l.second.c_str(), "state %255s %d", sName, &n) == 2)
			{
				if (cSomaLuxEntity *p = pMap ? pMap->GetEntity(tString(sName)) : NULL)
					SomaAgentChangeState(p, n);
			}
			else if (sscanf(l.second.c_str(), "save %255s", sName) == 1)
			{
				fprintf(f, "SAVED %d\n", cSomaSaveHandler::Save(cString::To16Char(sName)) ? 1 : 0);
			}
			else if (sscanf(l.second.c_str(), "load %255s", sName) == 1)
			{
				fprintf(f, "LOAD %d\n", cSomaSaveHandler::Load(cString::To16Char(sName)) ? 1 : 0);
				gPrev.clear();
				gSeen.clear();
			}
			else if (l.second == "dumpsave")
			{
				for (cSomaLuxEntity *p : gPrev)
				{
					cMatrixf m;
					float fYaw = 0;
					bool bS = false, bD = false;
					std::string sPath = SomaAgentSavePath(p);
					if (SomaAgentGetMatrix(p, m) == false)
						continue;
					SomaAgentSaveExtra(p, fYaw, bS, bD);
					fprintf(f, "SAVEAGENT %s st=%d pos=%.9g,%.9g,%.9g yaw=%.9g s=%d d=%d path=", p->msName.c_str(), SomaAgentGetState(p), m.GetTranslation().x, m.GetTranslation().y,
							m.GetTranslation().z, fYaw, bS, bD);
					for (unsigned char c : sPath)
						fprintf(f, "%02x", c);
					fputc('\n', f);
				}
			}
			else if (sscanf(l.second.c_str(), "msg %255s %d", sName, &n) == 2)
			{
				if (cSomaLuxEntity *p = pMap ? pMap->GetEntity(tString(sName)) : NULL)
					SomaAgentSendMessage(p, n);
			}
		}
	}
	unsigned lSeed = (unsigned)glTick * 2654435761u;
	for (char c : apEnt->msName)
		lSeed = lSeed * 31u + (unsigned char)c;
	srand(lSeed);
}

void OhplS4bEnd(cSomaLuxEntity *apEnt)
{
	FILE *f = Dump();
	iCharacterBody *pBody = SomaAgentGetBody(apEnt);
	if (f == NULL || pBody == NULL || gbEnded)
		return;
	cVector3f p = pBody->GetPosition(), v = pBody->GetVelocity(1.0f / 60.0f);
	cAnimationState *pAnim = apEnt->mpMesh && apEnt->mlCurrentAnim >= 0 ? apEnt->mpMesh->GetAnimationState(apEnt->mlCurrentAnim) : NULL;
	fprintf(f, "%s a=%d pos=%.9g,%.9g,%.9g vel=%.9g,%.9g,%.9g yaw=%.9g anim=%s@%.9g | %s\n", apEnt->msName.c_str(), apEnt->mbActive ? 1 : 0, p.x, p.y, p.z, v.x, v.y, v.z,
			pBody->GetYaw(), pAnim ? pAnim->GetName().c_str() : "-", pAnim ? pAnim->GetTimePosition() : -1.0f, SomaAgentDebug(apEnt).c_str());
	fflush(f);
}
