#ifndef HPL_PHYSICS_NEWTON_SCENES_H
#define HPL_PHYSICS_NEWTON_SCENES_H

// Differential scenes for the Newton backend: run once per OHPL_IMPL and compare the dumps bit for bit.
// OHPL_L4_DUMP=<file> writes the dump; OHPL_L4_VERBOSE=1 adds per-step matrices.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "physics/PhysicsWorld.h"
#include "physics/CollideShape.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsJointBall.h"
#include "physics/PhysicsJointHinge.h"
#include "physics/PhysicsJointSlider.h"
#include "physics/PhysicsJointScrew.h"
#include "physics/PhysicsMaterial.h"
#include "physics/CharacterBody.h"
#include "resources/BinaryBuffer.h"
#include "math/Math.h"

namespace hplscenes {

using namespace hpl;

struct Dump
{
	FILE* f = NULL;
	bool verbose = false;
	uint64_t hash = 0xcbf29ce484222325ULL;

	void Mix(const void* p, size_t n)
	{
		for (size_t i = 0; i < n; ++i) hash = (hash ^ ((const unsigned char*)p)[i]) * 0x100000001b3ULL;
	}
	void Float(float v) { Mix(&v, sizeof(v)); }
	void Int(int v) { Mix(&v, sizeof(v)); }
	void Vec(const cVector3f& v) { Float(v.x); Float(v.y); Float(v.z); }
	void Matrix(const cMatrixf& m) { Mix(&m.m[0][0], sizeof(float) * 16); }
	void Line(const char* tag, const cMatrixf& m)
	{
		if (!f) return;
		std::fprintf(f, "%s", tag);
		const unsigned* u = (const unsigned*)&m.m[0][0];
		for (int i = 0; i < 16; ++i) std::fprintf(f, " %08x", u[i]);
		std::fprintf(f, "\n");
	}
	void Begin(const char* scene) { hash = 0xcbf29ce484222325ULL; if (f) std::fprintf(f, "scene %s\n", scene); }
	void End(const char* scene, int steps)
	{
		if (f) std::fprintf(f, "end %s steps %d hash %016llx\n", scene, steps, (unsigned long long)hash);
	}
};

static Dump gD;

static void Step(iPhysicsWorld& w, std::vector<iPhysicsBody*>& bodies, int steps, float dt = 1.0f / 60.0f)
{
	for (int i = 0; i < steps; ++i)
	{
		w.Update(dt);
		for (size_t b = 0; b < bodies.size(); ++b)
		{
			gD.Matrix(bodies[b]->GetLocalMatrix());
			gD.Vec(bodies[b]->GetLinearVelocity());
			gD.Vec(bodies[b]->GetAngularVelocity());
			gD.Int(bodies[b]->GetEnabled() ? 1 : 0);
			if (gD.verbose && (i % 100) == 99)
			{
				char tag[32];
				std::snprintf(tag, sizeof(tag), "  b%d@%d", (int)b, i);
				gD.Line(tag, bodies[b]->GetLocalMatrix());
			}
		}
	}
	for (size_t b = 0; b < bodies.size(); ++b)
	{
		char tag[32];
		std::snprintf(tag, sizeof(tag), "  final%d", (int)b);
		gD.Line(tag, bodies[b]->GetLocalMatrix());
	}
}

static iPhysicsWorld* NewWorld(ePhysicsAccuracy acc = ePhysicsAccuracy_High)
{
	iPhysicsWorld* w = hplNew(iPhysicsWorld, ());
	w->SetGravity(cVector3f(0, -9.81f, 0));
	w->SetMaxTimeStep(1.0f / 60.0f);
	w->SetAccuracyLevel(acc);
	w->SetWorldSize(cVector3f(-100), cVector3f(100));
	return w;
}

static iPhysicsBody* Floor(iPhysicsWorld& w, float y = 0)
{
	iCollideShape* s = hplNew(iCollideShape, (eCollideShapeType_Mesh, 0, NULL, w.GetNewtonWorld(), &w));
	const unsigned int idx[6] = { 0, 1, 2, 0, 2, 3 };
	const float vtx[12] = { -20, y, -20, 20, y, -20, 20, y, 20, -20, y, 20 };
	s->CreateFromVertices(idx, 6, vtx, 3, 4);
	iPhysicsBody* b = w.CreateBody("Floor", s);
	b->SetMass(0);
	return b;
}

//-----------------------------------------------------------------------

static void SceneStack()
{
	gD.Begin("stack");
	iPhysicsWorld& w = *NewWorld();
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	for (int i = 0; i < 6; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Box", w.CreateBoxShape(cVector3f(0.5f, 0.5f, 0.5f), NULL));
		b->SetMass(1.0f + i * 0.1f);
		b->SetPosition(cVector3f(0.01f * i, 0.26f + i * 0.52f, 0.005f * i));
		bodies.push_back(b);
	}
	Step(w, bodies, 600);
	hplDelete(&w);
	gD.End("stack", 600);
}

static void ScenePile()
{
	gD.Begin("pile");
	iPhysicsWorld& w = *NewWorld(ePhysicsAccuracy_Medium);
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	cMatrixf off = cMath::MatrixRotateY(0.3f);
	iCollideShape* shapes[6] = {
		w.CreateSphereShape(cVector3f(0.3f, 0, 0), NULL),
		w.CreateCapsuleShape(0.2f, 0.9f, NULL),
		w.CreateCylinderShape(0.25f, 0.5f, NULL),
		w.CreateBoxShape(cVector3f(0.4f, 0.2f, 0.3f), &off),
		NULL, NULL };
	tCollideShapeVec sub;
	sub.push_back(w.CreateBoxShape(cVector3f(0.6f, 0.1f, 0.1f), NULL));
	cMatrixf mo = cMath::MatrixTranslate(cVector3f(0, 0.2f, 0));
	sub.push_back(w.CreateSphereShape(cVector3f(0.15f, 0, 0), &mo));
	shapes[4] = w.CreateCompundShape(sub);
	shapes[5] = w.CreateBoxShape(cVector3f(0.3f, 0.3f, 0.3f), NULL);
	for (int i = 0; i < 18; ++i)
	{
		iPhysicsBody* b = w.CreateBody("P", shapes[i % 6]);
		b->SetMass(0.5f + (i % 4) * 0.25f);
		b->SetPosition(cVector3f(((i * 37) % 11) * 0.07f - 0.35f, 0.5f + i * 0.6f, ((i * 53) % 7) * 0.06f));
		b->SetAngularVelocity(cVector3f(0.2f * (i % 3), 0.1f * (i % 5), 0));
		b->SetLinearDamping(0.1f);
		b->SetAngularDamping(0.2f);
		b->SetContinuousCollision((i & 1) != 0);
		bodies.push_back(b);
	}
	// heightfield + static scene shape
	std::vector<unsigned short> elev(9 * 9, 32768);
	iPhysicsBody* hf = w.CreateBody("Terrain", w.CreateHeightFieldShape(9, elev.data(), 1.0f, 2.0f / 65535.0f));
	hf->SetMass(0);
	hf->SetMatrix(cMath::MatrixTranslate(cVector3f(30, 0, 30)));
	bodies.push_back(hf);
	tCollideShapeVec sc;
	sc.push_back(w.CreateBoxShape(cVector3f(2, 0.2f, 2), NULL));
	iPhysicsBody* sb = w.CreateBody("Scene", w.CreateStaticSceneShape(sc, NULL));
	sb->SetMass(0);
	sb->SetPosition(cVector3f(-10, 0.5f, 0));
	bodies.push_back(sb);
	iPhysicsBody* onScene = w.CreateBody("OnScene", shapes[5]);
	onScene->SetMass(1);
	onScene->SetPosition(cVector3f(-10, 2, 0));
	bodies.push_back(onScene);
	Step(w, bodies, 600);
	hplDelete(&w);
	gD.End("pile", 600);
}

static void SceneJoints()
{
	gD.Begin("joints");
	iPhysicsWorld& w = *NewWorld();
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w, -8));
	iPhysicsBody* anchor = w.CreateBody("Anchor", w.CreateBoxShape(cVector3f(0.2f, 0.2f, 0.2f), NULL));
	anchor->SetMass(0);
	anchor->SetPosition(cVector3f(0, 5, 0));
	bodies.push_back(anchor);

	// ball chain
	iPhysicsBody* prev = anchor;
	for (int i = 0; i < 6; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Link", w.CreateBoxShape(cVector3f(0.15f, 0.4f, 0.15f), NULL));
		b->SetMass(0.5f);
		b->SetPosition(cVector3f(0.05f * i, 4.6f - i * 0.4f, 0));
		iPhysicsJointBall* j = w.CreateJointBall("ball", cVector3f(0, 4.8f - i * 0.4f, 0), cVector3f(0, 1, 0), prev, b);
		if (i == 2) j->SetConeLimits(0.5f, 0.3f);
		bodies.push_back(b);
		prev = b;
	}

	// hinge door with limits
	iPhysicsBody* post = w.CreateBody("Post", w.CreateBoxShape(cVector3f(0.1f, 2, 0.1f), NULL));
	post->SetMass(0);
	post->SetPosition(cVector3f(5, 1, 0));
	bodies.push_back(post);
	iPhysicsBody* door = w.CreateBody("Door", w.CreateBoxShape(cVector3f(1, 2, 0.1f), NULL));
	door->SetMass(4);
	door->SetPosition(cVector3f(5.55f, 1, 0));
	door->SetLinearVelocity(cVector3f(0, 0, 1.5f));
	door->SetAngularVelocity(cVector3f(0, 2.0f, 0));
	iPhysicsJointHinge* hinge = w.CreateJointHinge("hinge", cVector3f(5.05f, 1, 0), cVector3f(0, 1, 0), post, door);
	hinge->SetMaxAngle(1.2f);
	hinge->SetMinAngle(-0.4f);
	bodies.push_back(door);

	// free-hanging hinge (no parent)
	iPhysicsBody* flap = w.CreateBody("Flap", w.CreateBoxShape(cVector3f(1, 0.1f, 0.5f), NULL));
	flap->SetMass(1);
	flap->SetPosition(cVector3f(-5, 3, 0));
	w.CreateJointHinge("hinge2", cVector3f(-5.5f, 3, 0), cVector3f(0, 0, 1), NULL, flap);
	bodies.push_back(flap);

	// slider
	iPhysicsBody* rail = w.CreateBody("Rail", w.CreateBoxShape(cVector3f(3, 0.1f, 0.1f), NULL));
	rail->SetMass(0);
	rail->SetPosition(cVector3f(0, 3, 6));
	bodies.push_back(rail);
	iPhysicsBody* cart = w.CreateBody("Cart", w.CreateBoxShape(cVector3f(0.4f, 0.4f, 0.4f), NULL));
	cart->SetMass(2);
	cart->SetPosition(cVector3f(0, 3, 6));
	cart->SetLinearVelocity(cVector3f(2, 0, 0));
	iPhysicsJointSlider* sl = w.CreateJointSlider("slider", cVector3f(0, 3, 6), cVector3f(1, 0, 0), rail, cart);
	sl->SetMaxDistance(1.5f);
	sl->SetMinDistance(-1.0f);
	bodies.push_back(cart);

	// screw
	iPhysicsBody* nut = w.CreateBody("Nut", w.CreateBoxShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
	nut->SetMass(1);
	nut->SetPosition(cVector3f(0, 3, -6));
	nut->SetLinearVelocity(cVector3f(0, 1.5f, 0));
	iPhysicsJointScrew* sc = w.CreateJointScrew("screw", cVector3f(0, 3, -6), cVector3f(0, 1, 0), NULL, nut);
	sc->SetMaxDistance(1.0f);
	sc->SetMinDistance(-1.0f);
	bodies.push_back(nut);

	Step(w, bodies, 600);
	for (int k = 0; k < 4; ++k)
	{
		gD.Float(hinge->GetAngle());
		gD.Float(hinge->GetForceSize());
		gD.Vec(hinge->GetAngularVelocity());
		gD.Float(sl->GetDistance());
		gD.Float(sl->GetForceSize());
		gD.Vec(sl->GetVelocity());
		gD.Float(sc->GetDistance());
		gD.Float(sc->GetAngle());
		gD.Float(sc->GetForceSize());
		gD.Vec(sc->GetVelocity());
	}
	hplDelete(&w);
	gD.End("joints", 600);
}

