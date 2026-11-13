// H15 differential run, scripts/h15-diff.sh.
//  OHPL_H15_SURF=<file>[:<file>...] OHPL_H15_OUT=<out>  surface-data parse: the legacy TinyXML loader
//      (pre-Rust Physics.cpp) against cPhysics::LoadSurfaceData; one "path materials hash" line per file.
//  OHPL_H15_DUMP=<out>  world scenes with surface data: body state hash, every surface call at the
//      ohpl_phys_surface_* boundary (linker --wrap), update-list walk.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "PhysicsNewtonScenes.h"
#include "physics/Physics.h"
#include "physics/SurfaceData.h"
#include "physics/PhysicsRope.h"
#include "physics/PhysicsController.h"
#include "physics/CharacterBody.h"
#include "physics/PhysicsJoint.h"
#include "physics/RustPhysics.h"
#include "system/String.h"
#include "impl/tinyXML/tinyxml.h"

using namespace hpl;

extern "C" {
void __real_ohpl_phys_surface_on_impact(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, size_t);
void __real_ohpl_phys_surface_on_slide(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, int32_t, size_t, size_t);
void __real_ohpl_phys_surface_create_impact_effect(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, size_t, size_t);
void __real_ohpl_phys_surface_update_roll_effect(const OhplPhysVt*, const OhplSurfaceParams*, size_t);
}

