#ifndef HPL_DEV_HUD_H
#define HPL_DEV_HUD_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

union SDL_Event;

namespace hpl {

	class cEngine;
	class cGuiSet;
	class cGuiGfxElement;
	class iFontData;
	class cBitmap;

	class cDevHud
	{
	public:
		cDevHud(cEngine *apEngine);
		~cDevHud();

		void Render(bool abHud);

		void OpenBugReport();
		bool BugReportOpen(){ return mbBugOpen; }

	private:
		bool Init();
		bool FilterEvent(const SDL_Event &aEvent);
		void CloseBugReport();
		void SaveBugReport();
		void RenderBugReport(const cVector2l &avScreen);

		cEngine *mpEngine;
		cGuiSet *mpSet;
		iFontData *mpFont;
		cGuiGfxElement *mpBackground;
		tStringVec mvSystem;

		bool mbBugOpen;
		bool mbBugCapture;
		bool mbWasPaused;
		bool mbTextInputWasActive;
		unsigned long mlSavedTime;
		tWString msBugText;
		cBitmap *mpBugShot;
		tString msBugTime;
		tString msLastSaved;
	};

}

#endif
