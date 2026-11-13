// Scenario driver: identical RNG draws on both sides, state dump after every step.
#include <cstdlib>
#include <new>
#include <fstream>
#include <sstream>

struct Rng
{
	uint64_t s;
	uint64_t next()
	{
		uint64_t z = (s += 0x9e3779b97f4a7c15ull);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	}
	float f(float lo, float hi) { return lo + (hi - lo) * (float)((next() >> 40) & 0xffffff) / 16777216.0f; }
	int i(int lo, int hi) { return lo + (int)(next() % (uint64_t)(hi - lo + 1)); }
	bool p(float pr) { return f(0, 1) < pr; }
};

static int gFocus = 0;
struct Case
{
	uint64_t seed;
	int idx;
	std::string ent, map;
	std::map<std::string, std::string> vars;
	std::vector<cVector3f> nodes;
};

static float VarF(Case &c, const char *k, float d)
{
	std::map<std::string, std::string>::iterator it = c.vars.find(k);
	if(it == c.vars.end()) return d;
	return (float)atof(it->second.c_str());
}

class cTestEnemy : public iLuxEnemy
{
public:
	uint64_t mSalt;
	cTestEnemy(const tString &n, int id, cLuxMap *m) : iLuxEnemy(n, id, m, eLuxEnemyType_ManPig), mSalt(0) {}
	bool StateEventImplement(int s, eLuxEnemyStateEvent ev, cLuxStateMessage *m)
	{
		uint64_t h = Mix(Mix(s + 7, ev), (m ? (uint64_t)m->mType * 31 + Bits(m->mfCustomValue) : 99)) ^ mSalt;
		Rec("TE %d %d %d", s, (int)ev, m ? (int)m->mType : -1);
		if(s < 0 ? (h & 1) == 0 : (h & 3) == 0) return false;
		switch((h >> 3) % 7)
		{
		case 0: ChangeState((eLuxEnemyState)((h >> 9) % 17)); break;
		case 1: SendMessage((eLuxEnemyMessage)((h >> 9) % 23), 0.02f + (float)((h >> 17) % 5) * 0.01f, ((h >> 23) & 1) != 0); break;
		case 2: SendMessage(eLuxEnemyMessage_SoundHeard, 0.03f, false, cVector3f((float)((h >> 9) % 9), 0, (float)((h >> 15) % 9)), (float)((h >> 21) % 7) * 0.1f, 1); break;
		case 3: mfFOVMul = 1.0f + (float)((h >> 9) & 3); break;
		case 4: mpPathfinder->MoveTo(cVector3f((float)((h >> 9) % 30), 0, (float)((h >> 15) % 30))); break;
		case 5: mpMover->TurnToPos(cVector3f((float)((h >> 9) % 30), 0, (float)((h >> 15) % 30))); break;
		case 6: mpPathfinder->Stop(); PlayAnim("IdleExtra" + cString::ToString((int)((h >> 9) % 5)), false, 0.3f); break;
		}
		return true;
	}
	bool PlayerIsDetected() { return mbCanSeePlayer || (Mix(HV(mpCharBody->mvPos), mSalt) & 3) == 0; }
	float GetDamageMul(float, int) { return 1; }
	void OnSetupAfterLoad(cWorld *) {}
	void OnAfterWorldLoad() {}
	void UpdateEnemySpecific(float) {}
	void OnRenderSolidImplemented(cRendererCallbackFunctions *) {}
};

#ifdef NEWSIDE
#define MS(m, f) ((m)->mS.f)
static size_t QLen(iLuxEnemy *e) { return ohpl_ga3_queue_len(e->mpGa3Queue); }
static void QDump(iLuxEnemy *e)
{
	for(size_t i = 0; i < QLen(e); ++i)
	{
		OhplGa3Msg m;
		ohpl_ga3_queue_get(e->mpGa3Queue, i, &m);
		Rec("  Q %d %08x %08x %08x%08x%08x %08x %d %d", m.ty, Bits(m.time), Bits(m.count), Bits(m.customV[0]), Bits(m.customV[1]), Bits(m.customV[2]), Bits(m.customF), m.customI, m.state);
	}
}
static void QClear(iLuxEnemy *e) { ohpl_ga3_queue_clear(e->mpGa3Queue); }
#define PFWINLEN(pf) ((pf)->mWin.len)
#else
#define MS(m, f) MSMAP_##f(m)
#define MSMAP_turning(m) ((m)->mbTurning)
#define MSMAP_turnGoalAngle(m) ((m)->mfTurnGoalAngle)
#define MSMAP_turnSpeed(m) ((m)->mfTurnSpeed)
#define MSMAP_turnBreakAcc(m) ((m)->mfTurnBreakAcc)
#define MSMAP_stuckCounter(m) ((m)->mfStuckCounter)
#define MSMAP_moveState(m) ((int)(m)->mMoveState)
#define MSMAP_overideMoveState(m) ((m)->mbOverideMoveState)
static void QDump(iLuxEnemy *e)
{
	for(tLuxStateMessageListIt it = e->mlstMessages.begin(); it != e->mlstMessages.end(); ++it)
		Rec("  Q %d %08x %08x %08x%08x%08x %08x %d %d", (int)it->mType, Bits(it->mfTime), Bits(it->mfCount), Bits(it->mvCustomValue.x), Bits(it->mvCustomValue.y), Bits(it->mvCustomValue.z), Bits(it->mfCustomValue), it->mlCustomValue, (int)it->mState);
}
static void QClear(iLuxEnemy *e) { e->mlstMessages.clear(); }
#define PFWINLEN(pf) ((int)(pf)->mlstPathNodeDistances.size())
#endif

