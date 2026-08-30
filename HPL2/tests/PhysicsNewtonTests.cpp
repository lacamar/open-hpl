/*
 * Focused regression tests for the Newton Dynamics 2.x -> 3.14 port.
 *
 * These are plain, dependency-free checks (no GL/SDL/game-data needed) run
 * via CTest with a timeout, specifically so a hang like the one found in
 * cWorldLoaderHplMap::LoadCacheFile (deserializing a Newton collision blob
 * baked by the original Newton 2.x with our Newton 3.14 deserializer, which
 * has a different internal binary format) fails fast as a test timeout
 * instead of silently hanging deep inside a full game boot.
 */

#include <cstdio>
#include <cstdlib>

#include "impl/PhysicsWorldNewton.h"
#include "impl/CollideShapeNewton.h"
#include "physics/PhysicsBody.h"
#include "resources/BinaryBuffer.h"
#include "system/MemoryManager.h"

using namespace hpl;

static int gFailures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
			++gFailures; \
		} \
	} while (0)

//-----------------------------------------------------------------------

static void TestShapeCreation()
{
	cPhysicsWorldNewton world;

	iCollideShape* pNull = world.CreateNullShape();
	CHECK(pNull != NULL);

	iCollideShape* pBox = world.CreateBoxShape(cVector3f(1, 1, 1), NULL);
	CHECK(pBox != NULL);
	CHECK(pBox->GetVolume() > 0.0f);

	iCollideShape* pSphere = world.CreateSphereShape(cVector3f(1, 1, 1), NULL);
	CHECK(pSphere != NULL);
	CHECK(pSphere->GetVolume() > 0.0f);

	iCollideShape* pCylinder = world.CreateCylinderShape(0.5f, 2.0f, NULL);
	CHECK(pCylinder != NULL);
	CHECK(pCylinder->GetVolume() > 0.0f);

	iCollideShape* pCapsule = world.CreateCapsuleShape(0.5f, 2.0f, NULL);
	CHECK(pCapsule != NULL);
	CHECK(pCapsule->GetVolume() > 0.0f);

	tCollideShapeVec vShapes;
	vShapes.push_back(pBox);
	vShapes.push_back(pSphere);
	iCollideShape* pCompound = world.CreateCompundShape(vShapes);
	CHECK(pCompound != NULL);
}

//-----------------------------------------------------------------------

static void TestBodySimulationStep()
{
	cPhysicsWorldNewton world;
	world.SetGravity(cVector3f(0, -9.81f, 0));
	world.SetMaxTimeStep(1.0f / 60.0f);

	iCollideShape* pShape = world.CreateBoxShape(cVector3f(1, 1, 1), NULL);
	iPhysicsBody* pBody = world.CreateBody("TestBody", pShape);
	CHECK(pBody != NULL);

	pBody->SetMass(1.0f);
	cVector3f vStartPos = pBody->GetLocalPosition();

	// A handful of steps is enough to confirm the world/body/material
	// bindings (NewtonUpdate, the force/torque callback, mass matrix) all
	// still work end to end after the Newton 3.14 API port.
	for (int i = 0; i < 10; ++i)
	{
		world.Simulate(1.0f / 60.0f);
	}

	cVector3f vEndPos = pBody->GetLocalPosition();
	CHECK(vEndPos.y < vStartPos.y); // should have fallen under gravity
}

//-----------------------------------------------------------------------

static void TestBuoyancyDoesNotCrash()
{
	// Buoyancy has no Newton 3.14 equivalent to NewtonBodyAddBuoyancyForce -
	// PhysicsBodyNewton.cpp reimplements it on top of
	// NewtonConvexCollisionCalculateBuoyancyVolume. This just exercises that
	// path for a body sitting in the fluid plane and confirms it doesn't
	// crash; it is not a check of the physical accuracy of the result.
	cPhysicsWorldNewton world;
	world.SetGravity(cVector3f(0, -9.81f, 0));
	world.SetMaxTimeStep(1.0f / 60.0f);

	iCollideShape* pShape = world.CreateBoxShape(cVector3f(1, 1, 1), NULL);
	iPhysicsBody* pBody = world.CreateBody("BuoyantBody", pShape);
	pBody->SetMass(1.0f);

	pBody->SetBuoyancyActive(true);
	pBody->SetBuoyancyDensity(1000.0f);
	pBody->SetBuoyancyLinearViscosity(0.5f);
	pBody->SetBuoyancyAngularViscosity(0.5f);
	pBody->SetBuoyancySurface(cPlanef(0, 1, 0, 0)); // horizontal surface through the origin
	pBody->SetBuoyancyDensityMul(1.0f);

	for (int i = 0; i < 10; ++i)
	{
		world.Simulate(1.0f / 60.0f);
	}

	CHECK(true); // reaching here without crashing/hanging is the actual check
}

//-----------------------------------------------------------------------

static void TestMeshCollisionSerializationRoundTrip()
{
	// This is the direct regression test for the incident that motivated
	// this file: loading a real game's main-menu background world hung
	// inside cCollideShapeNewton::CreateFromSerializedData while
	// deserializing a Newton collision blob. That specific hang was caused
	// by reading a blob baked by the *original* Newton 2.x, which this test
	// cannot reproduce (that data no longer exists in a form we can
	// generate). What this test guards instead: that our own Newton 3.14
	// serialize -> deserialize round trip is internally self-consistent, so
	// a *future* regression in that path fails fast as a CTest timeout
	// rather than resurfacing as an unexplained hang deep in a real game
	// boot.
	cPhysicsWorldNewton world;

	cCollideShapeNewton* pMeshShape = hplNew(cCollideShapeNewton,
		(eCollideShapeType_Mesh, 0, NULL, world.GetNewtonWorld(), &world));

	// A simple two-triangle quad, wound consistently.
	const unsigned int vIndices[6] = { 0, 1, 2, 0, 2, 3 };
	const float vVertices[4 * 3] = {
		-1.0f, 0.0f, -1.0f,
		 1.0f, 0.0f, -1.0f,
		 1.0f, 0.0f,  1.0f,
		-1.0f, 0.0f,  1.0f,
	};
	pMeshShape->CreateFromVertices(vIndices, 6, vVertices, 3, 4);

	cBinaryBuffer serialized;
	world.SaveMeshShapeToBuffer(pMeshShape, &serialized);
	CHECK(serialized.GetSize() > 0);

	// Rewind so the buffer can be read back from the start, then deserialize
	// into a *different* physics world, exactly as the map loader does when
	// reading a cache file back on a later run.
	serialized.SetPos(0);

	cPhysicsWorldNewton otherWorld;
	iCollideShape* pRoundTripped = otherWorld.LoadMeshShapeFromBuffer(&serialized);
	CHECK(pRoundTripped != NULL);

	hplDelete(pMeshShape);
}

//-----------------------------------------------------------------------

// HPL2's own LowLevelSystemSDL.cpp provides main() (it wraps SDL's platform
// entry point) and expects the caller to define this instead - same
// contract the Amnesia/Launcher executables use.
int hplMain(const tString&)
{
	TestShapeCreation();
	TestBodySimulationStep();
	TestBuoyancyDoesNotCrash();
	TestMeshCollisionSerializationRoundTrip();

	if (gFailures > 0)
	{
		std::fprintf(stderr, "\n%d check(s) FAILED\n", gFailures);
		return 1;
	}

	std::printf("All physics/Newton port checks passed.\n");
	return 0;
}
