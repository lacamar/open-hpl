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

#include "engine/Engine.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Bitmap.h"
#include "resources/Resources.h"
#include "resources/BitmapLoaderHandler.h"
#include "system/Platform.h"
#include "system/Mutex.h"
#include "system/String.h"

#include <cctype>
#include <cstring>
#include <cerrno>

#ifndef WIN32
#define HPL_HEADLESS_CONTROL_POSIX 1
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#if USE_SDL2
#include "SDL2/SDL.h"
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// MINIMAL FLAT-JSON HELPERS (see HeadlessControl.h - not a general parser)
	//////////////////////////////////////////////////////////////////////////

	static tString JsonEscape(const tString &asIn)
	{
		tString sOut;
		sOut.reserve(asIn.size());
		for(size_t i=0; i<asIn.size(); ++i)
		{
			char c = asIn[i];
			switch(c)
			{
			case '"': sOut += "\\\""; break;
			case '\\': sOut += "\\\\"; break;
			case '\n': sOut += "\\n"; break;
			case '\r': sOut += "\\r"; break;
			case '\t': sOut += "\\t"; break;
			default:
				if((unsigned char)c >= 0x20) sOut += c;
			}
		}
		return sOut;
	}

	static void SkipWhitespace(const tString &asLine, size_t &alPos)
	{
		while(alPos < asLine.size() && isspace((unsigned char)asLine[alPos])) ++alPos;
	}

	// Parses one JSON string literal starting at asLine[alPos]=='"'; leaves
	// alPos just past the closing quote.
	static tString ParseJsonString(const tString &asLine, size_t &alPos)
	{
		tString sOut;
		if(alPos >= asLine.size() || asLine[alPos] != '"') return sOut;
		++alPos;
		while(alPos < asLine.size() && asLine[alPos] != '"')
		{
			char c = asLine[alPos];
			if(c == '\\' && alPos+1 < asLine.size())
			{
				char cNext = asLine[alPos+1];
				switch(cNext)
				{
				case 'n': sOut += '\n'; break;
				case 'r': sOut += '\r'; break;
				case 't': sOut += '\t'; break;
				default: sOut += cNext; break; // covers \" \\ \/ and anything else
				}
				alPos += 2;
			}
			else
			{
				sOut += c;
				++alPos;
			}
		}
		if(alPos < asLine.size()) ++alPos; // closing quote
		return sOut;
	}

	// Parses one bare token (number/true/false/null), stopping at ',' '}' or
	// whitespace.
	static tString ParseJsonBareToken(const tString &asLine, size_t &alPos)
	{
		size_t lStart = alPos;
		while(alPos < asLine.size())
		{
			char c = asLine[alPos];
			if(c == ',' || c == '}' || isspace((unsigned char)c)) break;
			++alPos;
		}
		return asLine.substr(lStart, alPos - lStart);
	}

	// Parses one flat {"key":value, ...} object into a key->raw-value-text
	// map. No nested objects/arrays - this protocol never needs them.
	static void ParseFlatJsonObject(const tString &asLine, std::map<tString,tString> &aOutFields)
	{
		size_t lPos = 0;
		SkipWhitespace(asLine, lPos);
		if(lPos >= asLine.size() || asLine[lPos] != '{') return;
		++lPos;

		while(true)
		{
			SkipWhitespace(asLine, lPos);
			if(lPos >= asLine.size() || asLine[lPos] == '}') break;
			if(asLine[lPos] != '"') break; // malformed - keep whatever parsed so far

			tString sKey = ParseJsonString(asLine, lPos);

			SkipWhitespace(asLine, lPos);
			if(lPos >= asLine.size() || asLine[lPos] != ':') break;
			++lPos;
			SkipWhitespace(asLine, lPos);

			tString sVal;
			if(lPos < asLine.size() && asLine[lPos] == '"') sVal = ParseJsonString(asLine, lPos);
			else sVal = ParseJsonBareToken(asLine, lPos);

			aOutFields[sKey] = sVal;

			SkipWhitespace(asLine, lPos);
			if(lPos < asLine.size() && asLine[lPos] == ',') { ++lPos; continue; }
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// cHeadlessRequest
	//////////////////////////////////////////////////////////////////////////

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
		return it == mmapFields.end() ? abDefault : cString::ToBool(it->second.c_str(), abDefault);
	}
	bool cHeadlessRequest::HasKey(const tString &asKey) const
	{
		return mmapFields.find(asKey) != mmapFields.end();
	}

	//////////////////////////////////////////////////////////////////////////
	// cHeadlessResponse
	//////////////////////////////////////////////////////////////////////////

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
		mvExtraFields.push_back(std::make_pair(asKey, cString::ToString(afVal, 6, true)));
	}
	void cHeadlessResponse::Set(const tString &asKey, int alVal)
	{
		mvExtraFields.push_back(std::make_pair(asKey, cString::ToString(alVal)));
	}
	void cHeadlessResponse::Set(const tString &asKey, bool abVal)
	{
		mvExtraFields.push_back(std::make_pair(asKey, tString(abVal ? "true" : "false")));
	}

	tString cHeadlessResponse::ToJson() const
	{
		tString sOut = "{\"ok\":";
		sOut += mbOk ? "true" : "false";
		if(!mbOk && !msError.empty())
		{
			sOut += ",\"error\":\"" + JsonEscape(msError) + "\"";
		}
		for(size_t i=0; i<mvExtraFields.size(); ++i)
		{
			sOut += ",\"" + mvExtraFields[i].first + "\":" + mvExtraFields[i].second;
		}
		sOut += "}";
		return sOut;
	}

	//////////////////////////////////////////////////////////////////////////
	// LOG CAPTURE - only ever one headless instance per process (env-var
	// gated singleton), so a plain global forwarding pointer is sufficient
	// to bridge SetLogMessageCallback's plain-C-function-pointer signature.
	//////////////////////////////////////////////////////////////////////////

	static cHeadlessControlServer *gpHeadlessLogInstance = NULL;

	static void HeadlessLogCallback(eLogOutputType aType, const char *asMessage)
	{
		if(gpHeadlessLogInstance) gpHeadlessLogInstance->PushLogLine(aType, asMessage);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	cHeadlessControlServer::cHeadlessControlServer(cEngine *apEngine, const tString &asSocketPath)
		: mpEngine(apEngine), msSocketPath(asSocketPath), mbListening(false), mlListenFd(-1), mpThread(NULL)
	{
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
			unlink(msSocketPath.c_str()); // remove a stale socket from a previous crashed run

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

	//-----------------------------------------------------------------------

	cHeadlessControlServer::~cHeadlessControlServer()
	{
		if(gpHeadlessLogInstance == this)
		{
			SetLogMessageCallback(NULL);
			gpHeadlessLogInstance = NULL;
		}

#ifdef HPL_HEADLESS_CONTROL_POSIX
		if(mlListenFd >= 0)
		{
			// Closing the listen fd out from under the listener thread's
			// blocking accept() reliably unblocks it with EBADF on Linux -
			// UpdateThread() treats that as "stop", matching mpThread->Stop()
			// below rather than hanging the process on shutdown.
			int lFd = mlListenFd;
			mlListenFd = -1;
			close(lFd);
		}
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

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	void cHeadlessControlServer::RegisterHandler(const tString &asCmd, tHeadlessCommandFunc apFunc, void *apUserData)
	{
		cHandlerEntry entry;
		entry.mpFunc = apFunc;
		entry.mpUserData = apUserData;
		mmapHandlers[asCmd] = entry;
	}

	//-----------------------------------------------------------------------

	void cHeadlessControlServer::Update()
	{
		while(true)
		{
			cPendingRequest pending;
			bool bHasOne = false;

			mpQueueMutex->Lock();
			if(!mlstPendingQueue.empty())
			{
				pending = mlstPendingQueue.front();
				mlstPendingQueue.pop_front();
				bHasOne = true;
			}
			mpQueueMutex->Unlock();

			if(!bHasOne) break;

			Dispatch(pending);
		}
	}

	//-----------------------------------------------------------------------

	void cHeadlessControlServer::UpdateThread()
	{
#ifdef HPL_HEADLESS_CONTROL_POSIX
		if(mlListenFd < 0) return;

		int lClientFd = accept(mlListenFd, NULL, NULL);
		if(lClientFd < 0) return; // shutting down, or a transient error - retried next call

		tString sBuffer;
		char vReadBuf[4096];
		while(true)
		{
			ssize_t lRead = recv(lClientFd, vReadBuf, sizeof(vReadBuf), 0);
			if(lRead <= 0) break; // disconnected or error

			sBuffer.append(vReadBuf, (size_t)lRead);

			size_t lNewlinePos;
			while((lNewlinePos = sBuffer.find('\n')) != tString::npos)
			{
				tString sLine = sBuffer.substr(0, lNewlinePos);
				sBuffer.erase(0, lNewlinePos+1);
				if(sLine.empty()) continue;

				cPendingRequest pending;
				pending.mlClientFd = lClientFd;
				ParseFlatJsonObject(sLine, pending.mRequest.mmapFields);

				mpQueueMutex->Lock();
				mlstPendingQueue.push_back(pending);
				mpQueueMutex->Unlock();
			}
		}

		close(lClientFd);
#endif
	}

	//-----------------------------------------------------------------------

	void cHeadlessControlServer::PushLogLine(eLogOutputType aType, const tString &asLine)
	{
		mpLogMutex->Lock();
		mlstLogLines.push_back(asLine);
		while(mlstLogLines.size() > 500) mlstLogLines.pop_front();
		mpLogMutex->Unlock();
	}

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	void cHeadlessControlServer::Dispatch(const cPendingRequest &aPending)
	{
		cHeadlessResponse response;
		tString sCmd = aPending.mRequest.GetCmd();

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

	//-----------------------------------------------------------------------

	void cHeadlessControlServer::SendResponse(int alClientFd, const cHeadlessResponse &aResp)
	{
#ifdef HPL_HEADLESS_CONTROL_POSIX
		tString sLine = aResp.ToJson() + "\n";
		send(alClientFd, sLine.c_str(), sLine.size(), MSG_NOSIGNAL);
#endif
	}

	//-----------------------------------------------------------------------

	void cHeadlessControlServer::RegisterBuiltins()
	{
		RegisterHandler("ping", SCmdPing, this);
		RegisterHandler("quit", SCmdQuit, this);
		RegisterHandler("screenshot", SCmdScreenshot, this);
		RegisterHandler("log_tail", SCmdLogTail, this);
		RegisterHandler("set_focus_wait", SCmdSetFocusWait, this);
		RegisterHandler("input", SCmdInput, this);
	}

	//////////////////////////////////////////////////////////////////////////
	// BUILT-IN COMMANDS
	//////////////////////////////////////////////////////////////////////////

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
		// A caller-given path (the normal case - see hpl_control.py) is used verbatim, but
		// the fallback default used to be a bare relative filename, landing wherever cwd
		// happens to be (typically the game's own Steam install directory). Cache data
		// belongs under XDG_CACHE_HOME instead.
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

		bool bSaved = mpEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp, cString::To16Char(sPath), 0);
		hplDelete(pBmp);

		if(!bSaved)
		{
			aResp.SetError("SaveBitmap('" + sPath + "') failed");
			return;
		}

		aResp.Set("path", sPath);
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
		}
		else if(sType == "mouse_move")
		{
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = aReq.GetInt("x", 0);
			ev.motion.y = aReq.GetInt("y", 0);
			ev.motion.xrel = aReq.GetInt("xrel", 0);
			ev.motion.yrel = aReq.GetInt("yrel", 0);
			SDL_PushEvent(&ev);
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
		else
		{
			aResp.SetError("unknown input type: '" + sType + "'");
		}
#else
		aResp.SetError("input injection requires USE_SDL2");
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// BUILT-IN COMMAND FORWARDERS (tHeadlessCommandFunc can't bind a member
	// function directly - see the .h)
	//////////////////////////////////////////////////////////////////////////

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

}
