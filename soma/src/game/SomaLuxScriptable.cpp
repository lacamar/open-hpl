#include "SomaLuxScriptable.h"
#include "SomaS5Dump.h"
#include "SomaScriptRuntime.h"

#include <map>

std::vector<cSomaLuxScriptable *> cSomaLuxScriptable::mvAll;
uint64_t cSomaLuxScriptable::mlAllVersion = 0;

cSomaLuxScriptable::cSomaLuxScriptable() : mpRuntime(NULL), mpScript(NULL)
{
	mvAll.push_back(this);
	++mlAllVersion;
	ohpl_s5_ts_new(this);
}

cSomaLuxScriptable::~cSomaLuxScriptable()
{
	++mlAllVersion;
	ohpl_s5_ts_free(this);
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

void cSomaLuxScriptable::UpdateTimers(float afTimeStep)
{
	ohpl_s5_ts_update(this, afTimeStep, +[](void *apCtx, uint64_t alId, const unsigned char *apFunc, size_t alLen) {
		cSomaLuxScriptable *pSelf = (cSomaLuxScriptable *)apCtx;
		tString sFunc((const char *)apFunc, alLen);
		if (OhplS5DumpEnabled())
			OhplS5Log("fire %s %s %llu", pSelf->msScriptName.c_str(), sFunc.c_str(), (unsigned long long)alId);
		pSelf->Call("void " + sFunc + "(uint64)", [alId](asIScriptContext *c) { c->SetArgQWord(0, alId); });
	}, this);

	if (OhplS5DumpEnabled())
	{
		static std::map<const cSomaLuxScriptable *, tString> smapLast;
		tString sNow;
		ohpl_s5_ts_describe(this, +[](void *apCtx, const unsigned char *apData, size_t alLen) { ((tString *)apCtx)->assign((const char *)apData, alLen); }, &sNow);
		tString &sLast = smapLast[this];
		if (sNow != sLast)
		{
			sLast = sNow;
			OhplS5Log("ts %s %s", msScriptName.c_str(), sNow.c_str());
		}
	}
}
