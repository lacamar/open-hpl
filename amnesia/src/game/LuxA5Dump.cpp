// OHPL_A5_PROPDUMP: per prop per map update, base + body + effect state and the serialised save record; a prop's line is
// written only when it differs from its previous line. OHPL_A5_SCRIPT: "<update> <script line>", "L<n> ..." after a load,
// "@WATCH <prop> ..." (dumped every update; the rest every 20th), "@INTERACT <prop>", "@SAVE <path>", "END"; libc rand is reseeded every update.

#include "LuxProp.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxSaveHandler.h"
#include "LuxPlayer.h"
#include "LuxBase.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <unistd.h>

static FILE *gpDump = NULL;
static bool gbInit = false;
static int glTick = 0;
static int glLoadTick = -1;
static long glLines = 0;
static std::map<std::string, std::string> gmapLast;
static std::set<std::string> gsetWatch;
static std::map<iLuxProp*, std::vector<bool> > gmapFrozen;

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A5_PROPDUMP");
		if(sPath && *sPath)
		{
			gpDump = fopen(sPath, "w");
			if(gpDump) setvbuf(gpDump, NULL, _IOFBF, 1 << 20);
		}
	}
	return gpDump;
}

static void Fb(std::string &s, float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	char b[16];
	snprintf(b, sizeof(b), "%08x,", u);
	s += b;
}

static void Fv(std::string &s, const float *p, int n) { for(int i=0; i<n; ++i) Fb(s, p[i]); }
static void Fc(std::string &s, const char *k, const cColor &c) { s += " "; s += k; s += "="; Fv(s, c.v, 4); }
static void Fd(std::string &s, const char *k, long v) { char b[48]; snprintf(b, sizeof(b), " %s=%ld", k, v); s += b; }
static void Ff(std::string &s, const char *k, float f) { s += " "; s += k; s += "="; Fb(s, f); }
static void Fa(std::string &s, const char *k, const float *p, int n) { s += " "; s += k; s += "="; Fv(s, p, n); }

static uint64_t Fnv(const std::string &s)
{
	uint64_t h = 1469598103934665603ull;
	for(size_t i=0; i<s.size(); ++i) { h ^= (unsigned char)s[i]; h *= 1099511628211ull; }
	return h;
}

struct OhplA5Dump
{
	static void Base(std::string &s, iLuxProp *p);
	static void Effects(std::string &s, iLuxProp *p);
	static std::string Record(iLuxProp *p);
	static void Run(iLuxProp *p);
	static void NoFlicker(cLuxMap *apMap);
};

