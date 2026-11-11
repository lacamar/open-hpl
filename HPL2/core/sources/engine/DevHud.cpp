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
#include "engine/RustEngine.h"
#include "resources/FileSearcher.h"
#include "resources/FontManager.h"
#include "resources/Resources.h"
#include "system/String.h"

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

	static tString ReadFile(const char *apPath)
	{
		std::ifstream file(apPath, std::ios::binary);
		return tString(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	static tString ReadFirstLine(const char *apPath)
	{
		tString sFile = ReadFile(apPath);
		tString sLine;
		ohpl_en_first_line(OhplEnP(sFile), sFile.size(), OhplEnAssign, &sLine);
		return sLine;
	}

	static tString OsName()
	{
		tString sFile = ReadFile("/etc/os-release");
		tString sName;
		ohpl_en_os_name(OhplEnP(sFile), sFile.size(), OhplEnAssign, &sName);
		return sName;
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
		long lMemGb = ohpl_en_mem_gb(sysconf(_SC_PHYS_PAGES), sysconf(_SC_PAGESIZE));

		tString sOs = OsName();
		tString sMachine = uts.machine;
		tString sKernel = uts.release;
		tString sVersion = OPENHPL_VERSION;
		OhplEnSystemInfo Info = { OhplEnB(sVersion), OhplEnB(sDevice), OhplEnB(sMachine), (int)std::thread::hardware_concurrency(), (int)lMemGb, OhplEnB(sOs), OhplEnB(sKernel) };
		ohpl_en_system_lines(&Info, OhplEnPush, &mvSystem);
		mvSystem.push_back(mpEngine->GetGraphics()->GetLowLevel()->GetRendererDescription());
		return true;
	}

	void cDevHud::Render(bool abHud)
	{
		bool bToast = mlSavedTime && ohpl_en_toast_active(mlSavedTime, cPlatform::GetApplicationTime());
		if(!ohpl_en_should_render(abHud, mbBugOpen, bToast)) return;
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
			if(mpBugShot) cEngineDiagnostics::ApplyDisplayGamma(mpBugShot, pLowGfx->GetGammaCorrection());
		}

		const cVector2l &vScreen = pLowGfx->GetScreenSizeInt();
		mpSet->SetVirtualSize(cVector2f((float)vScreen.x, (float)vScreen.y), -1000, 1000, 0, true);

		if(abHud)
		{
			float fSize = ohpl_en_hud_font_size(vScreen.y);
			cVector2f vFont(fSize);
			tWStringVec vLines;
			for(const tString &sLine : mvSystem) vLines.push_back(cString::UTF8ToWChar(sLine));
			tString sStats;
			ohpl_en_hud_stats_line(vScreen.x, vScreen.y, mpEngine->GetFPS(), mpEngine->GetAvgFrameTimeInMS(), OhplEnAssign, &sStats);
			vLines.push_back(cString::UTF8ToWChar(sStats));

			float fWidth = 0;
			for(const tWString &sLine : vLines)
				fWidth = cMath::Max(fWidth, mpFont->GetLength(vFont, sLine.c_str()));

			OhplEnHudLayout Layout = ohpl_en_hud_layout(fSize, fWidth, vLines.size());
			cVector3f vPos(Layout.pad, Layout.pad, 0);
			mpSet->DrawGfx(mpBackground, vPos, cVector2f(Layout.bgW, Layout.bgH));

			vPos = cVector3f(Layout.textX, Layout.textY, 1);
			for(size_t i = 0; i < vLines.size(); ++i)
			{
				cColor col = i == 0 ? cColor(1, 0.75f, 0.2f, 1) : cColor(1, 1);
				mpSet->DrawFont(vLines[i], mpFont, vPos, vFont, col);
				vPos.y += Layout.lineH;
			}
		}

		if(mbBugOpen || bToast) RenderBugReport(vScreen);

		pLowGfx->SetCurrentFrameBuffer(NULL);
		mpSet->Render(NULL);
	}

	void cDevHud::RenderBugReport(const cVector2l &avScreen)
	{
		OhplEnBugMetrics Metrics = ohpl_en_bug_metrics(avScreen.x, avScreen.y);
		cVector2f vFont(Metrics.font);
		float fBoxW = Metrics.boxW;

		tWStringVec vLines;
		if(mbBugOpen)
		{
			vLines.push_back(_W("Bug report  -  Enter: save   Shift+Enter: new line   Esc: cancel"));
			tWString sText = msBugText;
			sText += (wchar_t)ohpl_en_cursor_char(cPlatform::GetApplicationTime());

			struct tWrapCtx { iFontData *mpFont; cVector2f mvFont; tWStringVec *mpLines; } Ctx = { mpFont, vFont, &vLines };
			ohpl_en_wrap_bug_text((const uint32_t*)sText.data(), sText.size(), Metrics.textW,
				[](void *apCtx, const uint32_t *apText, size_t alLen) -> float
				{
					tWrapCtx *pCtx = static_cast<tWrapCtx*>(apCtx);
					tWString sLine((const wchar_t*)apText, alLen);
					return pCtx->mpFont->GetLength(pCtx->mvFont, sLine.c_str());
				},
				[](void *apCtx, const uint32_t *apText, size_t alLen)
				{
					static_cast<tWrapCtx*>(apCtx)->mpLines->push_back(tWString((const wchar_t*)apText, alLen));
				}, &Ctx);
		}
		else
		{
			vLines.push_back(_W("Bug report saved"));
			vLines.push_back(cString::UTF8ToWChar(msLastSaved));
			fBoxW = ohpl_en_bug_toast_box_w(Metrics, mpFont->GetLength(vFont, vLines[1].c_str()));
		}

		OhplEnBugBox Box = ohpl_en_bug_box(Metrics, fBoxW, vLines.size(), avScreen.x, avScreen.y);
		cVector3f vPos(Box.x, Box.y, 10);
		mpSet->DrawGfx(mpBackground, vPos, cVector2f(Box.w, Box.h));

		vPos = cVector3f(Box.textX, Box.textY, 11);
		for(size_t i = 0; i < vLines.size(); ++i)
		{
			cColor col = i == 0 ? cColor(1, 0.75f, 0.2f, 1) : cColor(1, 1);
			mpSet->DrawFont(vLines[i], mpFont, vPos, vFont, col);
			vPos.y += Metrics.lineH;
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
			if(ohpl_en_opens_report(aEvent.type, aEvent.type == SDL_KEYDOWN ? (int)aEvent.key.keysym.sym : 0, aEvent.type == SDL_KEYDOWN && aEvent.key.repeat))
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
			bool bCtrl = aEvent.key.keysym.mod & KMOD_CTRL;
			bool bShift = aEvent.key.keysym.mod & KMOD_SHIFT;
			switch(ohpl_en_key_action((int)aEvent.key.keysym.sym, bCtrl, bShift, msBugText.empty()))
			{
			case 1: CloseBugReport(); break;
			case 2: msBugText += L'\n'; break;
			case 3: SaveBugReport(); CloseBugReport(); break;
			case 4:
			{
				tWString sNew;
				ohpl_en_ctrl_backspace((const uint32_t*)msBugText.data(), msBugText.size(), OhplEnWAssign, &sNew);
				msBugText = sNew;
				break;
			}
			case 5: msBugText.pop_back(); break;
			case 6:
				if(SDL_HasClipboardText())
				{
					char *pClip = SDL_GetClipboardText();
					msBugText += cString::UTF8ToWChar(pClip);
					SDL_free(pClip);
				}
				break;
			}
			return true;
		}
		}
		return ohpl_en_swallowed_event(aEvent.type);
	}

	void cDevHud::SaveBugReport()
	{
		namespace fs = std::filesystem;
		Init();

		fs::path root = fs::path(cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome)).c_str()) / "open-hpl" / "bugs";
		tString sId;
		{
			struct tNameCtx { fs::path mRoot; tString msId; } NameCtx = { root, "" };
			ohpl_en_unique_name(OhplEnP(msBugTime), msBugTime.size(),
				[](void *apCtx, const uint8_t *apName, size_t alLen) -> bool
				{
					return fs::exists(static_cast<tNameCtx*>(apCtx)->mRoot / tString((const char*)apName, alLen).c_str());
				},
				[](void *apCtx, const uint8_t *apName, size_t alLen)
				{
					static_cast<tNameCtx*>(apCtx)->msId.assign((const char*)apName, alLen);
				}, &NameCtx);
			sId = NameCtx.msId;
		}
		fs::path dir = root / sId.c_str();
		std::error_code ec;
		fs::create_directories(dir, ec);
		if(ec)
		{
			Error("Bug report: can't create %s: %s\n", dir.c_str(), ec.message().c_str());
			return;
		}

		bool bShotSaved = mpBugShot && mpEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(
			mpBugShot, cString::UTF8ToWChar((dir / "screenshot.png").c_str()), 0);

		tString sDesc = cString::S16BitToUTF8(msBugText);
		Log("Bug report %s: %s\n", sId.c_str(), sDesc.c_str());

		tString sLog = cString::To8Char(GetLogFile());
		bool bLogCopied = fs::copy_file(sLog.c_str(), dir / "hpl.log", ec);

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

		tString sCmdline = ReadFile("/proc/self/cmdline");
		tStringVec vArgs;
		ohpl_en_cmdline_args(OhplEnP(sCmdline), sCmdline.size(), OhplEnPush, &vArgs);

		std::vector<std::pair<tString, tString> > vEnv;
		for(char **pEnv = environ; *pEnv; ++pEnv)
		{
			tString sVar = *pEnv;
			tStringVec vPair;
			if(!ohpl_en_env_pair(OhplEnP(sVar), sVar.size(), OhplEnPush, &vPair)) continue;
			vEnv.push_back(std::make_pair(vPair[0], vPair[1]));
		}

		tString sFiles;
		ohpl_en_files_json(bShotSaved, bLogCopied, OhplEnAssign, &sFiles);

		std::vector<OhplEnBytes> vArgBytes;
		for(const tString &sArg : vArgs) vArgBytes.push_back(OhplEnB(sArg));
		std::vector<OhplEnBytesPair> vEnvBytes;
		for(const std::pair<tString, tString> &Var : vEnv) vEnvBytes.push_back(OhplEnBytesPair{ OhplEnB(Var.first), OhplEnB(Var.second) });
		std::vector<OhplEnBytes> vSystemBytes;
		for(const tString &sLine : mvSystem) vSystemBytes.push_back(OhplEnB(sLine));

		const cVector2l &vScreen = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
		tString sVersion = OPENHPL_VERSION;
		tString sExeStr = sExe;
		tString sCwdStr = sCwd;
		tString sMap, sWorld, sRender;
		OhplEnReport Report = {};
		Report.id = OhplEnB(sId);
		Report.time = OhplEnB(msBugTime);
		Report.description = OhplEnB(sDesc);
		Report.version = OhplEnB(sVersion);
		Report.exe = OhplEnB(sExeStr);
		Report.cwd = OhplEnB(sCwdStr);
		Report.args = vArgBytes.data();
		Report.argsN = vArgBytes.size();
		Report.env = vEnvBytes.data();
		Report.envN = vEnvBytes.size();
		Report.system = vSystemBytes.data();
		Report.systemN = vSystemBytes.size();
		Report.resX = vScreen.x;
		Report.resY = vScreen.y;
		Report.fps = mpEngine->GetFPS();
		Report.frameMs = mpEngine->GetAvgFrameTimeInMS();
		Report.gameTime = (float)mpEngine->GetGameTime();
		if(pViewport)
		{
			cCamera *pCam = pViewport->GetCamera();
			sMap = pViewport->GetWorld()->GetName();
			sWorld = cEngineDiagnostics::GetWorldStatsJson(pViewport->GetWorld());
			sRender = cEngineDiagnostics::GetRenderStatsJson(pViewport, mpEngine->GetGraphics());
			Report.hasScene = true;
			Report.map = OhplEnB(sMap);
			Report.pos[0] = pCam->GetPosition().x; Report.pos[1] = pCam->GetPosition().y; Report.pos[2] = pCam->GetPosition().z;
			Report.forward[0] = pCam->GetForward().x; Report.forward[1] = pCam->GetForward().y; Report.forward[2] = pCam->GetForward().z;
			Report.worldJson = OhplEnB(sWorld);
			Report.renderJson = OhplEnB(sRender);
		}
		Report.files = OhplEnB(sFiles);
		tString sJson;
		ohpl_en_report_json(&Report, OhplEnAssign, &sJson);

		std::ofstream(dir / "report.json") << sJson << "\n";
		tString sMapName = pViewport ? tString(pViewport->GetWorld()->GetName()) : tString();
		tString sDir = dir.c_str();
		tString sIndex;
		ohpl_en_index_line(OhplEnB(sId), OhplEnB(sMapName), OhplEnB(sDesc), OhplEnB(sDir), OhplEnAssign, &sIndex);
		std::ofstream(root / "index.jsonl", std::ios::app) << sIndex;

		msLastSaved = dir.c_str();
		mlSavedTime = cPlatform::GetApplicationTime();
	}

}