static void FB(const char *tag, float f) { Rec("%s %08x", tag, Bits(f)); }

static void Dump(iLuxEnemy *e, const char *tag)
{
	Rec("S %s st %d %d %d see %d det %d rng %d los %d look %08x door %d %d %08x lpc %08x", tag, (int)e->mCurrentState, (int)e->mNextState, (int)e->mPreviousState,
		e->mbCanSeePlayer, e->mbPlayerDetected, e->mbPlayerInRange, e->mlPlayerInLOSCount, Bits(e->mfLookForPlayerCount), e->mbStuckAtDoor, e->mlStuckDoorID,
		Bits(e->mfCheckAtDoorCount), Bits(e->mfLastPlayerPosCount));
	Rec("  lk %016llx fov %08x sight %08x hp %08x tmp %016llx tv %08x tl %08x dmg %08x hit %d patrol %d lsn %d", (unsigned long long)HV(e->mvLastKnownPlayerPos),
		Bits(e->mfFOVMul), Bits(e->mfSightRange), Bits(e->mfHealth), (unsigned long long)HV(e->mvTempPos), Bits(e->mfTempVal), Bits(e->mlTempVal), Bits(e->mfDamageCount),
		e->mlAttackHitCounter, e->mlCurrentPatrolNode, e->mpLastSearchNode ? e->mpLastSearchNode->mlID : -1);
	Rec("  spd %08x %08x %08x %08x", Bits(e->mfForwardSpeed), Bits(e->mfBackwardSpeed), Bits(e->mfForwardAcc), Bits(e->mfForwardDeacc));
	cLuxEnemyMover *m = e->mpMover;
	Rec("  mv %d %08x %08x %08x %08x %d %d", MS(m, turning), Bits(MS(m, turnGoalAngle)), Bits(MS(m, turnSpeed)), Bits(MS(m, turnBreakAcc)), Bits(MS(m, stuckCounter)),
		MS(m, moveState), MS(m, overideMoveState));
	cLuxEnemyPathfinder *pf = e->mpPathfinder;
	std::string path;
	for(tAINodeListIt it = pf->mlstPathNodes.begin(); it != pf->mlstPathNodes.end(); ++it)
	{
		char b[16];
		snprintf(b, sizeof(b), " %d", (*it)->mlID);
		path += b;
	}
	Rec("  pf %d %016llx win %d path%s", pf->mbMoving, (unsigned long long)HV(pf->mvMoveGoalPos), PFWINLEN(pf), path.c_str());
	Rec("  anim %s %d", e->mpCurrentAnimation ? e->mpCurrentAnimation->msName.c_str() : "-", e->mbAnimationIsSpeedDependant);
	Rec("  a4 glow %08x %08x %08x snd %d %08x sid %d hall %d san %d spd %d %d", Bits(e->mfDarknessGlowAlpha), Bits(e->mfDarknessGlowAlphaGoal), Bits(e->mfDarknessGlowUpdateCount),
		(int)e->mSoundState, Bits(e->mfAmbientSoundCount), e->mpCurrentSound ? e->mlCurrentSoundID : -1, e->mbHallucination, e->mbCausesSanityDecrease,
		e->mbUseMoveAnimWhenCurrentIsOver, e->mfCheckGroundRayCount > 0);
	Rec("  a4 ok %08x %d", Bits(e->mfAnimationSpeedMul), e->mlAttackHitCounter);
	if(cLuxEnemy_Grunt *g = dynamic_cast<cLuxEnemy_Grunt *>(e)) Rec("  gr wait %08x run %08x", Bits(g->mfWaitTime), Bits(g->mfAlertRunTowardsCount));
	if(cLuxEnemy_ManPig *g = dynamic_cast<cLuxEnemy_ManPig *>(e))
		Rec("  pig wait %08x run %08x flee %08x miss %d force %d lan %08x chk %08x nt %08x hs %d sk %d pose %d", Bits(g->mfWaitTime), Bits(g->mfAlertRunTowardsCount),
			Bits(g->mfFleeCheckIfInvisbleCount), g->mbLastShortAttackWasMiss, g->mbForceChargeAttack, Bits(g->mfInLanternLightCount), Bits(g->mfCheckFlashLightShining),
			Bits(g->mfTeslaSpecialNoticeCount), g->mlHuntSoundId, e->mbSkipVisibilityRangeHandicaps, (int)e->mCurrentPose);
	if(dynamic_cast<cLuxEnemy_WaterLurker *>(e)) Rec("  wl san %d", e->mbCausesSanityDecrease);
	QDump(e);
}

