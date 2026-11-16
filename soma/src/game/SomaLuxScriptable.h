#ifndef SOMA_LUX_SCRIPTABLE_H
#define SOMA_LUX_SCRIPTABLE_H

#include "hpl.h"
#include "RustSomaLux.h"
#include "RustSomaLuxS5.h"

#include <angelscript.h>
#include <functional>

using namespace hpl;

class cSomaScriptRuntime;

inline float SomaEasing(int alType, float afT) { return ohpl_gs3_easing(alType, afT); }
inline uint64_t SomaHash64(const tString &asStr) { return ohpl_gs3_hash64((const unsigned char *)asStr.data(), asStr.size()); }

class cSomaLuxScriptable
{
public:
	cSomaLuxScriptable();
	virtual ~cSomaLuxScriptable();

	bool LoadScript(cSomaScriptRuntime *apRuntime, const tString &asFile, const tString &asClass, const tString &asBaseType);

	asIScriptObject *GetScript() { return mpScript; }

	tString msScriptName;
	bool mbSaved = true;
	static const std::vector<cSomaLuxScriptable *> &GetAll() { return mvAll; }
	static uint64_t GetAllVersion() { return mlAllVersion; }
	bool Call(std::string_view asDecl, const std::function<void(asIScriptContext *)> &aSetArgs = std::function<void(asIScriptContext *)>());
	bool CallWithFloat(std::string_view asDecl, float afX);
	bool CallWithObject(std::string_view asDecl, void *apObj);
	bool CallBool(std::string_view asDecl, const std::function<void(asIScriptContext *)> &aSetArgs, bool abDefault);
	bool HasMethod(std::string_view asDecl);

	// iLuxUpdateable callbacks as the engine dispatches them; overridden where the engine object adds behaviour
	virtual void OnMessage(const char *apDecl) { Call(apDecl); }
	virtual void OnUpdate(float afTimeStep);
	virtual void OnPostUpdate(float afTimeStep);
	virtual void OnGui(float afTimeStep) { CallWithFloat("void OnGui(float afTimeStep)", afTimeStep); }
	virtual void OnVariableUpdate(float afTimeStep);
	virtual void OnMapMessage(const char *apDecl, void *apMap);
	virtual void OnAction(int alAction, bool abPressed);
	virtual void OnAnalogInput(int alAnalogId, const cVector3f &avAmount);

	void UpdateTimers(float afTimeStep);
	bool TimersIdle() const { return ohpl_s5_ts_idle((void *)this); }

	static void RegisterTimerNatives(asIScriptEngine *apEngine, const char *apType) { ohpl_s5_register_timer_natives(apEngine, apType); }

protected:
	static std::vector<cSomaLuxScriptable *> mvAll;
	static uint64_t mlAllVersion;

	cSomaScriptRuntime *mpRuntime;
	asIScriptObject *mpScript;
};

#endif // SOMA_LUX_SCRIPTABLE_H