class cCounter : public iPhysicsBodyCallback
{
public:
	int aabb = 0, collide = 0;
	float maxSpeed = 0;
	bool veto = false;
	bool OnAABBCollide(iPhysicsBody*, iPhysicsBody*) { ++aabb; return !veto; }
	void OnBodyCollide(iPhysicsBody*, iPhysicsBody*, cPhysicsContactData* d)
	{
		++collide;
		if (d->mfMaxContactNormalSpeed > maxSpeed) maxSpeed = d->mfMaxContactNormalSpeed;
		gD.Float(d->mfMaxContactNormalSpeed);
		gD.Float(d->mfMaxContactTangentSpeed);
		gD.Vec(d->mvForce);
		gD.Vec(d->mvContactNormal);
		gD.Vec(d->mvContactPosition);
	}
};

static void SceneContacts()
{
	gD.Begin("contacts");
	iPhysicsWorld& w = *NewWorld();
	w.SetSaveContactPoints(true);
	std::vector<iPhysicsBody*> bodies;
	iPhysicsMaterial* mat = w.CreateMaterial("Bouncy");
	mat->SetElasticity(0.8f);
	mat->SetStaticFriction(0.9f);
	mat->SetKineticFriction(0.7f);
	mat->SetElasticityCombMode(ePhysicsMaterialCombMode_Max);
	bodies.push_back(Floor(w));
	static cCounter counter;
	counter = cCounter();
	for (int i = 0; i < 4; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Ball", w.CreateSphereShape(cVector3f(0.25f, 0, 0), NULL));
		b->SetMass(1);
		b->SetPosition(cVector3f(i * 0.8f, 1.5f + i, 0));
		if (i & 1) b->SetMaterial(mat);
		b->AddBodyCallback(&counter);
		if (i == 3) b->SetCollide(false);
		bodies.push_back(b);
	}
	Step(w, bodies, 300);
	counter.veto = true;
	Step(w, bodies, 300);
	gD.Int(counter.aabb);
	gD.Int(counter.collide);
	gD.Int((int)w.GetContactPoints()->size());
	hplDelete(&w);
	gD.End("contacts", 600);
}

