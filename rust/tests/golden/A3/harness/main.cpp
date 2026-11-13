// Differential driver: random player/move state sequences against fakes; prints one hash line per step.
#include "prelude.h"
#include "system/SerializeClass.h"
#include "corpus.inc"
#if defined(NEWSIDE)
#include "new.inc"
#else
#include "orig.inc"
#endif

std::string gLog;
cLuxBase *gpBase;
static bool gVerbose;
static int gVerboseCase = -1, gVerboseStep = -1;

void Rec(const char *fmt, ...)
{
	char b[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	gLog += b;
	gLog += '\n';
}
static bool gNanNorm = getenv("NANNORM") != NULL;
uint32_t Bits(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	if(gNanNorm && std::isnan(f)) return 0x7fc00000u;
	return u;
}
uint32_t HashV(const cVector3f &v, uint32_t salt)
{
	uint32_t h = 2166136261u ^ salt;
	for(float f : {v.x, v.y, v.z}) { h ^= Bits(f); h *= 16777619u; }
	return h;
}
float WorldRand(uint32_t h) { h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return (h & 0xffff) / 65536.0f; }

float cLuxGameCfg::GetFloat(const char *a, const char *k, float def)
{
	uint32_t h = 2166136261u;
	for(const char *p = a; *p; ++p) { h ^= (uint8_t)*p; h *= 16777619u; }
	for(const char *p = k; *p; ++p) { h ^= (uint8_t)*p; h *= 16777619u; }
	float r = WorldRand(h);
	if(strstr(k, "Speed") || strstr(k, "Mul") || strstr(k, "Force") || strstr(k, "Mass") || strstr(k, "Damage") || strstr(k, "Range") || strstr(k, "Dist")
		|| strstr(k, "Acc") || strstr(k, "Size") || strstr(k, "Length") || strstr(k, "Count"))
		return 0.2f + r * 6.0f;
	if(strstr(k, "PerSecond")) return 1.0f + r * 30.0f;
	return 0.2f + r * 3.0f;
}

tString cLuxPlayerStateVars::msHandObject;
cVector3f cLuxPlayerStateVars::mvInteractPos;
iPhysicsBody *cLuxPlayerStateVars::mpInteractBody;
iLuxProp *cLuxPlayerStateVars::mpInteractProp;
cLuxInventory_Item *cLuxPlayerStateVars::mpInventoryItem;
cLuxArea_Ladder *cLuxPlayerStateVars::mpLadder;
cVector3f cLuxPlayerStateVars::mvLadderStartPos;

// ---- random
static uint64_t gRng = 88172645463325252ull;
static uint64_t R()
{
	gRng ^= gRng << 13;
	gRng ^= gRng >> 7;
	gRng ^= gRng << 17;
	return gRng;
}
static int RI(int n) { return (int)(R() % (uint64_t)n); }
static bool RB(int pct = 50) { return RI(100) < pct; }
static float RU(float a, float b) { return a + (b - a) * (float)((R() >> 11) * (1.0 / 9007199254740992.0)); }
static float RF()
{
	switch(RI(24))
	{
	case 0: return 0.0f;
	case 1: return -0.0f;
	case 2: return 1e6f * RU(-1, 1);
	case 3: return std::numeric_limits<float>::quiet_NaN();
	case 4: return RU(-1e-5f, 1e-5f);
	case 5: return std::numeric_limits<float>::infinity() * (RB() ? 1 : -1);
	case 6: return 1.0f;
	default: return RU(-4, 4);
	}
}
static float RFin() { return RB(90) ? RU(-4, 4) : RF(); }
static float RPos(float lo, float hi) { return RB(92) ? RU(lo, hi) : RF(); }
static float RDt()
{
	switch(RI(12))
	{
	case 0: return 0.0f;
	case 1: return 1e9f;
	case 2: return std::numeric_limits<float>::quiet_NaN();
	case 3: return -RU(0, 0.1f);
	case 4: return RU(0.5f, 20.0f);
	case 5: return 1e-9f;
	default: return RU(0.0005f, 0.05f);
	}
}
static cVector3f RV3(float s = 4) { return cVector3f(RU(-s, s), RU(-s, s), RU(-s, s)); }
static cVector3f RV3x() { return RB(90) ? RV3() : cVector3f(RF(), RF(), RF()); }
static cVector3f RDir()
{
	cVector3f v = RV3(1);
	if(RB(5)) return cVector3f(0, 0, 0);
	v.Normalize();
	return RB(95) ? v : cVector3f(RF(), RF(), RF());
}
static cMatrixf RMtx(bool trans = true)
{
	cMatrixf m = cMath::MatrixRotate(RV3(3.2f), eEulerRotationOrder_XYZ);
	if(RB(10)) m = cMath::MatrixMul(cMath::MatrixScale(cVector3f(RU(0.2f, 2), RU(0.2f, 2), RU(0.2f, 2))), m);
	if(trans) m.SetTranslation(RV3(5));
	if(RB(3)) m.m[RI(3)][RI(4)] = RF();
	return m;
}
static cMatrixf RInertia()
{
	cMatrixf m = cMatrixf::Identity;
	m.m[0][0] = RU(0.1f, 3); m.m[1][1] = RU(0.1f, 3); m.m[2][2] = RU(0.1f, 3);
	if(RB(5)) m.m[RI(3)][RI(3)] = RF();
	return m;
}

static void DF(const char *n, float f) { Rec("D %s %08x", n, Bits(f)); }
static void DI(const char *n, int f) { Rec("D %s %d", n, f); }
static void DP(const char *n, const PidT &p) { uint64_t h = 1469598103934665603ull; for(size_t i = 0; i < sizeof(PidT); ++i) { h ^= ((const unsigned char *)&p)[i]; h *= 1099511628211ull; } Rec("P %s %llx", n, (unsigned long long)h); }

// ---- world
static cEngine gEngine;
static cLuxGameCfg gCfg;
static cLuxHelpFuncs gHelp;
static cLuxMapHandler gMapH;
static cLuxMessageHandler gMsg;
static cLuxEffectRenderer gFx;
static cLuxMapHelper gHelper;
static cLuxInputHandler gInput;
static cLuxPlayer gPlayer;
static cGuiSet gHud;
static cLuxBase gBase;
static iPhysicsBody gBodies[5];
static iPhysicsJoint gJoints[5];
static cLuxArea_Ladder gLadder;
static iLuxProp gProps[3];
static cLuxInventory_Item gItem;
static iLuxMoveState *gMN = NULL;
static iLuxMoveState *gML = NULL;

static void SetupWorld()
{
	gBase = cLuxBase();
	gBase.mpEngine = &gEngine; gBase.mpGameCfg = &gCfg; gBase.mpHelpFuncs = &gHelp; gBase.mpMapHandler = &gMapH; gBase.mpMessageHandler = &gMsg;
	gBase.mpEffectRenderer = &gFx; gBase.mpMapHelper = &gHelper; gBase.mpInputHandler = &gInput; gBase.mpPlayer = &gPlayer; gBase.mpGameHudSet = &gHud;
	gBase.mbHardMode = RB();
	gpBase = &gBase;
	gEngine.mGui.mlN = 0;
	gEngine.mInput.mbTrig = RB();
	gInput.mbSmooth = RB(); gInput.mbInvert = RB(); gInput.mvSmooth = cVector2f(RFin(), RFin());
	gMapH.mMap.mWorld.mvGravity = RB(90) ? cVector3f(0, -9.8f, 0) : RV3x();
	gMapH.mMap.mbHasCb = RB(); gMapH.mMap.mCb.mbAutoDestroy = RB();
	gMapH.mMap.mCb.msName = "n"; gMapH.mMap.mCb.msFunction = "f"; gMapH.mMap.mCb.msItem = "i"; gMapH.mMap.mCb.msEntity = "e";

	gPlayer = cLuxPlayer();
	cCamera &c = gPlayer.mCam;
	c.mfPitch = RFin(); c.mfYaw = RU(-7, 7); c.mfRoll = RB(80) ? 0 : RFin();
	c.mvPos = RV3(); c.mvFwd = RDir(); c.mvUp = RDir(); c.mvRight = RDir();
	c.mfPMin = RU(-2, 0); c.mfPMax = RU(0, 2);
	iCharacterBody &b = gPlayer.mBody;
	b.mvPos = RV3(5); b.mvFeet = b.mvPos - cVector3f(0, 0.9f, 0); b.mvSize = cVector3f(RPos(0.3f, 1), RPos(1, 2), RPos(0.3f, 1));
	b.mvForward = RDir(); b.mvRight = RDir(); b.mvVel = RV3x(); b.mvForceVel = RB(70) ? cVector3f(0, RU(-12, 3), 0) : RV3x();
	b.mvCustomGravity = RV3x(); b.mbOnGround = RB(60); b.mbMoved = RB(60); b.mbCustomGravityActive = RB(20); b.mfMass = RPos(40, 90);
	gPlayer.mCollapse.mbActive = RB(15); gPlayer.mLantern.mbActive = RB();
	gPlayer.mMoveState = RB(80) ? eLuxMoveState_Normal : eLuxMoveState_ClimbLedge;
	gPlayer.mfInteractMul = RPos(0.1f, 1); gPlayer.mfScriptMul = RPos(0.5f, 1.2f); gPlayer.mfEventMul = RPos(0.5f, 1.2f);
	gPlayer.mfHurtMul = RPos(0.3f, 1); gPlayer.mfCollapseMul = RPos(0.3f, 1); gPlayer.mfWaterMul = RPos(0.3f, 1);
	gPlayer.mfScriptRun = RPos(0, 1.5f); gPlayer.mfEventRun = RPos(0, 1.5f); gPlayer.mfJumpMul = RPos(0.5f, 1.5f); gPlayer.mfDefaultMass = RPos(40, 90);
	gPlayer.mvBodySize = cVector3f(0.6f, RPos(1.5f, 2), 0.6f); gPlayer.mvCrouchSize = cVector3f(0.6f, RPos(0.8f, 1.4f), 0.6f);
	gPlayer.mvHeadAdd = RB(70) ? RV3(0.5f) : RV3x();
	gPlayer.mbCrouchDisabled = RB(15); gPlayer.mbInWater = RB(15); gPlayer.mbPressRun = RB(); gPlayer.mbPressedMove = RB(60);
	gPlayer.mbNoFall = RB(15); gPlayer.mbDrag = RB(15); gPlayer.mIcon = RB() ? eLuxFocusIconStyle_Default : eLuxFocusIconStyle_Simple;
	gPlayer.mpMoveNormal = gMN; gPlayer.mpMoveLedge = gML;

	for(int i = 0; i < 5; ++i)
	{
		iPhysicsBody &p = gBodies[i];
		p = iPhysicsBody();
		p.mlId = i + 1; p.mMtx = RMtx(); p.mvVel = RV3x(); p.mvAng = RV3x(); p.mvCentre = RV3(0.5f);
		p.mfMass = RB(95) ? RU(0.05f, 200) : RF(); p.mInertia = RInertia(); p.mbGravity = RB(); p.mbCollideChar = RB();
		p.mJoints.clear();
	}
	for(int i = 0; i < 5; ++i)
	{
		iPhysicsJoint &j = gJoints[i];
		j.mvPin = RDir(); j.mvPivot = RV3(5);
		j.mpChild = RB(80) ? &gBodies[RI(5)] : NULL;
		j.mpParent = RB(80) ? &gBodies[RI(5)] : NULL;
	}
	for(int i = 0; i < 5; ++i) { int n = RI(3); for(int k = 0; k < n; ++k) gBodies[i].mJoints.push_back(&gJoints[RI(5)]); }
	for(int i = 0; i < 3; ++i)
	{
		iLuxProp &p = gProps[i];
		p = iLuxProp();
		p.mType = RB(85) ? eLuxEntityType_Prop : eLuxEntityType_Other;
		p.mPropType = RB(50) ? eLuxPropType_Item : eLuxPropType_Other;
		p.mfMaxFocus = RPos(0.5f, 6); p.mbCanInteract = RB(70); p.mbDisabled = RB(15);
		p.mCrosshair = (eLuxFocusCrosshair)RI(eLuxFocusCrosshair_LastEnum + 1);
		p.mbConnected = RB();
		int ns = RI(4);
		p.mMesh.mSubs.clear();
		for(int k = 0; k < ns; ++k) { cSubMeshEntity s; s.mpParent = RB(85) ? &gBodies[RI(5)] : NULL; p.mMesh.mSubs.push_back(s); }
		p.mBodies.clear();
		for(int k = 0; k < RI(3); ++k) p.mBodies.push_back(&gBodies[RI(5)]);
		cLuxInteractData_Grab &g = p.mGrab;
		if(RB(70))
		{
			const float *row = kCorpus_grab[RI(kCorpus_grab_n)];
			g.mfGrabDepth = row[0]; g.mfGrabDepthInc = row[1]; g.mfGrabMinDepth = row[2]; g.mfGrabMaxDepth = row[3];
			g.mfGrabThrowImpulse = row[4]; g.mfGrabMassMul = row[5]; g.mfForceMul = row[6]; g.mfTorqueMul = row[7];
		}
		else
		{
			g.mfGrabDepth = RFin(); g.mfGrabDepthInc = RFin(); g.mfGrabMinDepth = RFin(); g.mfGrabMaxDepth = RFin();
			g.mfGrabThrowImpulse = RFin(); g.mfGrabMassMul = RFin(); g.mfForceMul = RFin(); g.mfTorqueMul = RFin();
		}
		g.mbGrabUseDepth = RB(); g.mbGrabUseOffset = RB(); g.mbUseRotation = RB(70);
		g.mvGrabPositionOffset = RV3(); g.mvGrabRotationOffset = RV3(3.2f);
		if(RB(70)) { const float *row = kCorpus_push[RI(kCorpus_push_n)]; p.mPush.mfPushForceMul = row[0]; p.mPush.mfPushImpulse = row[1]; }
		else { p.mPush.mfPushForceMul = RFin(); p.mPush.mfPushImpulse = RFin(); }
		p.mPush.mbPushAtPoint = RB();
		if(RB(70)) { const float *row = kCorpus_slide[RI(kCorpus_slide_n)]; p.mSlide.mfSlideMaxSpeed = row[0]; p.mSlide.mfSlideSlowDownFactor = row[1]; p.mSlide.mfSlideSpeedFactor = row[2]; p.mSlide.mfSlideThrowImpulse = row[3]; }
		else { p.mSlide.mfSlideMaxSpeed = RFin(); p.mSlide.mfSlideSlowDownFactor = RFin(); p.mSlide.mfSlideSpeedFactor = RFin(); p.mSlide.mfSlideThrowImpulse = RFin(); }
		if(RB(70)) { const float *row = kCorpus_move[RI(kCorpus_move_n)]; p.mMove.mfMoveMaxSpeed = row[0]; p.mMove.mfMoveSlowDownFactor = row[1]; p.mMove.mfMoveSpeedFactor = row[2]; p.mMove.mfMoveThrowImpulse = row[3]; }
		else { p.mMove.mfMoveMaxSpeed = RFin(); p.mMove.mfMoveSlowDownFactor = RFin(); p.mMove.mfMoveSpeedFactor = RFin(); p.mMove.mfMoveThrowImpulse = RFin(); }
	}
	for(int i = 0; i < 5; ++i) gBodies[i].mpUser = (iLuxEntity *)&gProps[RI(3)];
	gLadder.mfMinY = RU(-2, 2); gLadder.mfMaxY = gLadder.mfMinY + RU(0, 6); if(RB(5)) gLadder.mfMaxY = RF();
	gLadder.mvFwd = RDir(); gLadder.mvStartRot = RV3(3.2f); gLadder.msMat = "wood";
	gHelper.mpBody = RB(85) ? &gBodies[RI(5)] : NULL; gHelper.mpEntity = RB(85) ? (iLuxEntity *)&gProps[RI(3)] : NULL; gHelper.mfDist = RPos(0.2f, 8);
	if(gHelper.mpEntity) gHelper.mpBody = &gBodies[RI(5)]; else gHelper.mpBody = RB(20) ? gHelper.mpBody : NULL;
	gItem = cLuxInventory_Item();
}

static uint64_t Fnv(const std::string &s)
{
	uint64_t h = 1469598103934665603ull;
	for(unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
	return h;
}
static int gCase;
static int gStep;
static uint64_t gTotal;
static long gSteps;
static void Step()
{
	uint64_t h = Fnv(gLog);
	gTotal = gTotal * 1000003ull ^ h;
	++gSteps;
	printf("%d %d %016llx\n", gCase, gStep, (unsigned long long)h);
	if(gVerbose && gCase == gVerboseCase && gStep == gVerboseStep) fputs(gLog.c_str(), stderr);
	gLog.clear();
	++gStep;
}

static void SetupPlayerStateCommon(iLuxPlayerState *s) { s->mPreviousState = (eLuxPlayerState)RI(eLuxPlayerState_LastEnum); }
static eLuxPlayerAction RAction() { return (eLuxPlayerAction)RI(5); }

static void RandomizeChar()
{
	iCharacterBody &b = gPlayer.mBody;
	b.mvVel = RB(80) ? RV3(6) : RV3x(); b.mvForceVel = RB(70) ? cVector3f(0, RU(-14, 3), 0) : RV3x();
	b.mbOnGround = RB(60); b.mbMoved = RB(70); b.mbCustomGravityActive = RB(20); b.mvCustomGravity = RV3x();
	gPlayer.mbPressRun = RB(); gPlayer.mbPressedMove = RB(70); gPlayer.mbInWater = RB(15); gPlayer.mbCrouchDisabled = RB(10);
	gPlayer.mCollapse.mbActive = RB(10); gPlayer.mbNoFall = RB(10); gPlayer.mbDrag = RB(10); gPlayer.mfJumpMul = RPos(0.5f, 1.5f);
	gPlayer.mfInteractMul = RPos(0.1f, 1); gPlayer.mfScriptRun = RPos(0, 1.5f);
	gPlayer.mvHeadAdd = RB(70) ? RV3(0.5f) : RV3x();
}

// ---- interact states
static void SetupInteract(iLuxProp **prop, iPhysicsBody **body)
{
	*prop = &gProps[RI(3)];
	(*prop)->mType = eLuxEntityType_Prop;
	*body = &gBodies[RI(5)];
	cLuxPlayerStateVars::mpInteractProp = *prop;
	cLuxPlayerStateVars::mpInteractBody = *body;
	cLuxPlayerStateVars::mvInteractPos = RB(80) ? RV3(5) : RV3x();
	if(RB(50)) cLuxPlayerStateVars::mvInteractPos = (*body)->GetLocalPosition() + RV3(0.5f);
}

static void Jitter()
{
	gPlayer.mCam.mfPitch += RU(-0.1f, 0.1f); gPlayer.mCam.mfYaw += RU(-0.1f, 0.1f);
	gPlayer.mCam.mvPos = gPlayer.mCam.mvPos + RV3(0.2f);
	gPlayer.mBody.mvFeet = gPlayer.mBody.mvFeet + RV3(0.2f);
	for(int i = 0; i < 5; ++i) { gBodies[i].mMtx.SetTranslation(gBodies[i].mMtx.GetTranslation() + RV3(0.3f)); gBodies[i].mvVel = RB(90) ? RV3(2) : RV3x(); gBodies[i].mvAng = RV3(2); }
	if(RB(10)) { gPlayer.mCam.mvPos = RV3(8); }
	if(RB(5)) { gPlayer.mCam.mvFwd = RDir(); gPlayer.mCam.mvUp = RDir(); gPlayer.mCam.mvRight = RDir(); }
}

static void Misc(iLuxPlayerState_Interact *s)
{
	switch(RI(3))
	{
	case 0: s->OnDestroyEntity((iLuxEntity *)&gProps[RI(3)]); break;
	case 1: s->OnAttachBodyToStickyArea(&gBodies[RI(5)]); break;
	case 2: s->OnDestroyEntity(RB(50) ? (iLuxEntity *)s->mpCurrentProp : (iLuxEntity *)&gProps[RI(3)]); break;
	}
}

static void DumpGrab(cLuxPlayerState_InteractGrab &g)
{
	DP("fp", g.mForcePid); DP("tp", g.mSpeedTorquePid); RecM("rot", g.m_mtxBodyRotation); RecV("off", g.mvLocalBodyOffset); DF("depth", g.mfDepth); DF("maxDist", g.mfMaxDistance); DF("massSum", g.mfMassSum);
	if(RB(15)) Rec("dbg %08x", Bits(g.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunGrab()
{
	SetupWorld();
	iLuxProp *prop; iPhysicsBody *body;
	SetupInteract(&prop, &body);
	cLuxPlayerState_InteractGrab *s = new cLuxPlayerState_InteractGrab(&gPlayer);
	SetupPlayerStateCommon(s);
	if(RB(40)) { s->mfMaxForce = RPos(50, 3000); s->mfMaxTorque = RPos(5, 600); s->mfMaxAngularSpeed = RPos(1, 20); }
	if(RB(60)) { s->mfMinSlowPlayerMass = RU(0, 120); s->mfMaxSlowPlayerMass = s->mfMinSlowPlayerMass + RU(1, 150); s->mfMinSlowPlayerMul = RU(0.1f, 1); }
	Rec("ctor"); DF("f", s->mfMaxForce); Step();
	s->OnEnterState(eLuxPlayerState_Normal);
	DumpGrab(*s); Step();
	for(int i = 0, n = 8 + RI(30); i < n; ++i)
	{
		Jitter();
		float dt = RDt();
		switch(RI(9))
		{
		case 0: case 1: case 2: s->PostUpdate(dt); break;
		case 3: Rec("ret %d", s->OnDoAction(RAction(), RB())); break;
		case 4: s->OnScroll(RFin()); break;
		case 5: gEngine.mInput.mbTrig = RB(80); gInput.mbInvert = RB(); Rec("ret %d", s->OnAddYaw(RFin())); break;
		case 6: gEngine.mInput.mbTrig = RB(80); gInput.mbInvert = RB(); Rec("ret %d", s->OnAddPitch(RFin())); break;
		case 7: s->PostUpdate(dt); s->PostUpdate(dt); break;
		case 8: if(RB(30)) { s->OnLeaveState(eLuxPlayerState_Normal); s->OnEnterState(eLuxPlayerState_Normal); } break;
		}
		if(RB(10)) Misc(s);
		if(RB(15)) { float m = RFin(); bool c = RB(); s->OnSaveBody(&gBodies[RI(5)], m, c); DF("sbm", m); DI("sbc", c); Rec("buoy %d", s->AllowBuoyancy(&gBodies[RI(5)])); }
		DumpGrab(*s);
		Step();
	}
	s->OnLeaveState(eLuxPlayerState_Normal);
	Step();
	delete s;
}

static void DumpPush(cLuxPlayerState_InteractPush &p)
{
	DP("vp", p.mVelocityPid); DP("sp", p.mStopVelocityPid); RecV("dir", p.mvPushDir); RecV("attach", p.mvLocalAttachPos); DF("maxAttach", p.mfMaxAttachPointDist);
	DF("m0", p.mfTotalMoveMul[0]); DF("m1", p.mfTotalMoveMul[1]); DI("mv0", p.mbMoving[0]); DI("mv1", p.mbMoving[1]);
	DF("pmin", p.mfPrevMinPitchLimit); DF("pmax", p.mfPrevMaxPitchLimit);
	if(RB(15)) Rec("dbg %08x", Bits(p.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunPush()
{
	SetupWorld();
	iLuxProp *prop; iPhysicsBody *body;
	SetupInteract(&prop, &body);
	cLuxPlayerState_InteractPush *s = new cLuxPlayerState_InteractPush(&gPlayer);
	SetupPlayerStateCommon(s);
	if(RB(40)) { s->mfMaxForce = RPos(10, 2000); }
	gMN = new cLuxMoveState_Normal(&gPlayer); gPlayer.mpMoveNormal = gMN;
	static_cast<cLuxMoveState_Normal *>(gMN)->mbRunning = RB(); static_cast<cLuxMoveState_Normal *>(gMN)->mbCrouching = RB();
	Rec("ctor"); Step();
	s->OnEnterState(eLuxPlayerState_Normal);
	DumpPush(*s); Step();
	for(int i = 0, n = 8 + RI(30); i < n; ++i)
	{
		Jitter();
		float dt = RDt();
		if(RB(60)) { int d = RI(2); s->OnMove((eCharDir)d, RB(85) ? RU(-1, 1) : RF()); }
		if(RB(20)) { int d = RI(2); s->OnMove((eCharDir)d, RU(0, 1)); }
		gPlayer.mMoveState = RB(80) ? eLuxMoveState_Normal : eLuxMoveState_ClimbLedge;
		static_cast<cLuxMoveState_Normal *>(gMN)->mbRunning = RB(); static_cast<cLuxMoveState_Normal *>(gMN)->mbCrouching = RB();
		switch(RI(6))
		{
		case 0: case 1: case 2: s->Update(dt); break;
		case 3: Rec("ret %d", s->OnDoAction(RAction(), RB())); break;
		case 4: s->PostUpdate(dt); break;
		case 5: s->Update(dt); s->Update(dt); break;
		}
		if(RB(8)) Misc(s);
		DumpPush(*s);
		Step();
	}
	s->OnLeaveState(eLuxPlayerState_Normal);
	Step();
	delete s;
	delete gMN; gMN = NULL;
}

static void DumpSlide(cLuxPlayerState_InteractSlide &s)
{
	DP("fp", s.mForcePid); DF("speed", s.mfSlideSpeed); RecV2("mouse", s.mvMouseAdd); RecV2("lastMouse", s.mvLastMouseAdd); RecV("lastForce", s.mvLastForce); DF("maxDist", s.mfMaxDistance);
	RecV("localInteract", s.mvLocalInteractPos);
	if(RB(15)) Rec("dbg %08x", Bits(s.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunSlide()
{
	SetupWorld();
	iLuxProp *prop; iPhysicsBody *body;
	SetupInteract(&prop, &body);
	if(body->mJoints.empty()) body->mJoints.push_back(&gJoints[RI(5)]);
	cLuxPlayerState_InteractSlide *s = new cLuxPlayerState_InteractSlide(&gPlayer);
	SetupPlayerStateCommon(s);
	if(RB(40)) s->mfMaxForce = RPos(10, 2000);
	Rec("ctor"); Step();
	s->OnEnterState(eLuxPlayerState_Normal);
	DumpSlide(*s); Step();
	for(int i = 0, n = 8 + RI(30); i < n; ++i)
	{
		Jitter();
		float dt = RDt();
		if(RB(70)) { gEngine.mInput.mbTrig = RB(); gInput.mbInvert = RB(); s->OnAddYaw(RFin()); s->OnAddPitch(RFin()); }
		if(RB(30)) s->OnMove((eCharDir)RI(2), RFin());
		if(gVerbose) { Rec("IN dt %08x mass %08x", Bits(dt), Bits(s->mpCurrentBody->mfMass)); RecV("IN pin", s->mpCurrentJoint->mvPin); RecV("IN vel", s->mpCurrentBody->mvVel); RecV("IN up", gPlayer.mCam.mvUp); RecV("IN right", gPlayer.mCam.mvRight); }
		switch(RI(5))
		{
		case 0: case 1: case 2: s->Update(dt); break;
		case 3: Rec("ret %d", s->OnDoAction(RAction(), RB())); break;
		case 4: s->Update(dt); s->Update(dt); break;
		}
		DumpSlide(*s);
		Step();
	}
	s->OnLeaveState(eLuxPlayerState_Normal);
	Step();
	delete s;
}

static void DumpRot(iLuxPlayerState_InteractRotateBase &s)
{
	DP("rp", s.mRotatePid); DF("rotSpeed", s.mfRotSpeed); RecV2("mouse", s.mvMouseAdd); RecV2("lastMouse", s.mvLastMouseAdd); DF("maxDist", s.mfMaxDistance); RecV("jointFwd", s.mvJointForward);
	if(RB(15)) Rec("dbg %08x", Bits(s.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunRotate(int kind)
{
	SetupWorld();
	iLuxProp *prop; iPhysicsBody *body;
	SetupInteract(&prop, &body);
	if(body->mJoints.empty()) body->mJoints.push_back(&gJoints[RI(5)]);
	iLuxPlayerState_InteractRotateBase *s;
	if(kind == 0) s = new cLuxPlayerState_InteractLever(&gPlayer);
	else if(kind == 1) s = new cLuxPlayerState_InteractWheel(&gPlayer);
	else s = new cLuxPlayerState_InteractSwingDoor(&gPlayer);
	SetupPlayerStateCommon(s);
	if(RB(40)) s->mfMaxTorque = RPos(10, 2000);
	Rec("ctor"); DF("torque", s->mfMaxTorque); Step();
	s->OnEnterState(eLuxPlayerState_Normal);
	DumpRot(*s); Step();
	for(int i = 0, n = 10 + RI(40); i < n; ++i)
	{
		Jitter();
		float dt = RDt();
		gInput.mbSmooth = RB(); gInput.mvSmooth = cVector2f(RB(80) ? RU(-0.05f, 0.05f) : RFin(), RB(80) ? RU(-0.05f, 0.05f) : RFin());
		if(RB(60)) { gEngine.mInput.mbTrig = RB(); gInput.mbInvert = RB(); s->OnAddYaw(RU(-0.1f, 0.1f)); s->OnAddPitch(RU(-0.1f, 0.1f)); }
		if(RB(15)) s->OnMove((eCharDir)RI(2), RFin());
		if(RB(10)) { s->mpCurrentJoint->mvPin = RDir(); }
		switch(RI(5))
		{
		case 0: case 1: case 2: s->Update(dt); break;
		case 3: Rec("ret %d", s->OnDoAction(RAction(), RB())); break;
		case 4: s->Update(dt); s->Update(dt); break;
		}
		if(RB(8)) Misc(s);
		DumpRot(*s);
		Step();
	}
	s->OnLeaveState(eLuxPlayerState_Normal);
	Step();
	delete s;
}

static void DumpLadder(cLuxPlayerState_Ladder &l)
{
	DI("state", l.mlState); DF("time", l.mfTimeCount); RecV("charPos", l.mvCharPosition); RecV("goalPos", l.mvGoalPos); RecV("goalRot", l.mvGoalRot);
	RecV("posAdd", l.mvPosAdd); RecV("rotAdd", l.mvRotAdd); DF("moveMul", l.mfMoveMul); DF("step", l.mfStepCount); DI("played", l.mbPlayedSound);
	DF("leave", l.mfLeaveAtTopCount); DI("lantern", l.mbLanternDrawn); DF("pmin", l.mfPitchMinLimit); DF("pmax", l.mfPitchMaxLimit);
	if(RB(15)) Rec("dbg %08x", Bits(l.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunLadder()
{
	SetupWorld();
	cLuxMoveState_Normal *mn = new cLuxMoveState_Normal(&gPlayer);
	gMN = mn; gPlayer.mpMoveNormal = mn;
	cLuxPlayerState_Ladder *l = new cLuxPlayerState_Ladder(&gPlayer);
	SetupPlayerStateCommon(l);
	cLuxPlayerStateVars::mpLadder = &gLadder;
	cLuxPlayerStateVars::mvLadderStartPos = RB(80) ? RV3(5) : RV3x();
	gPlayer.mBody.mvPos = RV3(5);
	Rec("ctor"); Step();
	l->OnEnterState(eLuxPlayerState_Normal);
	DumpLadder(*l); Step();
	for(int i = 0, n = 15 + RI(60); i < n; ++i)
	{
		RandomizeChar();
		gPlayer.mBody.mvPos = gPlayer.mBody.mvPos + RV3(0.3f);
		gPlayer.mBody.mvFeet = gPlayer.mBody.mvPos - cVector3f(0, 0.9f, 0);
		if(RB(50)) l->OnMove((eCharDir)RI(2), RB(60) ? RU(-1, 1) : (RB() ? 1.0f : RF()));
		float dt = RB(80) ? RU(0.01f, 0.2f) : RDt();
		switch(RI(8))
		{
		case 0: case 1: case 2: case 3: l->Update(dt); break;
		case 4: Rec("ret %d", l->OnDoAction(RAction(), RB())); break;
		case 5: Rec("ret %d %d", l->OnAddYaw(RFin()), l->OnAddPitch(RFin())); Rec("ret %d %d %d", l->OnRun(RB()), l->OnCrouch(RB()), l->OnJump(RB())); break;
		case 6: l->Update(dt); l->Update(dt); break;
		case 7: if(RB(20)) { gPlayer.mBody.mvPos = RV3(5); gLadder.mfMaxY = RU(-1, 6); } l->Update(dt); break;
		}
		DumpLadder(*l);
		Step();
	}
	l->OnLeaveState(eLuxPlayerState_Normal);
	Step();
	delete l; delete mn; gMN = NULL;
}

static void DumpDefault(iLuxPlayerState_DefaultBase &d)
{
	DF("focusDist", d.mfFocusDistance); RecV("focusPos", d.mvFocusPos); DI("outline", d.mbCurrentEntityHasOutline);
	DI("body", d.mpBodyInFocus ? d.mpBodyInFocus->mlId : -1); DI("ent", d.mpEntityInFocus ? (int)(((iLuxProp *)d.mpEntityInFocus) - gProps) : -1);
	cGuiGfxElement *g = d.GetCrosshair();
	DI("crosshair", g ? g->mlId : -1);
	if(RB(15)) Rec("dbg %08x", Bits(d.DrawDebug(&gHud, NULL, RB(80) ? RU(-5, 50) : RF())));
}

static void RunDefault(int kind)
{
	SetupWorld();
	iLuxPlayerState_DefaultBase *s;
	if(kind == 0) s = new cLuxPlayerState_Normal(&gPlayer);
	else if(kind == 1) s = new cLuxPlayerState_HandObject(&gPlayer);
	else
	{
		cLuxPlayerStateVars::mpInventoryItem = RB(85) ? &gItem : NULL;
		s = new cLuxPlayerState_UseItem(&gPlayer);
	}
	SetupPlayerStateCommon(s);
	s->mfFocusDistance = 0; s->mvFocusPos = cVector3f(0, 0, 0); s->mbCurrentEntityHasOutline = false; s->mpBodyInFocus = NULL; s->mpEntityInFocus = NULL;
	gMN = new cLuxMoveState_Normal(&gPlayer); gPlayer.mpMoveNormal = gMN; gPlayer.mpMoveLedge = gML = new cLuxMoveState_ClimbLedge(&gPlayer);
	Rec("ctor"); Step();
	s->OnEnterState(RB() ? eLuxPlayerState_Normal : eLuxPlayerState_Ladder);
	DumpDefault(*s); Step();
	for(int i = 0, n = 10 + RI(40); i < n; ++i)
	{
		gPlayer.mMoveState = RB(85) ? eLuxMoveState_Normal : eLuxMoveState_ClimbLedge;
		gHelper.mpEntity = RB(80) ? (iLuxEntity *)&gProps[RI(3)] : NULL;
		gHelper.mpBody = gHelper.mpEntity ? &gBodies[RI(5)] : NULL;
		gHelper.mfDist = RB(80) ? RU(0.1f, 8) : RF();
		for(int k = 0; k < 3; ++k)
		{
			gProps[k].mfMaxFocus = RB(80) ? RU(0.5f, 6) : RF(); gProps[k].mbCanInteract = RB(70); gProps[k].mbDisabled = RB(20);
			gProps[k].mType = RB(80) ? eLuxEntityType_Prop : eLuxEntityType_Other; gProps[k].mPropType = RB() ? eLuxPropType_Item : eLuxPropType_Other;
			gProps[k].mCrosshair = (eLuxFocusCrosshair)RI(eLuxFocusCrosshair_LastEnum + 1); gProps[k].mbConnected = RB();
		}
		gPlayer.mIcon = RB() ? eLuxFocusIconStyle_Default : eLuxFocusIconStyle_Simple;
		gMapH.mMap.mbHasCb = RB();
		switch(RI(6))
		{
		case 0: case 1: s->Update(RDt()); break;
		case 2: case 3: { eLuxPlayerAction act = RAction(); bool pr = RB(); if(gVerbose) Rec("IN kind %d act %d pressed %d", kind, (int)act, (int)pr); Rec("ret %d", s->OnDoAction(act, pr)); break; }
		case 4:
			if(kind == 2)
			{
				cLuxPlayerState_UseItem *u = static_cast<cLuxPlayerState_UseItem *>(s);
				u->mpCurrentItem = RB(85) ? &gItem : NULL;
				Rec("draw %d", u->OnDrawCrossHair(&gEngine.mGui.mGfx[0], RV3(), cVector2f(1, 1)));
			}
			else { s->OnLeaveState((eLuxPlayerState)RI(eLuxPlayerState_LastEnum)); s->OnEnterState((eLuxPlayerState)RI(eLuxPlayerState_LastEnum)); }
			break;
		case 5: if(RB(20)) { s->OnLeaveState(eLuxPlayerState_Normal); s->OnMapLeave(NULL); } else s->OnDraw(&gHud, RDt()); break;
		}
		DumpDefault(*s);
		Step();
	}
	delete s;
	delete gMN; delete gML; gMN = gML = NULL;
}

#ifdef NEWSIDE
static void GoldOpen();
static void GoldClose();
#endif
int main(int argc, char **argv)
{
#ifdef NEWSIDE
	setenv("OHPL_IMPL", "rust", 1);
	GoldOpen();
#endif
	int cases = argc > 1 ? atoi(argv[1]) : 100;
	if(argc > 2) gRng ^= (uint64_t)atoll(argv[2]) * 0x9E3779B97F4A7C15ull;
	if(argc > 4) { gVerbose = true; gVerboseCase = atoi(argv[3]); gVerboseStep = atoi(argv[4]); }
	for(gCase = 0; gCase < cases; ++gCase)
	{
		gStep = 0;
		gLog.clear();
		switch(gCase % 12)
		{
		case 0: case 1: RunGrab(); break;
		case 2: RunPush(); break;
		case 3: RunSlide(); break;
		case 4: RunRotate(0); break;
		case 5: RunRotate(1); break;
		case 6: RunRotate(2); break;
		case 7: RunLadder(); break;
		case 8: RunDefault(0); break;
		case 9: RunDefault(1); break;
		case 10: RunDefault(2); break;
		case 11: RunDefault(RI(3)); break;
		}
	}
	fprintf(stderr, "steps %ld total %016llx\n", gSteps, (unsigned long long)gTotal);
#ifdef NEWSIDE
	GoldClose();
#endif
	return 0;
}

namespace hpl { void Error(const char *, ...) {} }
namespace hpl { cSerializeClass::cSerializeClass(const char *, const char *, cSerializeMemberField *, size_t, iSerializable *(*)()) {} }

void *operator new(size_t n) { void *p = calloc(1, n ? n : 1); if(!p) abort(); return p; }
void operator delete(void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }

void cGuiSet::DrawFont(iFontData *, const cVector3f &p, int sz, const cColor &c, const wchar_t *fmt, ...)
{
	wchar_t b[512];
	va_list ap;
	va_start(ap, fmt);
	vswprintf(b, 512, fmt, ap);
	va_end(ap);
	char u[1024];
	wcstombs(u, b, sizeof(u));
	Rec("Font %08x %08x %08x %d %08x %s", Bits(p.x), Bits(p.y), Bits(p.z), sz, Bits(c.r), u);
}
void iLuxEntity::InFocusDraw(cGuiSet *, float f) { Rec("E.FocusDraw %08x", Bits(f)); }
iLuxMoveState::iLuxMoveState(cLuxPlayer *apPlayer) { mpPlayer = apPlayer; }
cLuxMoveState_Normal::cLuxMoveState_Normal(cLuxPlayer *apPlayer) : iLuxMoveState(apPlayer) { mbRunning = mbCrouching = false; }
cLuxMoveState_ClimbLedge::cLuxMoveState_ClimbLedge(cLuxPlayer *apPlayer) : iLuxMoveState(apPlayer) {}
void cLuxMoveState_Normal::SetCrouch(bool b) { Rec("MN.SetCrouch %d", b); }
void cLuxMoveState_Normal::ResetJumping() { Rec("MN.ResetJumping"); }
void cLuxMoveState_Normal::Jump() { Rec("MN.Jump"); }
void cLuxMoveState_Normal::OnEnterState(eLuxMoveState) {}
void cLuxMoveState_Normal::OnUpdate(float) {}
void cLuxMoveState_ClimbLedge::OnEnterState(eLuxMoveState) {}
void cLuxMoveState_ClimbLedge::OnUpdate(float) {}

#ifdef NEWSIDE
// ---- golden recorder: every ohpl_a3_* entry (state before as a diff, args, host traffic, state after, result) as u32 words
static FILE *gGold;
static std::map<const void *, uint32_t> gPtrIds;
static std::map<OhplA3Base *, std::pair<int, std::vector<uint32_t> > > gObjs;
static int gObjNext;
static size_t gGoldWords;

static void GReg(const void *p) { if(p && !gPtrIds.count(p)) gPtrIds[p] = (uint32_t)gPtrIds.size() + 1; }
static void GRegisterWorld()
{
	for(int i = 0; i < 5; ++i) { GReg(&gBodies[i]); GReg(&gJoints[i]); }
	for(int i = 0; i < 3; ++i) { GReg(&gProps[i]); GReg(&gProps[i].mMesh); GReg(&gProps[i].mGrab); GReg(&gProps[i].mPush); GReg(&gProps[i].mSlide); GReg(&gProps[i].mMove); }
	GReg(&gLadder); GReg(&gItem); GReg(&gMapH.mMap.mCb); GReg(&gHud);
	for(int i = 0; i < 16; ++i) GReg(&gEngine.mGui.mGfx[i]);
}
uint32_t GP(const void *p)
{
	if(!p) return 0;
	std::map<const void *, uint32_t>::iterator it = gPtrIds.find(p);
	return it == gPtrIds.end() ? 0xDEAD0000u : (0xABCD0000u | it->second);
}
static void GW(uint32_t w) { if(gGold) { fwrite(&w, 4, 1, gGold); ++gGoldWords; } }
static size_t GSize(int kind)
{
	switch(kind) { case 2: return 120; case 3: return 952; case 4: return 920; case 8: return 528; case 5: case 6: case 7: return 592; case 9: return 208; default: return 80; }
}
static void GSnap(OhplA3Base *b, size_t size, std::vector<uint32_t> &w)
{
	w.assign(size / 4, 0);
	for(size_t i = 0; i < size / 8; ++i)
	{
		uint64_t v;
		memcpy(&v, (char *)b + i * 8, 8);
		uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
		if(i == 9) { lo = v != 0; hi = 0; }
		else if(v != 0 && gPtrIds.count((const void *)v)) { lo = GP((const void *)v); hi = 0; }
		w[i * 2] = lo; w[i * 2 + 1] = hi;
	}
}
static void GDiff(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
{
	std::vector<uint32_t> d;
	for(size_t i = 0; i < b.size(); ++i) if(a[i] != b[i]) { d.push_back((uint32_t)i); d.push_back(b[i]); }
	GW((uint32_t)(d.size() / 2));
	for(size_t i = 0; i < d.size(); ++i) GW(d[i]);
}
static std::vector<uint32_t> gBefore;
void GBegin(int fn, OhplA3Base *b, int kind, std::initializer_list<uint32_t> args)
{
	if(!gGold) return;
	std::pair<int, std::vector<uint32_t> > &o = gObjs[b];
	if(fn == 1)
	{
		o.first = gObjNext++;
		o.second.assign(GSize(kind) / 4, 0);
	}
	size_t size = o.second.size() * 4;
	GSnap(b, size, gBefore);
	GW(0xC0); GW(fn); GW(o.first); GW((uint32_t)kind); GW((uint32_t)(size / 4)); GW((uint32_t)args.size());
	for(uint32_t a : args) GW(a);
	GDiff(o.second, gBefore);
}
void GEnd(OhplA3Base *b, uint32_t ret, std::initializer_list<uint32_t> outs, bool freed)
{
	if(!gGold) return;
	std::pair<int, std::vector<uint32_t> > &o = gObjs[b];
	std::vector<uint32_t> after;
	GSnap(b, o.second.size() * 4, after);
	GW(0xC2); GW(ret); GW((uint32_t)outs.size());
	for(uint32_t a : outs) GW(a);
	GDiff(gBefore, after);
	o.second = after;
	if(freed) gObjs.erase(b);
}
static uint32_t GStr(const void *p) { uint32_t h = 2166136261u; for(const unsigned char *s = (const unsigned char *)p; *s; ++s) { h ^= *s; h *= 16777619u; } return h; }
static void GIoWords(const OhplA3Io &io, uint32_t op, uint32_t w[30])
{
	bool cfg = op == kOhplA3Op_CFG_FLOAT;
	for(int k = 0; k < 3; ++k) w[k] = cfg && k < 2 ? GStr(io.p[k]) : GP(io.p[k]);
	for(int k = 0; k < 3; ++k) w[3 + k] = (uint32_t)io.i[k];
	for(int k = 0; k < 24; ++k) w[6 + k] = Bits(io.f[k]);
}
int32_t iLuxPlayerState::A3Host(void *apCtx, uint32_t alOp, OhplA3Io *apIo)
{
	if(!gGold) return A3HostReal(apCtx, alOp, apIo);
	uint32_t in[30], out[30];
	GIoWords(*apIo, alOp, in);
	int32_t r = A3HostReal(apCtx, alOp, apIo);
	GIoWords(*apIo, alOp, out);
	uint32_t mi = 0, mo = 0;
	for(int k = 0; k < 30; ++k) { if(in[k]) mi |= 1u << k; if(out[k] != in[k] && !(alOp == kOhplA3Op_CFG_FLOAT && k < 2)) mo |= 1u << k; }
	GW(0xC1); GW(alOp); GW(mi);
	for(int k = 0; k < 30; ++k) if(in[k]) GW(in[k]);
	GW((uint32_t)r); GW(mo);
	for(int k = 0; k < 30; ++k) if(mo & (1u << k)) GW(out[k]);
	return r;
}
#include "golden_wrap.inc"
static void GoldOpen()
{
	const char *p = getenv("A3_GOLDEN");
	if(p) { gGold = fopen(p, "wb"); GRegisterWorld(); }
}
static void GoldClose() { if(gGold) { fclose(gGold); fprintf(stderr, "golden words %zu\n", gGoldWords); } }
#endif
