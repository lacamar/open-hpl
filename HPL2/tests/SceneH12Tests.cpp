// H12 differential: cParticleEmitterData_UserData::LoadFromElement on real .ps data plus
// synthetic edge cases, then emitter frames driven through the particle kernel with a seeded rand().
// Run per OHPL_IMPL and compare the dumps; OHPL_IMPL=cpp output for a sample is recorded under
// rust/tests/golden/H12 and replayed by scene::tests_h12.
// Env: OHPL_H12_DIRS (':'-separated roots searched for *.ps), OHPL_H12_DUMP (default stdout),
// OHPL_H12_EVERY (dump every n-th emitter, default 1), OHPL_H12_ATTRS=1 (print attributes),
// OHPL_H12_SIM_EVERY (simulate every n-th dumped emitter, 0 = none), OHPL_H12_FRAMES (default 24).
// Lines: "emitter <id>", "a <name>\t<value>", "d <fields>", "sim <seed>", "f <n>", "p <slot> <floats>".

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "impl/tinyXML/tinyxml.h"
#include "math/Math.h"
#include "resources/XmlDocument.h"
#include "scene/ParticleEmitter_UserData.h"
#include "scene/RustParticles.h"

using namespace hpl;

namespace hpl {
	struct OhplH12PsProbe
	{
		static void Dump(FILE *f, cParticleEmitterData_UserData &d);
		static void Sim(FILE *f, cParticleEmitterData_UserData &d, int alSeed, int alFrames);
	};
}

namespace {

uint32_t Bits(float v) { uint32_t u; memcpy(&u, &v, 4); return u; }

struct Out
{
	FILE *f;
	void F(float v) { fprintf(f, " %08x", Bits(v)); }
	void I(int v) { fprintf(f, " %d", v); }
	void V2(const cVector2f &v) { F(v.x); F(v.y); }
	void V3(const cVector3f &v) { F(v.x); F(v.y); F(v.z); }
	void C(const cColor &c) { F(c.r); F(c.g); F(c.b); F(c.a); }
};

}

void hpl::OhplH12PsProbe::Dump(FILE *f, cParticleEmitterData_UserData &d)
{
	Out o = {f};
	fprintf(f, "d %s", d.msName.c_str());
	o.I(d.mPEType); o.I(d.mlMaxParticleNum); o.I(d.mbRespawn);
	o.F(d.mfParticlesPerSecond); o.F(d.mfStartTimeOffset); o.F(d.mfWarmUpTime); o.F(d.mfWarmUpStepsPerSec);
	o.F(d.mfMinPauseLength); o.F(d.mfMaxPauseLength); o.F(d.mfMinPauseInterval); o.F(d.mfMaxPauseInterval);
	o.V3(d.mvPosOffset); o.V3(d.mvAngleOffset);
	o.I(d.mvSubDiv.x); o.I(d.mvSubDiv.y); o.I(d.mSubDivType);
	o.F(d.mfFrameStep); o.F(d.mfMaxFrameTime);
	o.I(d.mStartPosType); o.V3(d.mvMinStartPos); o.V3(d.mvMaxStartPos);
	o.V2(d.mvMinStartAngles); o.V2(d.mvMaxStartAngles); o.F(d.mfMinStartRadius); o.F(d.mfMaxStartRadius);
	o.I(d.mStartVelType); o.V3(d.mvMinStartVel); o.V3(d.mvMaxStartVel);
	o.V2(d.mvMinStartVelAngles); o.V2(d.mvMaxStartVelAngles);
	o.F(d.mfMinStartVelSpeed); o.F(d.mfMaxStartVelSpeed); o.F(d.mfMinSpeedMultiply); o.F(d.mfMaxSpeedMultiply);
	o.V3(d.mvMinStartAcc); o.V3(d.mvMaxStartAcc); o.F(d.mfMinVelMaximum); o.F(d.mfMaxVelMaximum);
	o.I(d.mbUsesDirection); o.I(d.mGravityType); o.V3(d.mvGravityAcc); o.I(d.mCoordSystem);
	o.I(d.mbUsePartSpin); o.I(d.mPartSpinType); o.F(d.mfMinSpinRange); o.F(d.mfMaxSpinRange);
	o.I(d.mbUseRevolution); o.V3(d.mvMinRevVel); o.V3(d.mvMaxRevVel);
	o.F(d.mfMinLifeSpan); o.F(d.mfMaxLifeSpan); o.I(d.mDeathType);
	fprintf(f, " %s", d.msDeathPS.c_str());
	o.I(d.mDrawType); o.V2(d.mvMinStartSize); o.V2(d.mvMaxStartSize);
	o.F(d.mfStartRelSize); o.F(d.mfMiddleRelSize); o.F(d.mfMiddleRelSizeTime); o.F(d.mfMiddleRelSizeLength);
	o.F(d.mfEndRelSize); o.I(d.mbMultiplyRGBWithAlpha);
	o.C(d.mMinStartColor); o.C(d.mMaxStartColor); o.C(d.mStartRelColor); o.C(d.mMiddleRelColor);
	o.F(d.mfMiddleRelColorTime); o.F(d.mfMiddleRelColorLength); o.C(d.mEndRelColor);
	o.I(d.mbCollides); o.F(d.mfMinBounceAmount); o.F(d.mfMaxBounceAmount);
	o.I(d.mlMinCollisionMax); o.I(d.mlMaxCollisionMax); o.I(d.mlCollisionUpdateRate);
	o.I(d.mbUseBeamNoise); o.I(d.mlLowFreqPoints); o.I(d.mlHighFreqPoints);
	o.V3(d.mvMinLowFreqNoise); o.V3(d.mvMaxLowFreqNoise); o.V3(d.mvMinHighFreqNoise); o.V3(d.mvMaxHighFreqNoise);
	fprintf(f, "\n");
}

