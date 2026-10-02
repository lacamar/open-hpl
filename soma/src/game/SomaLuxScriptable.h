/*
 * Engine-side half of a SOMA script class: the object scripts see as their mBaseObj
 * (cLuxUserModule, cLuxEffect, cLuxMap, entities...). Owns the script instance, dispatches the
 * iLuxUpdateable_ScriptInterface callbacks and the per-class timers and faders
 * (cLuxScriptClassTimerContainer).
 */

#ifndef SOMA_LUX_SCRIPTABLE_H
#define SOMA_LUX_SCRIPTABLE_H

#include "hpl.h"

#include <angelscript.h>
#include <functional>

using namespace hpl;

class cSomaScriptRuntime;

float SomaEasing(int alType, float afT);
uint64_t SomaHash64(const tString &asStr);

class cSomaLuxScriptable
{
public:
	cSomaLuxScriptable();
	virtual ~cSomaLuxScriptable();

	// Compiles asFile, creates asClass and calls SetupBaseInterface(<asBaseType> @) with this
	bool LoadScript(cSomaScriptRuntime *apRuntime, const tString &asFile, const tString &asClass, const tString &asBaseType);

	asIScriptObject *GetScript() { return mpScript; }

	// Name cScript_RunGlobalFunc addresses this object by (entity, module, player state...)
	tString msScriptName;
	static const std::vector<cSomaLuxScriptable *> &GetAll() { return mvAll; }
	bool Call(const std::string &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs = std::function<void(asIScriptContext *)>());
	bool CallWithFloat(const std::string &asDecl, float afX);
	bool CallWithObject(const std::string &asDecl, void *apObj);
	// Result of a bool method, abDefault if it is missing or throws
	bool CallBool(const std::string &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs, bool abDefault);
	bool HasMethod(const std::string &asDecl);

	// iLuxUpdateable callbacks as the engine dispatches them; overridden where the engine object adds behaviour
	virtual void OnMessage(const char *apDecl) { Call(apDecl); }
	virtual void OnUpdate(float afTimeStep);
	virtual void OnPostUpdate(float afTimeStep);
	virtual void OnVariableUpdate(float afTimeStep);
	virtual void OnMapMessage(const char *apDecl, void *apMap);
	virtual void OnAction(int alAction, bool abPressed);
	virtual void OnAnalogInput(int alAnalogId, const cVector3f &avAmount);

	// Timers and faders, advanced before the script's own Update
	void UpdateTimers(float afTimeStep);

	void Timer_Add(uint64_t alId, float afTime, const tString &asFunc, bool abCreateIfExist, bool abRepeat);
	void Timer_Remove(uint64_t alId);
	bool Timer_Exists(uint64_t alId);
	float Timer_GetTimeLeft(uint64_t alId);
	void Timer_SetPaused(uint64_t alId, bool abX);
	bool Timer_TimeHasPassed(uint64_t alId, float afLength);
	float Timer_GetValue(uint64_t alId, float afMin, float afMax, int alEasing, bool abAbs);
	void Timer_ClearAll() { mvTimers.clear(); }

	void Fader_FadeTo(uint64_t alId, float afGoal, float afTime, bool abReverseAtEnd, bool abSkipIfExists);
	void Fader_Set(uint64_t alId, float afX, bool abSkipIfExists);
	void Fader_SetPaused(uint64_t alId, bool abPaused);
	float Fader_GetValue(uint64_t alId, float afMin, float afMax, int alEasing, bool abAbs);
	void Fader_ClearAll() { mvFaders.clear(); }

	// Registers the Timer_*/Fader_* script methods on a base type (all overloads)
	static void RegisterTimerNatives(asIScriptEngine *apEngine, const char *apType);

protected:
	friend class cSomaSaveState;
	struct cTimer
	{
		uint64_t mlId;
		float mfLength;
		float mfTimeLeft;
		tString msFunc;
		bool mbRepeat;
		bool mbPaused;
	};
	struct cFader
	{
		uint64_t mlId;
		float mfValue;
		float mfStart;
		float mfGoal;
		float mfSpeed;
		bool mbReverseAtEnd;
		bool mbPaused;
	};
	cTimer *FindTimer(uint64_t alId);
	cFader *FindFader(uint64_t alId);
	cFader *GetOrAddFader(uint64_t alId, bool abSkipIfExists);

	static std::vector<cSomaLuxScriptable *> mvAll;

	cSomaScriptRuntime *mpRuntime;
	asIScriptObject *mpScript;
	std::vector<cTimer> mvTimers;
	std::vector<cFader> mvFaders;
};

#endif // SOMA_LUX_SCRIPTABLE_H
