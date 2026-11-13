/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "system/HeadlessControl.h"
#include "system/EngineDiagnostics.h"
#include "system/RustSystem.h"
#include "impl/RustBackends.h"

#include "engine/Engine.h"
#include "engine/Updater.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Bitmap.h"
#include "resources/Resources.h"
#include "resources/BitmapLoaderHandler.h"
#include "system/Platform.h"
#include "system/Mutex.h"
#include "system/String.h"
#include "input/Input.h"
#include "impl/LowLevelInputSDL.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cerrno>

#ifndef WIN32
#define HPL_HEADLESS_CONTROL_POSIX 1
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if USE_SDL2
#include "SDL2/SDL.h"
#endif

namespace hpl {

	void AcquireHeadlessSingleInstanceLock()
	{
		if(getenv("OPENHPL_HEADLESS_SOCKET") == NULL) return;

		if(OhplBackendRustRequested())
		{
			ohpl_h22_hc_acquire_lock([](void*, int alLevel, const uint8_t *apMsg, size_t alLen) {
				tString sMsg((const char*)apMsg, alLen);
				if(alLevel == 1) Warning("%s", sMsg.c_str());
				else Log("%s", sMsg.c_str());
			}, NULL);
			return;
		}

#ifdef HPL_HEADLESS_CONTROL_POSIX
		const char *pRuntimeDir = getenv("XDG_RUNTIME_DIR");
		tString sLockPath = (pRuntimeDir && pRuntimeDir[0] != '\0' ? tString(pRuntimeDir) : tString("/tmp"))
							 + "/open-hpl-headless.lock";

		int lFd = open(sLockPath.c_str(), O_CREAT | O_RDWR, 0600);
		if(lFd < 0)
		{
			Warning("AcquireHeadlessSingleInstanceLock: couldn't open '%s' (%s) - proceeding unserialized\n",
					sLockPath.c_str(), strerror(errno));
			return;
		}

		if(flock(lFd, LOCK_EX | LOCK_NB) == 0) return;

		const char *pSlots = getenv("OPENHPL_HEADLESS_SLOTS");
		for(int i=1; pSlots && i<atoi(pSlots); ++i)
		{
			int lSlotFd = open((sLockPath + "." + std::to_string(i)).c_str(), O_CREAT | O_RDWR, 0600);
			if(lSlotFd >= 0 && flock(lSlotFd, LOCK_EX | LOCK_NB) == 0) { close(lFd); return; }
			if(lSlotFd >= 0) close(lSlotFd);
		}

		Log("Another headless instance already holds '%s' - waiting for it to exit "
			"(this is expected, not a hang - see AcquireHeadlessSingleInstanceLock())\n",
			sLockPath.c_str());

		if(flock(lFd, LOCK_EX) != 0)
		{
			Warning("AcquireHeadlessSingleInstanceLock: blocking flock() on '%s' failed (%s) - proceeding unserialized\n",
					sLockPath.c_str(), strerror(errno));
		}
#endif
	}

	static void AssignString(void *apCtx, const char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(apData, alLen);
	}

	static tString JsonEscape(const tString &asIn)
	{
		tString sOut;
		ohpl_hc_json_escape(asIn.data(), asIn.size(), AssignString, &sOut);
		return sOut;
	}

	static void ParseFlatJsonObject(const tString &asLine, std::map<tString,tString> &aOutFields)
	{
		ohpl_hc_parse(asLine.data(), asLine.size(),
			[](void *apCtx, const char *apKey, size_t alKeyLen, const char *apVal, size_t alValLen) {
				(*static_cast<std::map<tString,tString>*>(apCtx))[tString(apKey, alKeyLen)] = tString(apVal, alValLen);
			}, &aOutFields);
	}