void hpl::OhplH12PsProbe::Sim(FILE *f, cParticleEmitterData_UserData &d, int alSeed, int alFrames)
{
	Out o = {f};
	srand(alSeed);
	OhplEmitter *pe = ohpl_pe_new(&d.mRustParams, d.mlMaxParticleNum, d.mbRespawn, &d.mvSubDiv, 1);
	cMatrixf mtx = cMatrixf::Identity;
	fprintf(f, "sim %d\n", alSeed);
	for(int fr = 0; fr < alFrames; ++fr)
	{
		ohpl_pe_update_motion(pe, 1.0f/30.0f, &mtx, NULL, NULL);
		uint32_t n = ohpl_pe_num_particles(pe);
		fprintf(f, "f %u\n", n);
		for(uint32_t i = 0; i < n && i < 24; ++i)
		{
			OhplParticle p;
			if(ohpl_pe_particle(pe, i, &p) == false) continue;
			fprintf(f, "p %u", i);
			o.V3(p.mvPos); o.C(p.mColor); o.V2(p.mvSize);
			fprintf(f, "\n");
		}
	}
	ohpl_pe_free(pe);
}

namespace {

struct Attr { const char *k, *v; };

struct Synth { const char *name; std::vector<Attr> attrs; };

std::vector<Synth> SynthCases()
{
	return {
		{"empty", {}},
		{"enums_upper", {{"PEType","BEAM"},{"DrawType","LINE"},{"CoordSystem","Local"},{"SubDivType","ANIMATION"},
			{"StartPosType","Sphere"},{"StartVelType","SPHERE"},{"GravityType","Center"},{"PartSpinType","Movement"}}},
		{"enums_garbage", {{"PEType","x"},{"DrawType","fixed"},{"CoordSystem",""},{"SubDivType","?"},
			{"StartPosType","cube"},{"GravityType","vec"},{"PartSpinType","spin"}}},
		{"vectors_bad", {{"MinStartPos","1 2"},{"MaxStartPos","1 2 3 4"},{"MinStartSize","3"},{"MinStartColor","1 1 1"},
			{"SubDiv","2"},{"PosOffset","a b c"},{"AngleOffset","90 180 -45"}}},
		{"vectors_ok", {{"MinStartPos","-1.5 2e1 0.25"},{"MinStartAngles","45 90"},{"MinStartVelAngles","-30 60"},
			{"MinStartSize","0.5 2"},{"MaxStartColor","0.1 0.2 0.3 0.4"},{"SubDiv","4 2"},{"AngleOffset","10 20 30"},
			{"GravityAcc","0 -9.8 0"},{"MinRevVel","1 2 3"}}},
		{"scalars", {{"MaxParticleNum","25"},{"Respawn","true"},{"ParticlesPerSecond","12.5"},{"WarmUpTime","2"},
			{"WarmUpStepsPerSec","30"},{"MinLifeSpan","1"},{"MaxLifeSpan","2.5"},{"Collides","TRUE"},{"UsesDirection","1"},
			{"MaterialNum","3"},{"AnimationLength","2"},{"Name","synth name"},{"DeathPS","ps_x.ps"}}},
		{"scalars_bad", {{"MaxParticleNum","12abc"},{"Respawn","yes"},{"ParticlesPerSecond",""},{"MinLifeSpan"," 3"},
			{"LowFreqPoints","-2"},{"UseBeamNoise","True"}}},
		{"sim_basic", {{"MaxParticleNum","40"},{"Respawn","true"},{"ParticlesPerSecond","60"},{"MinLifeSpan","0.5"},
			{"MaxLifeSpan","1"},{"MinStartVel","-1 0 -1"},{"MaxStartVel","1 2 1"},{"GravityType","Vector"},
			{"GravityAcc","0 -3 0"},{"MinStartSize","0.2 0.2"},{"MaxStartSize","0.4 0.4"},{"StartRelSize","1"},
			{"MiddleRelSize","1.5"},{"MiddleRelSizeTime","0.5"},{"MiddleRelSizeLength","0.2"},{"EndRelSize","0.5"},
			{"MinStartColor","1 0.5 0.2 1"},{"MaxStartColor","1 1 1 1"},{"EndRelColor","1 1 1 0"},{"SubDiv","2 2"}}},
		{"sim_sphere_spin", {{"MaxParticleNum","30"},{"Respawn","true"},{"ParticlesPerSecond","40"},{"MinLifeSpan","1"},
			{"MaxLifeSpan","1.5"},{"StartPosType","sphere"},{"MinStartRadius","0.1"},{"MaxStartRadius","1"},
			{"StartVelType","sphere"},{"MinStartVelSpeed","0.5"},{"MaxStartVelSpeed","2"},{"MinStartVelAngles","0 0"},
			{"MaxStartVelAngles","360 180"},{"UsePartSpin","true"},{"PartSpinType","movement"},{"MinSpinRange","-1"},
			{"MaxSpinRange","1"},{"UseRevolution","true"},{"MinRevVel","0 1 0"},{"MaxRevVel","0 2 0"},
			{"MaxVelMaximum","3"},{"MinSpeedMultiply","0.9"},{"MaxSpeedMultiply","0.99"}}},
	};
}

void Collect(const std::string &dir, std::vector<std::string> &out)
{
	DIR *d = opendir(dir.c_str());
	if(!d) return;
	while(dirent *e = readdir(d))
	{
		if(e->d_name[0] == '.') continue;
		std::string p = dir + "/" + e->d_name;
		struct stat st;
		if(stat(p.c_str(), &st) != 0) continue;
		if(S_ISDIR(st.st_mode)) Collect(p, out);
		else if(p.size() > 3 && strcasecmp(p.c_str() + p.size() - 3, ".ps") == 0) out.push_back(p);
	}
	closedir(d);
}

}

