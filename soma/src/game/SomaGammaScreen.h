#ifndef SOMA_GAMMA_SCREEN_H
#define SOMA_GAMMA_SCREEN_H

#include "SomaS8Host.h"

class cSomaGammaScreen : public iUpdateable
{
public:
	cSomaGammaScreen(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaGammaScreen"), mR(apEngine, apBase)
	{
		ohpl_s8_gamma_new(&mR.mHost, &mR.mH);
	}
	~cSomaGammaScreen() { ohpl_s8_gamma_delete(mR.mH); }

	void Update(float afTimeStep) { ohpl_s8_gamma_update(&mR.mHost, mR.mH); }
	void OnDraw(float afFrameTime) { ohpl_s8_gamma_draw(&mR.mHost, mR.mH); }

	static bool ShouldShowAndMarkSeen()
	{
		cOhplS8Shell shell(NULL, NULL);
		return ohpl_s8_gamma_should_show(&shell.mHost);
	}

private:
	cOhplS8Shell mR;
};

#endif // SOMA_GAMMA_SCREEN_H
