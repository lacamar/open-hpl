
#include "resources/WorldLoaderHpm.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "resources/WorldLoaderHpmBuilder.h"

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
		ParseHpmMap(builder, asFile);
		builder.Finish();

		BuildLoadReport(cString::To8Char(cString::GetFileNameW(asFile)), (int)(cPlatform::GetApplicationTime() - lLoadStartTime), builder);
		Log(" -------- Loading complete ---------\n");

		return pWorld;
	}

	static tString HpmJsonEscape(const tString& asIn)
	{
		tString sOut;
		for (size_t i = 0; i < asIn.size(); ++i)
		{
			char c = asIn[i];
			if (c == '"' || c == '\\') { sOut += '\\'; sOut += c; }
			else if ((unsigned char)c < 0x20) sOut += ' ';
			else sOut += c;
		}
		return sOut;
	}

	void cWorldLoaderHpm::BuildLoadReport(const tString& asMap, int alTotalTimeMs, cHpmWorldBuilder& aBuilder)
	{
		tString sJson = "{\"map\":\"" + HpmJsonEscape(asMap) + "\",\"total_ms\":" + cString::ToString(alTotalTimeMs) +
						",\"terrain_active\":" + (aBuilder.IsTerrainActive() ? "true" : "false") + ",\"tracks\":{";

		bool bFirstTrack = true;
		const std::map<tString, cHpmTrackStats>& mapStats = aBuilder.GetTrackStats();
		for (std::map<tString, cHpmTrackStats>::const_iterator it = mapStats.begin(); it != mapStats.end(); ++it)
		{
			const cHpmTrackStats& stats = it->second;
			if (!bFirstTrack) sJson += ",";
			bFirstTrack = false;

			sJson += "\"" + it->first + "\":{\"xml\":" + cString::ToString(stats.mlInXml) +
					 ",\"created\":" + cString::ToString(stats.mlCreated) +
					 ",\"ms\":" + cString::ToString(stats.mlTimeMs) +
					 ",\"file_missing\":" + (stats.mbFileMissing ? "true" : "false") + ",\"skipped\":{";

			bool bFirstReason = true;
			for (std::map<tString, int>::const_iterator rIt = stats.mmapSkipped.begin(); rIt != stats.mmapSkipped.end(); ++rIt)
			{
				if (!bFirstReason) sJson += ",";
				bFirstReason = false;
				sJson += "\"" + HpmJsonEscape(rIt->first) + "\":" + cString::ToString(rIt->second);
			}
			sJson += "}}";

			int lSkipped = stats.mlInXml - stats.mlCreated;
			Log("  SOMA hpm track %-20s xml=%d created=%d skipped=%d (%d ms)\n", it->first.c_str(),
				stats.mlInXml, stats.mlCreated, lSkipped, stats.mlTimeMs);
		}
		sJson += "}}";

		msLastLoadReportJson = sJson;
	}

};