static void Setup(Case &c, Rng &r, cLuxPlayer &pl, cLuxMap &map, cAINodeContainer &nc, cAStarHandler &as, iCharacterBody &body, cMeshEntity &mesh, iLuxEnemy *e)
{
	e->mpCharBody = &body;
	e->mpMeshEntity = &mesh;
	e->mpMap = &map;
	e->mbUseAnimations = r.p(0.95f);
	e->mfHealth = VarF(c, "Health", 100);
	e->mlToughness = (int)VarF(c, "Toughness", 0);
	e->mfSightRange = VarF(c, "SightRange", 15);
	e->mfDarknessSightRange = VarF(c, "DarknessSightRange", 5);
	e->mfHearVolume = VarF(c, "HearVolume", 0.5f);
	e->mfFOV = cMath::ToRad(VarF(c, "FOV", 120));
	e->mfFOVXMul = VarF(c, "FOVXMul", 1);
	e->mfMaxRegenHealth = VarF(c, "MaxRegenHealth", 100);
	e->mfRegenHealthSpeed = VarF(c, "RegenHealthSpeed", 0);
	e->mfActivationDistance = VarF(c, "ActivationDistance", 30);
	e->mbAutoRemoveAtPathEnd = r.p(0.7f);
	e->mfPlayerSearchMaxAngle = cMath::ToRad(VarF(c, "PlayerSearchMaxAngle", 45));
	e->mfPlayerSearchMinDistMul = VarF(c, "PlayerSearchMinDistMul", 0.5f);
	e->mfPlayerSearchMaxDistMul = VarF(c, "PlayerSearchMaxDistMul", 2);
	e->mfPlayerSearchTime = VarF(c, "PlayerSearchTime", 10);
	e->mfPlayerPatrolMaxAngle = cMath::ToRad(VarF(c, "PlayerPatrolMaxAngle", 110));
	e->mfPlayerPatrolMinDist = VarF(c, "PlayerPatrolMinDist", 8);
	e->mfPlayerPatrolMaxDist = VarF(c, "PlayerPatrolMaxDist", 18);
	for(int pose = 0; pose < 2; ++pose)
	{
		e->mfDefaultForwardSpeed[pose][0] = (pose ? 1.25f : 1.0f) * VarF(c, "Walk_ForwardSpeed", 1.3f);
		e->mfDefaultBackwardSpeed[pose][0] = (pose ? 1.25f : 1.0f) * VarF(c, "Walk_BackwardSpeed", 0.4f);
		e->mfDefaultForwardAcc[pose][0] = (pose ? 1.25f : 1.0f) * VarF(c, "Walk_ForwardAcc", 2);
		e->mfDefaultForwardDeacc[pose][0] = (pose ? 1.25f : 1.0f) * VarF(c, "Walk_ForwardDeacc", 18);
		e->mfDefaultForwardSpeed[pose][1] = (pose ? 1.25f : 1.0f) * VarF(c, "Run_ForwardSpeed", 5.1f);
		e->mfDefaultBackwardSpeed[pose][1] = (pose ? 1.25f : 1.0f) * VarF(c, "Run_BackwardSpeed", 2);
		e->mfDefaultForwardAcc[pose][1] = (pose ? 1.25f : 1.0f) * VarF(c, "Run_ForwardAcc", 4);
		e->mfDefaultForwardDeacc[pose][1] = (pose ? 1.25f : 1.0f) * VarF(c, "Run_ForwardDeacc", 8);
		e->mfStoppedToWalkSpeed[pose] = VarF(c, "StoppedToWalkSpeed", 0.15f);
		e->mfWalkToStoppedSpeed[pose] = VarF(c, "WalkToStoppedSpeed", 0.1f);
		e->mfWalkToRunSpeed[pose] = VarF(c, "WalkToRunSpeed", 3.2f);
		e->mfRunToWalkSpeed[pose] = VarF(c, "RunToWalkSpeed", 2.8f);
	}
	e->mfTurnSpeedMul = VarF(c, "TurnSpeedMul", 6);
	e->mfTurnMaxSpeed = VarF(c, "TurnMaxSpeed", 12);
	e->mfTurnMinBreakAngle = cMath::ToRad(VarF(c, "TurnMinBreakAngle", 10));
	e->mfTurnBreakMul = VarF(c, "TurnBreakMul", 2);
	e->mfMoveSpeedAnimMul = VarF(c, "MoveSpeedAnimMul", 1.2f);
	e->mfNormalAttackDistance = VarF(c, "NormalAttackDistance", 1.9f);
	e->mfPlayerInDarknessLightLevel = 0.35f;
	e->mfCrouchVisibleRangeMul = 0.5f;
	e->mbSkipVisibilityRangeHandicaps = r.p(0.2f);
	e->msName = "enemy";
	e->mfEnemyDarknessGlowMaxDistance = r.f(5, 30);
	e->mfDarknessGlowAlphaGoal = r.f(0, 1);
	e->mbHallucination = r.p(0.15f);
	e->mfHallucinationEndDist = r.f(1, 6);
	e->mbAlignEntityWithGroundRay = r.p(0.5f);
	e->mfDamageCount = 0;
	for(int i = 0; i < 4; ++i)
	{
		e->msAmbientSound[i] = r.p(0.3f) ? "" : "amb" + cString::ToString(i);
		e->mfAmbientSoundMinTime[i] = r.f(0, 1);
		e->mfAmbientSoundMaxTime[i] = e->mfAmbientSoundMinTime[i] + r.f(0, 2);
	}
	e->mvAttackShapes.push_back((iCollideShape *)(uintptr_t)1);
	e->mvAttackShapes.push_back((iCollideShape *)(uintptr_t)2);
	e->mNormalAttackSize.mlShapeIdx = r.i(0, 1);
	e->mNormalAttackSize.mvOffset = cVector3f(r.f(-1, 1), r.f(-1, 1), r.f(-1, 1));
	e->mNormalAttackDamage.mfMaxDamage = r.f(30, 60);
	e->mNormalAttackDamage.mfForce = r.f(0, 10);
	e->mNormalAttackDamage.mlStrength = r.i(0, 3);
	e->mNormalAttackDamage.msHitSound = r.p(0.5f) ? "hit" : "";
	e->mBreakDoorAttackDamage.mfMaxDamage = r.f(30, 60);
	e->mBreakDoorAttackDamage.msHitSound = r.p(0.5f) ? "doorhit" : "";
	if(cLuxEnemy_Grunt *g = dynamic_cast<cLuxEnemy_Grunt *>(e))
	{
		g->msNoticeSound = "grunt/notice";
		g->msGiveUpNoticeSound = r.p(0.5f) ? "" : "grunt/giveup";
		g->msEnabledSound = "grunt/enabled";
		g->mfGroggyDamageCount = VarF(c, "GroggyDamageCount", 1);
		g->mfAlertToHuntDistance = VarF(c, "AlertToHuntDistance", 9);
		g->mfAlertToInstantHuntDistance = VarF(c, "AlertToInstantHuntDistance", 2.5f);
		g->mfHuntPauseMinTime = VarF(c, "HuntPauseMinTime", 2);
		g->mfHuntPauseMaxTime = VarF(c, "HuntPauseMaxTime", 4.5f);
		g->mfIncreaseAlertSpeedDistance = VarF(c, "IncreaseAlertSpeedDistance", 6);
		g->mfIncreasedAlertSpeedMul = VarF(c, "IncreasedAlertSpeedMul", 1.25f);
		g->mfAlertRunTowardsToHuntLimit = VarF(c, "AlertRunTowardsToHuntLimit", 30);
		g->mfAlertRunTowardsCheckDistance = VarF(c, "AlertRunTowardsCheckDistance", 6);
		g->mfIdleExtraTimeMin = VarF(c, "IdleExtraTimeMin", 2);
		g->mfIdleExtraTimeMax = VarF(c, "IdleExtraTimeMax", 6);
		g->mlIdleExtraNum = (int)VarF(c, "IdleExtraNum", 3);
	}
	if(cLuxEnemy_ManPig *g = dynamic_cast<cLuxEnemy_ManPig *>(e))
	{
		g->msNoticeSound = "pig/notice";
		g->msGiveUpNoticeSound = r.p(0.5f) ? "" : "pig/giveup";
		g->msEnabledSound = "pig/enabled";
		g->msChaseSound = r.p(0.8f) ? "pig/chase" : "";
		g->mfGroggyDamageCount = VarF(c, "GroggyDamageCount", 1);
		g->mfAlertToHuntDistance = VarF(c, "AlertToHuntDistance", 9);
		g->mfAlertToInstantHuntDistance = VarF(c, "AlertToInstantHuntDistance", 2.5f);
		g->mfHuntPauseMinTime = VarF(c, "HuntPauseMinTime", 2);
		g->mfHuntPauseMaxTime = VarF(c, "HuntPauseMaxTime", 4.5f);
		g->mfIncreaseAlertSpeedDistance = VarF(c, "IncreaseAlertSpeedDistance", 6);
		g->mfIncreasedAlertSpeedMul = VarF(c, "IncreasedAlertSpeedMul", 1.25f);
		g->mfAlertRunTowardsToHuntLimit = VarF(c, "AlertRunTowardsToHuntLimit", 30);
		g->mfAlertRunTowardsCheckDistance = VarF(c, "AlertRunTowardsCheckDistance", 6);
		g->mbIsTelsa = c.ent.find("tesla") != std::string::npos || r.p(0.15f);
		g->mbThreatenOnAlert = r.p(0.45f);
		g->mbFleeFromPlayer = r.p(0.45f);
		g->mbAutoDisableAfterFlee = r.p(0.5f);
		g->mIdleBehavior = (eLuxIdleBehavior)(r.p(0.5f) ? 2 : r.i(0, 2));
		g->mPatrolMoveSpeed = r.p(0.7f) ? eLuxEnemyMoveSpeed_Walk : eLuxEnemyMoveSpeed_Run;
		g->mbAllowZeroWaitTime = r.p(0.3f);
		g->mfDamageMul = r.f(0.5f, 2);
		g->mfRunSpeedMul = r.f(0.8f, 1.5f);
		g->mbPlayActivateSound = r.p(0.7f);
		g->mfLanternSensitivity = r.f(0.5f, 2);
		if(gFocus == 1 || gFocus == 2)
		{
			e->mfSightRange = 0.1f;
			e->mfHearVolume = 99;
			g->mbIsTelsa = false;
			g->mIdleBehavior = gFocus == 1 ? eLuxIdleBehavior_Track : eLuxIdleBehavior_Stalk;
		}
		if(gFocus == 3) { g->mbFleeFromPlayer = true; g->mbThreatenOnAlert = false; }
		if(gFocus == 4) g->mbThreatenOnAlert = true;
		g->mfHuntPauseTimeMul = r.f(0.5f, 2);
		e->mCurrentPose = r.p(0.5f) ? eLuxEnemyPoseType_Quadruped : eLuxEnemyPoseType_Biped;
	}
	if(cLuxEnemy_WaterLurker *w = dynamic_cast<cLuxEnemy_WaterLurker *>(e))
	{
		w->mfPlayerDetectionHeight = VarF(c, "PlayerDetectionHeight", 0.6f);
		w->mfEatDamage = VarF(c, "EatDamage", 10);
		w->mfAfterAttackPauseTime = r.f(0, 3);
		w->msSplashPS_Walk = r.p(0.7f) ? "ps/walk" : "";
		w->msSplashPS_Run = r.p(0.7f) ? "ps/run" : "";
		w->msSplashPS_Eat = r.p(0.7f) ? "ps/eats" : "";
		w->msSplashPS_Attack = r.p(0.7f) ? "ps/attack" : "";
		w->msSplashSound_Walk = r.p(0.7f) ? "snd/walk" : "";
		w->msSplashSound_Run = "snd/run";
		w->msSplashSound_Eat = r.p(0.7f) ? "snd/eat" : "";
		w->msSplashSound_Attack = "snd/attack";
	}
	e->SetupAfterLoad(NULL);
	e->mpPathfinder->mpNodeContainer = &nc;
	e->mpPathfinder->mpAStar = &as;
	e->mNormalAttackDamage.mfMinDamage = 30;
	e->mBreakDoorAttackDamage.mfMinDamage = 26;
}

