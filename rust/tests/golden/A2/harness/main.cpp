// Differential driver: random player sequences against fakes; prints one hash line per step.
#include "prelude.h"
#include "system/SerializeClass.h"
#if defined(NEWSIDE) && defined(NEW_DBG)
#include "new_dbg.inc"
#elif defined(NEWSIDE)
#include "new.inc"
#elif defined(ORIG_DBG)
#include "orig_dbg.inc"
#else
#include "orig.inc"
#endif

std::string gLog;
FILE *gRecFile;
unsigned gRecCount;
cLuxBase *gpBase;
static bool gFrustumIn;
static bool gVerbose;

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
uint32_t Bits(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	return u;
}
eCollision cFrustum::CollideBoundingVolume(cBoundingVolume *) { return gFrustumIn ? eCollision_Inside : eCollision_Outside; }
void cLuxPlayerFlashback::Start(const tString &f, const tString &c) { Rec("FlashbackStart %s %s", f.c_str(), c.c_str()); }

static uint64_t gRng = 88172645463325252ull;
static uint64_t R()
{
	gRng ^= gRng << 13;
	gRng ^= gRng >> 7;
	gRng ^= gRng << 17;
	return gRng;
}
static float RF(float lo, float hi) { return lo + (hi - lo) * (float)((R() >> 11) * (1.0 / 9007199254740992.0)); }
static int RI(int n) { return (int)(R() % (uint64_t)n); }
static bool RB(int pct = 50) { return RI(100) < pct; }

static float Special()
{
	switch(RI(14))
	{
	case 0: return 0.0f;
	case 1: return -0.0f;
	case 2: return 1e-8f;
	case 3: return -1e-8f;
	case 4: return 1e30f;
	case 5: return -1e30f;
	case 6: return NAN;
	case 7: return INFINITY;
	case 8: return -INFINITY;
	case 9: return 1.0f;
	case 10: return -1.0f;
	case 11: return 100.0f;
	case 12: return 0.004f;
	default: return RF(-2, 2);
	}
}
static float Val(float lo, float hi) { return RI(12) == 0 ? Special() : RF(lo, hi); }
static float DT()
{
	switch(RI(12))
	{
	case 0: return 0.0f;
	case 1: return 1e6f;
	case 2: return NAN;
	case 3: return -0.01f;
	case 4: return RF(0, 0.1f);
	case 5: return 3.0f;
	default: return 1.0f / 60.0f;
	}
}
static cVector3f V3(float lo, float hi) { return cVector3f(Val(lo, hi), Val(lo, hi), Val(lo, hi)); }

// config from the real game.cfg files
static std::map<std::string, float> gCfg[2];
static int gGame;
static float gJit;
static float C(const char *sec, const char *key, float def)
{
	std::string k = std::string(sec) + "." + key;
	std::map<std::string, float>::iterator it = gCfg[gGame].find(k);
	float v = it == gCfg[gGame].end() ? def : it->second;
	return v * gJit;
}
static void LoadCfg(const char *path)
{
	FILE *f = fopen(path, "r");
	if(!f) { fprintf(stderr, "no cfg\n"); exit(2); }
	char g[8], sec[128], key[128], val[128];
	while(fscanf(f, "%7s %127s %127s %127s", g, sec, key, val) == 4)
	{
		float x = (float)atof(val);
		gCfg[g[0] == 'T' ? 0 : 1][std::string(sec) + "." + key] = x;
	}
	fclose(f);
}

static uint64_t gH;
static void Mix(uint64_t v) { gH = (gH ^ v) * 1099511628211ull; }
static void MixF(float f) { Mix(Bits(f)); }
static std::string gState;
static void MixN(const char *name, uint64_t v)
{
	Mix(v);
	if(gVerbose) { char b[160]; snprintf(b, sizeof(b), "  %s=%llx\n", name, (unsigned long long)v); gState += b; }
}
#define MF(x) MixN(#x, Bits(x))
#define MI(x) MixN(#x, (uint64_t)(x))
#define MV(v) do { MixN(#v ".x", Bits((v).x)); MixN(#v ".y", Bits((v).y)); MixN(#v ".z", Bits((v).z)); } while(0)

template<class T> static T *Make()
{
	void *m = calloc(1, sizeof(T) + 64);
	return new(m) T();
}
template<class T> static T *MakeP(cLuxPlayer *p)
{
	void *m = calloc(1, sizeof(T) + 64);
	return new(m) T(p);
}

struct World
{
	cLuxBase base;
	cEngine eng;
	cLuxHelpFuncs help;
	cLuxHintHandler hint;
	cLuxMessageHandler msg;
	cLuxEffectHandler fx;
	cLuxPostEffectHandler post;
	cLuxInventory inv;
	cLuxSaveHandler save;
	cLuxMapHandler mapHandler;
	cLuxMapHelper mapHelper;
	cLuxPlayer *p;
	cCamera cam;
	iCharacterBody body;
	cLuxPlayerHudEffect hud;
	cLuxPlayerDeath *death;
	cLuxPlayerInsanityCollapse *collapse;
	cLuxPlayerSanity *sanity;
	cLuxPlayerLantern *lantern;
	cLuxPlayerLightLevel *lightLevel;
	cLuxPlayerInDarkness *darkness;
	cLuxPlayerFlashback *flashback;
	cLuxPlayerHands *hands;
	cLuxPlayerCamDirEffects *camdir;
	cLuxPlayerHurt *hurt;
	cLuxPlayerLookAt *lookat;
	cLightPoint lanternLight, ambientLight;
	cLuxHandObject handObj[2];
	cMeshEntity handsEntity;
	std::vector<iLuxEnemy *> enemies;
	std::vector<iLuxProp_CritterBase *> critters;
};
static World *W;