namespace {

struct Hasher
{
	uint64_t h = 0xcbf29ce484222325ULL;
	void Bytes(const void* p, size_t n)
	{
		for (size_t i = 0; i < n; ++i) h = (h ^ ((const unsigned char*)p)[i]) * 0x100000001b3ULL;
	}
	void U32(uint32_t v) { Bytes(&v, 4); }
	void F(float v) { Bytes(&v, 4); }
	void S(const tString& s) { U32((uint32_t)s.size()); Bytes(s.data(), s.size()); }
};

struct Peek : cSurfaceData
{
	static void Hash(cSurfaceData& d, Hasher& h)
	{
		Peek& p = static_cast<Peek&>(d);
		h.S(p.msName); h.S(p.msUserVar); h.S(p.msStepType); h.S(p.msScrapeSoundName); h.S(p.msRollSoundName);
		h.F(p.mfElasticity); h.F(p.mfKineticFriction); h.F(p.mfStaticFriction);
		h.U32(p.mlPriority); h.U32(p.mElasticityMode); h.U32(p.mFrictionMode);
		h.F(p.mfMinScrapeSpeed); h.F(p.mfMinScrapeFreq); h.F(p.mfMinScrapeFreqSpeed); h.F(p.mfMiddleScrapeSpeed);
		h.F(p.mfMaxScrapeFreqSpeed); h.F(p.mfMaxScrapeFreq); h.U32(p.mlMinScrapeContacts);
		h.F(p.mfMinRollSpeed); h.F(p.mfMinRollFreq); h.F(p.mfMinRollVolume); h.F(p.mfMinRollFreqSpeed);
		h.F(p.mfMiddleRollSpeed); h.F(p.mfMaxRollFreqSpeed); h.F(p.mfMaxRollFreq); h.F(p.mfMaxRollVolume);
		h.U32(p.mRollAxisFlags);
		const tSurfaceImpactDataVec* lists[2] = { &p.mvImpactData, &p.mvHitData };
		for (int l = 0; l < 2; ++l)
		{
			h.U32((uint32_t)lists[l]->size());
			for (size_t i = 0; i < lists[l]->size(); ++i)
			{
				cSurfaceImpactData* d = (*lists[l])[i];
				h.F(d->GetMinSpeed()); h.S(d->GetSoundName()); h.S(d->GetPSName()); h.U32(d->GetPSPrio());
			}
		}
	}
};

ePhysicsMaterialCombMode LegacyCombMode(const char* apName)
{
	if (apName == NULL) return ePhysicsMaterialCombMode_Average;
	tString sMode = cString::ToLowerCase(apName);
	if (sMode == "average") return ePhysicsMaterialCombMode_Average;
	if (sMode == "min") return ePhysicsMaterialCombMode_Min;
	if (sMode == "max") return ePhysicsMaterialCombMode_Max;
	if (sMode == "multiply") return ePhysicsMaterialCombMode_Multiply;
	return ePhysicsMaterialCombMode_Average;
}

// Pre-Rust cPhysics::LoadSurfaceData (git a32587b^) without the haptic branch.
bool LegacyLoad(cPhysics& phys, const tString& asFile)
{
	TiXmlDocument* pXmlDoc = new TiXmlDocument(asFile.c_str());
	if (pXmlDoc->LoadFile() == false) { delete pXmlDoc; return false; }
	TiXmlElement* pRootElem = pXmlDoc->RootElement();
	if (pRootElem == NULL) { delete pXmlDoc; return true; }
	TiXmlElement* pChildElem = pRootElem->FirstChildElement("Material");
	for (; pChildElem != NULL; pChildElem = pChildElem->NextSiblingElement("Material"))
	{
		tString sName = cString::ToString(pChildElem->Attribute("Name"), "");
		if (sName == "") continue;
		cSurfaceData* pData = phys.CreateSurfaceData(sName);
		pData->SetElasticity(cString::ToFloat(pChildElem->Attribute("Elasticity"), 0.5f));
		pData->SetKineticFriction(cString::ToFloat(pChildElem->Attribute("KineticFriction"), 0.3f));
		pData->SetStaticFriction(cString::ToFloat(pChildElem->Attribute("StaticFriction"), 0.3f));
		pData->SetPriority(cString::ToInt(pChildElem->Attribute("Priority"), 0));
		pData->SetUserVar(cString::ToString(pChildElem->Attribute("UserVar"), ""));
		pData->SetElasticityCombMode(LegacyCombMode(pChildElem->Attribute("ElasticityMode")));
		pData->SetFrictionCombMode(LegacyCombMode(pChildElem->Attribute("FrictionMode")));
		pData->GetStepType(cString::ToString(pChildElem->Attribute("StepType"), ""));
		pData->SetMinScrapeSpeed(cString::ToFloat(pChildElem->Attribute("MinScrapeSpeed"), 0.7f));
		pData->SetMinScrapeFreq(cString::ToFloat(pChildElem->Attribute("MinScrapeFreq"), 0.7f));
		pData->SetMinScrapeFreqSpeed(cString::ToFloat(pChildElem->Attribute("MinScrapeFreqSpeed"), 0.7f));
		pData->SetMiddleScrapeSpeed(cString::ToFloat(pChildElem->Attribute("MiddleScrapeSpeed"), 1.2f));
		pData->SetMaxScrapeFreqSpeed(cString::ToFloat(pChildElem->Attribute("MaxScrapeFreqSpeed"), 3));
		pData->SetMaxScrapeFreq(cString::ToFloat(pChildElem->Attribute("MaxScrapeFreq"), 2));
		pData->SetMinScrapeContacts(cString::ToInt(pChildElem->Attribute("MinScrapeContacts"), 4));
		pData->SetScrapeSoundName(cString::ToString(pChildElem->Attribute("ScrapeSoundName"), ""));
		pData->SetMinRollSpeed(cString::ToFloat(pChildElem->Attribute("MinRollSpeed"), 0.7f));
		pData->SetMinRollFreq(cString::ToFloat(pChildElem->Attribute("MinRollFreq"), 0.7f));
		pData->SetMinRollVolume(cString::ToFloat(pChildElem->Attribute("MinRollVolume"), 0.7f));
		pData->SetMinRollFreqSpeed(cString::ToFloat(pChildElem->Attribute("MinRollFreqSpeed"), 0.7f));
		pData->SetMiddleRollSpeed(cString::ToFloat(pChildElem->Attribute("MiddleRollSpeed"), 1.2f));
		pData->SetMaxRollFreqSpeed(cString::ToFloat(pChildElem->Attribute("MaxRollFreqSpeed"), 3));
		pData->SetMaxRollFreq(cString::ToFloat(pChildElem->Attribute("MaxRollFreq"), 2));
		pData->SetMaxRollVolume(cString::ToFloat(pChildElem->Attribute("MaxRollVolume"), 2));
		pData->SetRollSoundName(cString::ToString(pChildElem->Attribute("RollSoundName"), ""));

		tString sAxisVec = cString::ToString(pChildElem->Attribute("RollAxis"), "");
		tStringVec vAxes;
		tFlag axisFlags = 0;
		cString::GetStringVec(sAxisVec, vAxes);
		for (size_t i = 0; i < vAxes.size(); ++i)
		{
			tString sAxis = cString::ToLowerCase(vAxes[i]);
			if (sAxis == "x") axisFlags |= eRollAxisFlag_X;
			else if (sAxis == "y") axisFlags |= eRollAxisFlag_Y;
			else if (sAxis == "z") axisFlags |= eRollAxisFlag_Z;
		}
		pData->SetRollAxisFlags(axisFlags);

		for (TiXmlElement* e = pChildElem->FirstChildElement("Impact"); e != NULL; e = e->NextSiblingElement("Impact"))
		{
			cSurfaceImpactData* d = pData->CreateImpactData(cString::ToFloat(e->Attribute("MinSpeed"), 1));
			d->SetSoundName(cString::ToString(e->Attribute("SoundName"), ""));
			d->SetPSName(cString::ToString(e->Attribute("PSName"), ""));
			d->SetPSPrio(cString::ToInt(e->Attribute("PSPrio"), 10));
		}
		for (TiXmlElement* e = pChildElem->FirstChildElement("Hit"); e != NULL; e = e->NextSiblingElement("Hit"))
		{
			cSurfaceImpactData* d = pData->CreateHitData(cString::ToFloat(e->Attribute("MinSpeed"), 1));
			d->SetSoundName(cString::ToString(e->Attribute("SoundName"), ""));
			d->SetPSName(cString::ToString(e->Attribute("PSName"), ""));
			d->SetPSPrio(cString::ToInt(e->Attribute("PSPrio"), 10));
		}
	}
	delete pXmlDoc;
	return true;
}

void HashAll(cPhysics& phys, Hasher& h, int& n)
{
	cSurfaceDataIterator it = phys.GetSurfaceDataIterator();
	while (it.HasNext())
	{
		Peek::Hash(*it.Next(), h);
		++n;
	}
}

int RunSurfaceParse(const char* list, const char* outFile)
{
	FILE* out = outFile ? std::fopen(outFile, "w") : NULL;
	int bad = 0;
	std::string files = list;
	size_t pos = 0;
	while (pos < files.size())
	{
		size_t end = files.find(':', pos);
		if (end == std::string::npos) end = files.size();
		std::string path = files.substr(pos, end - pos);
		pos = end + 1;

		cPhysics legacy(NULL), now(NULL);
		bool okLegacy = LegacyLoad(legacy, path);
		bool okNow = now.LoadSurfaceData(path);
		Hasher a, b;
		int na = 0, nb = 0;
		HashAll(legacy, a, na);
		HashAll(now, b, nb);
		size_t slash = path.find_last_of('/');
		size_t slash2 = slash == std::string::npos || slash == 0 ? std::string::npos : path.find_last_of('/', slash - 1);
		std::string base = slash2 == std::string::npos ? path : path.substr(slash2 + 1);
		bool same = okLegacy == okNow && na == nb && a.h == b.h;
		std::printf("%s legacy=%d/%d/%016llx rust=%d/%d/%016llx %s\n", base.c_str(), okLegacy, na, (unsigned long long)a.h,
					okNow, nb, (unsigned long long)b.h, same ? "same" : "DIFF");
		if (!same) ++bad;
		if (out) std::fprintf(out, "%s %d %016llx\n", base.c_str(), nb, (unsigned long long)b.h);
	}
	if (out) std::fclose(out);
	return bad ? 1 : 0;
}

//-----------------------------------------------------------------------

struct Calls
{
	Hasher h;
	int impact = 0, slide = 0, effect = 0, roll = 0;
	std::map<size_t, int> names;
	std::map<size_t, int> bodies;
	FILE* f = NULL;
	bool verbose = false;

