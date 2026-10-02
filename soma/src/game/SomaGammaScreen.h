#ifndef SOMA_GAMMA_SCREEN_H
#define SOMA_GAMMA_SCREEN_H

#include "hpl.h"

using namespace hpl;

class cSomaBase;

class cSomaGammaScreen : public iUpdateable
{
public:
	cSomaGammaScreen(cEngine *apEngine, cSomaBase *apBase);
	~cSomaGammaScreen();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	static bool ShouldShowAndMarkSeen();

private:
	void Finish();
	bool AnyContinueInputThisFrame();

	static bool GammaSliderMoved_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData);
	bool GammaSliderMoved(iWidget *apWidget, const cGuiMessageData &aData);

	static bool ContinuePressed_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData);
	bool ContinuePressed(iWidget *apWidget, const cGuiMessageData &aData);

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	cGuiGfxElement *mpBackgroundGfx;
	cGuiGfxElement *mpCheckerboardGfx;
	cVector2f mvCheckerboardPos;
	cVector2f mvCheckerboardSize;

	iFontData *mpInstructionsFont;
	tWStringVec mvInstructionRows;
	cVector2f mvInstructionsPos;	// top-left of the wrapped block, in screen pixels
	float mfInstructionsRowHeight;
	float mfInstructionsFontHeight;

	cWidgetSlider *mpSlider;
	cWidgetButton *mpContinueButton;

	float mfGammaMinValue;
	float mfGammaMaxValue;

	bool mbFinished;
	bool mbMouseWasDown;
};

#endif // SOMA_GAMMA_SCREEN_H
