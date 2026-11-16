#ifndef LUX_SCRIPT_HANDLER_H
#define LUX_SCRIPT_HANDLER_H

#include "LuxBase.h"
#include "RustLuxScriptA1.h"

// The script API is registered and implemented in rust/src/luxscript (LuxScriptHostA1.cpp is its engine side).
class cLuxScriptHandler : public iLuxUpdateable
{
public:
	cLuxScriptHandler() : iLuxUpdateable("LuxScriptHandler")
	{
		ohpl_a1b_set_host(OhplA1HostCall2);
		ohpl_a1_register(AddFunc, gpBase->mpEngine->GetSystem()->GetLowLevel());
	}

	void OnStart(){}
	void Update(float afTimeStep){}
	void Reset(){}
	void OnDraw(float afFrameTime){}

private:
	static void AddFunc(void *apLowLevel, const char *asDecl, const void *apFunc)
	{
		static_cast<iLowLevelSystem*>(apLowLevel)->AddScriptFunc(asDecl, const_cast<void*>(apFunc));
	}
};

#endif // LUX_SCRIPT_HANDLER_H
