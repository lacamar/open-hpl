
#include "resources/WorldLoaderHpm.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "resources/WorldLoaderHpmBuilder.h"
#include "resources/RustHpm.h"

#include "scene/Scene.h"
#include "scene/World.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"

namespace hpl {

	cXmlElement* cWorldLoaderHpm::mpCurrentElement = NULL;
	tString cWorldLoaderHpm::msLastLoadReportJson = "";
	void (*cWorldLoaderHpm::mpObjectDoneCallback)(cWorld*) = NULL;

	cWorldLoaderHpm::cWorldLoaderHpm()
	{
		AddSupportedExtension("hpm");
	}

	cWorldLoaderHpm::~cWorldLoaderHpm()
	{
	}

	cWorld* cWorldLoaderHpm::LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags)
	{
		Log(" -------- Loading SOMA hpm map '%s' ---------\n", cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();

		cWorld* pWorld = mpScene->CreateWorld(cString::To8Char(cString::GetFileNameW(asFile)));
		pWorld->SetFilePath(asFile);

		iPhysicsWorld* pPhysicsWorld = mpPhysics->CreateWorld(true);
		pPhysicsWorld->SetAccuracyLevel(ePhysicsAccuracy_Medium);
		pPhysicsWorld->SetWorldSize(-300, 300);
		pPhysicsWorld->SetMaxTimeStep(1.0f / 60.0f);
		pWorld->SetPhysicsWorld(pPhysicsWorld);

		cHpmWorldBuilder builder(pWorld, pPhysicsWorld, mpResources, mpGraphics, mpObjectDoneCallback, &mpCurrentElement);
		ParseHpmMap(builder, asFile, pWorld->Rs());
		builder.Finish();

		const tString sMap = cString::To8Char(cString::GetFileNameW(asFile));
		ohpl_hpm_report(sMap.data(), sMap.size(), (int)(cPlatform::GetApplicationTime() - lLoadStartTime), [](void*, const char* json, size_t jn, const char* log, size_t ln){
			msLastLoadReportJson.assign(json, jn);
			Log("%.*s", (int)ln, log);
		}, NULL);
		Log(" -------- Loading complete ---------\n");

		return pWorld;
	}

};
