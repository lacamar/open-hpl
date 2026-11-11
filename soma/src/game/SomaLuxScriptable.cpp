#include "SomaLuxScriptable.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

#include <algorithm>
#include <cmath>
#include <memory>

uint64_t SomaHash64(const tString &asStr) { return ohpl_gs3_hash64((const unsigned char *)asStr.data(), asStr.size()); }

float SomaEasing(int alType, float t) { return ohpl_gs3_easing(alType, t); }

std::vector<cSomaLuxScriptable *> cSomaLuxScriptable::mvAll;
uint64_t cSomaLuxScriptable::mlAllVersion = 0;

cSomaLuxScriptable::cSomaLuxScriptable() : mpRuntime(NULL), mpScript(NULL)
{
	mvAll.push_back(this);
	++mlAllVersion;
}

cSomaLuxScriptable::~cSomaLuxScriptable()
{
	++mlAllVersion;
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

bool cSomaLuxScriptable::HasMethod(std::string_view asDecl)
{
	return mpScript && mpScript->GetObjectType()->GetMethodByDecl(std::string(asDecl).c_str()) != NULL;
}

bool cSomaLuxScriptable::CallBool(std::string_view asDecl, const std::function<void(asIScriptContext *)> &aSetArgs, bool abDefault)
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

bool cSomaLuxScriptable::Call(std::string_view asDecl, const std::function<void(asIScriptContext *)> &aSetArgs)
{
	return mpScript ? mpRuntime->Call(mpScript, asDecl, aSetArgs) : false;
}

bool cSomaLuxScriptable::CallWithFloat(std::string_view asDecl, float afX)
{
	return Call(asDecl, [afX](asIScriptContext *c) { c->SetArgFloat(0, afX); });
}

bool cSomaLuxScriptable::CallWithObject(std::string_view asDecl, void *apObj)
{
	return Call(asDecl, [apObj](asIScriptContext *c) { c->SetArgAddress(0, apObj); });
}

template <class T> static T *FindById(std::vector<T> &avItems, uint64_t alId)
{
	auto it = std::find_if(avItems.begin(), avItems.end(), [alId](const T &x) { return x.mlId == alId; });
	return it == avItems.end() ? NULL : &*it;
}

cSomaLuxScriptable::cTimer *cSomaLuxScriptable::FindTimer(uint64_t alId) { return FindById(mvTimers, alId); }
cSomaLuxScriptable::cFader *cSomaLuxScriptable::FindFader(uint64_t alId) { return FindById(mvFaders, alId); }

cSomaLuxScriptable::cFader *cSomaLuxScriptable::GetOrAddFader(uint64_t alId, bool abSkipIfExists)
{
	cFader *f = FindFader(alId);
	if (f)
		return abSkipIfExists ? NULL : f;
	mvFaders.push_back(cFader{alId, 0, 0, 0, 0, false, false});
	return &mvFaders.back();
}

void cSomaLuxScriptable::UpdateTimers(float afTimeStep)
{
	size_t lNum = mvTimers.size();
	std::vector<uint32_t> vDueIdx(lNum);
	std::unique_ptr<uint8_t[]> vKeep(new uint8_t[lNum]);
	size_t lDue = ohpl_gs3_timers_update(mvTimers.data(), lNum, sizeof(cTimer), afTimeStep, vDueIdx.data(), vKeep.get());
	std::vector<std::pair<uint64_t, tString>> vDue;
	for (size_t i = 0; i < lDue; ++i)
		vDue.emplace_back(mvTimers[vDueIdx[i]].mlId, mvTimers[vDueIdx[i]].msFunc);
	size_t lOut = 0;
	for (size_t i = 0; i < lNum; ++i)
		if (vKeep[i])
		{
			if (lOut != i)
				mvTimers[lOut] = std::move(mvTimers[i]);
			++lOut;
		}
	mvTimers.resize(lOut);
	for (auto &[lId, sFunc] : vDue)
	{
		if (sFunc.empty())
			continue;
		uint64_t lCallId = lId;
		Call("void " + sFunc + "(uint64)", [lCallId](asIScriptContext *c) { c->SetArgQWord(0, lCallId); });
	}

	ohpl_gs3_faders_update((OhplGs3Fader *)mvFaders.data(), mvFaders.size(), afTimeStep);
}

void cSomaLuxScriptable::Timer_Add(uint64_t alId, float afTime, const tString &asFunc, bool abCreateIfExist, bool abRepeat)
{
	cTimer *pTimer = FindTimer(alId);
	int lPlan = ohpl_gs3_timer_add(pTimer != NULL, abCreateIfExist);
	if (lPlan == 0)
		return;
	if (lPlan == 1)
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
	std::erase_if(mvTimers, [alId](const cTimer &t) { return t.mlId == alId; });
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
	return ohpl_gs3_timer_has_passed(t != NULL, t ? t->mfLength : 0, t ? t->mfTimeLeft : 0, afLength);
}

float cSomaLuxScriptable::Timer_GetValue(uint64_t alId, float afMin, float afMax, int alEasing, bool abAbs)
{
	cTimer *t = FindTimer(alId);
	return ohpl_gs3_timer_value(t != NULL, t ? t->mfLength : 0, t ? t->mfTimeLeft : 0, afMin, afMax, alEasing, abAbs);
}

void cSomaLuxScriptable::Fader_FadeTo(uint64_t alId, float afGoal, float afTime, bool abReverseAtEnd, bool abSkipIfExists)
{
	cFader *f = GetOrAddFader(alId, abSkipIfExists);
	if (f == NULL)
		return;
	f->mfStart = f->mfValue;
	f->mfGoal = afGoal;
	f->mfSpeed = ohpl_gs3_fade_speed(f->mfValue, afGoal, afTime);
	f->mbReverseAtEnd = abReverseAtEnd;
}

void cSomaLuxScriptable::Fader_Set(uint64_t alId, float afX, bool abSkipIfExists)
{
	if (cFader *f = GetOrAddFader(alId, abSkipIfExists))
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
	return ohpl_gs3_fader_value(f != NULL, f ? f->mfValue : 0, afMin, afMax, alEasing, abAbs);
}

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