	int Name(size_t p) { std::map<size_t, int>::iterator it = names.find(p); return it == names.end() ? -1 : it->second; }
	int Body(size_t p) { std::map<size_t, int>::iterator it = bodies.find(p); return it == bodies.end() ? -1 : it->second; }
	uint64_t rollSum = 0;

	// Update-list order follows body addresses, so roll events are summed, not chained.
	void Rec(const char* tag, size_t self, float speed, const cVector3f* pos, int a, int b, int c)
	{
		int s = Name(self);
		Hasher e;
		e.Bytes(tag, 1); e.U32(s); e.F(speed);
		if (pos) { e.F(pos->x); e.F(pos->y); e.F(pos->z); }
		e.U32(a); e.U32(b); e.U32(c);
		if (tag[0] == 'r') { rollSum += e.h; return; }
		h.U32((uint32_t)(e.h >> 32)); h.U32((uint32_t)e.h);
		if (f && verbose)
			std::fprintf(f, "  %s s%d %08x %d %d %d\n", tag, s, *(const unsigned*)&speed, a, b, c);
	}
};

Calls gC;

} // namespace

extern "C" {
void __wrap_ohpl_phys_surface_on_impact(const OhplPhysVt* vt, const OhplSurfaceParams* p, float speed, const cVector3f* pos, size_t body)
{
	++gC.impact;
	gC.Rec("i", p->self, speed, pos, gC.Body(body), 0, 0);
	__real_ohpl_phys_surface_on_impact(vt, p, speed, pos, body);
}
void __wrap_ohpl_phys_surface_on_slide(const OhplPhysVt* vt, const OhplSurfaceParams* p, float speed, const cVector3f* pos, int32_t contacts, size_t body, size_t against)
{
	++gC.slide;
	gC.Rec("s", p->self, speed, pos, gC.Body(body), gC.Body(against), contacts);
	__real_ohpl_phys_surface_on_slide(vt, p, speed, pos, contacts, body, against);
}
void __wrap_ohpl_phys_surface_create_impact_effect(const OhplPhysVt* vt, const OhplSurfaceParams* p, float speed, const cVector3f* pos, size_t second, size_t world)
{
	++gC.effect;
	gC.Rec("e", p->self, speed, pos, gC.Name(second), 0, 0);
	__real_ohpl_phys_surface_create_impact_effect(vt, p, speed, pos, second, world);
}
void __wrap_ohpl_phys_surface_update_roll_effect(const OhplPhysVt* vt, const OhplSurfaceParams* p, size_t body)
{
	++gC.roll;
	gC.Rec("r", p->self, 0, NULL, gC.Body(body), 0, 0);
	__real_ohpl_phys_surface_update_roll_effect(vt, p, body);
}
}