	tString cHeadlessRequest::GetString(const tString &asKey, const tString &asDefault) const
	{
		std::map<tString,tString>::const_iterator it = mmapFields.find(asKey);
		return it == mmapFields.end() ? asDefault : it->second;
	}
	float cHeadlessRequest::GetFloat(const tString &asKey, float afDefault) const
	{
		std::map<tString,tString>::const_iterator it = mmapFields.find(asKey);
		return it == mmapFields.end() ? afDefault : cString::ToFloat(it->second.c_str(), afDefault);
	}
	int cHeadlessRequest::GetInt(const tString &asKey, int alDefault) const
	{
		std::map<tString,tString>::const_iterator it = mmapFields.find(asKey);
		return it == mmapFields.end() ? alDefault : cString::ToInt(it->second.c_str(), alDefault);
	}
	bool cHeadlessRequest::GetBool(const tString &asKey, bool abDefault) const
	{
		std::map<tString,tString>::const_iterator it = mmapFields.find(asKey);
		return it == mmapFields.end() ? abDefault : it->second == "1" || cString::ToBool(it->second.c_str(), abDefault);
	}
	bool cHeadlessRequest::HasKey(const tString &asKey) const
	{
		return mmapFields.find(asKey) != mmapFields.end();
	}

	cHeadlessResponse::cHeadlessResponse() : mbOk(true)
	{
	}

	void cHeadlessResponse::Set(const tString &asKey, const tString &asVal)
	{
		mvExtraFields.push_back(std::make_pair(asKey, "\"" + JsonEscape(asVal) + "\""));
	}
	void cHeadlessResponse::Set(const tString &asKey, const char *apVal)
	{
		Set(asKey, tString(apVal));
	}
	void cHeadlessResponse::Set(const tString &asKey, float afVal)
	{
		tString sNum;
		ohpl_diag_num(afVal, AssignString, &sNum);
		mvExtraFields.push_back(std::make_pair(asKey, sNum));
	}
	void cHeadlessResponse::Set(const tString &asKey, int alVal)
	{
		mvExtraFields.push_back(std::make_pair(asKey, cString::ToString(alVal)));
	}
	void cHeadlessResponse::Set(const tString &asKey, bool abVal)
	{
		mvExtraFields.push_back(std::make_pair(asKey, tString(abVal ? "true" : "false")));
	}

	void cHeadlessResponse::SetRaw(const tString &asKey, const tString &asJson)
	{
		mvExtraFields.push_back(std::make_pair(asKey, asJson));
	}

	tString cHeadlessResponse::ToJson() const
	{
		std::vector<OhplHcField> vFields;
		for(size_t i=0; i<mvExtraFields.size(); ++i)
		{
			OhplHcField field = { (const uint8_t*)mvExtraFields[i].first.data(), mvExtraFields[i].first.size(),
								  (const uint8_t*)mvExtraFields[i].second.data(), mvExtraFields[i].second.size() };
			vFields.push_back(field);
		}
		tString sOut;
		ohpl_hc_response_json(mbOk, msError.data(), msError.size(), vFields.data(), vFields.size(), AssignString, &sOut);
		return sOut;
	}

	static cHeadlessControlServer *gpHeadlessLogInstance = NULL;

	static void HeadlessLogCallback(eLogOutputType aType, const char *asMessage)
	{
		if(gpHeadlessLogInstance) gpHeadlessLogInstance->PushLogLine(aType, asMessage);
	}