static void Setup()
{
	W = (World *)calloc(1, sizeof(World));
	new(&W->base) cLuxBase();
	new(&W->eng) cEngine();
	new(&W->help) cLuxHelpFuncs();
	new(&W->mapHandler) cLuxMapHandler();
	new(&W->cam) cCamera();
	W->p = Make<cLuxPlayer>();
	cLuxPlayer *p = W->p;
	W->death = MakeP<cLuxPlayerDeath>(p);
	W->collapse = MakeP<cLuxPlayerInsanityCollapse>(p);
	W->sanity = MakeP<cLuxPlayerSanity>(p);
	W->lantern = MakeP<cLuxPlayerLantern>(p);
	W->lightLevel = MakeP<cLuxPlayerLightLevel>(p);
	W->darkness = MakeP<cLuxPlayerInDarkness>(p);
	W->flashback = MakeP<cLuxPlayerFlashback>(p);
	W->hands = MakeP<cLuxPlayerHands>(p);
	W->camdir = MakeP<cLuxPlayerCamDirEffects>(p);
	W->hurt = MakeP<cLuxPlayerHurt>(p);
	W->lookat = MakeP<cLuxPlayerLookAt>(p);
	W->base.mpEngine = &W->eng;
	W->base.mpPlayer = p;
	W->base.mpHelpFuncs = &W->help;
	W->base.mpHintHandler = &W->hint;
	W->base.mpMessageHandler = &W->msg;
	W->base.mpEffectHandler = &W->fx;
	W->base.mpPostEffectHandler = &W->post;
	W->base.mpInventory = &W->inv;
	W->base.mpSaveHandler = &W->save;
	W->base.mpMapHandler = &W->mapHandler;
	W->base.mpMapHelper = &W->mapHelper;
	gpBase = &W->base;
	for(int i = 0; i < 3; ++i)
	{
		iLuxEnemy *e = (iLuxEnemy *)calloc(1, sizeof(iLuxEnemy));
		new(e) iLuxEnemy();
		e->mlId = i;
		W->enemies.push_back(e);
		W->mapHandler.mMap.mEnemies.push_back(e);
	}
	for(int i = 0; i < 3; ++i)
	{
		iLuxProp_CritterBase *c = (iLuxProp_CritterBase *)calloc(1, sizeof(iLuxProp_CritterBase));
		new(c) iLuxProp_CritterBase();
		W->critters.push_back(c);
		W->mapHandler.mMap.mEntities.push_back(c);
	}
}