void OhplA5Dump::Base(std::string &s, iLuxProp *p)
{
	const OhplGa4Base &b = p->Base();
	Fd(s, "type", p->GetPropType());
	Fd(s, "id", p->GetID());
	Fd(s, "act", p->IsActive());
	Fd(s, "nointer", p->GetInteractionDisabled());
	Fd(s, "fgs", p->GetFullGameSave());
	Fd(s, "saved", p->IsSaved());
	Fd(s, "destroy", p->GetDestroyMe());
	Fd(s, "look", p->IsLookedAtByPlayer());
	Fd(s, "outl", p->ShowOutlinesOnConnectedBodies());
	Fd(s, "inter", p->S().interacted_with);
	Fd(s, "sph", p->S().static_physics);
	Fd(s, "showmesh", p->S().show_mesh);
	Ff(s, "hp", p->S().health);
	Fd(s, "fx", b.effects_active); Fd(s, "fxfade", b.effects_fading); Ff(s, "fxa", b.effects_alpha);
	Fd(s, "outpl", b.check_outside_player);
	Fd(s, "mv", b.moving); Fd(s, "mvl", b.moving_linear); Fd(s, "mva", b.moving_angular);
	Ff(s, "mvvol", b.moving_volume); Ff(s, "mvstart", b.move_start_count);
	if(b.moving_linear)
	{
		Ff(s, "lms", b.linear.max_speed); Ff(s, "lacc", b.linear.acc); Ff(s, "lsp", b.linear.speed); Ff(s, "lsl", b.linear.slowdown);
		Fa(s, "lgoal", b.linear.goal, 3);
	}
	if(b.moving_angular)
	{
		Ff(s, "ams", b.angular.max_speed); Ff(s, "aacc", b.angular.acc); Ff(s, "asp", b.angular.speed); Ff(s, "asl", b.angular.slowdown);
		Fa(s, "agoal", b.angular.goal, 16);
		Fd(s, "ang", b.angular.no_goal); Fa(s, "angd", b.angular.no_goal_dir, 3); Fd(s, "auo", b.angular.use_offset);
		Fa(s, "awo", b.angular.world_offset, 3); Fa(s, "alo", b.angular.local_offset, 3);
	}
	Fa(s, "lbm", b.last_body_matrix, 16);
	Ff(s, "fia", b.fade_in_alpha); Ff(s, "fis", b.fade_in_speed);
	Fd(s, "anim", p->S().anim_index);
	s += " animcb=" + p->Str(kOhplA5eStr_AnimCallback);
	s += " parent=" + (p->mpAttachmentParent ? p->mpAttachmentParent->GetName() : std::string("-"));
	s += " pent=" + p->Str(kOhplA5eStr_ParentEntityName) + "/" + p->Str(kOhplA5eStr_ParentBoneName);
	Fd(s, "pbone", p->mpParentBone != NULL);
	s += " mbk=? mbkc=? mbka=?";  // never initialised by the ctor (bd open-hpl-smj.22.45.1)
	Ff(s, "mbkm", p->S().body_mass_backup);
	s += " att=";
	for(tLuxProp_AttachedPropListIt it = p->mlstAttachedProps.begin(); it != p->mlstAttachedProps.end(); ++it)
	{
		s += (*it)->msName + ":" + (*it)->msFileName + ":";
		Fv(s, (*it)->m_mtxOffset.v, 16);
		s += (*it)->mpProp ? (*it)->mpProp->GetName() : std::string("-");
		s += ";";
	}
	s += " conn=";
	for(size_t i=0; i<p->mvConnectedProps.size(); ++i) s += p->mvConnectedProps[i] + ";";
	Fd(s, "nic", p->mvInteractConnections.size());
	if(p->mpMeshEntity)
	{
		cMeshEntity *m = p->mpMeshEntity;
		Fd(s, "mevis", m->IsVisible()); Fd(s, "meact", m->IsActive());
		Ff(s, "meill", m->GetIlluminationAmount());
		Ff(s, "mecov", m->GetCoverageAmount());
	}
}

void OhplA5Dump::Effects(std::string &s, iLuxProp *p)
{
	for(size_t i=0; i<p->mvBodies.size(); ++i)
	{
		iPhysicsBody *b = p->mvBodies[i];
		s += " B" + std::to_string(i) + "=" + b->GetName() + ",";
		Fv(s, b->GetWorldMatrix().v, 16);
		Fv(s, b->GetLinearVelocity().v, 3);
		Fv(s, b->GetAngularVelocity().v, 3);
		Fb(s, b->GetMass());
		char c[24]; snprintf(c, sizeof(c), "%d%d%d%d", b->GetEnabled(), b->GetCollide(), b->GetCollideCharacter(), b->IsActive());
		s += c;
	}
	s += " mainbody=" + (p->mpMainBody ? p->mpMainBody->GetName() : std::string("-"));
	for(size_t i=0; i<p->mvLights.size(); ++i)
	{
		iLight *l = p->mvLights[i];
		if(l == NULL) { s += " L" + std::to_string(i) + "=-"; continue; }
		s += " L" + std::to_string(i) + "=" + l->GetName();
		Fd(s, "v", l->IsVisible()); Fd(s, "a", l->IsActive());
		Fc(s, "c", l->GetDiffuseColor());
		Ff(s, "r", l->GetRadius());
	}
	for(size_t i=0; i<p->mvParticleSystems.size(); ++i)
	{
		cParticleSystem *ps = p->mvParticleSystems[i];
		if(ps == NULL || p->mpWorld->ParticleSystemExists(ps) == false) { s += " PS" + std::to_string(i) + "=-"; continue; }
		s += " PS" + std::to_string(i) + "=" + ps->GetName();
		Fd(s, "v", ps->IsVisible()); Fd(s, "a", ps->IsActive()); Fd(s, "dead", ps->IsDead());
		Fc(s, "c", ps->GetColor());
	}
	for(size_t i=0; i<p->mvBillboards.size(); ++i)
	{
		cBillboard *bb = p->mvBillboards[i];
		if(bb == NULL) { s += " BB" + std::to_string(i) + "=-"; continue; }
		s += " BB" + std::to_string(i) + "=" + bb->GetName();
		Fd(s, "v", bb->GetVisibleVar()); Fd(s, "a", bb->IsActive());
		Fc(s, "c", bb->GetColor());
	}
	for(size_t i=0; i<p->mvSoundEntities.size(); ++i)
	{
		cSoundEntity *se = p->mvSoundEntities[i];
		if(se == NULL) { s += " S" + std::to_string(i) + "=-"; continue; }
		s += " S" + std::to_string(i) + "=" + se->GetName();
		Fd(s, "stopped", se->IsStopped()); Fd(s, "fadeout", se->IsFadingOut());
		Ff(s, "vol", se->GetVolume());
	}
	Fd(s, "nbeams", p->mvBeams.size());
	Fd(s, "njoints", p->mvJoints.size());
	Fd(s, "mlsnd", p->mpMoveLoopSound != NULL);
}