int hplMain(const hpl::tString&)
{
	const char *dump = getenv("OHPL_H12_DUMP");
	FILE *f = dump ? fopen(dump, "w") : stdout;
	if(!f) return 2;
	int lEvery = getenv("OHPL_H12_EVERY") ? atoi(getenv("OHPL_H12_EVERY")) : 1;
	if(lEvery < 1) lEvery = 1;
	int lSimEvery = getenv("OHPL_H12_SIM_EVERY") ? atoi(getenv("OHPL_H12_SIM_EVERY")) : 0;
	int lFrames = getenv("OHPL_H12_FRAMES") ? atoi(getenv("OHPL_H12_FRAMES")) : 24;
	bool bAttrs = getenv("OHPL_H12_ATTRS") && atoi(getenv("OHPL_H12_ATTRS")) != 0;

	std::vector<std::string> files;
	std::string roots = getenv("OHPL_H12_DIRS") ? getenv("OHPL_H12_DIRS") : "";
	for(size_t s = 0; s < roots.size();)
	{
		size_t e = roots.find(':', s);
		if(e == std::string::npos) e = roots.size();
		if(e > s) Collect(roots.substr(s, e - s), files);
		s = e + 1;
	}
	std::sort(files.begin(), files.end());

	int lId = 0, lDumped = 0, lSims = 0, lFilesBad = 0;
	auto emit = [&](const char *asLabel, cXmlElement &el, bool abForce, bool abSim)
	{
		if(lId++ % lEvery != 0 && !abForce) return;
		cParticleEmitterData_UserData data("", NULL, NULL);
		data.LoadFromElement(&el, false);
		fprintf(f, "emitter %d %s\n", lId - 1, asLabel);
		if(bAttrs)
			for(tAttributeMapIt it = el.GetAttributeMap()->begin(); it != el.GetAttributeMap()->end(); ++it)
				fprintf(f, "a %s\t%s\n", it->first.c_str(), it->second.c_str());
		OhplH12PsProbe::Dump(f, data);
		if(abSim || (lSimEvery > 0 && lDumped % lSimEvery == 0))
		{
			OhplH12PsProbe::Sim(f, data, 1000 + lId, lFrames);
			++lSims;
		}
		++lDumped;
	};

	for(const Synth &s : SynthCases())
	{
		cXmlElement el("ParticleEmitter", NULL);
		for(const Attr &a : s.attrs) el.SetAttribute(a.k, a.v);
		emit(s.name, el, true, strncmp(s.name, "sim_", 4) == 0);
	}

	for(const std::string &path : files)
	{
		TiXmlDocument doc;
		if(!doc.LoadFile(path.c_str())) { ++lFilesBad; continue; }
		TiXmlElement *root = doc.RootElement();
		if(!root || std::string(root->Value()) != "ParticleSystem") { ++lFilesBad; continue; }
		for(TiXmlElement *e = root->FirstChildElement(); e; e = e->NextSiblingElement())
		{
			if(std::string(e->Value()) != "ParticleEmitter") continue;
			cXmlElement el("ParticleEmitter", NULL);
			for(TiXmlAttribute *a = e->FirstAttribute(); a; a = a->Next()) el.SetAttribute(a->Name(), a->Value());
			emit(path.c_str(), el, false, false);
		}
	}
	fprintf(stderr, "files %zu (unreadable %d), emitters %d, dumped %d, simulated %d\n",
			files.size(), lFilesBad, lId, lDumped, lSims);
	if(dump) fclose(f);
	return 0;
}