namespace {

using namespace hplscenes;

iPhysicsBody* AddBody(cPhysicsWorldNewton& w, std::vector<iPhysicsBody*>& bodies, iPhysicsBody* b, iPhysicsMaterial* m)
{
	b->SetMaterial(m);
	gC.bodies[(size_t)b] = (int)bodies.size();
	bodies.push_back(b);
	return b;
}

void SceneSurfaceWorld(const char* name, bool mover)
{
	gD.Begin(name);
	gC = Calls();
	gC.f = gD.f;
	gC.verbose = gD.verbose;

	cPhysics phys(NULL);
	cSurfaceData* wood = phys.CreateSurfaceData("Wood");
	wood->SetPriority(1);
	wood->SetMinScrapeSpeed(0.05f);
	wood->SetMinScrapeContacts(1);
	wood->SetScrapeSoundName("scrape_wood");
	wood->CreateImpactData(4.0f)->SetSoundName("hard");
	wood->CreateImpactData(0.5f)->SetSoundName("soft");
	wood->GetImpactData(0)->SetPSName("dust");
	cSurfaceData* metal = phys.CreateSurfaceData("Metal");
	metal->SetPriority(2);
	metal->SetMinScrapeSpeed(0.05f);
	metal->SetMinScrapeContacts(1);
	metal->SetRollSoundName("roll_metal");
	metal->CreateImpactData(1.0f)->SetSoundName("clang");
	gC.names[(size_t)wood] = 0;
	gC.names[(size_t)metal] = 1;

	cPhysicsWorldNewton& w = *NewWorld();
	w.SetWorld(NULL);
	iPhysicsMaterial* mWood = wood->ToMaterial(&w);
	iPhysicsMaterial* mMetal = metal->ToMaterial(&w);

	std::vector<iPhysicsBody*> bodies;
	AddBody(w, bodies, Floor(w), mMetal);
	for (int i = 0; i < 4; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Box", w.CreateBoxShape(cVector3f(0.5f, 0.5f, 0.5f), NULL));
		b->SetMass(1.0f + i * 0.3f);
		b->SetPosition(cVector3f(-2.0f + i * 1.3f, 1.0f + i * 0.7f, 0.1f * i));
		b->SetLinearVelocity(cVector3f(1.5f - i, 0, 0.5f * i));
		AddBody(w, bodies, b, i % 2 ? mMetal : mWood);
	}
	for (int i = 0; i < 3; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Ball", w.CreateSphereShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
		b->SetMass(0.8f);
		b->SetPosition(cVector3f(1.0f + 0.4f * i, 2.0f + 0.9f * i, -1.0f));
		b->SetAngularVelocity(cVector3f(0, 0, 4.0f));
		AddBody(w, bodies, b, mMetal);
	}
	iPhysicsBody* moverBody = NULL;
	if (mover)
	{
		moverBody = w.CreateBody("Mover", w.CreateBoxShape(cVector3f(2, 0.2f, 2), NULL));
		moverBody->SetMass(0);
		moverBody->SetPosition(cVector3f(0, 0.2f, 3));
		AddBody(w, bodies, moverBody, mWood);
		iPhysicsBody* b = w.CreateBody("Rider", w.CreateBoxShape(cVector3f(0.4f, 0.4f, 0.4f), NULL));
		b->SetMass(1);
		b->SetPosition(cVector3f(0, 0.8f, 3));
		AddBody(w, bodies, b, mMetal);
	}

	const int steps = 600;
	for (int i = 0; i < steps; ++i)
	{
		if (moverBody) moverBody->SetPosition(cVector3f(0.01f * i, 0.2f, 3));
		w.Update(1.0f / 60.0f);
		for (size_t b = 0; b < bodies.size(); ++b)
		{
			gD.Matrix(bodies[b]->GetLocalMatrix());
			gD.Vec(bodies[b]->GetLinearVelocity());
			gD.Vec(bodies[b]->GetAngularVelocity());
			gD.Int(bodies[b]->GetEnabled() ? 1 : 0);
			gD.Int(bodies[b]->IsInUpdateList() ? 1 : 0);
			gD.Int(bodies[b]->HasSlide() ? 1 : 0);
		}
	}
	if (gD.f)
	{
		for (size_t b = 0; b < bodies.size(); ++b)
		{
			char tag[32];
			std::snprintf(tag, sizeof(tag), "  final%d", (int)b);
			gD.Line(tag, bodies[b]->GetLocalMatrix());
		}
		std::fprintf(gD.f, "  calls impact %d slide %d effect %d roll %d hash %016llx rollsum %016llx\n", gC.impact, gC.slide, gC.effect, gC.roll,
					(unsigned long long)gC.h.h, (unsigned long long)gC.rollSum);
	}
	hplDelete(&w);
	gD.End(name, steps);
}

struct CountCb : iPhysicsWorldCollisionCallback
{
	int n = 0;
	void OnCollision(iPhysicsBody* b, cCollideData* d) { ++n; gD.Float(b->GetMass()); gD.Int(d->mlNumOfPoints); }
};

// World object lists: lookups, iteration order, destroy order, joint breakage, shape-world queries, DestroyAll.
void SceneWorldLists()
{
	gD.Begin("world_lists");
	cPhysicsWorldNewton& w = *NewWorld();
	w.SetWorld(NULL);
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	for (int i = 0; i < 6; ++i)
	{
		char n[8];
		std::snprintf(n, sizeof(n), "B%d", i);
		iPhysicsBody* b = w.CreateBody(n, i % 2 ? w.CreateBoxShape(cVector3f(0.5f, 0.5f, 0.5f), NULL) : w.CreateSphereShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
		b->SetMass(i == 5 ? 0 : 1.0f + 0.2f * i);
		b->SetPosition(cVector3f(-2.0f + i * 0.9f, 0.6f + 0.5f * i, 0.2f * i));
		bodies.push_back(b);
	}
	iPhysicsJointBall* ball = w.CreateJointBall("j_ball", cVector3f(-1.5f, 1.0f, 0), cVector3f(0, 1, 0), bodies[1], bodies[2]);
	iPhysicsJointHinge* hinge = w.CreateJointHinge("j_hinge", cVector3f(0.5f, 2.0f, 0.4f), cVector3f(0, 1, 0), bodies[3], bodies[4]);
	hinge->SetBreakable(true);
	hinge->SetBreakForce(5.0f);
	iPhysicsJointSlider* slider = w.CreateJointSlider("j_slider", cVector3f(1.0f, 1.0f, 0), cVector3f(1, 0, 0), bodies[5], bodies[6]);
	(void)ball; (void)slider;
	iPhysicsController* c0 = w.CreateController("c0");
	iPhysicsController* c1 = w.CreateController("c1");
	c0->SetBody(bodies[4]); c0->SetType(ePhysicsControllerType_Pid); c0->SetA(10); c0->SetDestValue(1); c0->SetMaxOutput(10);
	c0->SetInputType(ePhysicsControllerInput_LinearSpeed, ePhysicsControllerAxis_Y);
	c0->SetOutputType(ePhysicsControllerOutput_Force, ePhysicsControllerAxis_Y);
	c0->SetActive(true);
	c1->SetBody(bodies[3]);
	iPhysicsRope* r0 = w.CreateRope("r0", cVector3f(0, 4, 3), cVector3f(2, 3.9f, 3));
	iPhysicsRope* r1 = w.CreateRope("r1", cVector3f(0, 4, -3), cVector3f(2, 3.9f, -3));
	r0->SetSegmentLength(0.25f); r1->SetSegmentLength(0.4f);
	r0->SetAttachedStartBody(bodies[6]);
	iCharacterBody* hero = w.CreateCharacterBody("Hero", cVector3f(0.5f, 1.8f, 0.5f));
	hero->SetFeetPosition(cVector3f(4, 0.05f, 4), false);
	w.CreateMaterial("Zeta");
	w.CreateMaterial("Alpha");
	iCollideShape* spare = w.CreateBoxShape(cVector3f(1, 1, 1), NULL);

	std::map<size_t, int> ids;
	for (size_t i = 0; i < bodies.size(); ++i) ids[(size_t)bodies[i]] = (int)i;
	ids[(size_t)hero] = 100; ids[(size_t)r0] = 101; ids[(size_t)r1] = 102; ids[(size_t)c0] = 103; ids[(size_t)c1] = 104;
	ids[(size_t)hinge] = 105; ids[(size_t)ball] = 106; ids[(size_t)slider] = 107;
	struct Id { std::map<size_t, int>& m; int operator()(const void* p) const { std::map<size_t, int>::iterator it = m.find((size_t)p); return it == m.end() ? -1 : it->second; } } id = { ids };

	for (int step = 0; step < 600; ++step)
	{
		hero->Move(eCharDir_Forward, 1.0f);
		if (step == 100) { w.DestroyBody(bodies[2]); bodies[2] = NULL; }
		if (step == 150) w.DestroyController(c0);
		if (step == 250) w.DestroyRope(r0);
		if (step == 300)
		{
			cBoundingVolume bv;
			bv.SetLocalMinMax(cVector3f(-3, -1, -3), cVector3f(3, 5, 3));
			w.EnableBodiesInBV(&bv, true);
		}
		w.Update(1.0f / 60.0f);
		if (step % 50 == 49)
		{
			gD.Int(step);
			gD.Int(id(w.GetBody("B1"))); gD.Int(id(w.GetBody("B2"))); gD.Int(id(w.GetBody("nope")));
			gD.Int(id(w.GetJoint("j_hinge"))); gD.Int(id(w.GetJoint("j_ball"))); gD.Int(id(w.GetJoint("j_slider")));
			gD.Int(w.JointExists(hinge) ? 1 : 0);
			gD.Int(id(w.GetRope("r0"))); gD.Int(id(w.GetRope("r1")));
			gD.Int(id(w.GetCharacterBody("Hero")));
			gD.Int(w.GetMaterialFromName("Alpha") ? 1 : 0); gD.Int(w.GetMaterialFromName("nope") ? 1 : 0);
			for (cPhysicsBodyIterator it = w.GetBodyIterator(); it.HasNext();) { iPhysicsBody* b = it.Next(); gD.Int(id(b)); gD.Matrix(b->GetLocalMatrix()); }
			for (cPhysicsJointIterator it = w.GetJointIterator(); it.HasNext();) gD.Int(id(it.Next()));
			for (cPhysicsMaterialIterator it = w.GetMaterialIterator(); it.HasNext();) { iPhysicsMaterial* m = it.Next(); gD.Int((int)m->GetName().size()); gD.Mix(m->GetName().c_str(), m->GetName().size()); }
			for (int q = 0; q < 4; ++q)
			{
				CountCb cb;
				cVector3f push(7, 7, 7);
				cMatrixf at = cMath::MatrixTranslate(cVector3f(-1.5f + q * 1.5f, 1.0f + 0.2f * q, 0.3f));
				bool hit = w.CheckShapeWorldCollision(q == 3 ? NULL : &push, spare, at, q == 1 ? bodies[1] : NULL, q == 2, q == 0, q == 0 ? &cb : NULL, q != 1, q, eFlagBit_All, false);
				gD.Int(hit ? 1 : 0); gD.Vec(push); gD.Int(cb.n);
			}
			gD.Int(hero->IsOnGround() ? 1 : 0); gD.Vec(hero->GetPosition());
		}
	}
	w.DestroyJoint(slider);
	gD.Int(w.JointExists(slider) ? 1 : 0);
	w.DestroyCharacterBody(hero);
	gD.Int(id(w.GetCharacterBody("Hero")));
	w.DestroyShape(spare);
	if (gD.f)
		for (size_t i = 0; i < bodies.size(); ++i)
			if (bodies[i]) { char tag[32]; std::snprintf(tag, sizeof(tag), "  final%d", (int)i); gD.Line(tag, bodies[i]->GetLocalMatrix()); }
	w.DestroyAll();
	gD.Int(id(w.GetBody("B1"))); gD.Int(id(w.GetJoint("j_ball"))); gD.Int(id(w.GetRope("r1")));
	gD.Int(w.GetMaterialFromName("Alpha") ? 1 : 0);
	hplDelete(&w);
	gD.End("world_lists", 600);
}

} // namespace

int hplMain(const tString&)
{
	if (const char* list = std::getenv("OHPL_H15_SURF"))
		return RunSurfaceParse(list, std::getenv("OHPL_H15_OUT"));
	if (const char* dump = std::getenv("OHPL_H15_DUMP"))
	{
		gD.f = std::fopen(dump, "w");
		gD.verbose = std::getenv("OHPL_H15_VERBOSE") != NULL;
		SceneSurfaceWorld("surface_drop", false);
		SceneSurfaceWorld("surface_mover", true);
		SceneWorldLists();
		if (gD.f) std::fclose(gD.f);
		return 0;
	}
	std::printf("set OHPL_H15_SURF or OHPL_H15_DUMP\n");
	return 0;
}