class cRayRec : public iPhysicsRayCallback
{
public:
	int calls = 0;
	bool before = true;
	bool BeforeIntersect(iPhysicsBody* b) { gD.Int(1); return before; }
	bool OnIntersect(iPhysicsBody* b, cPhysicsRayParams* p)
	{
		++calls;
		gD.Float(p->mfT);
		gD.Float(p->mfDist);
		gD.Vec(p->mvNormal);
		gD.Vec(p->mvPoint);
		return true;
	}
};

static void SceneQueries()
{
	gD.Begin("queries");
	iPhysicsWorld& w = *NewWorld();
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	iCollideShape* shp[3] = { w.CreateBoxShape(cVector3f(1, 1, 1), NULL), w.CreateSphereShape(cVector3f(0.5f, 0, 0), NULL),
		w.CreateCapsuleShape(0.3f, 1.2f, NULL) };
	for (int i = 0; i < 3; ++i)
	{
		iPhysicsBody* b = w.CreateBody("Q", shp[i]);
		b->SetMass(i == 0 ? 0 : 1);
		b->SetPosition(cVector3f(i * 2.0f, 0.5f + i * 0.2f, 0));
		bodies.push_back(b);
	}
	Step(w, bodies, 60);
	cRayRec rr;
	for (int pf = 0; pf < 2; ++pf)
		for (int i = 0; i < 6; ++i)
		{
			rr.before = (i != 4);
			w.CastRay(&rr, cVector3f(-1 + i * 0.7f, 5, 0.01f * i), cVector3f(-1 + i * 0.7f, -1, 0.01f * i), true, true, true, pf == 1);
		}
	gD.Int(rr.calls);

	cMatrixf ma = cMath::MatrixTranslate(cVector3f(0, 0, 0));
	cMatrixf mb = cMath::MatrixTranslate(cVector3f(0.8f, 0.1f, 0));
	cCollideData cd;
	cd.SetMaxSize(8);
	for (int c = 0; c < 2; ++c)
	{
		bool hit = w.CheckShapeCollision(shp[0], ma, shp[1], mb, cd, 4, c == 1);
		gD.Int(hit ? 1 : 0);
		gD.Int(cd.mlNumOfPoints);
		for (int i = 0; hit && i < cd.mlNumOfPoints; ++i)
		{
			gD.Float(cd.mvContactPoints[i].mfDepth);
			gD.Vec(cd.mvContactPoints[i].mvNormal);
			gD.Vec(cd.mvContactPoints[i].mvPoint);
		}
	}
	tCollideShapeVec sub;
	sub.push_back(shp[0]);
	sub.push_back(shp[1]);
	iCollideShape* comp = w.CreateCompundShape(sub);
	bool hit = w.CheckShapeCollision(comp, ma, shp[2], mb, cd, 6, true);
	gD.Int(hit ? 1 : 0);
	gD.Int(cd.mlNumOfPoints);
	for (int i = 0; hit && i < cd.mlNumOfPoints; ++i)
	{
		gD.Float(cd.mvContactPoints[i].mfDepth);
		gD.Vec(cd.mvContactPoints[i].mvNormal);
		gD.Vec(cd.mvContactPoints[i].mvPoint);
	}
	cVector3f n;
	for (int l = 0; l < 2; ++l)
	{
		gD.Float(w.CastRayOnShape(shp[0], ma, cVector3f(0.1f, 3, 0.1f), cVector3f(0.1f, -3, 0.1f), n, l == 1));
		gD.Vec(n);
	}
	cBoundingVolume bv;
	bv.SetLocalMinMax(cVector3f(-1, -1, -1), cVector3f(3, 3, 1));
	std::vector<iPhysicsBody*> found;
	w.GetBodiesInBV(&bv, &found);
	gD.Int((int)found.size());
	for (size_t i = 0; i < found.size(); ++i) gD.Matrix(found[i]->GetLocalMatrix());
	hplDelete(&w);
	gD.End("queries", 60);
}

