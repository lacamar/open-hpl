#ifndef SOMA_SPLASH_H
#define SOMA_SPLASH_H

#include "hpl.h"

using namespace hpl;

class cSomaBase;

class cSomaSplash : public iUpdateable
{
public:
	cSomaSplash(cEngine *apEngine, cSomaBase *apBase);
	~cSomaSplash();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	bool ScriptsMayRun();
	void DrawLoadingScreen();

private:
	void Finish();

	bool AnySkipInputThisFrame();

	void DrawBrainIcon(float afAlpha);
	cVector3f VirtualToScreen(const cVector2f &avPos, float afZ);
	cVector2f VirtualSizeToScreen(const cVector2f &avSize);

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;
	float mfVirtualWidth;

	cGuiGfxElement *mpBlackBg;
	cGuiGfxElement *mpPremenuBg;
	cGuiGfxElement *mpLoadingBar;
	cGuiGfxElement *mpLoadingFrame;

	static const int mlBrainFrameCount = 26;
	cGuiGfxElement *mvBrainFrames[mlBrainFrameCount];

	// CreateChild() regions are only freed with their parent; one region, rect updated per frame
	cGuiClipRegion *mpBarClipRegion;

	float mfElapsed;

	bool mbFinished;
	bool mbMouseWasDown;
};

#endif // SOMA_SPLASH_H
