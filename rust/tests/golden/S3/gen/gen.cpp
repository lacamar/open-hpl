// S3 golden generator: the object-id registry of SomaLuxEntity.cpp (SomaClearObjectIDs,
// SomaRegisterBodyIDs, SomaObjectID, SomaObjectFromID, IsEntity3DType, IsAlive) copied verbatim over a
// mock object table, plus the per-body liquid decisions of LiquidBuoyancy as a decision table.
//   gen <seeds> [ops]
// Mock objects 0..7 at 0x1000+0x10*i; kind table KINDS; RETYPE swaps the class at an address (freed
// memory reused), KILL/REV toggle world liveness of sound entities and particle systems.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

typedef std::string tString;

struct cSomaID
{
	uint8_t mA = 0;
	int32_t mB = 0;
	int32_t mC = 0;
};

static const char *KINDS[] = {"cMeshEntity", "cLightPoint", "cSoundEntity", "iPhysicsBody", "iPhysicsJointHinge", "iPhysicsJoint", "cParticleSystem", "iCharacterBody"};
static const char *QUERY[] = {"cMeshEntity", "cLightPoint", "cSoundEntity", "iPhysicsBody", "iPhysicsJointHinge", "iPhysicsJoint", "cParticleSystem", "iCharacterBody",
							  "iEntity3D", "iLight", "cLightBox", "iPhysicsJointSlider", "iPhysicsJointBall"};
static const int NK = 8, NQ = 13, NO = 8;
static int gKind[NO];
static bool gDead[NO];
static int gJoint[NO]; // 0 hinge 1 slider 2 ball (class behind a generic iPhysicsJoint entry)

static int Idx(size_t o) { return (int)((o - 0x1000) / 0x10); }
static size_t Addr(int i) { return 0x1000 + 0x10 * i; }

static bool Castable(const char *kind, const tString &ty)
{
	if (ty == kind)
		return true;
	tString k = kind;
	if (k == "iCharacterBody" || k.compare(0, 13, "iPhysicsJoint") == 0)
		return false;
	if (ty == "iEntity3D")
		return true;
	return ty == "iLight" && k == "cLightPoint";
}

namespace
{
	struct cObjectEntry
	{
		void *mpObj;
		tString msType;
		int mlCreationID;
	};
	std::map<void *, cSomaID> gmapObjectToID;
	std::map<int32_t, cObjectEntry> gmapIDToObject;
	int32_t glNextObjectID;

	bool IsEntity3DType(const tString &asType) { return asType != "iPhysicsJoint" && asType.compare(0, 13, "iPhysicsJoint") != 0 && asType != "iCharacterBody"; }

	void *CastEntity3D(void *e, const tString &asType)
	{
		int i = Idx((size_t)e);
		return Castable(KINDS[gKind[i]], asType) ? e : NULL;
	}

	bool IsAlive(const cObjectEntry &aEntry)
	{
		int i = Idx((size_t)aEntry.mpObj);
		if (aEntry.msType == "cSoundEntity")
			return !gDead[i] && i >= 0 && i < NO;
		if (aEntry.msType == "cParticleSystem")
			return !gDead[i];
		return true;
	}
}

void SomaClearObjectIDs()
{
	gmapObjectToID.clear();
	gmapIDToObject.clear();
}

void SomaRegisterBodyIDs(const std::vector<void *> &avBodies)
{
	int32_t l = 0;
	for (void *b : avBodies)
	{
		cSomaID id;
		id.mA = 0xfe;
		id.mB = --l;
		gmapObjectToID[b] = id;
		gmapIDToObject[id.mB] = cObjectEntry{b, "iPhysicsBody", 0};
	}
}

cSomaID SomaObjectID(void *apObj, const tString &asType)
{
	if (apObj == NULL)
		return cSomaID();
	auto it = gmapObjectToID.find(apObj);
	if (it != gmapObjectToID.end())
	{
		const cObjectEntry &entry = gmapIDToObject[it->second.mB];
		if (IsAlive(entry) && IsEntity3DType(entry.msType) == IsEntity3DType(asType) &&
			(entry.msType == asType || IsEntity3DType(asType) == false || CastEntity3D((void *)apObj, entry.msType) != NULL))
			return it->second;
		gmapIDToObject.erase(it->second.mB);
	}
	cSomaID id;
	id.mA = 0xfe;
	id.mB = glNextObjectID++;
	int lCreationID = asType == "cSoundEntity" ? (int)(Idx((size_t)apObj) + 100) : 0;
	gmapObjectToID[apObj] = id;
	gmapIDToObject[id.mB] = cObjectEntry{apObj, asType, lCreationID};
	return id;
}

void *SomaObjectFromID(const cSomaID &aID, const tString &asType)
{
	if (aID.mA != 0xfe)
		return NULL;
	auto it = gmapIDToObject.find(aID.mB);
	if (it == gmapIDToObject.end() || IsAlive(it->second) == false)
		return NULL;
	const cObjectEntry &entry = it->second;
	if (entry.msType == asType)
		return entry.mpObj;
	if (IsEntity3DType(entry.msType) && IsEntity3DType(asType))
	{
		if (entry.msType != "iEntity3D" && entry.msType != "iLight")
			return asType == "iEntity3D" || (asType == "iLight" && entry.msType.compare(0, 6, "cLight") == 0) ? entry.mpObj : NULL;
		return CastEntity3D(entry.mpObj, asType);
	}
	if (asType == "iPhysicsJoint" && entry.msType.compare(0, 13, "iPhysicsJoint") == 0)
		return entry.mpObj;
	if (asType.compare(0, 13, "iPhysicsJoint") == 0 && entry.msType == "iPhysicsJoint")
	{
		int j = gJoint[Idx((size_t)entry.mpObj)];
		if (asType == "iPhysicsJointHinge") return j == 0 ? entry.mpObj : NULL;
		if (asType == "iPhysicsJointSlider") return j == 1 ? entry.mpObj : NULL;
		if (asType == "iPhysicsJointBall") return j == 2 ? entry.mpObj : NULL;
	}
	return NULL;
}