std::string OhplA5Dump::Record(iLuxProp *p)
{
	static std::string sTmp;
	if(sTmp.empty())
	{
		const char *sRun = getenv("XDG_RUNTIME_DIR");
		sTmp = std::string(sRun && *sRun ? sRun : "/tmp") + "/ohpl-a5-rec-" + std::to_string((long)getpid()) + ".xml";
	}
	iLuxEntity_SaveData *pSave = p->CreateSaveData();
	p->SaveToSaveData(pSave);
	cSerializeClass::SaveToFile(pSave, cString::To16Char(sTmp), "R");
	hplDelete(pSave);
	std::ifstream in(sTmp.c_str(), std::ios::binary);
	std::string sRec((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	// ctor leaves these uninitialised: the XML carries heap garbage (bd open-hpl-smj.22.45.1)
	static const char *vMask[] = { "name=\"mbBodyCollideBackup\" val=\"", "name=\"mbBodyCollideCharacterBackup\" val=\"",
		"name=\"mbBodyActiveBackup\" val=\"", "name=\"mpParentBoneOffsetMatrix\" val=\"", "name=\"mBodyMatrixBackup\" val=\"" };
	for(size_t i=0; i<sizeof(vMask)/sizeof(vMask[0]); ++i)
	{
		size_t lPos = sRec.find(vMask[i]);
		while(lPos != std::string::npos)
		{
			lPos += strlen(vMask[i]);
			size_t lEnd = sRec.find('"', lPos);
			sRec.replace(lPos, lEnd - lPos, "?");
			lPos = sRec.find(vMask[i], lPos);
		}
	}
	// the oil amount is only set once the barrel was asked for it (bd open-hpl-smj.22.45.1)
	static const char *sCalc = "name=\"mbAmountCalculated\" val=\"false\"";
	static const char *sOil = "name=\"mfOilAmount\" val=\"";
	size_t lPos = sRec.find(sCalc);
	while(lPos != std::string::npos)
	{
		size_t lOil = sRec.find(sOil, lPos);
		if(lOil != std::string::npos)
		{
			lOil += strlen(sOil);
			sRec.replace(lOil, sRec.find('"', lOil) - lOil, "?");
		}
		lPos = sRec.find(sCalc, lPos + 1);
	}
	return sRec;
}

// libc rand drives the light flicker; its draws interleave with other threads, so flicker is switched off every update
void OhplA5Dump::NoFlicker(cLuxMap *apMap)
{
	cLuxEntityIterator it = apMap->GetEntityIterator();
	while(it.HasNext())
	{
		iLuxEntity *pEnt = it.Next();
		if(pEnt->GetEntityType() != eLuxEntityType_Prop) continue;
		iLuxProp *p = static_cast<iLuxProp*>(pEnt);
		for(size_t i=0; i<p->mvLights.size(); ++i) if(p->mvLights[i]) p->mvLights[i]->SetFlickerActive(false);
	}
}

void OhplA5Dump::Run(iLuxProp *p)
{
	FILE *f = Dump();
	if(f == NULL || glLines > 600000) return;
	bool bWatch = gsetWatch.count(p->GetName()) != 0;
	if(bWatch == false && glTick % 20 != 0) return;
	bool bFull = bWatch && glTick % 120 == 0;
	std::string s;
	Base(s, p);
	Effects(s, p);
	std::string sRec = Record(p);
	char b[48];
	snprintf(b, sizeof(b), " rec=%016llx/%zu", (unsigned long long)Fnv(sRec), sRec.size());
	s += b;
	std::string sKey = p->GetMap()->GetName() + " " + p->GetName();
	std::string &sLast = gmapLast[sKey];
	if(sLast == s && bFull == false) return;
	sLast = s;
	fprintf(f, "P %d %s%s\n", glTick, sKey.c_str(), s.c_str());
	if(bFull) fprintf(f, "R %d %s %s\n", glTick, sKey.c_str(), sRec.c_str());
	++glLines;
	fflush(f);
}

// the physics steps taken between map creation and the first map update vary with load timing: bodies sleep until update 1
void OhplA5Freeze(iLuxProp *apProp)
{
	if(Dump() == NULL) return;
	std::vector<bool> &vWas = gmapFrozen[apProp];
	vWas.resize((size_t)apProp->GetBodyNum(), false);
	for(size_t i=0; i<(size_t)apProp->GetBodyNum(); ++i)
	{
		iPhysicsBody *pBody = apProp->GetBody(i);
		if(pBody && pBody->GetEnabled()) { vWas[i] = true; pBody->Sleep(); }  // earlier freezes stay recorded
	}
}

static void Unfreeze(cLuxMap *apMap)
{
	cLuxEntityIterator it = apMap->GetEntityIterator();
	while(it.HasNext())
	{
		iLuxEntity *pEnt = it.Next();
		if(pEnt->GetEntityType() != eLuxEntityType_Prop) continue;
		std::map<iLuxProp*, std::vector<bool> >::iterator f = gmapFrozen.find(static_cast<iLuxProp*>(pEnt));
		if(f == gmapFrozen.end()) continue;
		iLuxProp *p = f->first;
		for(size_t i=0; i<f->second.size() && i<(size_t)p->GetBodyNum(); ++i) if(f->second[i]) p->GetBody(i)->Enable();
	}
	gmapFrozen.clear();
}

// loader and ctor draws (animation start, critter timers) would otherwise follow the clock-seeded libc state
void OhplA5Seed(const std::string &asName)
{
	if(Dump() == NULL) return;
	srand((unsigned)Fnv(asName));
}

void OhplA5PropDump(iLuxProp *apProp)
{
	OhplA5Dump::Run(apProp);
}

void OhplA5OnLoad()
{
	if(Dump() == NULL || glLoadTick == 0) return;
	glTick = 0;
	glLoadTick = 0;
	gmapLast.clear();
	fprintf(Dump(), "LOADED\n");
	fflush(Dump());
}

void OhplA5Tick(cLuxMap *apMap)
{
	static const char *sPath = getenv("OHPL_A5_SCRIPT");
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
			if(line.second.compare(0, 7, "@WATCH ") == 0)
			{
				std::istringstream names(line.second.substr(7));
				std::string sName;
				while(names >> sName) gsetWatch.insert(sName);
				continue;
			}
			(bLoad ? vLoadLines : vLines).push_back(line);
		}
	}
	++glTick;
	if(glTick == 1) Unfreeze(apMap);
	if(getenv("OHPL_A5_NOFLICKER")) OhplA5Dump::NoFlicker(apMap);
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
		else if(sCmd.compare(0, 10, "@INTERACT ") == 0)
		{
			iLuxEntity *pEnt = apMap->GetEntityByName(sCmd.substr(10));
			if(pEnt)
			{
				iLuxProp *pProp = dynamic_cast<iLuxProp*>(pEnt);
				iPhysicsBody *pBody = pProp && pProp->GetBodyNum() > 0 ? pProp->GetMainBody() : NULL;
				bool bOk = pEnt->OnInteract(pBody, pBody ? pBody->GetWorldMatrix().GetTranslation() : cVector3f(0, 0, 0));
				fprintf(Dump(), "INTERACT %d %s %d\n", glTick, sCmd.c_str() + 10, (int)bOk);
			}
			else fprintf(Dump(), "INTERACT %d %s missing\n", glTick, sCmd.c_str() + 10);
		}
		else apMap->RunScript(sCmd);
	}
}