static void InitCase()
{
	cLuxPlayer *p = W->p;
	gJit = RB(40) ? RF(0.4f, 2.5f) : 1.0f;
	gGame = RB(50) ? 0 : 1;
	// player
	p->mpCamera = &W->cam;
	p->mpCharBody = &W->body;
	p->mpHudEffect = &W->hud;
	p->mpDeath = W->death;
	p->mpInsanityCollapse = W->collapse;
	p->mpSanity = W->sanity;
	p->mpLantern = W->lantern;
	p->mpHelperLightLevel = W->lightLevel;
	p->mpHelperInDarkness = W->darkness;
	p->mpFlashback = W->flashback;
	p->mpHands = W->hands;
	p->mbFreeCameraActive = RB(10);
	p->mfHealth = Val(0, 100);
	if(RB(80)) p->mfHealth = RF(1, 100);
	p->mfSanity = RB(80) ? RF(0, 100) : Val(0, 100);
	p->mfLampOil = RB(80) ? RF(0, 100) : Val(0, 100);
	p->mfTerror = RB(60) ? 0 : RF(0, 1);
	p->mbActive = RB(80);
	p->mbUsePermaDeath = RB(20);
	p->mbSanityDrainDisabled = RB(15);
	p->mfAspectMul = p->mfFOVMul = 1;
	p->mfAspectMulGoal = RB(50) ? 1 : Val(0.5f, 1.5f);
	p->mfFOVMulGoal = RB(50) ? 1 : Val(0.5f, 1.5f);
	p->mfAspectMulSpeed = Val(0, 4);
	p->mfFOVMulSpeed = Val(0, 4);
	p->mfRoll = RB(50) ? 0 : Val(-1, 1);
	p->mfRollGoal = RB(50) ? p->mfRoll : Val(-1, 1);
	p->mfRollSpeedMul = Val(0, 12);
	p->mfRollMaxSpeed = Val(0, 12);
	p->mbFadingPitch = RB(30);
	p->mfPitchGoal = Val(-1.5f, 1.5f);
	p->mfPitchSpeedMul = Val(0, 12);
	p->mfPitchMaxSpeed = Val(0, 12);
	p->mfLeanRoll = RB(50) ? 0 : Val(-0.3f, 0.3f);
	p->mfLeanRollGoal = RB(50) ? p->mfLeanRoll : Val(-0.3f, 0.3f);
	p->mfLeanRollSpeedMul = Val(0, 6);
	p->mfLeanRollMaxSpeed = Val(0, 6);
	p->mvCamAnimPos = RB(50) ? cVector3f(0, 0, 0) : V3(-1, 1);
	p->mvCamAnimPosGoal = RB(50) ? p->mvCamAnimPos : V3(-1, 1);
	p->mfCamAnimPosSpeedMul = Val(0, 1);
	p->mfCamAnimPosMaxSpeed = Val(0, 1);
	p->mvHeadSpinSpeed = RB(50) ? cVector2f(0, 0) : cVector2f(Val(-8, 8), Val(-8, 8));
	p->mfFocusTextAlpha = Val(0, 1);
	p->msFocusText = RB(50) ? L"" : L"x";
	p->mlMaxPrevSpeeds = RB(10) ? RI(3) - 1 : (int)C("Player_General", "MaxPrevSpeedsSaved", 60);
	p->mlstPrevSpeeds.clear();
	p->mfAvgSpeed = 0;
	p->mvCameraPosAdd = V3(-1, 1);
	p->mfHeadSpinDamageSpeed = C("Player_General", "HeadSpinDamageSpeed", 0);
	p->mfHeadSpinDeacc = C("Player_General", "HeadSpinDeacc", 0);
	p->mfTerrorIncSpeed = C("Player_General", "TerrorIncSpeed", 0);
	p->mfTerrorDecSpeed = C("Player_General", "TerrorDecSpeed", 0);
	p->mfFOV = C("Player_General", "FOV", 0) * 0.017453293f;
	p->mfAspect = RF(1, 2.5f);
	p->msTerrorSound = "terror";
	p->mpTerrorSound = RB(30) ? &W->eng.mSound.mHandler.mEntry : NULL;
	p->mlTerrorSoundID = RI(3);
	p->m_setTerrorEnemies.clear();
	if(RB(50)) p->m_setTerrorEnemies.insert(1);
	p->mvHeadPosAdds.assign(eLuxHeadPosAdd_LastEnum, cLuxHeadPosAdd());
	for(size_t i = 0; i < p->mvHeadPosAdds.size(); ++i)
	{
		cLuxHeadPosAdd &a = p->mvHeadPosAdds[i];
		a.mvAdd = RB(50) ? cVector3f(0, 0, 0) : V3(-1, 1);
		a.mbMoving = RB(35);
		a.mvMoveGoal = V3(-1, 1);
		a.mfMoveSpeed = Val(0, 3);
		a.mfMoveSlowDownDist = RB(30) ? 0 : Val(0, 0.2f);
	}
	p->mState.mbLantern = RB(80);
	W->hud = cLuxPlayerHudEffect();
	// camera
	W->cam.mfPitch = Val(-1.2f, 1.2f);
	W->cam.mfYaw = Val(-6, 6);
	W->cam.mfRoll = 0;
	W->cam.mfPitchLimitMin = RB(80) ? -1.2217305f : 0;
	W->cam.mfPitchLimitMax = RB(80) ? 1.2217305f : 0;
	W->cam.mvPos = V3(-20, 20);
	W->cam.mvRight = V3(-1, 1);
	W->cam.mvUp = V3(-1, 1);
	W->cam.mvForward = V3(-1, 1);
	W->body.mvPos = V3(-20, 20);
	W->body.mvSize = RB(80) ? cVector3f(0.8f, 1.8f, 0.8f) : V3(0, 2);
	W->body.mvForward = V3(-1, 1);
	W->body.mvVel = RB(50) ? cVector3f(0, 0, 0) : (RB(40) ? cVector3f(RF(-0.06f, 0.06f), 0, RF(-0.06f, 0.06f)) : V3(-3, 3));
	// world
	W->eng.mSound.mHandler.mbPlayOk = RB(70);
	W->eng.mSound.mHandler.mbValid = RB(70);
	W->eng.mSound.mHandler.mEntry.mlId = RI(3);
	W->base.mbHardMode = RB(25);
	W->help.mbExtraOk = RB(60);
	W->msg.mbPause = RB(20);
	W->inv.mbHas = RB(80);
	W->save.mbOk = RB(50);
	W->fx.mEmotion.mbOn = RB(15);
	W->fx.mVoice.mbDone = RB(30);
	W->mapHandler.mMap.msLantern = RB(50) ? "LanternCb" : "";
	for(int i = 0; i < 16; ++i) W->mapHelper.mfLevel[i] = RB(50) ? RF(0, 0.5f) : Val(0, 1);
	for(int i = 0; i < 64; ++i) W->mapHelper.mbLos[i] = RB(50);
	W->mapHelper.mlCalls = 0;
	gFrustumIn = RB(70);
	for(size_t i = 0; i < W->enemies.size(); ++i)
	{
		iLuxEnemy *e = W->enemies[i];
		e->mbActive = RB(80);
		e->mbCausesSanity = RB(80);
		e->mBody.mvPos = V3(-20, 20);
		if(RB(60)) e->mBody.mvPos = cVector3f(RF(-10, 10), RF(-3, 3), RF(-10, 10)) + W->cam.mvPos;
		e->mBody.mvSize = cVector3f(0.8f, 1.8f, 0.8f);
	}
	for(size_t i = 0; i < W->critters.size(); ++i)
	{
		iLuxProp_CritterBase *c = W->critters[i];
		c->mbActive = RB(80);
		c->mType = eLuxEntityType_Prop;
		c->mPropType = eLuxPropType_Critter;
		c->mbCausesSanity = RB(80);
		c->mBody.mvLocal = RB(50) ? W->cam.mvPos + cVector3f(RF(-4, 4), RF(-4, 4), RF(-4, 4)) : V3(-20, 20);
	}
	// collapse
	cLuxPlayerInsanityCollapse *c = W->collapse;
	c->mbActive = RB(40);
	c->mlState = RI(3);
	c->mfHeightAdd = RB(50) ? 0 : RF(-0.9f, 0);
	c->mfRoll = Val(0, 0.6f);
	c->mfT = Val(0, 10);
	c->mfRandomCount = Val(-2, 2);
	c->mfHeightAddGoal = C("Player_General", "InsanityCollapse_HeightAddGoal", 0);
	c->mfHeightAddCollapseSpeed = C("Player_General", "InsanityCollapse_HeightAddCollapseSpeed", 0);
	c->mfHeightAddAwakeSpeed = C("Player_General", "InsanityCollapse_HeightAddAwakeSpeed", 0);
	c->mfRollCollapseSpeed = C("Player_General", "InsanityCollapse_RollCollapseSpeed", 0);
	c->mfSleepTime = C("Player_General", "InsanityCollapse_SleepTime", 0);
	c->mfWakeUpSpeedMul = C("Player_General", "InsanityCollapse_WakeUpSpeedMul", 0);
	c->mfAwakenSanity = C("Player_General", "InsanityCollapse_AwakenSanity", 0);
	c->mfSleepRandomMinTime = C("Player_General", "InsanityCollapse_SleepRandomMinTime", 0);
	c->mfSleepRandomMaxTime = C("Player_General", "InsanityCollapse_SleepRandomMaxTime", 0);
	c->msSleepRandomSound = "rnd";
	c->msAwakenSound = "awaken";
	c->mpLoopSound = RB(50) ? &W->eng.mSound.mHandler.mEntry : NULL;
	// camdir
	cLuxPlayerCamDirEffects *d = W->camdir;
	d->mfStartSwayMaxSanity = C("Player_Sanity", "StartSwayMaxSanity", 0);
	d->mbSwayActive = RB(40);
	d->mlstPrevAdd.clear();
	int n = RI(4) == 0 ? 0 : RI(8);
	for(int i = 0; i < n; ++i) d->mlstPrevAdd.push_back(cVector2f(Val(-1, 1), Val(-1, 1)));
	d->mvSwayAdd = cVector2f(Val(-1, 1), Val(-1, 1));
	d->mlMaxPositions = RB(10) ? RI(3) - 1 : (int)C("Player_Sanity", "SwayMaxSavedPositions", 0);
	d->mfSwayAlpha = RB(30) ? 0 : Val(0, 1);
	d->mvNextAdd = cVector2f(Val(-1, 1), Val(-1, 1));
	// hurt
	cLuxPlayerHurt *h = W->hurt;
	h->mvNoiseGfx.assign(8, 0);
	h->mfEffectStartHealth = C("Player_General", "Hurt_EffectStartHealth", 0);
	if(h->mfEffectStartHealth == 0) h->mfEffectStartHealth = 30 * gJit;
	h->mfMinMoveMul = C("Player_General", "Hurt_MinSpeedMul", 0.5f);
	h->mfMaxPantCount = C("Player_General", "Hurt_MaxPantCount", 4);
	h->mfPantSpeed = C("Player_General", "Hurt_PantSpeed", 0.5f);
	h->mfPantSize = C("Player_General", "Hurt_PantSize", 0.1f);
	h->mlCurrentNoise = RI(8);
	h->mfNoiseUpdateCount = Val(-1, 1);
	h->mfNoiseFreq = C("Player_General", "Hurt_NoiseFreq", 10);
	h->mfPantCount = Val(-1, 3);
	h->mfPantPosAdd = RB(50) ? 0 : Val(-0.1f, 0.1f);
	h->mfPantPosAddVel = Val(-1, 1);
	h->mfPantPosAddDir = RB(50) ? 1.0f : -1.0f;
	h->mfAlpha = RB(40) ? 0 : Val(0, 1);
	h->mfHealthRegainSpeed = C("Player_General", "HealthRegainSpeed", 1);
	h->mfHealthRegainLimit = C("Player_General", "HealthRegainLimit", 50);
	// look at
	cLuxPlayerLookAt *l = W->lookat;
	l->mbActive = RB(60);
	l->mvTargetPos = V3(-20, 20);
	l->mvCurrentSpeed = V3(-2, 2);
	l->mfSpeedMul = Val(0, 4);
	l->mfMaxSpeed = RB(50) ? 9999.0f : Val(0, 5);
	l->msAtTargetCallback = "AtTarget";
	// flashback
	cLuxPlayerFlashback *f = W->flashback;
	f->mfRadialBlurSize = C("Player_General", "FlashbackRadialBlurSize", 0.12f);
	f->mfRadialBlurStartDist = C("Player_General", "FlashbackRadialBlurStartDist", 0.4f);
	f->mfMoveSpeedMul = C("Player_General", "FlashbackMoveSpeedMul", 1.0f);
	f->mfRunSpeedMul = C("Player_General", "FlashbackRunSpeedMul", 1.0f);
	f->mfWorldSoundVolume = C("Player_General", "FlashbackWorldSoundVolume", 0.4f);
	f->mfFlashDelay = RB(50) ? 0 : Val(0, 0.6f);
	f->mbActive = RB(50);
	f->mfFlashbackStartCount = Val(-1, 2);
	f->msFlashbackFile = "fb";
	f->msCallback = RB(50) ? "cb" : "";
	f->mlstFlashbackQueue.clear();
	if(RB(40))
	{
		cLuxFlashbackData fd;
		fd.msFile = "q";
		fd.msCallback = "qc";
		f->mlstFlashbackQueue.push_back(fd);
	}
	// sanity
	cLuxPlayerSanity *s = W->sanity;
	s->mfHitAlpha = RB(50) ? 0 : Val(0, 1);
	s->mbHitActive = RB(30);
	s->mfSanityLostCount = RB(50) ? 0 : Val(0, 1);
	s->mfAtLowSanityCount = RB(50) ? 0 : Val(0, 400);
	s->mbHitIsUpdated = RB(20);
	s->mbSanityLostIsUpdated = RB(20);
	s->mfPantCount = Val(-1, 5);
	s->mfCheckEnemySeenCount = RB(50) ? 0 : Val(0, 0.3f);
	s->mbSanityEffectUpdated = RB(30);
	s->mfT = Val(0, 100);
	s->mfInsaneWaveAlpha = RB(50) ? 0 : Val(0, 1);
	s->mfSanityDrainCount = Val(0, 1);
	s->mfSanityDrainVolume = RB(50) ? 0 : Val(0, 1);
	s->mfSanityHeartbeatCount = Val(0, 1);
	s->mfSeenEnemyCount = RB(50) ? 0 : Val(0, 1);
	s->mbEnemyIsSeen = RB(40);
	s->mfShowHintTimer = RB(50) ? 0 : Val(0, 3);
	s->mfHitZoomInSpeed = C("Player_Sanity", "HitZoomInSpeed", 0);
	s->mfHitZoomOutSpeed = C("Player_Sanity", "HitZoomOutSpeed", 0);
	s->mfHitZoomInFOVMul = C("Player_Sanity", "HitZoomInFOVMul", 0);
	s->mfHitZoomInAspectMul = C("Player_Sanity", "HitZoomInAspectMul", 0);
	s->mfSanityRegainSpeed = C("Player_Sanity", "SanityRegainSpeed", 0);
	s->mfSanityRegainLimit = C("Player_Sanity", "SanityRegainLimit", 0);
	s->mfSanityVeryLowLimit = C("Player_Sanity", "SanityVeryLowLimit", 0);
	s->mfSanityEffectsStart = C("Player_Sanity", "SanityEffectsStart", 0);
	s->mfSanityWaveAlphaMul = C("Player_Sanity", "SanityWaveAlphaMul", 0);
	s->mfSanityWaveSpeedMul = C("Player_Sanity", "SanityWaveSpeedMul", 0);
	s->mfSanityLowLimit = C("Player_Sanity", "SanityLowLimit", 0);
	s->mfSanityLowLimitMaxTime = C("Player_Sanity", "SanityLowLimitMaxTime", 0);
	s->mfSanityLowNewSanityAmount = C("Player_Sanity", "SanityLowNewSanityAmount", 0);
	s->mfCheckNearEnemyInterval = C("Player_Sanity", "CheckNearEnemyInterval", 0);
	s->mfNearEnemyDecrease = C("Player_Sanity", "NearEnemyDecrease", 0);
	s->mfNearCritterDecrease = C("Player_Sanity", "NearCritterDecrease", 0);
	// lantern
	cLuxPlayerLantern *la = W->lantern;
	la->mDefaultColor = cColor(C("Player_Lantern", "R", 0.88f), 0.82f * gJit, 0.4f, 0.8f);
	la->msTurnOnSound = "on";
	la->msTurnOffSound = "off";
	la->msOutOfOilSound = "ooo";
	la->mvLocalOffset = cVector3f(0.3f, -0.3f, -0.1f);
	la->mfLowerOilSpeed = C("Player_Lantern", "LowerOilSpeed", 0);
	la->mfFadeLightOilAmount = C("Player_Lantern", "FadeLightOilAmount", 0);
	la->mbDisabled = RB(15);
	la->mbActive = RB(50);
	la->mfAlpha = RB(50) ? 1 : Val(0, 1);
	la->mpLight = &W->lanternLight;
	// light level
	cLuxPlayerLightLevel *ll = W->lightLevel;
	ll->mfRadiusAdd = C("Player_Darkness", "RadiusAdd", 0);
	ll->mfExtendedLightLevel = ll->mfNormalLightLevel = 1;
	ll->mfUpdateCount = RB(50) ? 0 : Val(0, 0.5f);
	// darkness
	cLuxPlayerInDarkness *dk = W->darkness;
	dk->mbActive = RB(90);
	dk->mpAmbientLight = &W->ambientLight;
	W->ambientLight.mfRadius = 6.5f;
	dk->mbAmbientLightIsOn = RB(50);
	dk->mbInDarkness = RB(50);
	dk->mfMinDarknessLightLevel = C("Player_Darkness", "MinLightLevel", 0);
	dk->mfAmbientLightMinLightLevel = C("Player_Darkness", "AmbientLightMinLightLevel", 0);
	dk->mfAmbientLightIntensity = C("Player_Darkness", "AmbientLightIntensity", 0);
	dk->mfAmbientLightFadeInTime = C("Player_Darkness", "AmbientLightFadeInTime", 0);
	dk->mfAmbientLightFadeOutTime = C("Player_Darkness", "AmbientLightFadeOutTime", 0);
	dk->mAmbientLightColor = cColor(0.5f, 0.5f, 1, 0);
	dk->mfLoopSoundCount = Val(0, 1);
	dk->mfLoopSoundStartupTime = C("Player_Darkness", "LoopSoundStartupTime", 0);
	dk->mfSanityLossPerSecond = C("Player_Darkness", "SanityLossPerSecond", 0);
	dk->mfSanityLossMul = RB(50) ? 0 : Val(0, 1);
	dk->mfShowHintTimer = RB(50) ? 0 : Val(0, 3);
	// death
	cLuxPlayerDeath *de = W->death;
	de->mbActive = RB(70);
	de->mbShowHint = RB(70);
	de->mlState = RI(7) == 0 ? 5 : RI(5);
	de->msStartSound = "ds";
	de->msAwakenSound = "da";
	de->mfHeightAddSpeed = C("Player_General", "Death_HeightAddSpeed", 0);
	de->mfRollSpeed = C("Player_General", "Death_RollSpeed", 0);
	de->mfFadeOutTime = C("Player_General", "Death_FadeTime", 1000);
	if(p->mbUsePermaDeath) { de->mfFadeOutTime /= 2.0f; de->mfHeightAddSpeed *= 1.2f; de->mfRollSpeed *= 1.2f; }
	de->mfHeightAdd = RB(50) ? 0 : RF(-1.3f, 0);
	de->mfRoll = Val(0, 1.1f);
	de->mfT = Val(0, 10);
	de->mfMinHeightAdd = RB(50) ? C("Player_General", "Death_HeightAdd", 0) : C("Player_General", "Death_HeightAddCrouch", 0);
	de->msHintCat = "hc";
	de->msHintEntry = "he";
	de->mfFadeAlpha = RB(30) ? 1.0f : Val(0, 1);
	de->mfTextAlpha1 = Val(0, 1);
	de->mfTextOnScreenCount = RB(50) ? 0 : Val(0, 6);
	de->mfWhiteCount = Val(0, 0.7f);
	de->mpVoiceEntry = RB(50) ? &W->eng.mSound.mHandler.mEntry : NULL;
	de->mlVoiceEntryId = RI(3);
	de->mbToMainMenu = false;
	// hands
	cLuxPlayerHands *ha = W->hands;
	W->handsEntity.mlAnims = RI(3);
	W->handsEntity.mbObjActive = RB(70);
	ha->mpHandsEntity = &W->handsEntity;
	for(int i = 0; i < 2; ++i)
	{
		W->handObj[i].mbAnimOver = RB(50);
		W->handObj[i].mbDoAction = RB(50);
		W->handObj[i].mbMesh = RB(70);
		W->handObj[i].mMesh.mbObjActive = RB(70);
	}
	ha->mvHandObjects.clear();
	ha->mvHandObjects.push_back(&W->handObj[0]);
	ha->mvHandObjects.push_back(&W->handObj[1]);
	ha->mpCurrentHandObject = RB(60) ? &W->handObj[RI(2)] : NULL;
	ha->mHandState = (eLuxHandsState)RI(5);
	if(ha->mHandState == eLuxHandsState_Draw && ha->mpCurrentHandObject == NULL) ha->mHandState = eLuxHandsState_Disabled; // null deref in the original
	ha->msCurrentAnim = "";
	ha->mlstCamRotations.clear();
	n = RI(5);
	for(int i = 0; i < n; ++i) ha->mlstCamRotations.push_back(V3(-3, 3));
	ha->mlMaxCamRotations = RB(10) ? RI(3) - 2 : (int)C("Player_General", "HandsRotationSmoothNum", 6);
	ha->mfPosAddMul = C("Player_General", "HandsPosAddMul", 1);
	ha->mbAnimOver = RB(50);
}

