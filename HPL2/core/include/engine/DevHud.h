#ifndef HPL_DEV_HUD_H
#define HPL_DEV_HUD_H

#include "system/SystemTypes.h"

namespace hpl {

	class cEngine;
	class cGuiSet;
	class cGuiGfxElement;
	class iFontData;

	class cDevHud
	{
	public:
		cDevHud(cEngine *apEngine);

		void Render();

	private:
		cEngine *mpEngine;
		cGuiSet *mpSet;
		iFontData *mpFont;
		cGuiGfxElement *mpBackground;
		tWStringVec mvLines;
	};

}

#endif
