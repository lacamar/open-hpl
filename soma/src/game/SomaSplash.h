#ifndef SOMA_SPLASH_H
#define SOMA_SPLASH_H

#include "SomaS8Host.h"

class cSomaSplash : public iUpdateable
{
public:
	cSomaSplash(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaSplash"), mR(apEngine, apBase)
	{
		ohpl_s8_splash_new(&mR.mHost, &mR.mH);
	}
	~cSomaSplash() { ohpl_s8_splash_delete(&mR.mHost, mR.mH); }

	void Update(float afTimeStep) { ohpl_s8_splash_update(&mR.mHost, mR.mH, afTimeStep); }
	void OnDraw(float afFrameTime) { ohpl_s8_splash_draw(&mR.mHost, mR.mH); }

	bool ScriptsMayRun() { return ohpl_s8_splash_scripts_may_run(mR.mH); }
	void DrawLoadingScreen(bool abBoot = false) { ohpl_s8_splash_load_draw(&mR.mHost, mR.mH, abBoot); }
	void StartLoad() { ohpl_s8_splash_start_load(&mR.mHost, mR.mH); }
	void SetLoadBar(const cVector2f &avPos, const cVector2f &avSize) { ohpl_s8_splash_set_load_bar(mR.mH, avPos.v, avSize.v); }

private:
	cOhplS8Shell mR;
};

#endif // SOMA_SPLASH_H