static void SceneSleepForces()
{
	gD.Begin("sleep");
	iPhysicsWorld& w = *NewWorld();
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	for (int i = 0; i < 5; ++i)
	{
		iPhysicsBody* b = w.CreateBody("S", w.CreateBoxShape(cVector3f(0.4f, 0.4f, 0.4f), NULL));
		b->SetMass(1);
		b->SetPosition(cVector3f(i, 0.21f, 0));
		b->SetAutoDisable(i != 0);
		b->SetMaxLinearSpeed(i == 2 ? 3.0f : 0);
		b->SetMaxAngularSpeed(i == 2 ? 2.0f : 0);
		bodies.push_back(b);
	}
	Step(w, bodies, 120);
	bodies[2]->AddForce(cVector3f(500, 200, 0));
	bodies[3]->AddForceAtPosition(cVector3f(0, 300, 0), bodies[3]->GetWorldPosition() + cVector3f(0.2f, 0, 0));
	bodies[4]->AddTorque(cVector3f(0, 200, 0));
	bodies[1]->AddImpulse(cVector3f(0, 4, 0));
	bodies[5]->AddImpulseAtPosition(cVector3f(1, 0, 0), bodies[5]->GetWorldPosition() + cVector3f(0, 0.2f, 0));
	bodies[1]->SetMassCentre(cVector3f(0.05f, 0, 0));
	Step(w, bodies, 300);
	bodies[3]->Sleep();
	Step(w, bodies, 60);
	bodies[3]->Enable();
	bodies[1]->SetGravity(false);
	Step(w, bodies, 120);
	for (size_t b = 1; b < bodies.size(); ++b)
	{
		gD.Vec(bodies[b]->GetInertiaVector());
		gD.Matrix(bodies[b]->GetInertiaMatrix());
		gD.Vec(bodies[b]->GetMassCentre());
		gD.Float(bodies[b]->GetLinearDamping());
		gD.Float(bodies[b]->GetAngularDamping());
		gD.Int(bodies[b]->GetAutoDisable() ? 1 : 0);
		gD.Int(bodies[b]->GetContinuousCollision() ? 1 : 0);
	}
	hplDelete(&w);
	gD.End("sleep", 600);
}

