#include "engine/DevHud.h"

#include "engine/Engine.h"
#include "graphics/Bitmap.h"
#include "impl/LowLevelInputSDL.h"
#include "input/Input.h"
#include "resources/BitmapLoaderHandler.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "scene/Viewport.h"
#include "scene/World.h"
#include "system/EngineDiagnostics.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
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
#include <SDL2/SDL.h>
#include <ctime>
#include <filesystem>
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

	static tString JsonVec(const cVector3f &v)
	{
		return "[" + cString::ToString(v.x) + "," + cString::ToString(v.y) + "," + cString::ToString(v.z) + "]";
	}

	static tString JsonStr(const tString &s)
	{
		return "\"" + cEngineDiagnostics::JsonEscape(s) + "\"";
	}

	cDevHud::cDevHud(cEngine *apEngine)
	{
		mpEngine = apEngine;
		mpSet = NULL;
		mpFont = NULL;
		mpBackground = NULL;
		mbBugOpen = false;
		mbBugCapture = false;
		mbWasPaused = false;
		mpBugShot = NULL;
		mlSavedTime = 0;

		static_cast<cLowLevelInputSDL*>(apEngine->GetInput()->GetLowLevel())->mEventFilter =
			[this](const SDL_Event &aEvent) { return FilterEvent(aEvent); };
	}

	cDevHud::~cDevHud()
	{
		static_cast<cLowLevelInputSDL*>(mpEngine->GetInput()->GetLowLevel())->mEventFilter = nullptr;
		if(mpBugShot) hplDelete(mpBugShot);
	}

	bool cDevHud::Init()
	{
		if(mpSet) return true;

		for(const char *sFont : { "vera.fnt", "font_default.fnt", "default_small.fnt" })
		{
			if(mpEngine->GetResources()->GetFileSearcher()->GetFilePath(sFont) == _W("")) continue;
			mpFont = mpEngine->GetResources()->GetFontManager()->CreateFontData(sFont);
			if(mpFont) break;
		}
		if(mpFont == NULL) return false;

		mpSet = mpEngine->GetGui()->CreateSet("OpenHplDevHud", NULL);
		mpBackground = mpEngine->GetGui()->CreateGfxFilledRect(cColor(0, 0.6f), eGuiMaterial_Alpha);

		utsname uts;
		uname(&uts);
		tString sDevice = ReadFirstLine("/sys/firmware/devicetree/base/model");
		if(sDevice.empty()) sDevice = ReadFirstLine("/sys/class/dmi/id/product_name");
		long lMemGb = (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE) + (1L << 29)) >> 30;

		mvSystem.push_back("Open HPL " OPENHPL_VERSION);
		mvSystem.push_back(sDevice + "  |  " + uts.machine + ", " +
			cString::ToString((int)std::thread::hardware_concurrency()) + " cores, " +
			cString::ToString((int)lMemGb) + " GB");
		mvSystem.push_back(OsName() + "  |  Linux " + uts.release);
		mvSystem.push_back(tString((const char*)glGetString(GL_RENDERER)) +
			"  |  OpenGL " + (const char*)glGetString(GL_VERSION));
		return true;
	}

	void cDevHud::Render(bool abHud)
	{
		bool bToast = mlSavedTime && cPlatform::GetApplicationTime() - mlSavedTime < 4000;
		if(!abHud && !mbBugOpen && !bToast) return;
		if(!Init()) return;

		iLowLevelGraphics *pLowGfx = mpEngine->GetGraphics()->GetLowLevel();

		if(mbBugCapture)
		{
			mbBugCapture = false;
			mpBugShot = pLowGfx->CopyFrameBufferToBitmap();
			if(mpBugShot && mpBugShot->GetBytesPerPixel() == 4)
			{
				cBitmapData *pData = mpBugShot->GetData(0,0);
				for(size_t i = 3; i < (size_t)pData->mlSize; i += 4) pData->mpData[i] = 255;
			}
		}

		const cVector2l &vScreen = pLowGfx->GetScreenSizeInt();
		mpSet->SetVirtualSize(cVector2f((float)vScreen.x, (float)vScreen.y), -1000, 1000, 0, true);

		if(abHud)
		{
			float fSize = cMath::Max(14.0f, vScreen.y / 60.0f);
			cVector2f vFont(fSize);
			tWStringVec vLines;
			for(const tString &sLine : mvSystem) vLines.push_back(cString::UTF8ToWChar(sLine));
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
		}

		if(mbBugOpen || bToast) RenderBugReport(vScreen);

		pLowGfx->SetCurrentFrameBuffer(NULL);
		mpSet->Render(NULL);
	}

	void cDevHud::RenderBugReport(const cVector2l &avScreen)
	{
		float fSize = cMath::Max(16.0f, avScreen.y / 45.0f);
		cVector2f vFont(fSize);
		float fPad = fSize * 0.75f;
		float fLineH = fSize * 1.25f;
		float fBoxW = avScreen.x * 0.6f;
		float fTextW = fBoxW - fPad * 2;

		tWStringVec vLines;
		if(mbBugOpen)
		{
			vLines.push_back(_W("Bug report  -  Enter: save   Shift+Enter: new line   Esc: cancel"));
			tWString sText = msBugText + ((cPlatform::GetApplicationTime() / 500) % 2 ? _W("_") : _W(" "));
			tWString sLine;
			for(wchar_t c : sText)
			{
				if(c == L'\n') { vLines.push_back(sLine); sLine.clear(); continue; }
				sLine += c;
				if(mpFont->GetLength(vFont, sLine.c_str()) > fTextW)
				{
					size_t lSpace = sLine.find_last_of(L' ');
					size_t lCut = lSpace == tWString::npos || lSpace == 0 ? sLine.size() - 1 : lSpace + 1;
					vLines.push_back(sLine.substr(0, lCut));
					sLine = sLine.substr(lCut);
				}
			}
			vLines.push_back(sLine);
		}
		else
		{
			vLines.push_back(_W("Bug report saved"));
			vLines.push_back(cString::UTF8ToWChar(msLastSaved));
			fBoxW = cMath::Max(fBoxW, mpFont->GetLength(vFont, vLines[1].c_str()) + fPad * 2);
		}

		float fBoxH = fLineH * cMath::Max((int)vLines.size(), 4) + fPad * 2;
		cVector3f vPos((avScreen.x - fBoxW) * 0.5f, avScreen.y * 0.25f, 10);
		mpSet->DrawGfx(mpBackground, vPos, cVector2f(fBoxW, fBoxH));

		vPos += cVector3f(fPad, fPad, 1);
		for(size_t i = 0; i < vLines.size(); ++i)
		{
			cColor col = i == 0 ? cColor(1, 0.75f, 0.2f, 1) : cColor(1, 1);
			mpSet->DrawFont(vLines[i], mpFont, vPos, vFont, col);
			vPos.y += fLineH;
		}
	}

	void cDevHud::OpenBugReport()
	{
		if(mbBugOpen) return;
		mbBugOpen = true;
		mbBugCapture = true;
		mlSavedTime = 0;
		msBugText.clear();
		if(mpBugShot) hplDelete(mpBugShot);
		mpBugShot = NULL;

		time_t t = time(NULL);
		char sTime[32];
		strftime(sTime, sizeof(sTime), "%Y%m%d-%H%M%S", localtime(&t));
		msBugTime = sTime;

		mbWasPaused = mpEngine->GetPaused();
		mpEngine->SetPaused(true);
		mbTextInputWasActive = SDL_IsTextInputActive();
		SDL_StartTextInput();
	}

	void cDevHud::CloseBugReport()
	{
		mbBugOpen = false;
		if(mpBugShot) hplDelete(mpBugShot);
		mpBugShot = NULL;
		mpEngine->SetPaused(mbWasPaused);
		if(!mbTextInputWasActive) SDL_StopTextInput();
	}

	bool cDevHud::FilterEvent(const SDL_Event &aEvent)
	{
		if(!mbBugOpen)
		{
			if(aEvent.type == SDL_KEYDOWN && aEvent.key.keysym.sym == SDLK_F9 && !aEvent.key.repeat)
			{
				OpenBugReport();
				return true;
			}
			return false;
		}

		switch(aEvent.type)
		{
		case SDL_TEXTINPUT:
			msBugText += cString::UTF8ToWChar(aEvent.text.text);
			return true;
		case SDL_KEYDOWN:
		{
			SDL_Keycode key = aEvent.key.keysym.sym;
			bool bCtrl = aEvent.key.keysym.mod & KMOD_CTRL;
			if(key == SDLK_ESCAPE) CloseBugReport();
			else if((key == SDLK_RETURN || key == SDLK_KP_ENTER) && (aEvent.key.keysym.mod & KMOD_SHIFT)) msBugText += L'\n';
			else if(key == SDLK_RETURN || key == SDLK_KP_ENTER) { SaveBugReport(); CloseBugReport(); }
			else if(key == SDLK_BACKSPACE && bCtrl)
			{
				size_t lEnd = msBugText.find_last_not_of(L" \n");
				size_t lCut = lEnd == tWString::npos ? 0 : msBugText.find_last_of(L" \n", lEnd);
				msBugText.resize(lCut == tWString::npos ? 0 : lCut + 1);
			}
			else if(key == SDLK_BACKSPACE && !msBugText.empty()) msBugText.pop_back();
			else if(key == SDLK_v && bCtrl && SDL_HasClipboardText())
			{
				char *pClip = SDL_GetClipboardText();
				msBugText += cString::UTF8ToWChar(pClip);
				SDL_free(pClip);
			}
			return true;
		}
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL:
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERAXISMOTION:
			return true;
		}
		return false;
	}

	void cDevHud::SaveBugReport()
	{
		namespace fs = std::filesystem;
		Init();

		fs::path root = fs::path(cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome)).c_str()) / "open-hpl" / "bugs";
		tString sId = msBugTime;
		fs::path dir = root / sId.c_str();
		for(int i = 2; fs::exists(dir); ++i) dir = root / (sId + "-" + cString::ToString(i)).c_str();
		sId = dir.filename().string().c_str();
		std::error_code ec;
		fs::create_directories(dir, ec);
		if(ec)
		{
			Error("Bug report: can't create %s: %s\n", dir.c_str(), ec.message().c_str());
			return;
		}

		tString sFiles;
		if(mpBugShot && mpEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(
			mpBugShot, cString::UTF8ToWChar((dir / "screenshot.png").c_str()), 0))
			sFiles += "\"screenshot\":\"screenshot.png\"";

		tString sDesc = cString::S16BitToUTF8(msBugText);
		Log("Bug report %s: %s\n", sId.c_str(), sDesc.c_str());

		tString sLog = cString::To8Char(GetLogFile());
		if(fs::copy_file(sLog.c_str(), dir / "hpl.log", ec))
			sFiles += tString(sFiles.empty() ? "" : ",") + "\"log\":\"hpl.log\"";

		cViewport *pViewport = NULL;
		for(cViewport *pVp : mpEngine->GetScene()->GetViewportList())
		{
			if(pVp->IsActive() && pVp->IsVisible() && pVp->GetWorld() && pVp->GetCamera()) { pViewport = pVp; break; }
		}

		char sCwd[4096] = "";
		if(!getcwd(sCwd, sizeof(sCwd))) sCwd[0] = 0;
		char sExe[4096] = "";
		ssize_t lExe = readlink("/proc/self/exe", sExe, sizeof(sExe) - 1);
		sExe[lExe > 0 ? lExe : 0] = 0;

		tString sArgs;
		std::ifstream cmdline("/proc/self/cmdline");
		for(std::string sArg; std::getline(cmdline, sArg, '\0');)
			sArgs += tString(sArgs.empty() ? "" : ",") + JsonStr(sArg.c_str());

		tString sEnv;
		for(char **pEnv = environ; *pEnv; ++pEnv)
		{
			tString sVar = *pEnv;
			if(sVar.rfind("OPENHPL_", 0) != 0 && sVar.rfind("OHPL_", 0) != 0) continue;
			size_t lEq = sVar.find('=');
			sEnv += tString(sEnv.empty() ? "" : ",") + JsonStr(sVar.substr(0, lEq)) + ":" + JsonStr(sVar.substr(lEq + 1));
		}

		tString sSystem;
		for(const tString &sLine : mvSystem) sSystem += tString(sSystem.empty() ? "" : ",") + JsonStr(sLine);

		const cVector2l &vScreen = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
		tString sJson = "{\"id\":" + JsonStr(sId) +
			",\"time\":" + JsonStr(msBugTime) +
			",\"description\":" + JsonStr(sDesc) +
			",\"version\":" + JsonStr(OPENHPL_VERSION) +
			",\"exe\":" + JsonStr(sExe) +
			",\"cwd\":" + JsonStr(sCwd) +
			",\"args\":[" + sArgs + "]" +
			",\"env\":{" + sEnv + "}" +
			",\"system\":[" + sSystem + "]" +
			",\"resolution\":[" + cString::ToString(vScreen.x) + "," + cString::ToString(vScreen.y) + "]" +
			",\"fps\":" + cString::ToString(mpEngine->GetFPS()) +
			",\"frame_ms\":" + cString::ToString(mpEngine->GetAvgFrameTimeInMS()) +
			",\"game_time\":" + cString::ToString((float)mpEngine->GetGameTime());
		if(pViewport)
		{
			cCamera *pCam = pViewport->GetCamera();
			sJson += ",\"map\":" + JsonStr(pViewport->GetWorld()->GetName()) +
				",\"camera\":{\"pos\":" + JsonVec(pCam->GetPosition()) +
				",\"forward\":" + JsonVec(pCam->GetForward()) + "}" +
				",\"world\":" + cEngineDiagnostics::GetWorldStatsJson(pViewport->GetWorld()) +
				",\"render\":" + cEngineDiagnostics::GetRenderStatsJson(pViewport, mpEngine->GetGraphics());
		}
		sJson += ",\"files\":{" + sFiles + "}}";

		std::ofstream(dir / "report.json") << sJson << "\n";
		std::ofstream(root / "index.jsonl", std::ios::app) << "{\"id\":" << JsonStr(sId) <<
			",\"map\":" << JsonStr(pViewport ? pViewport->GetWorld()->GetName() : "") <<
			",\"description\":" << JsonStr(sDesc) << ",\"dir\":" << JsonStr(dir.c_str()) << "}\n";

		msLastSaved = dir.c_str();
		mlSavedTime = cPlatform::GetApplicationTime();
	}

}
