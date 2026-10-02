#include "engine/DevHud.h"

#include "engine/Engine.h"
#include "graphics/FontData.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "math/Math.h"
#include "resources/FileSearcher.h"
#include "resources/FontManager.h"
#include "resources/Resources.h"
#include "system/String.h"

#include <GL/gl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fstream>
#include <thread>

#ifndef OPENHPL_VERSION
#define OPENHPL_VERSION "unknown"
#endif

namespace hpl {

	static tString ReadFirstLine(const char *apPath)
	{
		std::ifstream file(apPath);
		tString sLine;
		std::getline(file, sLine);
		return sLine.c_str();
	}

	static tString OsName()
	{
		std::ifstream file("/etc/os-release");
		tString sLine;
		while(std::getline(file, sLine))
		{
			if(sLine.rfind("PRETTY_NAME=", 0) == 0)
				return cString::ReplaceStringTo(sLine.substr(12), "\"", "");
		}
		return "Linux";
	}

	cDevHud::cDevHud(cEngine *apEngine)
	{
		mpEngine = apEngine;
		mpSet = NULL;
		mpFont = NULL;
		mpBackground = NULL;
	}

	void cDevHud::Render()
	{
		cGui *pGui = mpEngine->GetGui();
		iLowLevelGraphics *pLowGfx = mpEngine->GetGraphics()->GetLowLevel();

		if(mpSet == NULL)
		{
			for(const char *sFont : { "vera.fnt", "font_default.fnt", "default_small.fnt" })
			{
				if(mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sFont) == _W("")) continue;
				mpFont = mpEngine->GetResources()->GetFontManager()->CreateFontData(sFont);
				if(mpFont) break;
			}
			if(mpFont == NULL) return;

			mpSet = pGui->CreateSet("OpenHplDevHud", NULL);
			mpBackground = pGui->CreateGfxFilledRect(cColor(0, 0.6f), eGuiMaterial_Alpha);

			utsname uts;
			uname(&uts);
			tString sDevice = ReadFirstLine("/sys/firmware/devicetree/base/model");
			if(sDevice.empty()) sDevice = ReadFirstLine("/sys/class/dmi/id/product_name");
			long lMemGb = (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE) + (1L << 29)) >> 30;

			mvLines.push_back(_W("Open HPL ") + cString::To16Char(OPENHPL_VERSION));
			mvLines.push_back(cString::To16Char(sDevice + "  |  " + uts.machine + ", " +
				cString::ToString((int)std::thread::hardware_concurrency()) + " cores, " +
				cString::ToString((int)lMemGb) + " GB"));
			mvLines.push_back(cString::To16Char(OsName() + "  |  Linux " + uts.release));
			mvLines.push_back(cString::To16Char(tString((const char*)glGetString(GL_RENDERER)) +
				"  |  OpenGL " + (const char*)glGetString(GL_VERSION)));
		}

		const cVector2l &vScreen = pLowGfx->GetScreenSizeInt();
		mpSet->SetVirtualSize(cVector2f((float)vScreen.x, (float)vScreen.y), -1000, 1000, 0, true);

		float fSize = cMath::Max(14.0f, vScreen.y / 60.0f);
		cVector2f vFont(fSize);
		tWStringVec vLines = mvLines;
		wchar_t sStats[128];
		swprintf(sStats, 128, L"%dx%d  |  %.0f FPS  %.1f ms", vScreen.x, vScreen.y,
			mpEngine->GetFPS(), mpEngine->GetAvgFrameTimeInMS());
		vLines.push_back(sStats);

		float fWidth = 0;
		for(const tWString &sLine : vLines)
			fWidth = cMath::Max(fWidth, mpFont->GetLength(vFont, sLine.c_str()));

		float fPad = fSize * 0.5f;
		float fLineH = fSize * 1.25f;
		cVector3f vPos(fPad, fPad, 0);
		mpSet->DrawGfx(mpBackground, vPos, cVector2f(fWidth + fPad * 2, fLineH * vLines.size() + fPad * 2));

		vPos += cVector3f(fPad, fPad, 1);
		for(size_t i = 0; i < vLines.size(); ++i)
		{
			cColor col = i == 0 ? cColor(1, 0.75f, 0.2f, 1) : cColor(1, 1);
			mpSet->DrawFont(vLines[i], mpFont, vPos, vFont, col);
			vPos.y += fLineH;
		}

		pLowGfx->SetCurrentFrameBuffer(NULL);
		mpSet->Render(NULL);
	}

}