static void SceneBuoyancy()
{
	gD.Begin("buoyancy");
	for (int mode = 0; mode < 2; ++mode)
	{
		iPhysicsBody::SetHpl3Buoyancy(mode == 1);
		iPhysicsWorld& w = *NewWorld();
		std::vector<iPhysicsBody*> bodies;
		for (int i = 0; i < 4; ++i)
		{
			iCollideShape* s = i == 0 ? w.CreateBoxShape(cVector3f(1, 1, 1), NULL)
				: i == 1 ? w.CreateSphereShape(cVector3f(0.5f, 0, 0), NULL)
				: i == 2 ? w.CreateCapsuleShape(0.3f, 1.2f, NULL) : w.CreateCylinderShape(0.5f, 0.4f, NULL);
			iPhysicsBody* b = w.CreateBody("Buoy", s);
			b->SetMass(1);
			b->SetPosition(cVector3f(i * 1.5f, -3 + i * 0.5f, 0));
			b->SetBuoyancyActive(true);
			b->SetBuoyancyDensity(1000);
			b->SetBuoyancyLinearViscosity(0.5f);
			b->SetBuoyancyAngularViscosity(0.5f);
			b->SetBuoyancySurface(cPlanef(0, 1, 0, 0));
			b->SetBuoyancyDensityMul(1);
			b->SetMaxLinearSpeed(10);
			bodies.push_back(b);
		}
		Step(w, bodies, 300);
		hplDelete(&w);
	}
	iPhysicsBody::SetHpl3Buoyancy(false);
	gD.End("buoyancy", 300);
}

