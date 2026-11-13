// Definitions of the fakes that need the enemy classes, plus stubs for what the harness does not extract.
std::string gLog;
cLuxBase *gpBase = NULL;

void Rec(const char *fmt, ...)
{
	char b[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	gLog += b;
	gLog += '\n';
}
uint32_t Bits(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	return u;
}
uint64_t Mix(uint64_t a, uint64_t b)
{
	a ^= b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
	a *= 0xff51afd7ed558ccdull;
	a ^= a >> 33;
	return a;
}
uint64_t HV(const cVector3f &v) { return Mix(Mix(Bits(v.x), Bits(v.y)), Bits(v.z)); }
namespace hpl
{
void Error(const char *fmt, ...) { Rec("ERROR"); }
cSerializeClass::cSerializeClass(const char *, const char *, cSerializeMemberField *, size_t, iSerializable *(*)()) {}
}
void Warning(const char *fmt, ...) { Rec("WARN"); }
void FatalError(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char b[256];
	vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	Rec("FATAL %s", b);
}

bool cLuxMapHelper::CheckLineOfSight(const cVector3f &a, const cVector3f &b, bool abAnything)
{
	return (Mix(HV(a), HV(b)) % 100) < (uint64_t)mlDensity;
}
void cLuxDebugHandler::AddMessage(const std::wstring &s, bool)
{
	std::string n;
	for(size_t i = 0; i < s.size(); ++i) n += (char)s[i];
	Rec("Debug %s", n.c_str());
}
cAnimationState *cMeshEntity::GetAnimationStateFromName(const std::string &s)
{
	uint64_t h = 1469598103934665603ull;
	for(size_t i = 0; i < s.size(); ++i) h = (h ^ (unsigned char)s[i]) * 1099511628211ull;
	if(h % 13 == 0) return NULL;
	std::map<std::string, cAnimationState *>::iterator it = mAnims.find(s);
	if(it != mAnims.end()) return it->second;
	cAnimationState *a = new cAnimationState();
	a->msName = s;
	a->mbActive = false;
	a->mbOver = false;
	a->mbFading = false;
	a->mbLoop = false;
	a->mfSpecial = 0.5f;
	a->mfPrev = 0.3f;
	a->mfTime = 0.4f;
	a->mfRel = 0.25f;
	mAnims[s] = a;
	return a;
}
cAINodeIterator cAINodeContainer::GetNodeIterator(const cVector3f &p, float d)
{
	cAINodeIterator it;
	it.mi = 0;
	for(size_t i = 0; i < mvNodes.size(); ++i)
		if(cMath::Vector3Dist(mvNodes[i]->mvPos, p) <= d * 1.3f + 0.5f) it.mv.push_back(mvNodes[i]);
	return it;
}
bool cAINodeContainer::FreePath(const cVector3f &a, const cVector3f &b, int rays, int flags)
{
	return (Mix(Mix(HV(a), HV(b)), (uint64_t)(rays * 131 + flags * 977)) % 10) < 8;
}
bool cAStarHandler::GetPath(const cVector3f &a, const cVector3f &b, tAINodeList *l)
{
	uint64_t h = Mix(HV(a), HV(b));
	if(h % 9 == 0 || mpC->mvNodes.empty()) return false;
	int n = 1 + (int)(h % 4);
	for(int i = 0; i < n; ++i) l->push_back(mpC->mvNodes[Mix(h, i) % mpC->mvNodes.size()]);
	return true;
}
iLuxEntity *cLuxMap::GetEntityByID(int id, eLuxEntityType, int)
{
	for(size_t i = 0; i < mvAll.size(); ++i)
		if(mvAll[i]->mlID == id) return mvAll[i];
	return NULL;
}
void cLuxMap::BroadcastEnemyMessage(eLuxEnemyMessage aType, bool abHasPosition, const cVector3f &avPos, float afRadius, float afTime, bool abLocalScope,
									const cVector3f &avX, float afX, int alX)
{
	Rec("Broadcast %d pos %08x%08x%08x r %08x x %08x%08x%08x", (int)aType, Bits(avPos.x), Bits(avPos.y), Bits(avPos.z), Bits(afRadius), Bits(avX.x), Bits(avX.y), Bits(avX.z));
	for(size_t i = 0; i < mvEnemies.size(); ++i)
	{
		iLuxEnemy *pEnemy = mvEnemies[i];
		if(pEnemy->IsActive() == false) continue;
		if(abHasPosition)
		{
			cBoundingVolume *pBv = pEnemy->GetCharacterBody()->GetCurrentBody()->GetBoundingVolume();
			if(cMath::CheckSphereIntersection(avPos, afRadius, pBv->GetWorldCenter(), pBv->GetRadius()) == false) continue;
		}
		pEnemy->SendMessage(aType, afTime, abLocalScope, avX, afX, alX);
	}
}
void iLuxEntity::SetActive(bool abX)
{
	if(mbActive == abX) return;
	mbActive = abX;
	Rec("SetActive %d", abX);
	OnSetActive(abX);
}

iLuxEnemy::~iLuxEnemy() {}
void iLuxEnemy::AfterWorldLoad() {}
void iLuxEnemy::OnMapEnter() {}
void iLuxEnemy::OnRenderSolid(cRendererCallbackFunctions *) {}
bool iLuxEnemy::CanInteract(iPhysicsBody *) { return false; }
bool iLuxEnemy::OnInteract(iPhysicsBody *, const cVector3f &) { return false; }
void iLuxEnemy::OnConnectionStateChange(iLuxEntity *, int) {}
iEntity3D *iLuxEnemy::GetAttachEntity() { return &mAttach; }
bool iLuxEnemy::InRangeOfFood(iPhysicsBody *) { return false; }
void iLuxEnemy::SaveToSaveData(iLuxEntity_SaveData *) {}
void iLuxEnemy::LoadFromSaveData(iLuxEntity_SaveData *) {}
void iLuxEnemy::SetupSaveData(iLuxEntity_SaveData *) {}
static cSoundEntity gFakeSound;
static int gSoundCounter = 0;
cSoundEntity *iLuxEnemy::PlaySound(const tString &asName)
{
	if(asName == "") return NULL;
	Rec("PlaySound %s", asName.c_str());
	gFakeSound.mlID = ++gSoundCounter;
	return &gFakeSound;
}
void cParticleSystem::SetPosition(const cVector3f &p) { Rec("PS.Pos %016llx", (unsigned long long)HV(p)); }
bool cWorld::SoundEntityExists(cSoundEntity *s, int id) { return ((id * 7 + s->mlID) % 3) != 0; }
cParticleSystem *cWorld::CreateParticleSystem(const std::string &n, const std::string &t, float f)
{
	Rec("PS %s %s %08x", n.c_str(), t.c_str(), Bits(f));
	return (t.size() % 3) == 0 ? NULL : &mPS;
}

void cLuxEnemy_Grunt::OnSetupAfterLoad(cWorld *) {}
void cLuxEnemy_Grunt::OnAfterWorldLoad() {}
void cLuxEnemy_Grunt::UpdateEnemySpecific(float) {}
void cLuxEnemy_Grunt::OnRenderSolidImplemented(cRendererCallbackFunctions *) {}
cLuxEnemy_Grunt::~cLuxEnemy_Grunt() {}
iLuxEntity_SaveData *cLuxEnemy_Grunt::CreateSaveData() { return NULL; }
void cLuxEnemy_Grunt::SaveToSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_Grunt::LoadFromSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_Grunt::SetupSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemyMover::UpdateStepEffects(float) {}
void cLuxEnemyMover::SetupAfterLoad(cWorld *) {}
cLuxEnemyMover::~cLuxEnemyMover() {}
cLuxEnemyPathfinder::~cLuxEnemyPathfinder() {}
void cLuxEnemyPathfinder::SetupAfterLoad(cWorld *) {}

void cLuxEnemy_ManPig::OnSetupAfterLoad(cWorld *) {}
void cLuxEnemy_ManPig::OnAfterWorldLoad() {}
void cLuxEnemy_ManPig::OnRenderSolidImplemented(cRendererCallbackFunctions *) {}
void cLuxEnemy_ManPig::OnSetActiveEnemySpecific(bool) {}
void cLuxEnemy_ManPig::UpdateTesla(float afTimeStep) { Rec("Tesla %08x", Bits(afTimeStep)); }
cLuxEnemy_ManPig::~cLuxEnemy_ManPig() {}
iLuxEntity_SaveData *cLuxEnemy_ManPig::CreateSaveData() { return NULL; }
void cLuxEnemy_ManPig::SaveToSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_ManPig::LoadFromSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_ManPig::SetupSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_WaterLurker::OnSetupAfterLoad(cWorld *) {}
void cLuxEnemy_WaterLurker::OnAfterWorldLoad() {}
void cLuxEnemy_WaterLurker::OnRenderSolidImplemented(cRendererCallbackFunctions *) {}
void cLuxEnemy_WaterLurker::OnSetActiveEnemySpecific(bool) {}
cLuxEnemy_WaterLurker::~cLuxEnemy_WaterLurker() {}
iLuxEntity_SaveData *cLuxEnemy_WaterLurker::CreateSaveData() { return NULL; }
void cLuxEnemy_WaterLurker::SaveToSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_WaterLurker::LoadFromSaveData(iLuxEntity_SaveData *) {}
void cLuxEnemy_WaterLurker::SetupSaveData(iLuxEntity_SaveData *) {}

void iLuxEnemy::FadeToSmoke(bool b) { Rec("FadeToSmoke %d", b); }
float cLuxMapHelper::GetLightLevelAtPos(const cVector3f &p, std::vector<iLight *> *, float) { return (float)(Mix(HV(p), 55) % 1000) / 1000.0f; }
bool cLuxMapHelper::GetClosestCharCollider(const cVector3f &s, const cVector3f &d, float len, float *dist, cVector3f *n, iPhysicsBody **b)
{
	uint64_t h = Mix(HV(s), Bits(len));
	if(h % 5 == 0) return false;
	*dist = 0.01f + (float)(h % 400) / 1000.0f;
	return true;
}
bool cLuxMapHelper::ShapeDamage(iCollideShape *s, const cMatrixf &m, const cVector3f &o, float mn, float mx, float force, float imp, int str, float hs, eLuxDamageType dt, eLuxWeaponHitType wt,
								bool ce, bool cp, bool cpr, bool lethal, bool *hitPlayer)
{
	Rec("ShapeDamage %d %016llx %016llx %08x %08x %08x %08x %d", (int)(uintptr_t)s, (unsigned long long)Mix(Mix(Bits(m.m[0][0]), Bits(m.m[1][2])), Mix(Bits(m.m[0][3]), Mix(Bits(m.m[1][3]), Bits(m.m[2][3])))),
		(unsigned long long)HV(o), Bits(mn), Bits(mx), Bits(force), Bits(imp), str);
	uint64_t h = Mix(HV(o), (uint64_t)str);
	if(hitPlayer) *hitPlayer = ((h >> 5) & 1) != 0;
	return (h & 3) != 0;
}