	cHeadlessControlServer::cHeadlessControlServer(cEngine *apEngine, const tString &asSocketPath)
		: mpRs(NULL), mpEngine(apEngine), msSocketPath(asSocketPath), mbListening(false), mlListenFd(-1), mlClientFd(-1), mpThread(NULL),
		  mpQueueMutex(NULL), mpLogMutex(NULL)
	{
		if(OhplBackendRustRequested())
		{
			InitRustHost();
			mpRs = ohpl_h22_hc_new(&mRsHost, OhplEnP(msSocketPath), msSocketPath.size());
			mbListening = ohpl_h22_hc_listening(mpRs);
			gpHeadlessLogInstance = this;
			SetLogMessageCallback(HeadlessLogCallback);
			return;
		}

		mpQueueMutex = cPlatform::CreateMutEx();
		mpLogMutex = cPlatform::CreateMutEx();

#ifdef HPL_HEADLESS_CONTROL_POSIX
		mlListenFd = socket(AF_UNIX, SOCK_STREAM, 0);
		if(mlListenFd < 0)
		{
			Error("HeadlessControl: socket() failed: %s\n", strerror(errno));
		}
		else
		{
			unlink(msSocketPath.c_str());

			struct sockaddr_un addr;
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, msSocketPath.c_str(), sizeof(addr.sun_path)-1);

			if(bind(mlListenFd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
			{
				Error("HeadlessControl: bind('%s') failed: %s\n", msSocketPath.c_str(), strerror(errno));
				close(mlListenFd);
				mlListenFd = -1;
			}
			else if(listen(mlListenFd, 4) != 0)
			{
				Error("HeadlessControl: listen() failed: %s\n", strerror(errno));
				close(mlListenFd);
				mlListenFd = -1;
			}
			else
			{
				mpThread = cPlatform::CreateThread(this);
				mpThread->Start();
				mbListening = true;
				Log("HeadlessControl: listening on '%s'\n", msSocketPath.c_str());
			}
		}
#else
		Warning("HeadlessControl: not supported on this platform, ignoring OPENHPL_HEADLESS_SOCKET\n");
#endif

		RegisterBuiltins();

		gpHeadlessLogInstance = this;
		SetLogMessageCallback(HeadlessLogCallback);
	}

	cHeadlessControlServer::~cHeadlessControlServer()
	{
		if(gpHeadlessLogInstance == this)
		{
			SetLogMessageCallback(NULL);
			gpHeadlessLogInstance = NULL;
		}

		if(mpRs)
		{
			ohpl_h22_hc_free(mpRs);
			return;
		}

#ifdef HPL_HEADLESS_CONTROL_POSIX
		if(mlListenFd >= 0)
		{
			int lFd = mlListenFd;
			mlListenFd = -1;
			shutdown(lFd, SHUT_RDWR);
			close(lFd);
		}
		if(mlClientFd >= 0) shutdown(mlClientFd, SHUT_RDWR);
#endif
		if(mpThread)
		{
			mpThread->Stop();
			hplDelete(mpThread);
		}

#ifdef HPL_HEADLESS_CONTROL_POSIX
		if(!msSocketPath.empty()) unlink(msSocketPath.c_str());
#endif

		if(mpQueueMutex) hplDelete(mpQueueMutex);
		if(mpLogMutex) hplDelete(mpLogMutex);
	}

	void cHeadlessControlServer::RegisterHandler(const tString &asCmd, tHeadlessCommandFunc apFunc, void *apUserData)
	{
		if(mpRs) return;
		cHandlerEntry entry;
		entry.mpFunc = apFunc;
		entry.mpUserData = apUserData;
		mmapHandlers[asCmd] = entry;
	}

	void cHeadlessControlServer::LogicStep()
	{
		if(mpRs) { ohpl_h22_hc_logic_step(mpRs, &mRsHost); return; }
#if USE_SDL2
		if(!mdqDragPath.empty())
		{
			mlDragX = mdqDragPath.front().first;
			mlDragY = mdqDragPath.front().second;
			mdqDragPath.pop_front();
		}
		else if(mlDragFrames > 0) --mlDragFrames;
		else return;
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_MOUSEMOTION;
		ev.motion.xrel = mlDragX;
		ev.motion.yrel = mlDragY;
		SDL_PushEvent(&ev);
#endif
	}

	void cHeadlessControlServer::Update(bool abLoading)
	{
		if(mpRs) { ohpl_h22_hc_update(mpRs, &mRsHost, abLoading); return; }
		for(size_t i=0; i<mvFrameWaiters.size(); )
		{
			bool bTimedOut = cPlatform::GetApplicationTime() >= mvFrameWaiters[i].mlDeadlineMs;
			unsigned int lFrame = cEngineDiagnostics::GetRenderedFrameCount();
			if(lFrame < mvFrameWaiters[i].mlTargetFrame && !bTimedOut) { ++i; continue; }

			cHeadlessResponse response;
			response.Set("frames", (int)(lFrame - mvFrameWaiters[i].mlStartFrame));
			response.Set("timed_out", bTimedOut);
			SendResponse(mvFrameWaiters[i].mlClientFd, response);
			mvFrameWaiters.erase(mvFrameWaiters.begin() + i);
		}

		while(true)
		{
			cPendingRequest pending;
			bool bHasOne = false;

			mpQueueMutex->Lock();
			for(auto it = mlstPendingQueue.begin(); it != mlstPendingQueue.end(); ++it)
			{
				// Mid-load: nothing that touches the world
				if(abLoading && it->mRequest.GetCmd() != "screenshot" && it->mRequest.GetCmd() != "ping")
					continue;
				pending = *it;
				mlstPendingQueue.erase(it);
				bHasOne = true;
				break;
			}
			mpQueueMutex->Unlock();

			if(!bHasOne) break;

			Dispatch(pending);
		}
	}

	void cHeadlessControlServer::UpdateThread()
	{
#ifdef HPL_HEADLESS_CONTROL_POSIX
		if(mlListenFd < 0) return;

		int lClientFd = accept(mlListenFd, NULL, NULL);
		if(lClientFd < 0) return;
		mlClientFd = lClientFd;

		OhplHcSplitter *pSplitter = ohpl_hc_splitter_new();
		char vReadBuf[4096];
		while(true)
		{
			ssize_t lRead = recv(lClientFd, vReadBuf, sizeof(vReadBuf), 0);
			if(lRead <= 0) break;

			struct cLineSink { cHeadlessControlServer *mpServer; int mlClientFd; } sink = { this, lClientFd };
			ohpl_hc_splitter_push(pSplitter, vReadBuf, (size_t)lRead,
				[](void *apCtx, const char *apLine, size_t alLen) {
					cLineSink *pSink = static_cast<cLineSink*>(apCtx);
					cPendingRequest pending;
					pending.mlClientFd = pSink->mlClientFd;
					ParseFlatJsonObject(tString(apLine, alLen), pending.mRequest.mmapFields);

					pSink->mpServer->mpQueueMutex->Lock();
					pSink->mpServer->mlstPendingQueue.push_back(pending);
					pSink->mpServer->mpQueueMutex->Unlock();
				}, &sink);
		}
		ohpl_hc_splitter_free(pSplitter);

		mlClientFd = -1;
		close(lClientFd);
#endif
	}

	void cHeadlessControlServer::PushLogLine(eLogOutputType aType, const tString &asLine)
	{
		if(mpRs) { ohpl_h22_hc_push_log(mpRs, OhplEnP(asLine), asLine.size()); return; }
		mpLogMutex->Lock();
		mlstLogLines.push_back(asLine);
		while(mlstLogLines.size() > 500) mlstLogLines.pop_front();
		mpLogMutex->Unlock();
	}

	void cHeadlessControlServer::Dispatch(const cPendingRequest &aPending)
	{
		cHeadlessResponse response;
		tString sCmd = aPending.mRequest.GetCmd();

		if(sCmd == "wait_frames")
		{
			cFrameWaiter waiter;
			waiter.mlClientFd = aPending.mlClientFd;
			waiter.mlFramesTotal = aPending.mRequest.GetInt("n", 1);
			waiter.mlStartFrame = cEngineDiagnostics::GetRenderedFrameCount();
			waiter.mlTargetFrame = waiter.mlStartFrame + (unsigned int)waiter.mlFramesTotal;
			waiter.mlDeadlineMs = cPlatform::GetApplicationTime() + (unsigned long)aPending.mRequest.GetInt("max_ms", 600000);
			mvFrameWaiters.push_back(waiter);
			return;
		}

		std::map<tString, cHandlerEntry>::iterator it = mmapHandlers.find(sCmd);
		if(it == mmapHandlers.end())
		{
			response.SetError("unknown command: '" + sCmd + "'");
		}
		else
		{
			it->second.mpFunc(it->second.mpUserData, aPending.mRequest, response);
		}

		SendResponse(aPending.mlClientFd, response);
	}

	void cHeadlessControlServer::SendResponse(int alClientFd, const cHeadlessResponse &aResp)
	{
#ifdef HPL_HEADLESS_CONTROL_POSIX
		tString sLine = aResp.ToJson() + "\n";
		send(alClientFd, sLine.c_str(), sLine.size(), MSG_NOSIGNAL);
#endif
	}

	void cHeadlessControlServer::InitRustHost()
	{
		mRsHost.ctx = this;
		mRsHost.now_ms = [](void*) -> uint64_t { return cPlatform::GetApplicationTime(); };
		mRsHost.rendered_frames = [](void*) -> uint32_t { return cEngineDiagnostics::GetRenderedFrameCount(); };
		mRsHost.log = [](void*, int alLevel, const uint8_t *apMsg, size_t alLen) {
			tString sMsg((const char*)apMsg, alLen);
			if(alLevel == 2) Error("%s", sMsg.c_str());
			else if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Log("%s", sMsg.c_str());
		};
		mRsHost.exit = [](void *apCtx) { ((cHeadlessControlServer*)apCtx)->mpEngine->Exit(); };
		mRsHost.set_wait_focus = [](void *apCtx, bool abX) { ((cHeadlessControlServer*)apCtx)->mpEngine->SetWaitIfAppOutOfFocus(abX); };
		mRsHost.dev_hud_active = [](void *apCtx) -> bool { return ((cHeadlessControlServer*)apCtx)->mpEngine->GetDevHudActive(); };
		mRsHost.set_dev_hud = [](void *apCtx, bool abX) { ((cHeadlessControlServer*)apCtx)->mpEngine->SetDevHudActive(abX); };
		mRsHost.screen_stats = [](void *apCtx, float *apMax, float *apMean, float *apOver) -> bool {
			return ((cHeadlessControlServer*)apCtx)->mpEngine->GetGraphics()->GetLowLevel()->GetScreenFloatStats(*apMax, *apMean, *apOver);
		};
		mRsHost.force_window_size = [](void *apCtx, int alW, int alH) -> bool {
			return ((cHeadlessControlServer*)apCtx)->mpEngine->GetGraphics()->GetLowLevel()->ForceWindowSize(alW, alH);
		};
		mRsHost.shader_fail_count = [](void*) -> int { return cEngineDiagnostics::GetShaderFailCount(); };
		mRsHost.shader_report_json = [](void*, bool abFailedOnly, OhplH22Sink apSink, void *apSinkCtx) {
			tString sJson = cEngineDiagnostics::GetShaderReportJson(abFailedOnly);
			apSink(apSinkCtx, (const uint8_t*)sJson.data(), sJson.size());
		};
		mRsHost.frame_stats = [](void *apCtx, OhplH22Sink apFrameSink, void *apFrameCtx, OhplH22Sink apGlSink, void *apGlCtx) -> bool {
			cEngine *pEngine = ((cHeadlessControlServer*)apCtx)->mpEngine;
			cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
			if(pBmp == NULL) return false;
			tString sFrame = cEngineDiagnostics::GetFrameStatsJson(pBmp->GetData(0,0)->mpData, pBmp->GetWidth(), pBmp->GetHeight(),
																	 pBmp->GetBytesPerPixel());
			tString sGl = cEngineDiagnostics::PollGLErrorsJson(false);
			hplDelete(pBmp);
			apFrameSink(apFrameCtx, (const uint8_t*)sFrame.data(), sFrame.size());
			apGlSink(apGlCtx, (const uint8_t*)sGl.data(), sGl.size());
			return true;
		};
		mRsHost.xdg_cache_home = [](void*, OhplH22Sink apSink, void *apSinkCtx) {
			tString sDir = cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome));
			apSink(apSinkCtx, (const uint8_t*)sDir.data(), sDir.size());
		};
		mRsHost.ensure_folder = [](void*, const uint8_t *apDir, size_t alLen) {
#if defined(__linux__)
			tWString sDir = cString::To16Char(tString((const char*)apDir, alLen));
			if(cPlatform::FolderExists(sDir) == false) cPlatform::CreateFolder(sDir);
#endif
		};
		mRsHost.screenshot = [](void *apCtx, const uint8_t *apPath, size_t alLen) -> int {
			cEngine *pEngine = ((cHeadlessControlServer*)apCtx)->mpEngine;
			tString sPath((const char*)apPath, alLen);
			cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
			if(pBmp == NULL) return 1;

			if(pBmp->GetBytesPerPixel() == 4)
			{
				cBitmapData *pData = pBmp->GetData(0,0);
				for(size_t i = 3; i < (size_t)pData->mlSize; i += 4) pData->mpData[i] = 255;
			}
			cEngineDiagnostics::ApplyDisplayGamma(pBmp, pEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection());

			bool bSaved = pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp, cString::To16Char(sPath), 0);
			hplDelete(pBmp);
			return bSaved ? 0 : 2;
		};
		mRsHost.inject_text = [](void *apCtx, const uint8_t *apText, size_t alLen) {
#if USE_SDL2
			SDL_Event ev;
			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_TEXTINPUT;
			memcpy(ev.text.text, apText, alLen);
			static_cast<cLowLevelInputSDL*>(((cHeadlessControlServer*)apCtx)->mpEngine->GetInput()->GetLowLevel())->mlstInjected.push_back(ev);
#endif
		};
		mRsHost.run_message = [](void *apCtx, int alMsg) {
			((cHeadlessControlServer*)apCtx)->mpEngine->GetUpdater()->RunMessage((eUpdateableMessage)alMsg);
		};
	}

	void cHeadlessControlServer::RegisterBuiltins()
	{
		RegisterHandler("ping", SCmdPing, this);
		RegisterHandler("quit", SCmdQuit, this);
		RegisterHandler("screenshot", SCmdScreenshot, this);
		RegisterHandler("log_tail", SCmdLogTail, this);
		RegisterHandler("set_focus_wait", SCmdSetFocusWait, this);
		RegisterHandler("input", SCmdInput, this);
		RegisterHandler("resize", SCmdResizeWindow, this);
		RegisterHandler("shader_report", SCmdShaderReport, this);
		RegisterHandler("frame_stats", SCmdFrameStats, this);
		RegisterHandler("screen_stats", [](void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp) {
			float fMax, fMean, fOver;
			if(((cHeadlessControlServer*)apUserData)->mpEngine->GetGraphics()->GetLowLevel()->GetScreenFloatStats(fMax, fMean, fOver) == false)
			{
				aResp.SetError("no float screen target");
				return;
			}
			aResp.Set("max", fMax);
			aResp.Set("mean", fMean);
			aResp.Set("over1", fOver);
		}, this);
		RegisterHandler("dev_hud", [](void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp) {
			cEngine *pEngine = ((cHeadlessControlServer*)apUserData)->mpEngine;
			aResp.Set("prev", pEngine->GetDevHudActive());
			if(aReq.HasKey("value")) pEngine->SetDevHudActive(aReq.GetBool("value", true));
		}, this);
	}

	void cHeadlessControlServer::CmdPing(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		aResp.Set("pong", true);
	}

	void cHeadlessControlServer::CmdQuit(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		mpEngine->Exit();
	}

	void cHeadlessControlServer::CmdScreenshot(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		tString sDefaultPath = aReq.HasKey("path") ? "" :
			cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome)) + "open-hpl/headless_screenshot.bmp";
		tString sPath = aReq.GetString("path", sDefaultPath);