static void SceneCharacter()
{
	gD.Begin("character");
	iPhysicsWorld& w = *NewWorld();
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	iPhysicsBody* step = w.CreateBody("Step", w.CreateBoxShape(cVector3f(1, 0.2f, 1), NULL));
	step->SetMass(0);
	step->SetPosition(cVector3f(0, 0.1f, 2));
	bodies.push_back(step);
	iPhysicsBody* crate = w.CreateBody("Crate", w.CreateBoxShape(cVector3f(0.6f, 0.6f, 0.6f), NULL));
	crate->SetMass(5);
	crate->SetPosition(cVector3f(1.5f, 0.3f, -1));
	bodies.push_back(crate);
	iCharacterBody* ch = w.CreateCharacterBody("Player", cVector3f(0.5f, 1.8f, 0.5f));
	ch->SetPosition(cVector3f(0, 1.0f, -2));
	for (int i = 0; i < 600; ++i)
	{
		if (i < 300) ch->Move(eCharDir_Forward, 1.0f);
		if (i >= 300 && i < 400) ch->Move(eCharDir_Right, 1.0f);
		w.Update(1.0f / 60.0f);
		gD.Vec(ch->GetPosition());
		gD.Matrix(crate->GetLocalMatrix());
	}
	gD.Line("  charpos", cMath::MatrixTranslate(ch->GetPosition()));
	gD.Line("  crate", crate->GetLocalMatrix());
	hplDelete(&w);
	gD.End("character", 600);
}


static void SceneStress()
{
	gD.Begin("stress");
	iPhysicsWorld& w = *NewWorld(ePhysicsAccuracy_Medium);
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w));
	iPhysicsMaterial* mats[3];
	for (int m = 0; m < 3; ++m)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "Mat%d", m);
		mats[m] = w.CreateMaterial(name);
		mats[m]->SetElasticity(0.1f + 0.3f * m);
		mats[m]->SetStaticFriction(0.4f + 0.2f * m);
		mats[m]->SetKineticFriction(0.3f + 0.2f * m);
		mats[m]->SetFrictionCombMode((ePhysicsMaterialCombMode)m);
	}
	static cCounter counter;
	counter = cCounter();
	iCollideShape* shapes[3] = { w.CreateBoxShape(cVector3f(0.4f, 0.3f, 0.5f), NULL), w.CreateSphereShape(cVector3f(0.22f, 0, 0), NULL),
		w.CreateCapsuleShape(0.15f, 0.7f, NULL) };
	for (int i = 0; i < 120; ++i)
	{
		iPhysicsBody* b = w.CreateBody("S", shapes[i % 3]);
		b->SetMass(0.4f + (i % 5) * 0.3f);
		b->SetMaterial(mats[i % 3]);
		b->SetPosition(cVector3f(((i * 7) % 13) * 0.45f - 2.7f, 0.4f + (i / 13) * 0.55f + (i % 3) * 0.02f, ((i * 11) % 9) * 0.5f - 2.0f));
		b->SetLinearVelocity(cVector3f(0.1f * (i % 4), 0, -0.05f * (i % 7)));
		if (i % 10 == 0) b->AddBodyCallback(&counter);
		bodies.push_back(b);
	}
	Step(w, bodies, 600);
	gD.Int(counter.aabb);
	gD.Int(counter.collide);
	hplDelete(&w);
	gD.End("stress", 600);
}