static void Dump()
{
	cLuxPlayer *p = W->p;
	MF(p->mfHealth); MF(p->mfSanity); MF(p->mfLampOil); MF(p->mfTerror);
	MF(p->mfAspectMul); MF(p->mfFOVMul); MF(p->mfAspectMulGoal); MF(p->mfFOVMulGoal); MF(p->mfAspectMulSpeed); MF(p->mfFOVMulSpeed);
	MF(p->mfRoll); MF(p->mfRollGoal); MF(p->mfRollSpeedMul); MF(p->mfRollMaxSpeed); MI(p->mbFadingPitch);
	MF(p->mfPitchGoal); MF(p->mfPitchSpeedMul); MF(p->mfPitchMaxSpeed);
	MF(p->mfLeanRoll); MF(p->mfLeanRollGoal); MF(p->mfLeanRollSpeedMul); MF(p->mfLeanRollMaxSpeed);
	MV(p->mvCamAnimPos); MF(p->mvHeadSpinSpeed.x); MF(p->mvHeadSpinSpeed.y); MV(p->mvHeadPosAddSum);
	MF(p->mfFocusTextAlpha); MF(p->mfAvgSpeed); MI(p->mlstPrevSpeeds.size());
	MF(p->mfHurtMoveSpeedMul); MF(p->mfInsanityCollapseSpeedMul); MF(p->mfEventMoveSpeedMul); MF(p->mfEventRunSpeedMul);
	MI(p->mbJumpDisabled); MI(p->mbCrouchDisabled); MI(p->mlTerrorSoundID); MI(p->mpTerrorSound != NULL);
	for(size_t i = 0; i < p->mvHeadPosAdds.size(); ++i) { MV(p->mvHeadPosAdds[i].mvAdd); MI(p->mvHeadPosAdds[i].mbMoving); }
	MF(W->cam.mfPitch); MF(W->cam.mfYaw); MF(W->cam.mfRoll);
	cLuxPlayerInsanityCollapse *c = W->collapse;
	MI(c->mbActive); MI(c->mlState); MF(c->mfHeightAdd); MF(c->mfRoll); MF(c->mfT); MF(c->mfRandomCount);
	cLuxPlayerCamDirEffects *d = W->camdir;
	MI(d->mbSwayActive); MF(d->mvSwayAdd.x); MF(d->mvSwayAdd.y); MF(d->mfSwayAlpha); MF(d->mvNextAdd.x); MF(d->mvNextAdd.y);
	MI(d->mlstPrevAdd.size());
	for(std::list<cVector2f>::iterator it = d->mlstPrevAdd.begin(); it != d->mlstPrevAdd.end(); ++it) { MF(it->x); MF(it->y); }
	cLuxPlayerHurt *h = W->hurt;
	MI(h->mlCurrentNoise); MF(h->mfNoiseUpdateCount); MF(h->mfPantCount); MF(h->mfPantPosAdd); MF(h->mfPantPosAddVel);
	MF(h->mfPantPosAddDir); MF(h->mfAlpha);
	// lookat z is uninitialised in the original
	cLuxPlayerLookAt *l = W->lookat;
	MF(l->mvCurrentSpeed.x); MF(l->mvCurrentSpeed.y);
	cLuxPlayerFlashback *f = W->flashback;
	MF(f->mfFlashDelay); MI(f->mbActive); MF(f->mfFlashbackStartCount); MI(f->mlstFlashbackQueue.size());
	cLuxPlayerSanity *s = W->sanity;
	MF(s->mfHitAlpha); MI(s->mbHitActive); MF(s->mfSanityLostCount); MF(s->mfAtLowSanityCount); MI(s->mbHitIsUpdated);
	MI(s->mbSanityLostIsUpdated); MF(s->mfPantCount); MF(s->mfCheckEnemySeenCount); MI(s->mbSanityEffectUpdated); MF(s->mfT);
	MF(s->mfInsaneWaveAlpha); MF(s->mfSanityDrainCount); MF(s->mfSanityDrainVolume); MF(s->mfSanityHeartbeatCount);
	MF(s->mfSeenEnemyCount); MI(s->mbEnemyIsSeen); MF(s->mfShowHintTimer);
	cLuxPlayerLantern *la = W->lantern;
	MI(la->mbActive); MF(la->mfAlpha);
	cLuxPlayerLightLevel *ll = W->lightLevel;
	MF(ll->mfExtendedLightLevel); MF(ll->mfNormalLightLevel); MF(ll->mfUpdateCount);
	cLuxPlayerInDarkness *dk = W->darkness;
	MI(dk->mbAmbientLightIsOn); MI(dk->mbInDarkness); MF(dk->mfLoopSoundCount); MF(dk->mfSanityLossMul); MF(dk->mfShowHintTimer);
	cLuxPlayerDeath *de = W->death;
	MI(de->mbActive); MI(de->mlState); MF(de->mfHeightAdd); MF(de->mfRoll); MF(de->mfT); MF(de->mfFadeAlpha);
	MF(de->mfTextAlpha1); MF(de->mfTextOnScreenCount); MF(de->mfWhiteCount); MI(de->mbToMainMenu);
	cLuxPlayerHands *ha = W->hands;
	MI(ha->mHandState); MI(ha->mpCurrentHandObject != NULL); MI(ha->mlstCamRotations.size());
	for(std::list<cVector3f>::iterator it = ha->mlstCamRotations.begin(); it != ha->mlstCamRotations.end(); ++it) MV(*it);
	for(size_t i = 0; i < ha->msCurrentAnim.size(); ++i) MI((unsigned char)ha->msCurrentAnim[i]);
}

