#include <cstdio>
#include <cstdlib>
#include <vector>

#include "impl/PhysicsWorldNewton.h"
#include "math/Math.h"
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
	CHECK(world.CreateSphereShape(cVector3f(0.5f, 0, 0), NULL)->GetBoundingVolume().GetSize() == cVector3f(1));

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

static void TestHeightFieldSupportsBody()
{
	cPhysicsWorldNewton world;
	world.SetGravity(cVector3f(0, -9.81f, 0));
	world.SetMaxTimeStep(1.0f / 60.0f);

	std::vector<unsigned short> vElev(9 * 9, 32768);
	iPhysicsBody* pGround = world.CreateBody("Terrain", world.CreateHeightFieldShape(9, vElev.data(), 1.0f, 2.0f / 65535.0f));
	pGround->SetMass(0);
	pGround->SetMatrix(cMath::MatrixTranslate(cVector3f(-4, 0, -4)));

	iPhysicsBody* pBox = world.CreateBody("Box", world.CreateBoxShape(cVector3f(1, 1, 1), NULL));
	pBox->SetMass(1.0f);
	pBox->SetPosition(cVector3f(0, 3, 0));
	for (int i = 0; i < 180; ++i) world.Simulate(1.0f / 60.0f);

	CHECK(cMath::Abs(pBox->GetLocalPosition().y - 1.5f) < 0.1f);
}

//-----------------------------------------------------------------------

static void TestThinBoxRestsOnMesh()
{
	cPhysicsWorldNewton world;
	world.SetGravity(cVector3f(0, -9.81f, 0));
	world.SetMaxTimeStep(1.0f / 60.0f);
	world.SetAccuracyLevel(ePhysicsAccuracy_Medium);

	cCollideShapeNewton* pFloorShape = hplNew(cCollideShapeNewton,
		(eCollideShapeType_Mesh, 0, NULL, world.GetNewtonWorld(), &world));
	const unsigned int vIndices[6] = { 0, 1, 2, 0, 2, 3 };
	const float vVertices[4 * 3] = {
		-5.0f, 0.0f, -5.0f,
		 5.0f, 0.0f, -5.0f,
		 5.0f, 0.0f,  5.0f,
		-5.0f, 0.0f,  5.0f,
	};
	pFloorShape->CreateFromVertices(vIndices, 6, vVertices, 3, 4);
	world.CreateBody("Floor", pFloorShape)->SetMass(0);

	// SOMA brain_chunk_07_rigid gib
	cMatrixf mtxOffset = cMath::MatrixRotateY(0.346119f);
	iPhysicsBody* pChunk = world.CreateBody("Chunk", world.CreateBoxShape(cVector3f(0.0395f, 0.0135f, 0.0251f), &mtxOffset));
	pChunk->SetMass(0.25f);
	pChunk->SetContinuousCollision(true);
	pChunk->SetPosition(cVector3f(0.3f, 0.2f, 0.1f));
	for (int i = 0; i < 600; ++i) world.Simulate(1.0f / 60.0f);

	CHECK(cMath::Abs(pChunk->GetLocalPosition().y - 0.00675f) < 0.003f);
}

//-----------------------------------------------------------------------

class cHitCounter : public iPhysicsRayCallback
{
public:
	int mlHits = 0;
	bool OnIntersect(iPhysicsBody*, cPhysicsRayParams*) { ++mlHits; return true; }
};

// Amnesia 20_sewer focus ray: start on a cell boundary, near-axis direction
static void TestNearAxisRayCast()
{
	cPhysicsWorldNewton world;
	world.SetWorldSize(cVector3f(-128), cVector3f(128));
	iPhysicsBody* pBox = world.CreateBody("Box", world.CreateBoxShape(cVector3f(0.49998f, 0.5f, 0.5f), NULL));
	pBox->SetMass(0);
	pBox->SetPosition(cVector3f(-0.25f, 9.745821f, 10.0f));

	cHitCounter hits;
	world.CastRay(&hits, cVector3f(0, 9.745821f, 25.75f), cVector3f(-0.000044f, 9.745821f, 5.75f), true, false, false, false);
	CHECK(hits.mlHits == 1);
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
	TestHeightFieldSupportsBody();
	TestThinBoxRestsOnMesh();
	TestNearAxisRayCast();

	if (gFailures > 0)
	{
		std::fprintf(stderr, "\n%d check(s) FAILED\n", gFailures);
		return 1;
	}

	std::printf("All physics/Newton port checks passed.\n");
	return 0;
}