static uint32_t gs;
static uint32_t Rnd() { return gs = gs * 1664525u + 1013904223u; }
static unsigned Fb(float f)
{
	unsigned u;
	memcpy(&u, &f, 4);
	return u;
}

// LiquidBuoyancy per-body branch structure, evaluated over every input combination
struct Plan
{
	bool skip = false, exitcb = false, off = false, init = false, wave = false, reset = false;
	int under = -1, grav = -1;
	float speed = 0;
};

static Plan Liquid(bool collide, bool active, bool mass, bool chr, bool inside, bool nguw, bool buoy, bool ownerHeld, bool waves, bool moved, float maxY, float cy, float ey)
{
	Plan p;
	if (collide == false || active == false || mass == false || chr)
	{
		p.skip = true;
		return p;
	}
	if (inside == false)
	{
		if (buoy)
			p.exitcb = true;
		p.off = true;
		if (nguw)
		{
			p.under = 0;
			if (ownerHeld == false)
				p.grav = 1;
			p.speed = 20;
		}
		return p;
	}
	if (nguw)
	{
		bool bUnder = maxY <= cy + ey - 0.2f;
		if (ownerHeld == false)
			p.grav = !bUnder;
		p.under = bUnder;
		p.speed = bUnder ? 1 : 20;
	}
	if (buoy == false)
		p.init = true;
	if (waves)
		p.wave = true;
	else if (moved)
		p.reset = true;
	return p;
}

int main(int argc, char **argv)
{
	int seeds = argc > 1 ? atoi(argv[1]) : 12;
	int nops = argc > 2 ? atoi(argv[2]) : 120;
	for (int sd = 1; sd <= seeds; ++sd)
	{
		gs = sd * 104729u;
		SomaClearObjectIDs();
		glNextObjectID = (int32_t)(Rnd() & 0x3fffffff) + 1;
		for (int i = 0; i < NO; ++i)
			gKind[i] = Rnd() % NK, gDead[i] = false, gJoint[i] = Rnd() % 3;
		printf("S %d %d", sd, glNextObjectID);
		for (int i = 0; i < NO; ++i)
			printf(" %d %d", gKind[i], gJoint[i]);
		printf("\n");
		for (int n = 0; n < nops; ++n)
		{
			int op = Rnd() % 16;
			if (op < 7)
			{
				int o = Rnd() % (NO + 1) - 1, q = Rnd() % NQ;
				// bias: query with the object's own kind half of the time
				if (o >= 0 && (Rnd() & 1))
					q = gKind[o];
				cSomaID id = SomaObjectID(o < 0 ? NULL : (void *)Addr(o), QUERY[q]);
				printf("ID %d %d = %d %d %d\n", o, q, id.mA, id.mB, id.mC);
			}
			else if (op < 12)
			{
				cSomaID id;
				if (Rnd() % 5 == 0)
					id.mA = Rnd() & 1 ? 0xfe : 0, id.mB = (int32_t)(Rnd() % 7) - 3, id.mC = 0;
				else
				{
					// pick a live id from the map
					if (gmapIDToObject.empty() == false)
					{
						auto it = gmapIDToObject.begin();
						std::advance(it, Rnd() % gmapIDToObject.size());
						id.mA = 0xfe, id.mB = it->first;
					}
					else
						id.mA = 0xfe, id.mB = 1;
				}
				int q = Rnd() % NQ;
				void *r = SomaObjectFromID(id, QUERY[q]);
				printf("FROM %d %d %d %d = %d\n", id.mA, id.mB, id.mC, q, r ? Idx((size_t)r) : -1);
			}
			else if (op == 12)
			{
				int k = Rnd() % 4;
				std::vector<void *> v;
				for (int i = 0; i < k; ++i)
					v.push_back((void *)Addr(Rnd() % NO));
				SomaRegisterBodyIDs(v);
				printf("REG %d", k);
				for (void *p : v)
					printf(" %d", Idx((size_t)p));
				printf("\n");
			}
			else if (op == 13)
			{
				if (Rnd() % 4 == 0)
					SomaClearObjectIDs(), printf("CLR\n");
				else
				{
					int i = Rnd() % NO;
					gKind[i] = Rnd() % NK, gJoint[i] = Rnd() % 3;
					printf("RETYPE %d %d %d\n", i, gKind[i], gJoint[i]);
				}
			}
			else
			{
				int i = Rnd() % NO;
				gDead[i] = op == 14;
				printf("%s %d\n", op == 14 ? "KILL" : "REV", i);
			}
		}
	}
	// liquid decision table: every flag combination x three heights
	for (unsigned m = 0; m < 1024; ++m)
		for (int h = 0; h < 3; ++h)
		{
			float maxY = h == 0 ? 0.5f : h == 1 ? 1.79f : 1.81f;
			Plan p = Liquid(m & 1, m & 2, m & 4, m & 8, m & 16, m & 32, m & 64, m & 128, m & 256, m & 512, maxY, 1.0f, 1.0f);
			printf("LQ %u %08x = %d %d %d %d %d %d %d %d %08x\n", m, Fb(maxY), (int)p.skip, (int)p.exitcb, (int)p.off, (int)p.init, (int)p.wave, (int)p.reset, p.under, p.grav, Fb(p.speed));
		}
	return 0;
}
