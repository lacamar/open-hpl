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

/*
 * Opt-in headless automation server. When the OPENHPL_HEADLESS_SOCKET
 * environment variable is set at engine startup, cEngine (see Engine.cpp)
 * constructs one of these bound to that Unix domain socket path and drains
 * it once per frame from cEngine::Run(). An external test script (see
 * scripts/hpl_control.py) connects and sends one newline-delimited JSON
 * command per line, gets one JSON response line back:
 *
 *   {"cmd":"state"}                              -> {"ok":true,...}
 *   {"cmd":"teleport","x":1,"y":2,"z":3}          -> {"ok":true}
 *   {"cmd":"run_script","line":"AddTinderboxes(1,\"Player\")"} -> {"ok":true}
 *
 * This replaces attaching gdb by hand to drive/inspect a running game
 * process (see PORTING_NOTES.md for the pain that caused) with a real,
 * repeatable interface. Fully inactive - no thread, no socket, zero
 * overhead - whenever the environment variable is unset, so it can never
 * affect normal play.
 *
 * A handful of generic commands (ping/quit/screenshot/log_tail/
 * set_focus_wait/input) are implemented here since they only need core
 * engine APIs. Game-specific commands (run_script/state/teleport/
 * start_map for Amnesia; camera_state/set_camera for the AMFP/SOMA
 * free-fly scaffolds) are registered by each game's Base class via
 * RegisterHandler() - this class knows nothing about cLuxPlayer/cCamera/etc.
 *
 * Not a general JSON parser/serializer: the protocol only ever needs flat
 * objects (string/number/bool fields, no nesting), so cHeadlessRequest/
 * cHeadlessResponse are deliberately minimal for that shape only.
 */

#ifndef HPL_HEADLESS_CONTROL_H
#define HPL_HEADLESS_CONTROL_H

#include "system/SystemTypes.h"
#include "system/Thread.h"
#include "system/LowLevelSystem.h"

#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace hpl {

	class cEngine;
	class iMutex;

	//------------------------------------------------------

	// Serializes headless instances against each other: only one
	// OPENHPL_HEADLESS_SOCKET process may be past this call at a time -
	// concurrent worktree-agents/test scripts each launching their own
	// instance were observed competing for the same GPU (FPS as low as
	// ~3-8, versus ~22 with only one instance running - see PORTING_NOTES.md
	// "SOMA: shader-filename/sampler-binding fixes..."). Call as early as
	// possible in headless startup (cSDLEngineSetup's constructor, before
	// SDL_Init - see SDLEngineSetup.cpp) so a queued instance doesn't even
	// open a window/X11 connection on the real desktop while waiting.
	// Blocks (does not fail/exit) until the lock is free, logging once if it
	// has to wait, so scripts/headless-check.sh's CPU-tick-based hang
	// detection doesn't misreport a merely-queued process as stuck - see the
	// .cpp for why a flock()'d fd (not a pidfile) is used. A no-op, like the
	// rest of this header, whenever OPENHPL_HEADLESS_SOCKET is unset.
	void AcquireHeadlessSingleInstanceLock();

	//------------------------------------------------------

	class cHeadlessRequest
	{
	public:
		tString GetCmd() const { return GetString("cmd", ""); }

		tString GetString(const tString &asKey, const tString &asDefault) const;
		float GetFloat(const tString &asKey, float afDefault) const;
		int GetInt(const tString &asKey, int alDefault) const;
		bool GetBool(const tString &asKey, bool abDefault) const;
		bool HasKey(const tString &asKey) const;

		// Raw (already-unescaped) value text, keyed by field name. Populated
		// by ParseFlatJsonObject() in the .cpp.
		std::map<tString, tString> mmapFields;
	};

	//------------------------------------------------------

	class cHeadlessResponse
	{
	public:
		cHeadlessResponse();

		void SetOk(bool abOk) { mbOk = abOk; }
		void SetError(const tString &asMsg) { mbOk = false; msError = asMsg; }
		bool IsOk() const { return mbOk; }

		void Set(const tString &asKey, const tString &asVal);
		void Set(const tString &asKey, const char *apVal);
		void Set(const tString &asKey, float afVal);
		void Set(const tString &asKey, int alVal);
		void Set(const tString &asKey, bool abVal);

		tString ToJson() const;

	private:
		bool mbOk;
		tString msError;
		std::vector<std::pair<tString, tString> > mvExtraFields; // value text is pre-encoded JSON
	};

	//------------------------------------------------------

	typedef void (*tHeadlessCommandFunc)(void *apUserData, const cHeadlessRequest &aRequest, cHeadlessResponse &aResponse);

	//------------------------------------------------------

	class cHeadlessControlServer : public iThreadClass
	{
	public:
		cHeadlessControlServer(cEngine *apEngine, const tString &asSocketPath);
		~cHeadlessControlServer();

		bool IsListening() { return mbListening; }

		// Handlers are dispatched synchronously from Update() on the main
		// thread - safe to touch script/GL/game state directly.
		void RegisterHandler(const tString &asCmd, tHeadlessCommandFunc apFunc, void *apUserData);

		// Call once per frame from cEngine::Run(): drains queued requests
		// and dispatches each to its registered handler.
		void Update();

		// iThreadClass - runs on the listener thread (see cEngine's
		// cThreadSDL). One call = accept one connection, then read/queue
		// requests from it until it disconnects.
		void UpdateThread();

		// Fed by the process-wide log message callback installed in the
		// constructor (see SetLogMessageCallback in LowLevelSystem.h), so
		// log_tail works without polling hpl.log from disk.
		void PushLogLine(eLogOutputType aType, const tString &asLine);

	private:
		struct cPendingRequest
		{
			int mlClientFd;
			cHeadlessRequest mRequest;
		};

		struct cHandlerEntry
		{
			tHeadlessCommandFunc mpFunc;
			void *mpUserData;
		};

		void RegisterBuiltins();
		void Dispatch(const cPendingRequest &aPending);
		void SendResponse(int alClientFd, const cHeadlessResponse &aResp);

		// Built-in generic commands (need only cEngine's own public API).
		// Each is registered via a matching static S...() free function
		// (tHeadlessCommandFunc's signature can't bind a member function
		// directly) that casts apUserData back to `this` and forwards.
		void CmdPing(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdQuit(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdScreenshot(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdLogTail(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdSetFocusWait(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdInput(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdResizeWindow(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);

		static void SCmdPing(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdQuit(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdScreenshot(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdLogTail(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdSetFocusWait(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdInput(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdResizeWindow(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);

		cEngine *mpEngine;
		tString msSocketPath;
		bool mbListening;

		int mlListenFd;
		iThread *mpThread;
		iMutex *mpQueueMutex;
		iMutex *mpLogMutex;

		std::deque<cPendingRequest> mlstPendingQueue;
		std::map<tString, cHandlerEntry> mmapHandlers;

		std::deque<tString> mlstLogLines;
	};

}
#endif // HPL_HEADLESS_CONTROL_H
