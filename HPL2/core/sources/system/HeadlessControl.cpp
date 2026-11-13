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

		ohpl_h22_hc_acquire_lock([](void*, int alLevel, const uint8_t *apMsg, size_t alLen) {
			tString sMsg((const char*)apMsg, alLen);
			if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Log("%s", sMsg.c_str());
		}, NULL);
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
		: mpRs(NULL), mpEngine(apEngine), msSocketPath(asSocketPath), mbListening(false)
	{
		InitRustHost();
		mpRs = ohpl_h22_hc_new(&mRsHost, OhplEnP(msSocketPath), msSocketPath.size());
		mbListening = ohpl_h22_hc_listening(mpRs);
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

		ohpl_h22_hc_free(mpRs);
	}

	void cHeadlessControlServer::RegisterHandler(const tString &asCmd, tHeadlessCommandFunc apFunc, void *apUserData)
	{
		cHandlerEntry entry;
		entry.mpFunc = apFunc;
		entry.mpUserData = apUserData;
		size_t lSlot = ohpl_h22_hc_register(mpRs, OhplEnP(asCmd), asCmd.size());
		if(lSlot >= mvRsHandlers.size()) mvRsHandlers.resize(lSlot + 1);
		mvRsHandlers[lSlot] = entry;
	}

	void cHeadlessControlServer::LogicStep()
	{
		ohpl_h22_hc_logic_step(mpRs, &mRsHost);
	}

	void cHeadlessControlServer::Update(bool abLoading)
	{
		ohpl_h22_hc_update(mpRs, &mRsHost, abLoading);
	}

	void cHeadlessControlServer::PushLogLine(eLogOutputType aType, const tString &asLine)
	{
		ohpl_h22_hc_push_log(mpRs, OhplEnP(asLine), asLine.size());
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
		mRsHost.call_handler = [](void *apCtx, size_t alSlot, const uint8_t *apBlob, size_t alLen, OhplH22Sink apSink, void *apSinkCtx) {
			cHeadlessControlServer *pThis = (cHeadlessControlServer*)apCtx;
			cHeadlessRequest request;
			for(size_t i = 0; i + 8 <= alLen;)
			{
				tString vPart[2];
				for(int p = 0; p < 2 && i + 8 <= alLen; ++p)
				{
					uint64_t lN;
					memcpy(&lN, apBlob + i, 8);
					i += 8;
					vPart[p].assign((const char*)apBlob + i, (size_t)lN);
					i += (size_t)lN;
				}
				request.mmapFields[vPart[0]] = vPart[1];
			}
			cHeadlessResponse response;
			const cHandlerEntry &entry = pThis->mvRsHandlers[alSlot];
			entry.mpFunc(entry.mpUserData, request, response);
			tString sJson = response.ToJson();
			apSink(apSinkCtx, (const uint8_t*)sJson.data(), sJson.size());
		};
		mRsHost.run_message = [](void *apCtx, int alMsg) {
			((cHeadlessControlServer*)apCtx)->mpEngine->GetUpdater()->RunMessage((eUpdateableMessage)alMsg);
		};
	}

}