#if defined(__linux__)
		if(aReq.HasKey("path") == false)
		{
			tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome) + _W("open-hpl/");
			if(cPlatform::FolderExists(sDir) == false) cPlatform::CreateFolder(sDir);
		}
#endif

		cBitmap *pBmp = mpEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		if(pBmp == NULL)
		{
			aResp.SetError("CopyFrameBufferToBitmap() failed");
			return;
		}

		if(pBmp->GetBytesPerPixel() == 4)
		{
			cBitmapData *pData = pBmp->GetData(0,0);
			for(size_t i = 3; i < (size_t)pData->mlSize; i += 4) pData->mpData[i] = 255;
		}
		cEngineDiagnostics::ApplyDisplayGamma(pBmp, mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection());

		bool bSaved = mpEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp, cString::To16Char(sPath), 0);
		hplDelete(pBmp);

		if(!bSaved)
		{
			aResp.SetError("SaveBitmap('" + sPath + "') failed");
			return;
		}

		aResp.Set("path", sPath);
	}

	void cHeadlessControlServer::CmdShaderReport(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		aResp.Set("failures", cEngineDiagnostics::GetShaderFailCount());
		aResp.SetRaw("shaders", cEngineDiagnostics::GetShaderReportJson(aReq.GetBool("failed_only", true)));
	}

	void cHeadlessControlServer::CmdFrameStats(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		cBitmap *pBmp = mpEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		if(pBmp == NULL)
		{
			aResp.SetError("CopyFrameBufferToBitmap() failed");
			return;
		}

		aResp.SetRaw("frame", cEngineDiagnostics::GetFrameStatsJson(pBmp->GetData(0,0)->mpData, pBmp->GetWidth(), pBmp->GetHeight(),
																	 pBmp->GetBytesPerPixel()));
		aResp.SetRaw("gl_errors", cEngineDiagnostics::PollGLErrorsJson(false));
		hplDelete(pBmp);
	}

	void cHeadlessControlServer::CmdLogTail(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		int lWantLines = aReq.GetInt("lines", 50);

		mpLogMutex->Lock();
		int lStart = (int)mlstLogLines.size() - lWantLines;
		if(lStart < 0) lStart = 0;
		tString sJoined;
		for(size_t i=(size_t)lStart; i<mlstLogLines.size(); ++i)
		{
			sJoined += mlstLogLines[i];
			sJoined += "\n";
		}
		mpLogMutex->Unlock();

		aResp.Set("log", sJoined);
	}

	void cHeadlessControlServer::CmdSetFocusWait(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		mpEngine->SetWaitIfAppOutOfFocus(aReq.GetBool("enabled", false));
	}

	void cHeadlessControlServer::CmdResizeWindow(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
		int lW = aReq.GetInt("width", 0);
		int lH = aReq.GetInt("height", 0);
		if(lW <= 0 || lH <= 0)
		{
			aResp.SetError("resize requires positive 'width' and 'height'");
			return;
		}

		if(mpEngine->GetGraphics()->GetLowLevel()->ForceWindowSize(lW, lH) == false)
		{
			aResp.SetError("could not resize window");
		}
	}

	void cHeadlessControlServer::CmdInput(const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{
#if USE_SDL2
		tString sType = aReq.GetString("type", "");
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));

		if(sType == "key")
		{
			tString sKeyName = aReq.GetString("key", "");
			tString sAction = aReq.GetString("action", "down");

			SDL_Keycode lKeyCode = SDL_GetKeyFromName(sKeyName.c_str());
			if(lKeyCode == SDLK_UNKNOWN)
			{
				aResp.SetError("unknown key name: '" + sKeyName + "'");
				return;
			}

			bool bUp = (sAction == "up");
			ev.type = bUp ? SDL_KEYUP : SDL_KEYDOWN;
			ev.key.state = bUp ? SDL_RELEASED : SDL_PRESSED;
			ev.key.repeat = 0;
			ev.key.keysym.sym = lKeyCode;
			ev.key.keysym.scancode = SDL_GetScancodeFromKey(lKeyCode);
			SDL_PushEvent(&ev);
			if(sAction == "tap")
			{
				ev.type = SDL_KEYUP;
				ev.key.state = SDL_RELEASED;
				SDL_PushEvent(&ev);
			}
		}
		else if(sType == "mouse_move")
		{
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = aReq.GetInt("x", 0);
			ev.motion.y = aReq.GetInt("y", 0);
			ev.motion.xrel = aReq.GetInt("xrel", 0);
			ev.motion.yrel = aReq.GetInt("yrel", 0);
			mdqDragPath.clear();
			// path "dx,dy;dx,dy;...": one step per logic update
			tString sPath = aReq.GetString("path", "");
			int lX, lY, lN;
			for(const char *p = sPath.c_str(); sscanf(p, "%d,%d%n", &lX, &lY, &lN) == 2; p += lN + (p[lN] == ';'))
				mdqDragPath.push_back(std::make_pair(lX, lY));
			if(mdqDragPath.empty()) SDL_PushEvent(&ev);
			mlDragFrames = aReq.GetInt("frames", 1) - 1;
			mlDragX = ev.motion.xrel;
			mlDragY = ev.motion.yrel;
		}
		else if(sType == "text")
		{
			ev.type = SDL_TEXTINPUT;
			tString sText = aReq.GetString("text", "");
			for(size_t lPos = 0; lPos < sText.size();)
			{
				size_t lLen = std::min(sText.size() - lPos, sizeof(ev.text.text) - 1);
				while(lPos + lLen < sText.size() && (sText[lPos + lLen] & 0xc0) == 0x80) --lLen;
				memset(ev.text.text, 0, sizeof(ev.text.text));
				memcpy(ev.text.text, sText.data() + lPos, lLen);
				static_cast<cLowLevelInputSDL*>(mpEngine->GetInput()->GetLowLevel())->mlstInjected.push_back(ev);
				lPos += lLen;
			}
		}
		else if(sType == "mouse_button")
		{
			tString sButton = aReq.GetString("button", "left");
			tString sAction = aReq.GetString("action", "down");

			Uint8 lButton = SDL_BUTTON_LEFT;
			if(sButton == "right") lButton = SDL_BUTTON_RIGHT;
			else if(sButton == "middle") lButton = SDL_BUTTON_MIDDLE;

			bool bUp = (sAction == "up");
			ev.type = bUp ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
			ev.button.button = lButton;
			ev.button.state = bUp ? SDL_RELEASED : SDL_PRESSED;
			ev.button.clicks = 1;
			ev.button.x = aReq.GetInt("x", 0);
			ev.button.y = aReq.GetInt("y", 0);
			SDL_PushEvent(&ev);
		}
		else if(sType == "focus")
		{
			tString sState = aReq.GetString("state", "");
			if(sState == "lost")
			{
				mpEngine->GetUpdater()->RunMessage(eUpdateableMessage_AppLostInputFocus);
			}
			else if(sState == "gained")
			{
				mpEngine->GetUpdater()->RunMessage(eUpdateableMessage_AppGotInputFocus);
			}
			else
			{
				aResp.SetError("focus requires 'state' of 'lost' or 'gained'");
			}
		}
		else
		{
			aResp.SetError("unknown input type: '" + sType + "'");
		}
#else
		aResp.SetError("input injection requires USE_SDL2");
#endif
	}

	void cHeadlessControlServer::SCmdShaderReport(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdShaderReport(aReq, aResp); }
	void cHeadlessControlServer::SCmdFrameStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdFrameStats(aReq, aResp); }
	void cHeadlessControlServer::SCmdPing(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdPing(aReq, aResp); }
	void cHeadlessControlServer::SCmdQuit(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdQuit(aReq, aResp); }
	void cHeadlessControlServer::SCmdScreenshot(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdScreenshot(aReq, aResp); }
	void cHeadlessControlServer::SCmdLogTail(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdLogTail(aReq, aResp); }
	void cHeadlessControlServer::SCmdSetFocusWait(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdSetFocusWait(aReq, aResp); }
	void cHeadlessControlServer::SCmdInput(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdInput(aReq, aResp); }
	void cHeadlessControlServer::SCmdResizeWindow(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
	{ ((cHeadlessControlServer*)apUserData)->CmdResizeWindow(aReq, aResp); }

}