static void Run(Case &c, FILE *out)
{
	gFocus = c.ent.find("waterlurker") == std::string::npos ? (c.idx / 6) % 6 : 0;
	Rng r;
	r.s = c.seed * 7919 + 13;
	srand((unsigned)c.seed);
	gLog.clear();
	cLuxPlayer pl;
	memset((void *)&pl.mLantern, 0, sizeof(pl.mLantern));
	pl.mbDead = false;
	pl.mMoveState = 0;
	pl.mfAvgSpeed = 0;
	pl.mfTerror = 0;
	pl.mFlash.mbOn = false;
	pl.mLight.mfLevel = 0.5f;
	pl.mNormal.mbCrouch = false;
	cLuxMap map;
	cWorld world;
	map.mpWorld = &world;
	map.mlUsedMask = r.i(0, 1000);
	cLuxMapHelper mh;
	mh.mlDensity = r.p(0.4f) ? r.i(1, 12) : r.i(15, 95);
	cLuxMusicHandler mus;
	cLuxDebugHandler dbg;
	cLuxGlobalDataHandler gdh;
	gdh.mbAllowed = r.p(0.7f);
	cLuxProgressLogHandler plog;
	cEngine eng;
	cLuxEffectRenderer effr;
	cGameCfg cfg;
	cLuxBase base;
	base.mpEngine = &eng;
	base.mpPlayer = &pl;
	base.mpMapHelper = &mh;
	base.mpMusicHandler = &mus;
	base.mpDebugHandler = &dbg;
	base.mpGlobalDataHandler = &gdh;
	base.mpProgressLogHandler = &plog;
	base.mpGameCfg = &cfg;
	base.mpEffectRenderer = &effr;
	pl.mfSanity = r.f(0, 100);
	gpBase = &base;

	cAINodeContainer nc;
	std::vector<cAINode> nodes(c.nodes.size());
	for(size_t i = 0; i < nodes.size(); ++i)
	{
		nodes[i].mvPos = c.nodes[i];
		nodes[i].mlID = (int)i + 100;
		nc.mvNodes.push_back(&nodes[i]);
	}
	cAStarHandler as;
	as.mpC = &nc;
	iCharacterBody body;
	cMeshEntity mesh;
	mesh.mbVisible = true;
	mesh.mBV.SetSize(cVector3f(1, 2, 1));
	cVector3f origin = c.nodes.empty() ? cVector3f(0) : c.nodes[0];
	body.mvPos = origin + cVector3f(0, 0.9f, 0);
	body.mvLast = body.mvPos;
	body.mBody.mBV.SetSize(cVector3f(0.8f, 1.8f, 0.8f));
	body.mBody.mBV.SetPosition(body.mvPos);
	body.mBody.mfMass = 70;
	pl.mBody.mvPos = body.mvPos + cVector3f(r.f(-25, 25), r.p(0.5f) ? 0.0f : r.f(-1.0f, 1.0f), r.f(-25, 25));
	pl.mBody.mvLast = pl.mBody.mvPos;
	pl.mBody.mvSize = cVector3f(0.5f, 1.8f, 0.5f);
	pl.mBody.mBody.mBV.SetSize(pl.mBody.mvSize);

	iLuxEnemy *e;
	bool lurker = c.ent.find("waterlurker") != std::string::npos;
	if(lurker) e = new(calloc(1, sizeof(cLuxEnemy_WaterLurker))) cLuxEnemy_WaterLurker("enemy", 5, &map);
	else e = new(calloc(1, sizeof(cLuxEnemy_ManPig))) cLuxEnemy_ManPig("enemy", 5, &map);
	Setup(c, r, pl, map, nc, as, body, mesh, e);
	map.mvEnemies.push_back(e);
	e->mvStartPosition = body.mvPos;
	for(int i = 0, n = r.i(0, 4); i < n && !nodes.empty(); ++i) e->AddPatrolNode(&nodes[r.i(0, (int)nodes.size() - 1)], r.f(0, 3), r.p(0.4f) ? "PatAnim" : "", r.p(0.5f));
	std::vector<iLuxProp *> doors;
	for(int i = 0; i < 2; ++i)
	{
		iLuxProp *d = new iLuxProp(900 + i);
		iPhysicsBody *b = new iPhysicsBody();
		b->mfMass = i == 0 ? 20.0f : 0.0f;
		b->mBV.SetSize(cVector3f(1.5f, 2, 1.5f));
		b->mvLocal = body.mvPos;
		d->mvBodies.push_back(b);
		d->mMain.mvWorld = body.mvPos + cVector3f(0.5f, 0, 0);
		d->mAttach.mvPos = body.mvPos + cVector3f(1, 0, 0);
		doors.push_back(d);
		map.mvAll.push_back(d);
	}
	std::vector<cLuxProp_Object *> foods;
	for(int i = 0; i < 3; ++i)
	{
		cLuxProp_Object *f = new cLuxProp_Object(950 + i);
		iPhysicsBody *b = new iPhysicsBody();
		b->mfMass = 5;
		b->mBV.SetSize(cVector3f(0.4f, 0.3f, 0.4f));
		b->mvLocal = body.mvPos + cVector3f(r.f(-3, 3), r.f(-0.5f, 0.5f), r.f(-3, 3));
		b->mBV.SetPosition(b->mvLocal);
		f->mvBodies.push_back(b);
		f->mbFood = r.p(0.8f);
		f->mfHealth = r.f(0, 20);
		foods.push_back(f);
		map.mvAll.push_back(f);
	}
	std::vector<iLuxEntity *> notDoors;
	Rec("CASE %d %s", c.idx, lurker ? "lurker" : "pig");
	int steps = r.i(300, 700);
	cVector3f pvel(0);
	float stuckFor = 0;
	for(int step = 0; step < steps; ++step)
	{
		float dt = r.p(0.05f) ? 0.001f : (1.0f / 60.0f);
		// world
		if(r.p(0.02f)) pl.mbDead = !pl.mbDead && r.p(0.3f);
		if(r.p(0.03f)) pl.mLantern.mbOn = !pl.mLantern.mbOn;
		if(r.p(0.03f)) pl.mNormal.mbCrouch = !pl.mNormal.mbCrouch;
		if(r.p(0.03f)) pl.mMoveState = r.p(0.8f) ? 0 : 1;
		if(r.p(0.05f)) pl.mLight.mfLevel = r.f(0, 1);
		if(r.p(0.05f)) pl.mfAvgSpeed = r.p(0.5f) ? 0.0f : r.f(0, 3);
		if(r.p(0.04f)) pl.mfTerror = r.p(0.5f) ? 1.0f : r.f(0, 1);
		if(r.p(0.02f)) pl.mFlash.mbOn = !pl.mFlash.mbOn;
		if(r.p(0.2f)) pvel = cVector3f(r.f(-3, 3), 0, r.f(-3, 3));
		pl.mBody.mvLast = pl.mBody.mvPos;
		pl.mBody.mvPos = pl.mBody.mvPos + pvel * dt;
		pl.mBody.mvVel = pvel;
		pl.mCam.mvPos = pl.mBody.mvPos + cVector3f(0, 0.8f, 0);
		pl.mBody.mBody.mBV.SetPosition(pl.mBody.mvPos);
		if(r.p(0.03f)) mh.mlDensity = r.p(0.4f) ? r.i(1, 12) : r.i(10, 100);
		if(r.p(0.05f)) { notDoors.clear(); map.mvEntities.clear(); if(r.p(0.85f)) for(size_t i = 0; i < doors.size(); ++i) map.mvEntities.push_back(doors[i]); if(r.p(0.8f)) for(size_t i = 0; i < foods.size(); ++i) map.mvEntities.push_back(foods[i]); }
		if(r.p(0.05f))
			for(size_t i = 0; i < foods.size(); ++i)
			{
				foods[i]->mvBodies[0]->mvLocal = body.mvPos + cVector3f(r.f(-4, 4), r.f(-0.8f, 0.8f), r.f(-4, 4));
				foods[i]->mvBodies[0]->mBV.SetPosition(foods[i]->mvBodies[0]->mvLocal);
				if(r.p(0.3f)) foods[i]->mfHealth = r.f(0, 20);
				if(r.p(0.1f)) foods[i]->mbFood = !foods[i]->mbFood;
			}
		if(r.p(0.01f)) pl.mBody.mvPos = body.mvPos + cVector3f(r.f(-3, 3), r.p(0.5f) ? 0.0f : r.f(-1.0f, 1.0f), r.f(-3, 3));
		if(!lurker && r.p(0.002f))
		{
			static const int hot[] = {5, 5, 18, 9, 9, 12, 13, 8, 6, 10, 11, 15, 2};
			e->ChangeState((eLuxEnemyState)hot[r.i(0, 12)]);
		}
		if(r.p(0.03f)) e->SendMessage(eLuxEnemyMessage_FoodInRange, 0, false, foods[r.i(0, 2)]->mvBodies[0]->mvLocal, 0);
		if(r.p(0.01f)) e->SendMessage(eLuxEnemyMessage_ChangePose, 0, false);
		if(r.p(0.04f)) { pl.mCam.mFrustum.mSalt = r.next(); pl.mBody.mfYaw = r.f(-3.2f, 3.2f); }
		pl.mCam.mFrustum.mvOrigin = pl.mCam.mvPos;
		if(r.p(0.02f)) map.msBroken.insert(900 + r.i(0, 1));
		if(r.p(0.03f)) map.mlUsedMask = r.i(0, 1000);
		if(r.p(0.02f)) e->mbBlind = !e->mbBlind;
		if(r.p(0.02f)) e->mbDisableTriggers = !e->mbDisableTriggers;
		if(r.p(0.004f)) e->mbDisabled = !e->mbDisabled;
		if(r.p(0.03f)) e->mbIsSeenByPlayer = !e->mbIsSeenByPlayer;
		if(r.p(0.03f)) e->mpCurrentAnimation && (e->mpCurrentAnimation->mbOver = true);
		if(e->mpCurrentAnimation)
		{
			bool ev = r.p(0.08f);
			e->mpCurrentAnimation->mfPrev = ev ? 0.4f : 0.6f;
			e->mpCurrentAnimation->mfTime = ev ? 0.6f : 0.7f;
		}
		for(size_t i = 0; i < doors.size(); ++i)
		{
			doors[i]->mvBodies[0]->mBV.SetPosition(body.mvPos);
			doors[i]->mvBodies[0]->mvLocal = body.mvPos + (r.p(0.7f) ? body.mvForward * 0.8f : cVector3f(r.f(-1, 1), 0, r.f(-1, 1)));
		}
		if(r.p(0.03f))
			e->SendMessage(eLuxEnemyMessage_SoundHeard, 0, false, cVector3f(r.f(-20, 20), 0, r.f(-20, 20)) + origin, r.f(0, 1));
		if(r.p(0.015f)) e->SendMessage(eLuxEnemyMessage_TakeHit, 0, false, cVector3f(0), r.f(0, 3), r.i(0, 3));
		if(r.p(0.005f)) e->SendMessage(eLuxEnemyMessage_PlayerDead, 0, false);
		if(r.p(0.004f)) e->SendMessage(eLuxEnemyMessage_Reset, 0, false);
		if(r.p(0.01f)) e->SendMessage(eLuxEnemyMessage_HelpMe, r.f(0, 0.05f), r.p(0.5f));
		if(r.p(0.01f)) e->SetActive(r.p(0.5f));
		if(r.p(0.002f))
		{
			static const int pigStates[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 18, 19, 20};
			static const int wlStates[] = {0, 1, 4, 6, 7, 9, 10, 12, 15, 16};
			int st = r.i(0, 21);
			int pick = r.i(0, 17);
			if(r.p(0.9f)) st = lurker ? wlStates[pick % 10] : pigStates[pick];
			e->ChangeState((eLuxEnemyState)st);
		}
		if(gFocus >= 1 && gFocus <= 4 && r.p(0.03f) && !nodes.empty()) pl.mBody.mvPos = nodes[r.i(0, (int)nodes.size() - 1)].mvPos + cVector3f(r.f(-8, 8), 0.9f, r.f(-8, 8));
		if(gFocus >= 1 && gFocus <= 4 && r.p(0.02f))
		{
			eLuxEnemyState tgt = gFocus == 1 ? eLuxEnemyState_Track : (gFocus == 2 ? eLuxEnemyState_Stalk : eLuxEnemyState_Alert);
			if(e->mCurrentState != tgt && !(gFocus >= 3 && (e->mCurrentState == eLuxEnemyState_Flee || e->mCurrentState == eLuxEnemyState_Hunt))) e->ChangeState(tgt);
		}
		if(!lurker && r.p(0.002f)) e->ChangeState(r.p(0.5f) ? eLuxEnemyState_Track : eLuxEnemyState_Stalk);
		if(r.p(0.01f)) e->ClearPatrolNodes();
		if(r.p(0.01f) && !nodes.empty()) e->AddPatrolNode(&nodes[r.i(0, (int)nodes.size() - 1)], r.f(0, 3), r.p(0.4f) ? "PatAnim" : "", r.p(0.5f));
		if(r.p(0.01f)) e->mfHealth = r.f(-5, 100);
		if(r.p(0.01f)) { e->mbInWater = false; e->mCurrentPose = (r.p(0.9f) ? eLuxEnemyPoseType_Biped : eLuxEnemyPoseType_Quadruped); }
		// direct calls
		if(r.p(0.04f))
		{
			cVector3f p = origin + cVector3f(r.f(-20, 20), 0, r.f(-20, 20));
			cAINode *n = e->mpPathfinder->GetNodeAtPos(p, r.f(0, 3), r.f(3, 15), r.p(0.5f), r.p(0.5f), r.p(0.5f), r.p(0.3f) && !nodes.empty() ? &nodes[0] : NULL, r.p(0.5f) ? -1 : r.i(1, 3), 2, r.p(0.8f));
			Rec("GNP %d", n ? n->mlID : -1);
		}
		if(r.p(0.03f))
		{
			cAINode *n = e->GetSearchForPlayerNode(r.i(1, 6), r.f(0.5f, 1.5f), r.f(0.5f, 1.5f), r.f(0.5f, 1.5f));
			Rec("GSN %d", n ? n->mlID : -1);
		}
		if(r.p(0.03f))
		{
			cAINode *n = e->GetPatrolAroundPlayerNode(r.i(1, 6), r.f(0.5f, 1.5f), r.f(0.5f, 1.5f), r.f(0.5f, 1.5f));
			Rec("GPN %d", n ? n->mlID : -1);
		}
		if(r.p(0.04f))
		{
			cVector3f p = pl.mBody.mvPos + cVector3f(r.f(-3, 3), 0, r.f(-3, 3));
			cVector3f sz(r.f(0.3f, 1), r.f(0.5f, 2), r.f(0.3f, 1));
			bool fov = r.p(0.5f);
			bool src = r.p(0.3f);
			bool v = src ? e->LineOfSight(p, sz, fov, pl.mCam.mvPos) : e->LineOfSight(p, sz, fov);
			Rec("LOS %d %d", fov, v);
			Rec("FOV %d", e->InFOV(p));
			Rec("PIF %d", e->PlayerInFOV());
			FB("D2P", e->DistToPlayer());
			FB("D2P2", e->DistToPlayer2D());
			FB("PMT", e->GetPlayerMovementTowardEnemyAmount());
			FB("MS", e->mpMover->GetMoveSpeed());
			FB("WSA", e->mpMover->GetWantedSpeedAmount());
			FB("NGP", (float)HV(e->mpPathfinder->GetNextGoalPos()));
		}
		if(r.p(0.02f))
		{
			bool ok = e->mpPathfinder->MoveTo(origin + cVector3f(r.f(-20, 20), 0, r.f(-20, 20)));
			Rec("MoveTo %d", ok);
		}
		if(r.p(0.01f)) e->mpPathfinder->Stop();
		if(r.p(0.01f) && e->GetPatrolNodeNum() > 0) { e->IncCurrentPatrolNode(r.p(0.5f)); Rec("ILP %d", e->IsAtLastPatrolNode()); }
		if(r.p(0.01f)) e->mpMover->TurnToAngle(r.f(-3, 3));
		if(r.p(0.01f)) e->ShowPlayerPosition();
		if(r.p(0.02f)) mesh.mbVisible = r.p(0.8f);
		if(r.p(0.04f)) pl.mfSanity = r.f(0, 100);
		if(r.p(0.02f)) { e->mfHealth = r.f(-5, 100); e->GiveDamage(r.f(0, 60), r.i(0, 4)); }
		if(r.p(0.02f)) e->AlertOfPlayerPresence();
		if(r.p(0.02f)) e->ChangeSoundState((eLuxEnemySoundState)r.i(0, 3));
		if(r.p(0.015f)) { Rec("ATK %d", e->Attack(e->mNormalAttackSize, r.p(0.5f) ? e->mNormalAttackDamage : e->mBreakDoorAttackDamage, r.f(0.5f, 2))); }
		if(r.p(0.015f)) { cMatrixf mt = e->GetDamageShapeMatrix(cVector3f(r.f(-2, 2), r.f(-2, 2), r.f(-2, 2))); Rec("DSM %016llx", (unsigned long long)Mix(Mix(Bits(mt.m[0][0]), Bits(mt.m[2][1])), Mix(Bits(mt.m[0][3]), Mix(Bits(mt.m[1][3]), Bits(mt.m[2][3]))))); }
		if(r.p(0.03f))
		{
			cLuxProp_Object *fd = e->GetClosestFood(r.f(0.5f, 6), r.f(0.1f, 2));
			Rec("FOOD %d", fd ? fd->mlID : -1);
			Rec("OSR %d", e->OutsideStartRadius());
			Rec("ISP %d", e->IsSeenByPlayer());
			cVector3f ff = pl.mBody.mvPos + cVector3f(r.f(-3, 3), 0, r.f(-3, 3));
			Rec("IFF %d %d", e->IsInPlayerFovAtFeetPos(ff), e->IsVisibleToPlayerAtFeetPos(ff));
			FB("D2D", e->Dist2D(ff));
			FB("DTC", e->DistToChar(&pl.mBody));
			FB("DTC2", e->DistToChar2D(&pl.mBody));
			FB("AHC", e->AbsHeightDistToChar(&pl.mBody));
			FB("AHP", e->AbsHeightDistToPlayer());
			FB("CAT", e->ConvertAnimToAbsoluteTime(r.f(0, 1)));
			cVector3f dd = e->GetDirection2D(ff);
			Rec("GD2 %016llx", (unsigned long long)HV(dd));
		}
		if(r.p(0.03f)) e->PlayAnim("Anim" + cString::ToString(r.i(0, 3)), r.p(0.5f), r.f(0, 1), r.p(0.5f), r.f(0.5f, 2), r.p(0.5f), r.p(0.5f), r.p(0.5f));
		if(r.p(0.003f)) { e->ResetProperties(); Rec("RESET"); }
		if(r.p(0.01f)) { Rec("Z"); e->FadeOutCurrentAnim(0.2f); }
		// update
		e->OnUpdate(dt);
		// integrate the body
		body.mvLast = body.mvPos;
		if(stuckFor <= 0 && r.p(0.02f)) stuckFor = r.f(0.5f, 4);
		stuckFor -= dt;
		bool moved = body.mfMaxPos > 0 && body.mbMove;
		if(moved)
			body.mfWanted = std::min(body.mfWanted + body.mfAcc * dt, body.mfMaxPos);
		else
			body.mfWanted = std::max(body.mfWanted - body.mfDeacc * dt, 0.0f);
		float yaw = body.mfYaw;
		body.mvForward = cMath::MatrixMul(cMath::MatrixRotateY(yaw), cVector3f(0, 0, -1));
		cVector3f nextPos = body.mvPos;
		if(stuckFor <= 0) nextPos = body.mvPos + body.mvForward * (body.mfWanted * dt);
		body.mvVel = (nextPos - body.mvPos) * (1.0f / dt);
		body.mvPos = nextPos;
		body.mBody.mBV.SetPosition(body.mvPos);
		body.mbMove = false;
		body.mfMoveDelay = r.p(0.2f) ? 0.0f : r.f(0, 0.5f);
		Dump(e, "step");
		if(r.p(0.002f)) { QClear(e); Rec("QClear"); }
	}
	fputs(gLog.c_str(), out);
	fprintf(out, "ENDCASE %d\n", c.idx);
}

