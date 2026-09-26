#include "SomaLuxScriptable.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

#include <cmath>

//---------------------------------------

// hpl::cString::GetHash64 in the official binary: FNV-style, characters taken from the end, sign-extended
uint64_t SomaHash64(const tString &asStr)
{
	if (asStr.empty())
		return 0;
	uint64_t h = 0x14650fb0739d0383ull;
	for (size_t i = asStr.size(); i-- > 0;)
		h = ((uint64_t)(int64_t)(signed char)asStr[i] ^ h) * 0x100000001b3ull;
	return h;
}

float SomaEasing(int alType, float t)
{
	const float kPi = 3.14159265f;
	auto bounceOut = [](float x) {
		if (x < 1 / 2.75f) return 7.5625f * x * x;
		if (x < 2 / 2.75f) { x -= 1.5f / 2.75f; return 7.5625f * x * x + 0.75f; }
		if (x < 2.5f / 2.75f) { x -= 2.25f / 2.75f; return 7.5625f * x * x + 0.9375f; }
		x -= 2.625f / 2.75f;
		return 7.5625f * x * x + 0.984375f;
	};
	const float c1 = 1.70158f, c2 = c1 * 1.525f, c3 = c1 + 1, c4 = (2 * kPi) / 3, c5 = (2 * kPi) / 4.5f;
	switch (alType)
	{
	case 1: return 1 - cosf(t * kPi / 2);
	case 2: return sinf(t * kPi / 2);
	case 3: return -(cosf(kPi * t) - 1) / 2;
	case 4: return t * t;
	case 5: return 1 - (1 - t) * (1 - t);
	case 6: return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
	case 7: return t * t * t;
	case 8: return 1 - powf(1 - t, 3);
	case 9: return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
	case 10: return t * t * t * t;
	case 11: return 1 - powf(1 - t, 4);
	case 12: return t < 0.5f ? 8 * powf(t, 4) : 1 - powf(-2 * t + 2, 4) / 2;
	case 13: return powf(t, 5);
	case 14: return 1 - powf(1 - t, 5);
	case 15: return t < 0.5f ? 16 * powf(t, 5) : 1 - powf(-2 * t + 2, 5) / 2;
	case 16: return t == 0 ? 0 : powf(2, 10 * t - 10);
	case 17: return t == 1 ? 1 : 1 - powf(2, -10 * t);
	case 18: return t == 0 ? 0 : t == 1 ? 1 : t < 0.5f ? powf(2, 20 * t - 10) / 2 : (2 - powf(2, -20 * t + 10)) / 2;
	case 19: return 1 - sqrtf(1 - t * t);
	case 20: return sqrtf(1 - (t - 1) * (t - 1));
	case 21: return t < 0.5f ? (1 - sqrtf(1 - 4 * t * t)) / 2 : (sqrtf(1 - powf(-2 * t + 2, 2)) + 1) / 2;
	case 22: return c3 * t * t * t - c1 * t * t;
	case 23: return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
	case 24: return t < 0.5f ? (powf(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2 : (powf(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
	case 25: return t == 0 ? 0 : t == 1 ? 1 : -powf(2, 10 * t - 10) * sinf((t * 10 - 10.75f) * c4);
	case 26: return t == 0 ? 0 : t == 1 ? 1 : powf(2, -10 * t) * sinf((t * 10 - 0.75f) * c4) + 1;
	case 27: return t == 0 ? 0 : t == 1 ? 1 : t < 0.5f ? -(powf(2, 20 * t - 10) * sinf((20 * t - 11.125f) * c5)) / 2
																	   : (powf(2, -20 * t + 10) * sinf((20 * t - 11.125f) * c5)) / 2 + 1;
	case 28: return 1 - bounceOut(1 - t);
	case 29: return bounceOut(t);
	case 30: return t < 0.5f ? (1 - bounceOut(1 - 2 * t)) / 2 : (1 + bounceOut(2 * t - 1)) / 2;
	default: return t;
	}
}

//---------------------------------------

std::vector<cSomaLuxScriptable *> cSomaLuxScriptable::mvAll;

cSomaLuxScriptable::cSomaLuxScriptable() : mpRuntime(NULL), mpScript(NULL)
{
	mvAll.push_back(this);
}

cSomaLuxScriptable::~cSomaLuxScriptable()
{
	for (size_t i = 0; i < mvAll.size(); ++i)
		if (mvAll[i] == this)
		{
			mvAll.erase(mvAll.begin() + i);
			break;
		}
	if (mpScript)
		mpScript->Release();
}

bool cSomaLuxScriptable::LoadScript(cSomaScriptRuntime *apRuntime, const tString &asFile, const tString &asClass, const tString &asBaseType)
{
	mpRuntime = apRuntime;
	mpScript = apRuntime->CreateObject(apRuntime->GetModule(asFile), asClass);
	if (mpScript == NULL)
		return false;
	CallWithObject("void SetupBaseInterface(" + asBaseType + " @aObj)", this);
	Call("void Init()");
	return true;
}

bool cSomaLuxScriptable::HasMethod(const std::string &asDecl)
{
	return mpScript && mpScript->GetObjectType()->GetMethodByDecl(asDecl.c_str()) != NULL;
}

bool cSomaLuxScriptable::CallBool(const std::string &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs, bool abDefault)
{
	bool bRet = abDefault;
	if (mpScript)
		mpRuntime->Call(mpScript, asDecl, aSetArgs, [&bRet](asIScriptContext *c) { bRet = c->GetReturnByte() != 0; });
	return bRet;
}

void cSomaLuxScriptable::OnUpdate(float afTimeStep)
{
	UpdateTimers(afTimeStep);
	CallWithFloat("void Update(float afTimeStep)", afTimeStep);
}

void cSomaLuxScriptable::OnPostUpdate(float afTimeStep)
{
	CallWithFloat("void PostUpdate(float afTimeStep)", afTimeStep);
}

void cSomaLuxScriptable::OnVariableUpdate(float afTimeStep)
{
	CallWithFloat("void VariableUpdate(float afDeltaTime)", afTimeStep);
}

void cSomaLuxScriptable::OnMapMessage(const char *apDecl, void *apMap)
{
	CallWithObject(apDecl, apMap);
}

void cSomaLuxScriptable::OnAction(int alAction, bool abPressed)
{
	auto args = [=](asIScriptContext *c) { c->SetArgDWord(0, alAction); c->SetArgByte(1, abPressed); };
	if (Call("void OnAction(int alAction, bool abPressed)", args) == false)
		Call("bool OnAction(int alAction, bool abPressed)", args);
}

void cSomaLuxScriptable::OnAnalogInput(int alAnalogId, const cVector3f &avAmount)
{
	auto args = [&](asIScriptContext *c) { c->SetArgDWord(0, alAnalogId); c->SetArgAddress(1, (void *)&avAmount); };
	if (Call("void OnAnalogInput(int alAnalogId, const cVector3f &in avAmount)", args) == false)
		Call("bool OnAnalogInput(int alAnalogId, const cVector3f &in avAmount)", args);
}

bool cSomaLuxScriptable::Call(const std::string &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	return mpScript ? mpRuntime->Call(mpScript, asDecl, aSetArgs) : false;
}

bool cSomaLuxScriptable::CallWithFloat(const std::string &asDecl, float afX)
{
	return Call(asDecl, [afX](asIScriptContext *c) { c->SetArgFloat(0, afX); });
}

bool cSomaLuxScriptable::CallWithObject(const std::string &asDecl, void *apObj)
{
	return Call(asDecl, [apObj](asIScriptContext *c) { c->SetArgAddress(0, apObj); });
}

bool cSomaLuxScriptable::CallWithString(const std::string &asDecl, const tString &asX)
{
	return Call(asDecl, [&asX](asIScriptContext *c) { c->SetArgObject(0, (void *)&asX); });
}

//---------------------------------------

cSomaLuxScriptable::cTimer *cSomaLuxScriptable::FindTimer(uint64_t alId)
{
	for (size_t i = 0; i < mvTimers.size(); ++i)
		if (mvTimers[i].mlId == alId)
			return &mvTimers[i];
	return NULL;
}

cSomaLuxScriptable::cFader *cSomaLuxScriptable::FindFader(uint64_t alId)
{
	for (size_t i = 0; i < mvFaders.size(); ++i)
		if (mvFaders[i].mlId == alId)
			return &mvFaders[i];
	return NULL;
}

void cSomaLuxScriptable::UpdateTimers(float afTimeStep)
{
	std::vector<cTimer> vDue;
	for (size_t i = 0; i < mvTimers.size();)
	{
		cTimer &t = mvTimers[i];
		if (t.mbPaused == false)
			t.mfTimeLeft -= afTimeStep;
		if (t.mbPaused == false && t.mfTimeLeft <= 0)
		{
			vDue.push_back(t);
			if (t.mbRepeat)
			{
				t.mfTimeLeft += t.mfLength;
				++i;
			}
			else
				mvTimers.erase(mvTimers.begin() + i);
		}
		else
			++i;
	}
	for (size_t i = 0; i < vDue.size(); ++i)
	{
		if (vDue[i].msFunc.empty())
			continue;
		uint64_t lId = vDue[i].mlId;
		Call("void " + vDue[i].msFunc + "(uint64)", [lId](asIScriptContext *c) { c->SetArgQWord(0, lId); });
	}

	for (size_t i = 0; i < mvFaders.size(); ++i)
	{
		cFader &f = mvFaders[i];
		if (f.mbPaused || f.mfValue == f.mfGoal)
			continue;
		float fStep = f.mfSpeed * afTimeStep;
		if (fabsf(f.mfGoal - f.mfValue) <= fStep)
		{
			f.mfValue = f.mfGoal;
			if (f.mbReverseAtEnd)
			{
				std::swap(f.mfStart, f.mfGoal);
				f.mbReverseAtEnd = false;
			}
		}
		else
			f.mfValue += f.mfGoal > f.mfValue ? fStep : -fStep;
	}
}

void cSomaLuxScriptable::Timer_Add(uint64_t alId, float afTime, const tString &asFunc, bool abCreateIfExist, bool abRepeat)
{
	cTimer *pTimer = FindTimer(alId);
	if (pTimer && abCreateIfExist == false)
		return;
	if (pTimer == NULL)
	{
		mvTimers.push_back(cTimer());
		pTimer = &mvTimers.back();
	}
	pTimer->mlId = alId;
	pTimer->mfLength = afTime;
	pTimer->mfTimeLeft = afTime;
	pTimer->msFunc = asFunc;
	pTimer->mbRepeat = abRepeat;
	pTimer->mbPaused = false;
}

void cSomaLuxScriptable::Timer_Remove(uint64_t alId)
{
	for (size_t i = 0; i < mvTimers.size(); ++i)
		if (mvTimers[i].mlId == alId)
		{
			mvTimers.erase(mvTimers.begin() + i);
			return;
		}
}

bool cSomaLuxScriptable::Timer_Exists(uint64_t alId) { return FindTimer(alId) != NULL; }

float cSomaLuxScriptable::Timer_GetTimeLeft(uint64_t alId)
{
	cTimer *t = FindTimer(alId);
	return t ? t->mfTimeLeft : 0;
}

void cSomaLuxScriptable::Timer_SetPaused(uint64_t alId, bool abX)
{
	if (cTimer *t = FindTimer(alId))
		t->mbPaused = abX;
}

bool cSomaLuxScriptable::Timer_TimeHasPassed(uint64_t alId, float afLength)
{
	cTimer *t = FindTimer(alId);
	return t == NULL || t->mfLength - t->mfTimeLeft >= afLength;
}

float cSomaLuxScriptable::Timer_GetValue(uint64_t alId, float afMin, float afMax, int alEasing, bool abAbs)
{
	cTimer *t = FindTimer(alId);
	float fT = (t == NULL || t->mfLength <= 0) ? 1.0f : 1.0f - t->mfTimeLeft / t->mfLength;
	fT = fT < 0 ? 0 : (fT > 1 ? 1 : fT);
	float fV = afMin + (afMax - afMin) * SomaEasing(alEasing, fT);
	return abAbs ? fabsf(fV) : fV;
}

void cSomaLuxScriptable::Fader_FadeTo(uint64_t alId, float afGoal, float afTime, bool abReverseAtEnd, bool abSkipIfExists)
{
	cFader *f = FindFader(alId);
	if (f && abSkipIfExists)
		return;
	if (f == NULL)
	{
		mvFaders.push_back(cFader{alId, 0, 0, 0, 0, false, false});
		f = &mvFaders.back();
	}
	f->mfStart = f->mfValue;
	f->mfGoal = afGoal;
	f->mfSpeed = afTime > 0 ? fabsf(afGoal - f->mfValue) / afTime : 1e9f;
	f->mbReverseAtEnd = abReverseAtEnd;
}

void cSomaLuxScriptable::Fader_Set(uint64_t alId, float afX, bool abSkipIfExists)
{
	cFader *f = FindFader(alId);
	if (f && abSkipIfExists)
		return;
	if (f == NULL)
	{
		mvFaders.push_back(cFader{alId, 0, 0, 0, 0, false, false});
		f = &mvFaders.back();
	}
	f->mfValue = f->mfStart = f->mfGoal = afX;
}

void cSomaLuxScriptable::Fader_SetPaused(uint64_t alId, bool abPaused)
{
	if (cFader *f = FindFader(alId))
		f->mbPaused = abPaused;
}

float cSomaLuxScriptable::Fader_GetValue(uint64_t alId, float afMin, float afMax, int alEasing, bool abAbs)
{
	cFader *f = FindFader(alId);
	float fT = f ? f->mfValue : 0;
	fT = fT < 0 ? 0 : (fT > 1 ? 1 : fT);
	float fV = afMin + (afMax - afMin) * SomaEasing(alEasing, fT);
	return abAbs ? fabsf(fV) : fV;
}

//---------------------------------------

static uint64_t Id(const tString &s) { return SomaHash64(s); }

void cSomaLuxScriptable::RegisterTimerNatives(asIScriptEngine *e, const char *T)
{
	typedef cSomaLuxScriptable S;
	SOMA_METHOD(e, T, "void Timer_ClearAll()", +[](S *s) { s->Timer_ClearAll(); });
	SOMA_METHOD(e, T, "void Timer_Add(uint64 alID, float afTime, const tString&in asFunc=\"\", bool abCreateIfExist=true, bool abRepeat=false)",
				+[](S *s, asQWORD id, float t, const tString &f, bool c, bool r) { s->Timer_Add(id, t, f, c, r); });
	SOMA_METHOD(e, T, "void Timer_Remove(uint64 alID)", +[](S *s, asQWORD id) { s->Timer_Remove(id); });
	SOMA_METHOD(e, T, "bool Timer_Exists(uint64 alID)", +[](S *s, asQWORD id) { return s->Timer_Exists(id); });
	SOMA_METHOD(e, T, "float Timer_GetTimeLeft(uint64 alID)", +[](S *s, asQWORD id) { return s->Timer_GetTimeLeft(id); });
	SOMA_METHOD(e, T, "void Timer_SetPaused(uint64 alID, bool abX)", +[](S *s, asQWORD id, bool b) { s->Timer_SetPaused(id, b); });
	SOMA_METHOD(e, T, "bool Timer_TimeHasPassed(uint64 alID, float afLength)", +[](S *s, asQWORD id, float l) { return s->Timer_TimeHasPassed(id, l); });
	SOMA_METHOD(e, T, "float Timer_GetValue(uint64 alID, float afMin=0, float afMax=1, eEasing aEasing=eEasing_Linear, bool abAbsValue=false)",
				+[](S *s, asQWORD id, float a, float b, int ease, bool abs) { return s->Timer_GetValue(id, a, b, ease, abs); });
	SOMA_METHOD(e, T, "void Timer_Add(const tString&in asID, float afTime, const tString&in asFunc=\"\", bool abCreateIfExist=true, bool abRepeat=false)",
				+[](S *s, const tString &id, float t, const tString &f, bool c, bool r) { s->Timer_Add(Id(id), t, f, c, r); });
	SOMA_METHOD(e, T, "void Timer_Remove(const tString&in asID)", +[](S *s, const tString &id) { s->Timer_Remove(Id(id)); });
	SOMA_METHOD(e, T, "bool Timer_Exists(const tString&in asID)", +[](S *s, const tString &id) { return s->Timer_Exists(Id(id)); });
	SOMA_METHOD(e, T, "float Timer_GetTimeLeft(const tString&in asID)", +[](S *s, const tString &id) { return s->Timer_GetTimeLeft(Id(id)); });
	SOMA_METHOD(e, T, "void Timer_SetPaused(const tString&in asID, bool abX)", +[](S *s, const tString &id, bool b) { s->Timer_SetPaused(Id(id), b); });
	SOMA_METHOD(e, T, "bool Timer_TimeHasPassed(const tString&in asID, float afLength)", +[](S *s, const tString &id, float l) { return s->Timer_TimeHasPassed(Id(id), l); });
	SOMA_METHOD(e, T, "float Timer_GetValue(const tString&in asID, float afMin=0, float afMax=1, eEasing aEasing=eEasing_Linear, bool abAbsValue=false)",
				+[](S *s, const tString &id, float a, float b, int ease, bool abs) { return s->Timer_GetValue(Id(id), a, b, ease, abs); });
	SOMA_METHOD(e, T, "void Fader_ClearAll()", +[](S *s) { s->Fader_ClearAll(); });
	SOMA_METHOD(e, T, "void Fader_FadeTo(uint alID, float afGoal, float afTime, bool abReverseAtEnd=false, bool abSkipIfExists=false)",
				+[](S *s, asUINT id, float g, float t, bool r, bool k) { s->Fader_FadeTo(id, g, t, r, k); });
	SOMA_METHOD(e, T, "void Fader_Set(uint alID, float afX, bool abSkipIfExists=false)", +[](S *s, asUINT id, float x, bool k) { s->Fader_Set(id, x, k); });
	SOMA_METHOD(e, T, "void Fader_SetPaused(uint alID, bool abPaused)", +[](S *s, asUINT id, bool p) { s->Fader_SetPaused(id, p); });
	SOMA_METHOD(e, T, "float Fader_GetValue(uint alID, float afMin=0, float afMax=1, eEasing aEasing=eEasing_Linear, bool abAbsValue=false)",
				+[](S *s, asUINT id, float a, float b, int ease, bool abs) { return s->Fader_GetValue(id, a, b, ease, abs); });
	SOMA_METHOD(e, T, "void Fader_FadeTo(const tString&in asName, float afGoal, float afTime, bool abReverseAtEnd=false, bool abSkipIfExists=false)",
				+[](S *s, const tString &n, float g, float t, bool r, bool k) { s->Fader_FadeTo(Id(n), g, t, r, k); });
	SOMA_METHOD(e, T, "void Fader_Set(const tString&in asName, float afX, bool abSkipIfExists=false)", +[](S *s, const tString &n, float x, bool k) { s->Fader_Set(Id(n), x, k); });
	SOMA_METHOD(e, T, "void Fader_SetPaused(const tString&in asName, bool abPaused)", +[](S *s, const tString &n, bool p) { s->Fader_SetPaused(Id(n), p); });
	SOMA_METHOD(e, T, "float Fader_GetValue(const tString&in asName, float afMin=0, float afMax=1, eEasing aEasing=eEasing_Linear, bool abAbsValue=false)",
				+[](S *s, const tString &n, float a, float b, int ease, bool abs) { return s->Fader_GetValue(Id(n), a, b, ease, abs); });
}