static void SceneSerialized()
{
	gD.Begin("serialized");
	iPhysicsWorld& src = *NewWorld();
	iCollideShape* mesh = hplNew(iCollideShape, (eCollideShapeType_Mesh, 0, NULL, src.GetNewtonWorld(), &src));
	const int tris = 300;
	std::vector<unsigned int> idx(tris * 3);
	std::vector<float> vtx(tris * 9);
	for (int i = 0; i < tris; ++i)
	{
		float x = (i % 20) * 1.0f - 10, z = (i / 20) * 1.0f - 7, y = 0.1f * ((i * 7) % 5);
		const float v[9] = { x, y, z, x + 1, y, z, x, y, z + 1 };
		for (int j = 0; j < 9; ++j) vtx[i * 9 + j] = v[j];
		for (int j = 0; j < 3; ++j) idx[i * 3 + j] = i * 3 + j;
	}
	mesh->CreateFromVertices(idx.data(), (int)idx.size(), vtx.data(), 3, tris * 3);
	cBinaryBuffer buf;
	src.SaveMeshShapeToBuffer(mesh, &buf);
	gD.Int((int)buf.GetSize());
	gD.Mix(buf.GetDataPointer(), buf.GetSize());
	buf.SetPos(0);
	iPhysicsWorld& w = *NewWorld();
	iPhysicsBody* ground = w.CreateBody("Ground", w.LoadMeshShapeFromBuffer(&buf));
	ground->SetMass(0);
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(ground);
	for (int i = 0; i < 12; ++i)
	{
		iPhysicsBody* b = w.CreateBody("D", w.CreateSphereShape(cVector3f(0.2f, 0, 0), NULL));
		b->SetMass(1);
		b->SetPosition(cVector3f(-3 + i * 0.5f, 1.5f + 0.1f * i, 0.1f * i));
		bodies.push_back(b);
	}
	Step(w, bodies, 600);
	hplDelete(&w);
	hplDelete(mesh);
	hplDelete(&src);
	gD.End("serialized", 600);
}

