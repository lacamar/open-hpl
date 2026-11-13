#ifndef HPL_PHYSICS_H14_SCENES_H
#define HPL_PHYSICS_H14_SCENES_H

// Differential scenes for CharacterBody, PhysicsRope, VerletParticle and PhysicsController:
// run once per OHPL_IMPL and compare the dumps bit for bit. OHPL_H14_DUMP=<file>.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "impl/PhysicsWorldNewton.h"
#include "impl/CollideShapeNewton.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsController.h"
#include "physics/PhysicsRope.h"
#include "physics/VerletParticle.h"
#include "physics/CharacterBody.h"
#include "scene/Camera.h"
#include "scene/Entity3D.h"
#include "math/Math.h"

namespace h14scenes {

using namespace hpl;

struct Dump
{
	FILE* f = NULL;
	uint64_t hash = 0xcbf29ce484222325ULL;
	int steps = 0;

	void Mix(const void* p, size_t n)
	{
		for (size_t i = 0; i < n; ++i) hash = (hash ^ ((const unsigned char*)p)[i]) * 0x100000001b3ULL;
	}
	void Float(float v) { Mix(&v, sizeof(v)); }
	void Int(int v) { Mix(&v, sizeof(v)); }
	void Vec(const cVector3f& v) { Float(v.x); Float(v.y); Float(v.z); }
	void Matrix(const cMatrixf& m) { Mix(&m.m[0][0], sizeof(float) * 16); }
	void Line(const char* tag, const cVector3f& v)
	{
		if (f) std::fprintf(f, "%s %08x %08x %08x\n", tag, *(const unsigned*)&v.x, *(const unsigned*)&v.y, *(const unsigned*)&v.z);
	}
	void Begin(const char* scene) { hash = 0xcbf29ce484222325ULL; steps = 0; if (f) std::fprintf(f, "scene %s\n", scene); }
	void End(const char* scene)
	{
		if (f) std::fprintf(f, "end %s steps %d hash %016llx\n", scene, steps, (unsigned long long)hash);
	}
};

static Dump gD;

class cTestEntity : public iEntity3D
{
public:
	cTestEntity() : iEntity3D("h14") {}
	tString GetEntityType() { return "H14"; }
};

static cPhysicsWorldNewton* NewWorld()
{
	cPhysicsWorldNewton* w = hplNew(cPhysicsWorldNewton, ());
	w->SetGravity(cVector3f(0, -9.81f, 0));
	w->SetMaxTimeStep(1.0f / 60.0f);
	w->SetAccuracyLevel(ePhysicsAccuracy_High);
	w->SetWorldSize(cVector3f(-100), cVector3f(100));
	return w;
}

static iPhysicsBody* Floor(cPhysicsWorldNewton& w)
{
	cCollideShapeNewton* s = hplNew(cCollideShapeNewton, (eCollideShapeType_Mesh, 0, NULL, w.GetNewtonWorld(), &w));
	const unsigned int idx[6] = { 0, 1, 2, 0, 2, 3 };
	const float vtx[12] = { -30, 0, -30, 30, 0, -30, 30, 0, 30, -30, 0, 30 };
	s->CreateFromVertices(idx, 6, vtx, 3, 4);
	iPhysicsBody* b = w.CreateBody("Floor", s);
	b->SetMass(0);
	return b;
}

static iPhysicsBody* Box(cPhysicsWorldNewton& w, const char* name, const cVector3f& size, float mass, const cVector3f& pos)
{
	iPhysicsBody* b = w.CreateBody(name, w.CreateBoxShape(size, NULL));
	b->SetMass(mass);
	b->SetPosition(pos);
	return b;
}

static void Body(iPhysicsBody* b)
{
	gD.Matrix(b->GetLocalMatrix());
	gD.Vec(b->GetLinearVelocity());
	gD.Vec(b->GetAngularVelocity());
}

static void Char(iCharacterBody* c)
{
	gD.Vec(c->GetPosition());
	gD.Vec(c->GetLastPosition());
	gD.Vec(c->GetForceVelocity());
	gD.Vec(c->GetLastGroundNormal());
	gD.Vec(c->GetForward());
	gD.Float(c->GetYaw());
	gD.Int(c->IsOnGround());
	gD.Int(c->IsClimbing());
	gD.Int(c->GetMovedLastUpdate());
}

static void Rope(iPhysicsRope* r)
{
	int n = r->GetParticleNum();
	for (int i = 0; i < n; ++i)
	{
		cVerletParticle* p = r->GetParticle(i);
		gD.Vec(p->GetPosition());
		gD.Vec(p->GetPrevPosition());
		gD.Vec(p->GetSmoothPosition());
	}
}

static void ScenePlayer(bool useCamera, bool useEntity, const char* name)
{
	gD.Begin(name);
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	Box(w, "Step", cVector3f(1, 0.2f, 1), 0, cVector3f(0, 0.1f, 2));
	iPhysicsBody* crate = Box(w, "Crate", cVector3f(0.6f, 0.6f, 0.6f), 5, cVector3f(1.5f, 0.3f, -1));
	iPhysicsBody* light = Box(w, "Light", cVector3f(0.4f, 0.4f, 0.4f), 1, cVector3f(-1.0f, 0.2f, -1.5f));
	iCharacterBody* ch = w.CreateCharacterBody("Player", cVector3f(0.5f, 1.8f, 0.5f));
	ch->SetPosition(cVector3f(0, 1.0f, -2));
	ch->SetPushForce(100);
	ch->SetMaxPushMass(50);
	ch->SetupSafePositions(0.1f, 2.0f);

	cCamera cam;
	cTestEntity ent;
	if (useCamera)
	{
		ch->SetCamera(&cam);
		ch->SetCameraPosAdd(cVector3f(0.1f, 0.05f, 0.02f));
		ch->SetCameraSmoothPosNum(5);
		ch->SetCameraUseSmoothing(true);
	}
	if (useEntity)
	{
		ch->SetEntity(&ent);
		ch->SetEntitySmoothPosNum(4);
		ch->SetUseEntitySmoothYPos(true);
		ch->SetEntitySmoothYPosNum(6);
		ch->SetEntityOffset(cMath::MatrixTranslate(cVector3f(0, 0.5f, 0)));
		ch->SetEntityPostOffset(cMath::MatrixTranslate(cVector3f(0.01f, 0.02f, 0)));
	}
	for (int i = 0; i < 600; ++i)
	{
		if (i < 150) ch->Move(eCharDir_Forward, 1.0f);
		if (i >= 150 && i < 250) ch->Move(eCharDir_Right, 1.0f);
		if (i >= 250 && i < 400) { ch->AddYaw(0.01f); ch->Move(eCharDir_Forward, 0.7f); }
		if (i == 420) ch->SetForceVelocity(cVector3f(0, 4, 0));
		if (i >= 450 && i < 520) ch->Move(eCharDir_Forward, -1.0f);
		if (i == 560) ch->TeleportToSafePosition(0.5f, true);
		w.Update(1.0f / 60.0f);
		Char(ch);
		Body(crate);
		Body(light);
		if (useCamera) { gD.Vec(cam.GetPosition()); gD.Float(cam.GetYaw()); }
		if (useEntity) gD.Matrix(ent.GetLocalMatrix());
		++gD.steps;
	}
	gD.Line("  charpos", ch->GetPosition());
	gD.Line("  crate", crate->GetLocalPosition());
	if (useEntity) gD.Line("  entpos", ent.GetLocalPosition());
	hplDelete(&w);
	gD.End(name);
}

static void ScenePlatform(const char* name, bool rotate, bool velocity, int axes, bool yaw)
{
	gD.Begin(name);
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* plat = Box(w, "Platform", cVector3f(3, 0.3f, 3), 20, cVector3f(0, 0.15f, 0));
	plat->SetGravity(false);
	plat->SetGravityCanAttachCharacter(true);
	plat->SetGravityAttachmentRotation(rotate);
	plat->SetGravityAttachmentVelocity(velocity);
	plat->SetGravityAttachmentVelocityAxes((eVelocityAxes)axes);
	plat->SetPushedByCharacterGravity(true);
	iCharacterBody* ch = w.CreateCharacterBody("Player", cVector3f(0.5f, 1.8f, 0.5f));
	ch->SetPosition(cVector3f(0.5f, 1.4f, 0.3f));
	ch->SetRotateYawWhenGravityAttached(yaw);
	for (int i = 0; i < 600; ++i)
	{
		float t = i * (1.0f / 60.0f);
		plat->SetLinearVelocity(cVector3f(cos(t * 0.8f) * 1.2f, i > 300 ? 0.4f : 0.0f, sin(t) * 0.5f));
		if (rotate) plat->SetAngularVelocity(cVector3f(0, 0.5f, 0));
		if (i < 100) ch->Move(eCharDir_Forward, 0.3f);
		if (i == 400) ch->SetForceVelocity(cVector3f(0, 3, 0));
		w.Update(1.0f / 60.0f);
		Char(ch);
		Body(plat);
		++gD.steps;
	}
	gD.Line("  charpos", ch->GetPosition());
	gD.Line("  platpos", plat->GetLocalPosition());
	hplDelete(&w);
	gD.End(name);
}

static void SceneConnect(const char* name, unsigned flags, bool charRot)
{
	gD.Begin(name);
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* crate = Box(w, "Crate", cVector3f(0.8f, 0.8f, 0.8f), 8, cVector3f(0, 0.4f, -1.2f));
	iCharacterBody* ch = w.CreateCharacterBody("Player", cVector3f(0.5f, 1.8f, 0.5f));
	ch->SetPosition(cVector3f(0, 1.0f, 0));
	for (int i = 0; i < 30; ++i) w.Update(1.0f / 60.0f);
	ch->ConnectToBody(crate, cVector3f(0, 0.5f, -0.8f), flags);
	for (int i = 0; i < 480; ++i)
	{
		if (i < 200) ch->Move(eCharDir_Forward, -1.0f);
		if (charRot && i >= 100 && i < 300) ch->AddYaw(0.015f);
		if (i >= 200 && i < 330) ch->Move(eCharDir_Right, 0.5f);
		if (i == 400) ch->DisconnectBody();
		w.Update(1.0f / 60.0f);
		Char(ch);
		Body(crate);
		gD.Vec(ch->GetBodyConnectionPos());
		gD.Vec(ch->GetCharConnectionPos());
		++gD.steps;
	}
	gD.Line("  charpos", ch->GetPosition());
	gD.Line("  crate", crate->GetLocalPosition());
	hplDelete(&w);
	gD.End(name);
}

static void SceneRope(const char* name, bool collide, bool attachEnd, float stiffness)
{
	gD.Begin(name);
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* anchor = Box(w, "Anchor", cVector3f(0.4f, 0.4f, 0.4f), 0, cVector3f(0, 4, 0));
	iPhysicsBody* weight = Box(w, "Weight", cVector3f(0.4f, 0.4f, 0.4f), 3, cVector3f(2.5f, 3.9f, 0));
	Box(w, "Block", cVector3f(1, 1, 1), 0, cVector3f(1.2f, 1.0f, 0));
	iPhysicsRope* rope = w.CreateRope("Rope", cVector3f(0, 4, 0), cVector3f(2.5f, 3.9f, 0));
	rope->SetSegmentLength(0.25f);
	rope->SetStiffness(stiffness);
	rope->SetCollide(collide);
	rope->SetParticleRadius(0.05f);
	rope->SetAttachedStartBody(anchor);
	if (attachEnd) rope->SetAttachedEndBody(weight);
	rope->SetMaxIterations(4);
	for (int i = 0; i < 600; ++i)
	{
		if (i == 300) weight->AddImpulse(cVector3f(0, 20, 5));
		w.Update(1.0f / 60.0f);
		Rope(rope);
		Body(weight);
		++gD.steps;
	}
	hplDelete(&w);
	gD.End(name);
}

static void SceneController()
{
	gD.Begin("controller");
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* body = Box(w, "Body", cVector3f(0.5f, 0.5f, 0.5f), 2, cVector3f(0, 2, 0));
	body->SetGravity(false);
	iPhysicsController* c = w.CreateController("Ctrl");
	c->SetBody(body);
	c->SetType(ePhysicsControllerType_Pid);
	c->SetA(30);
	c->SetB(0.5f);
	c->SetC(8);
	c->SetPidIntegralSize(8);
	c->SetDestValue(2);
	c->SetMaxOutput(40);
	c->SetInputType(ePhysicsControllerInput_LinearSpeed, ePhysicsControllerAxis_Y);
	c->SetOutputType(ePhysicsControllerOutput_Force, ePhysicsControllerAxis_Y);
	c->SetMulMassWithOutput(true);
	c->SetActive(true);
	for (int i = 0; i < 600; ++i)
	{
		if (i == 300) c->SetDestValue(-1);
		w.Update(1.0f / 60.0f);
		Body(body);
		++gD.steps;
	}
	gD.Line("  body", body->GetLocalPosition());
	hplDelete(&w);
	gD.End("controller");
}

static void ScenePush(const char* name, bool in2d, bool impulse, bool customGravity)
{
	gD.Begin(name);
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* crate = Box(w, "Crate", cVector3f(0.6f, 0.6f, 0.6f), 4, cVector3f(1.0f, 0.3f, -2.2f));
	iPhysicsBody* ball = w.CreateBody("Ball", w.CreateSphereShape(cVector3f(0.3f, 0.3f, 0.3f), NULL));
	ball->SetMass(1);
	ball->SetPosition(cVector3f(-1.0f, 0.3f, -2.0f));
	iPhysicsBody* top = Box(w, "Top", cVector3f(0.5f, 0.2f, 0.5f), 2, cVector3f(0.4f, 3.0f, 0.4f));
	iCharacterBody* a = w.CreateCharacterBody("A", cVector3f(0.5f, 1.8f, 0.5f));
	iCharacterBody* b = w.CreateCharacterBody("B", cVector3f(0.5f, 1.6f, 0.5f));
	a->SetPosition(cVector3f(0, 1.0f, 0));
	b->SetPosition(cVector3f(0.1f, 0.9f, -1.6f));
	a->SetPushForce(80);
	a->SetMaxPushMass(50);
	a->SetPushIn2D(in2d);
	if (impulse) a->SetPushImpulse(6.0f);
	a->SetCharacterMaxPushMass(50);
	a->SetCharacterPushForce(40);
	a->SetCharacterPushIn2D(in2d);
	b->SetMass(2);
	if (customGravity) { a->SetCustomGravityActive(true); a->SetCustomGravity(cVector3f(0, -4.0f, 0)); }
	for (int i = 0; i < 600; ++i)
	{
		if (i < 300) a->Move(eCharDir_Forward, 1.0f);
		if (i >= 300 && i < 420) { a->Move(eCharDir_Forward, -1.0f); b->Move(eCharDir_Right, 0.5f); }
		if (i == 200) a->SetPosition(a->GetPosition() + cVector3f(0.7f, 0.2f, 0), true);
		w.Update(1.0f / 60.0f);
		Char(a);
		Char(b);
		Body(crate);
		Body(ball);
		Body(top);
		++gD.steps;
	}
	gD.Line("  a", a->GetPosition());
	gD.Line("  b", b->GetPosition());
	gD.Line("  crate", crate->GetLocalPosition());
	hplDelete(&w);
	gD.End(name);
}

static void SceneRopeMotor()
{
	gD.Begin("rope_motor");
	cPhysicsWorldNewton& w = *NewWorld();
	Floor(w);
	iPhysicsBody* anchor = Box(w, "Anchor", cVector3f(0.4f, 0.4f, 0.4f), 0, cVector3f(0, 4, 0));
	iPhysicsBody* weight = Box(w, "Weight", cVector3f(0.4f, 0.4f, 0.4f), 3, cVector3f(2.0f, 3.9f, 0));
	iPhysicsBody* other = Box(w, "Other", cVector3f(0.4f, 0.4f, 0.4f), 2, cVector3f(-2.0f, 3.5f, 0));
	iPhysicsRope* rope = w.CreateRope("Rope", cVector3f(0, 4, 0), cVector3f(2.0f, 3.9f, 0));
	rope->SetSegmentLength(0.3f);
	rope->SetMaxIterations(4);
	rope->SetAttachedStartBody(anchor);
	rope->SetAttachedEndBody(weight);
	rope->SetMotorMinSpeed(-2.0f);
	rope->SetMotorMaxSpeed(2.0f);
	for (int i = 0; i < 600; ++i)
	{
		if (i == 50) rope->SetMotorActive(true);
		if (i == 330) rope->SetMotorActive(false);
		if (i == 340) { rope->SetAutoMoveActive(true); rope->SetAutoMoveAcc(0.0f); rope->SetAutoMoveSpeed(0.0f); }
		if (i == 420) rope->SetAutoMoveActive(false);
		if (i == 450) rope->SetAttachedEndBody(other);
		if (i == 520) rope->RemoveAttachedBody(weight, true);
		w.Update(1.0f / 60.0f);
		Rope(rope);
		Body(weight);
		Body(other);
		++gD.steps;
	}
	hplDelete(&w);
	gD.End("rope_motor");
}

static void RunAll(const char* dumpFile)
{
	gD.f = dumpFile ? std::fopen(dumpFile, "w") : NULL;
	ScenePlayer(false, false, "player_plain");
	ScenePlayer(true, true, "player_camera_entity");
	ScenePlatform("platform_pos", false, false, 0, false);
	ScenePlatform("platform_rot", true, true, 2, true);
	ScenePlatform("platform_vel_xz", false, true, 0, false);
	ScenePlatform("platform_vel_y", true, true, 1, false);
	SceneConnect("connect_body_rot", ePhysicsConnectionFlag_DependOnBodyRotation | ePhysicsConnectionFlag_AlignBodyCenter, false);
	SceneConnect("connect_all", 0xff & ~ePhysicsConnectionFlag_AlignCharRotation, true);
	SceneConnect("connect_char_align", ePhysicsConnectionFlag_AlignCharRotation | ePhysicsConnectionFlag_AffectChar |
		ePhysicsConnectionFlag_AlignBodyRotation | ePhysicsConnectionFlag_DependOnBodyRotation, true);
	SceneConnect("connect_plain", ePhysicsConnectionFlag_AlignBodyRotation | ePhysicsConnectionFlag_AffectChar |
		ePhysicsConnectionFlag_DisableBodyGravity, true);
	ScenePush("push_2d", true, false, false);
	ScenePush("push_3d_impulse", false, true, false);
	ScenePush("push_custom_gravity", true, true, true);
	SceneRope("rope_collide", true, true, 1.0f);
	SceneRope("rope_free", false, false, 0.6f);
	SceneRopeMotor();
	SceneController();
	if (gD.f) std::fclose(gD.f);
}

} // namespace h14scenes

#endif
