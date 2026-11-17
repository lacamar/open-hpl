#include "SomaS9Dump.h"
#include "SomaBase.h"
#include "SomaS5Dump.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void OhplS9Log(const char *fmt, ...)
{
	if (OhplS5DumpEnabled() == false)
		return;
	char sBuf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(sBuf, sizeof(sBuf), fmt, ap);
	va_end(ap);
	OhplS5Log("s9 %s", sBuf);
}

static unsigned Bits(float afX)
{
	unsigned l;
	memcpy(&l, &afX, sizeof(l));
	return l;
}

void OhplS9DumpEnter(cSomaLuxMap *apMap)
{
	if (OhplS5DumpEnabled() == false)
		return;
	static const char *vGroups[] = {"", "+player", "-player", "+a +b -c", "+a", "-a -b", "+x +y +z",
									"+q1 +q2 +q3 +q4 +q5 +q6 +q7 +q8 +q9 +q10 +q11 +q12 +q13 +q14 +q15 +q16 +q17", "+player -player", "bogus"};
	for (const char *s : vGroups)
		OhplS9Log("cf '%s' %x", s, SomaCollideFlag(s));
	cWorld *pWorld = apMap->GetWorld();
	OhplS9Log("map %s ents %d", apMap->GetName().c_str(), (int)apMap->GetEntities().size());
	auto Count = [](auto it) {
		int l = 0;
		while (it.HasNext())
		{
			it.Next();
			++l;
		}
		return l;
	};
	OhplS9Log("it light %d smesh %d dmesh %d ps %d ff %d snd %d bb %d lf %d beam %d fog %d", Count(pWorld->GetLightIterator()),
			  Count(pWorld->GetStaticMeshEntityIterator()), Count(pWorld->GetDynamicMeshEntityIterator()), Count(pWorld->GetParticleSystemIterator()),
			  Count(pWorld->GetForceFieldIterator()), Count(pWorld->GetSoundEntityIterator()), Count(pWorld->GetBillboardIterator()),
			  Count(pWorld->GetLensFlareIterator()), Count(pWorld->GetBeamIterator()), Count(pWorld->GetFogAreaIterator()));
	std::vector<cVector3f> vPoints = {cVector3f(0, 0, 0), cVector3f(0, 1, 0), cVector3f(5, 1, 5), cVector3f(-5, 2, 5)};
	if (cStartPosEntity *pStart = pWorld->GetFirstStartPosEntityByName())
		vPoints.push_back(pStart->GetWorldMatrix().GetTranslation());
	for (const cVector3f &p : vPoints)
		OhplS9Log("ll %08x %08x %08x %08x %08x", Bits(p.x), Bits(p.y), Bits(p.z), Bits(SomaLightLevelAtPos(p, NULL, 0)), Bits(SomaLightLevelAtPos(p, NULL, 1.5f)));
}