static std::vector<Case> Load(const char *path)
{
	std::vector<Case> cs;
	std::ifstream in(path);
	std::string line;
	Case cur;
	bool open = false;
	while(std::getline(in, line))
	{
		std::istringstream ss(line);
		std::string tag;
		ss >> tag;
		if(tag == "CASE")
		{
			cur = Case();
			ss >> cur.seed >> cur.idx >> cur.ent >> cur.map;
			open = true;
		}
		else if(tag == "V" && open)
		{
			std::string k, rest;
			ss >> k;
			std::getline(ss, rest);
			size_t b = rest.find_first_not_of(' ');
			cur.vars[k] = b == std::string::npos ? "" : rest.substr(b);
		}
		else if(tag == "N" && open)
		{
			float x, y, z;
			ss >> x >> y >> z;
			cur.nodes.push_back(cVector3f(x, y, z));
		}
		else if(tag == "END" && open)
		{
			cs.push_back(cur);
			open = false;
		}
	}
	return cs;
}

int main(int argc, char **argv)
{
#ifdef RECORD
	gRecF = fopen(getenv("A4_REC"), "wb");
#endif
	std::vector<Case> cs = Load(argv[1]);
	int lo = argc > 2 ? atoi(argv[2]) : 0, hi = argc > 3 ? atoi(argv[3]) : (int)cs.size();
	for(int i = lo; i < hi && i < (int)cs.size(); ++i) Run(cs[i], stdout);
#ifdef RECORD
	if(gRecF) fclose(gRecF);
	fprintf(stderr, "recorded %llu entries\n", (unsigned long long)gRecTotal);
#endif
	return 0;
}