static void SceneJointsAdvanced()
{
	gD.Begin("joints2");
	iPhysicsWorld& w = *NewWorld(ePhysicsAccuracy_Low);
	std::vector<iPhysicsBody*> bodies;
	bodies.push_back(Floor(w, -6));
	iPhysicsBody* anchor = w.CreateBody("Anchor", w.CreateBoxShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
	anchor->SetMass(0);
	anchor->SetPosition(cVector3f(0, 4, 0));
	bodies.push_back(anchor);
	// ball joint with no parent, pendulum
	iPhysicsBody* bob = w.CreateBody("Bob", w.CreateSphereShape(cVector3f(0.3f, 0, 0), NULL));
	bob->SetMass(2);
	bob->SetPosition(cVector3f(2, 4, 0));
	iPhysicsJointBall* ball = w.CreateJointBall("pend", cVector3f(0, 4, 0), cVector3f(1, 0, 0), NULL, bob);
	ball->SetCollideBodies(false);
	ball->SetStiffness(0.5f);
	bodies.push_back(bob);
	// hinge, sticky limits + auto sleep, driven by a ramping angular velocity
	iPhysicsBody* base = w.CreateBody("Base", w.CreateBoxShape(cVector3f(0.2f, 1.5f, 0.2f), NULL));
	base->SetMass(0);
	base->SetPosition(cVector3f(6, 2, 0));
	bodies.push_back(base);
	iPhysicsBody* lid = w.CreateBody("Lid", w.CreateBoxShape(cVector3f(1.2f, 0.1f, 0.8f), NULL));
	lid->SetMass(1.5f);
	lid->SetPosition(cVector3f(6.7f, 3.5f, 0));
	lid->SetAngularVelocity(cVector3f(0, 0, 3));
	iPhysicsJointHinge* h = w.CreateJointHinge("lid", cVector3f(6.1f, 3.5f, 0), cVector3f(0, 0, 1), base, lid);
	h->SetMaxAngle(1.0f);
	h->SetMinAngle(-0.2f);
	h->SetStickyMaxLimit(true);
	h->SetStickyMinLimit(true);
	h->SetLimitAutoSleep(true);
	h->SetLimitAutoSleepDist(0.05f);
	h->SetAllowPositionReset(true);
	bodies.push_back(lid);
	// slider and screw with a dynamic parent
	iPhysicsBody* p = w.CreateBody("P", w.CreateBoxShape(cVector3f(0.5f, 0.5f, 0.5f), NULL));
	p->SetMass(3);
	p->SetPosition(cVector3f(-6, 3, 0));
	bodies.push_back(p);
	iPhysicsBody* c = w.CreateBody("C", w.CreateBoxShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
	c->SetMass(1);
	c->SetPosition(cVector3f(-6, 3.6f, 0));
	c->SetLinearVelocity(cVector3f(0, 2, 0));
	iPhysicsJointSlider* sl = w.CreateJointSlider("sl", cVector3f(-6, 3.3f, 0), cVector3f(0, 1, 0), p, c);
	sl->SetMaxDistance(0.5f);
	sl->SetMinDistance(-0.5f);
	sl->SetStickyMaxLimit(true);
	bodies.push_back(c);
	iPhysicsBody* n = w.CreateBody("N", w.CreateBoxShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
	n->SetMass(1);
	n->SetPosition(cVector3f(-8, 3, 0));
	n->SetLinearVelocity(cVector3f(1, 0, 0));
	iPhysicsJointScrew* sc = w.CreateJointScrew("sc", cVector3f(-8, 3, 0), cVector3f(1, 0, 0), p, n);
	sc->SetMaxDistance(0.4f);
	sc->SetMinDistance(-0.4f);
	bodies.push_back(n);
	Step(w, bodies, 600);
	gD.Float(h->GetAngle());
	gD.Float(h->GetMaxAngle());
	gD.Float(h->GetMinAngle());
	gD.Float(sl->GetMaxDistance());
	gD.Float(sl->GetMinDistance());
	gD.Float(sc->GetMaxDistance());
	gD.Float(sc->GetMinDistance());
	gD.Int(ball->GetCollideBodies() ? 1 : 0);
	gD.Float(ball->GetStiffness());
	gD.Vec(ball->GetAngles());
	gD.Vec(ball->GetAngularVelocity());
	gD.Float(ball->GetForceSize());
	hplDelete(&w);
	gD.End("joints2", 600);
}


// Hinge row geometry exactly as iPhysicsJointHinge::SubmitConstraints computes it; inputs and outputs as raw bits.
static void DumpHingeMath()
{
	if (!gD.f) return;
	uint32_t seed = 12345;
	auto Rnd = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
	};
	for (int c = 0; c < 48; ++c)
	{
		cMatrixf in[4];
		for (int k = 0; k < 4; ++k)
		{
			if (c < 8)
				in[k] = cMath::MatrixRotateY(0.3f * c + k) ;
			else
				for (int i = 0; i < 16; ++i) (&in[k].m[0][0])[i] = Rnd();
			if (c < 8) in[k].SetTranslation(cVector3f(Rnd(), Rnd(), Rnd()));
		}
		cMatrixf m0 = cMath::MatrixMul(in[0], in[2]);
		cMatrixf m1 = cMath::MatrixMul(in[1], in[3]);
		cVector3f p0 = m0.GetTranslation(), p1 = m1.GetTranslation();
		cMatrixf i0 = m0.GetTranspose(), i1 = m1.GetTranspose();
		cVector3f q0(p0 + i0.GetUp() * 50.0f);
		cVector3f q1(p1 + i1.GetUp() * 50.0f);
		float fSin = cMath::Vector3Dot(cMath::Vector3Cross(i0.GetRight(), i1.GetRight()), i0.GetUp());
		float fCos = cMath::Vector3Dot(i0.GetRight(), i1.GetRight());
		float fAngle = atan2(fSin, fCos);
		float fDist = cMath::Vector3DistSqr(p0, p1);
		std::fprintf(gD.f, "hinge");
		for (int k = 0; k < 4; ++k)
			for (int i = 0; i < 16; ++i) std::fprintf(gD.f, " %08x", ((const unsigned*)&in[k].m[0][0])[i]);
		std::fprintf(gD.f, " :");
		const cVector3f outs[7] = { p0, p1, i0.GetRight(), i0.GetUp(), i0.GetForward(), q0, q1 };
		for (int i = 0; i < 7; ++i) std::fprintf(gD.f, " %08x %08x %08x", *(const unsigned*)&outs[i].x, *(const unsigned*)&outs[i].y, *(const unsigned*)&outs[i].z);
		std::fprintf(gD.f, " %08x %08x\n", *(const unsigned*)&fAngle, *(const unsigned*)&fDist);
	}
}

static void RunAll(const char* dumpFile, bool verbose)
{
	gD.f = dumpFile ? std::fopen(dumpFile, "w") : NULL;
	gD.verbose = verbose;
	SceneStack();
	ScenePile();
	SceneJoints();
	SceneContacts();
	SceneQueries();
	SceneSleepForces();
	SceneBuoyancy();
	SceneCharacter();
	SceneStress();
	DumpHingeMath();
	SceneSerialized();
	SceneJointsAdvanced();
	if (gD.f) std::fclose(gD.f);
}

} // namespace hplscenes

#endif
