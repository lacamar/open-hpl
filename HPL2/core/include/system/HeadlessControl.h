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

	void AcquireHeadlessSingleInstanceLock();

	class cHeadlessRequest
	{
	public:
		tString GetCmd() const { return GetString("cmd", ""); }

		tString GetString(const tString &asKey, const tString &asDefault) const;
		float GetFloat(const tString &asKey, float afDefault) const;
		int GetInt(const tString &asKey, int alDefault) const;
		bool GetBool(const tString &asKey, bool abDefault) const;
		bool HasKey(const tString &asKey) const;

		std::map<tString, tString> mmapFields;
	};

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
		void SetRaw(const tString &asKey, const tString &asJson);

		tString ToJson() const;

	private:
		bool mbOk;
		tString msError;
		std::vector<std::pair<tString, tString> > mvExtraFields;
	};

	typedef void (*tHeadlessCommandFunc)(void *apUserData, const cHeadlessRequest &aRequest, cHeadlessResponse &aResponse);

	class cHeadlessControlServer : public iThreadClass
	{
	public:
		cHeadlessControlServer(cEngine *apEngine, const tString &asSocketPath);
		~cHeadlessControlServer();

		bool IsListening() { return mbListening; }

		void RegisterHandler(const tString &asCmd, tHeadlessCommandFunc apFunc, void *apUserData);

		void Update();
		void LogicStep();

		void UpdateThread();

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

		void CmdPing(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdQuit(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdScreenshot(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdLogTail(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdSetFocusWait(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdInput(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdResizeWindow(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdShaderReport(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		void CmdFrameStats(const cHeadlessRequest &aReq, cHeadlessResponse &aResp);

		static void SCmdPing(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdQuit(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdScreenshot(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdLogTail(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdSetFocusWait(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdInput(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdResizeWindow(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdShaderReport(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);
		static void SCmdFrameStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp);

		cEngine *mpEngine;
		tString msSocketPath;
		bool mbListening;

		int mlListenFd;
		iThread *mpThread;
		iMutex *mpQueueMutex;
		iMutex *mpLogMutex;

		std::deque<cPendingRequest> mlstPendingQueue;

		struct cFrameWaiter
		{
			int mlClientFd;
			unsigned int mlTargetFrame;
			unsigned int mlStartFrame;
			int mlFramesTotal;
			unsigned long mlDeadlineMs;
		};
		std::vector<cFrameWaiter> mvFrameWaiters;
		int mlDragFrames = 0, mlDragX = 0, mlDragY = 0;
		std::deque<std::pair<int,int> > mdqDragPath;
		std::map<tString, cHandlerEntry> mmapHandlers;

		std::deque<tString> mlstLogLines;
	};

}
#endif // HPL_HEADLESS_CONTROL_H