static void Step(int act, int caseNo, int stepNo)
{
	cLuxPlayer *p = W->p;
	gLog.clear();
	float dt = DT();
	switch(act)
	{
	case 0: p->GiveDamage(Val(0, 60), RI(3), (eLuxDamageType)RI(3), RB(), RB()); break;
	case 1: p->SetHealth(Val(-5, 120)); break;
	case 2: p->SetSanity(Val(-5, 120)); break;
	case 3: p->LowerSanity(Val(0, 40), RB()); break;
	case 4: p->AddHealth(Val(-30, 60)); break;
	case 5: p->AddSanity(Val(-30, 60), RB()); break;
	case 6: p->AddLampOil(Val(-30, 60)); break;
	case 7:
		switch(RI(6))
		{
		case 0: p->FadePitchTo(Val(-1.5f, 1.5f), Val(0, 10), Val(0, 10)); break;
		case 1: p->FadeFOVMulTo(Val(0.5f, 1.5f), Val(0, 10)); break;
		case 2: p->FadeAspectMulTo(Val(0.5f, 1.5f), Val(0, 10)); break;
		case 3: p->FadeRollTo(Val(-1, 1), Val(0, 10), Val(0, 10)); break;
		case 4: p->FadeLeanRollTo(Val(-0.3f, 0.3f), Val(0, 10), Val(0, 10)); break;
		default: p->SetRoll(Val(-1, 1)); break;
		}
		break;
	case 8:
		p->UpdateHeadPosAdd(dt);
		p->UpdateHeadSpin(dt);
		p->UpdateCamera(dt);
		p->UpdateTerror(dt);
		p->UpdateFocusText(dt);
		p->UpdateAvgSpeed(dt);
		break;
	case 9:
	{
		int t = RI(eLuxHeadPosAdd_LastEnum);
		p->mvHeadPosAdds[t].mbMoving = true;
		p->mvHeadPosAdds[t].mvMoveGoal = V3(-1, 1);
		p->mvHeadPosAdds[t].mfMoveSpeed = Val(0, 3);
		p->mvHeadPosAdds[t].mfMoveSlowDownDist = Val(0, 0.2f);
		break;
	}
	case 10:
		W->hud = cLuxPlayerHudEffect();
		W->lightLevel->Update(dt);
		W->darkness->Update(dt);
		W->death->Update(dt);
		W->lantern->Update(dt);
		W->sanity->Update(dt);
		W->lookat->mbActive ? W->lookat->Update(dt) : (void)0;
		W->flashback->Update(dt);
		W->hurt->Update(dt);
		W->camdir->Update(dt);
		W->collapse->Update(dt);
		break;
	case 11: W->lantern->SetActive(RB(), RB(), RB(70), RB(70)); break;
	case 12:
	{
		float a = Val(-1, 1);
		float r = RB() ? W->camdir->AddAndGetYawAdd(a) : W->camdir->AddAndGetPitchAdd(a);
		Rec("camdir %08x", Bits(r));
		break;
	}
	case 13: p->SpinHead(Val(0, 10)); p->UpdateHeadSpin(dt); break;
	case 14:
		switch(RI(4))
		{
		case 0: W->hands->DoAction(eLuxPlayerAction_Lantern, RB()); break;
		case 1: W->hands->SetCurrentHandObject(RB(70) ? &W->handObj[RI(2)] : NULL); break;
		case 2: W->hands->Update(dt); break;
		default: W->hands->UpdatePlayerHandsPos(dt); break;
		}
		W->hands->mbAnimOver = RB();
		W->handObj[RI(2)].mbAnimOver = RB();
		break;
	case 15:
	{
		W->fx.mEmotion.mbOn = RB(20);
		W->fx.mVoice.mbDone = RB(40);
		W->msg.mbPause = RB(20);
		W->base.mbHardMode = RB(25);
		gFrustumIn = RB(70);
		if(RB(30))
		{
			cLuxFlashbackData fd;
			fd.msFile = "q2";
			fd.msCallback = "";
			W->flashback->mlstFlashbackQueue.push_back(fd);
		}
		p->mbFreeCameraActive = RB(10);
		for(size_t i = 0; i < W->enemies.size(); ++i) W->enemies[i]->mbActive = RB(80);
		W->sanity->mfSanityLostCount = RB(30) ? 1.0f : W->sanity->mfSanityLostCount;
		W->sanity->mbHitActive = RB(20);
		break;
	}
	case 16: W->sanity->UpdateCheckEnemySeen(dt); break;
	}
	gH = 14695981039346656037ull;
	Dump();
	for(size_t i = 0; i < gLog.size(); ++i) Mix((unsigned char)gLog[i]);
	if(gVerbose) printf("=== case %d step %d act %d\n%s%s", caseNo, stepNo, act, gLog.c_str(), gState.c_str());
	gState.clear();
	printf("S %d %d %d %016llx %zu\n", caseNo, stepNo, act, (unsigned long long)gH, gLog.size());
}

int main(int argc, char **argv)
{
	if(argc < 5) { fprintf(stderr, "usage: cfg.txt first last steps [verbose]\n"); return 2; }
	LoadCfg(argv[1]);
	int first = atoi(argv[2]), last = atoi(argv[3]), steps = atoi(argv[4]);
	gVerbose = argc > 5;
	if(getenv("A2_REC")) gRecFile = fopen(getenv("A2_REC"), "wb");
	Setup();
	for(int c = first; c < last; ++c)
	{
		gRng = 88172645463325252ull ^ ((uint64_t)(c + 1) * 0x9E3779B97F4A7C15ull);
		for(int i = 0; i < 4; ++i) R();
		srand(c + 1);
		gLog.clear();
		InitCase();
		for(int s = 0; s < steps; ++s)
		{
			int act = RI(100) < 40 ? 10 : RI(17);
			if(act == 10 && RB(30)) act = 8;
			Step(act, c, s);
		}
		printf("ENDCASE %d\n", c);
		if(gRecFile) fflush(gRecFile);
	}
	return 0;
}

namespace hpl { void Error(const char *, ...) {} cSerializeClass::cSerializeClass(const char *, const char *, cSerializeMemberField *, size_t, iSerializable *(*)()) {} }
