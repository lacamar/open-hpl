// H22 headless-control differential harness. Runs cHeadlessControlServer (the
// same class) over a fake cEngine and prints a transcript: the script lines interleaved with the
// replies, SDL events, injected text and engine effects they caused.
//   usage: H22_SCRIPT=<script> h22_hc ; env H22_XDG = scratch XDG_CACHE_HOME (printed as $XDG)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#define private public
#define protected public
#include "engine/Engine.h"
#include "engine/Updater.h"
#include "engine/Updateable.h"
#include "graphics/Bitmap.h"
#include "graphics/Graphics.h"
#include "impl/LowLevelInputSDL.h"
#include "input/Input.h"
#include "resources/BitmapLoader.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/Resources.h"
#include "system/EngineDiagnostics.h"
#include "system/HeadlessControl.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"
#include "RecordingLowLevelGraphics.h"
#undef private
#undef protected

using namespace hpl;

static std::string gXdg;

static std::string Norm(std::string s)
{
	if(gXdg.empty()) return s;
	for(size_t p; (p = s.find(gXdg)) != std::string::npos;) s.replace(p, gXdg.size(), "$XDG/");
	return s;
}

static void Out(const std::string &s) { std::cout << Norm(s) << std::endl; }

struct Knobs { int copyOk = 1, saveOk = 1, resizeOk = 1, floatOk = 1, gamma100 = 100; } gK;

class cFakeLow : public cRecordingLowLevelGraphics
{
public:
	cBitmap* CopyFrameBufferToBitmap(const cVector2l &avScreenPos=0, const cVector2l &avScreenSize=-1)
	{
		if(!gK.copyOk) return NULL;
		cBitmap *pBmp = new cBitmap();
		pBmp->CreateData(cVector3l(4, 2, 1), ePixelFormat_RGBA, 0, 0);
		cBitmapData *pData = pBmp->GetData(0, 0);
		for(int i = 0; i < pData->mlSize; ++i) pData->mpData[i] = (unsigned char)(i * 37 + 11);
		return pBmp;
	}
	bool GetScreenFloatStats(float &afMax, float &afMean, float &afOver) { afMax = 1.5f; afMean = 0.25f; afOver = 0.125f; return gK.floatOk; }
	bool ForceWindowSize(int w, int h) { Out("state resize " + std::to_string(w) + "x" + std::to_string(h)); return gK.resizeOk; }
	float GetGammaCorrection() { return gK.gamma100 / 100.0f; }
};

class cFakeLoader : public iBitmapLoader
{
public:
	cFakeLoader() { AddSupportedExtension("bmp"); }
	cBitmap* LoadBitmap(const tWString&, tBitmapLoadFlag) { return NULL; }
	bool SaveBitmap(cBitmap *apBitmap, const tWString &asFile, tBitmapLoadFlag)
	{
		unsigned long long lHash = 1469598103934665603ULL;
		cBitmapData *pData = apBitmap->GetData(0, 0);
		for(int i = 0; i < pData->mlSize; ++i) lHash = (lHash ^ pData->mpData[i]) * 1099511628211ULL;
		char sBuf[64];
		snprintf(sBuf, sizeof(sBuf), " bpp=%d n=%d h=%016llx", (int)apBitmap->GetBytesPerPixel(), pData->mlSize, lHash);
		Out("state shot " + cString::To8Char(asFile) + sBuf);
		return gK.saveOk;
	}
};

class cRecUpd : public iUpdateable
{
public:
	cRecUpd() : iUpdateable("rec") {}
	void AppGotInputFocus() { Out("state msg got_input_focus"); }
	void AppLostInputFocus() { Out("state msg lost_input_focus"); }
};

static void DrainEvents()
{
	SDL_Event ev;
	while(SDL_PollEvent(&ev))
	{
		char b[200];
		switch(ev.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			snprintf(b, sizeof(b), "ev key type=%x state=%d repeat=%d sym=%d sc=%d mod=%d", ev.type, ev.key.state, ev.key.repeat,
					 (int)ev.key.keysym.sym, (int)ev.key.keysym.scancode, (int)ev.key.keysym.mod);
			break;
		case SDL_MOUSEMOTION:
			snprintf(b, sizeof(b), "ev motion x=%d y=%d xrel=%d yrel=%d", ev.motion.x, ev.motion.y, ev.motion.xrel, ev.motion.yrel);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			snprintf(b, sizeof(b), "ev button type=%x button=%d state=%d clicks=%d x=%d y=%d", ev.type, ev.button.button, ev.button.state,
					 ev.button.clicks, ev.button.x, ev.button.y);
			break;
		default:
			snprintf(b, sizeof(b), "ev other type=%x", ev.type);
		}
		Out(b);
	}
}

static std::string Hex(const char *p, size_t n)
{
	std::string s;
	char b[4];
	for(size_t i = 0; i < n; ++i) { snprintf(b, sizeof(b), "%02x", (unsigned char)p[i]); s += b; }
	return s;
}

static std::string Unescape(const std::string &s)
{
	std::string o;
	for(size_t i = 0; i < s.size(); ++i)
	{
		if(s[i] == '\\' && i + 1 < s.size())
		{
			char c = s[++i];
			if(c == 'n') o += '\n';
			else if(c == 'x' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), NULL, 16); i += 2; }
			else o += c;
		}
		else o += s[i];
	}
	return o;
}

static void HEcho(void *apUd, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	std::string sF;
	for(auto &kv : aReq.mmapFields) sF += Hex(kv.first.data(), kv.first.size()) + "=" + Hex(kv.second.data(), kv.second.size()) + ",";
	Out("state handler ud=" + std::to_string((int)(intptr_t)apUd) + " " + sF);
	aResp.Set("ud", (int)(intptr_t)apUd);
	aResp.Set("cmd", aReq.GetCmd());
	aResp.Set("a", aReq.GetString("a", ""));
	aResp.Set("n", aReq.GetInt("n", -1));
	aResp.Set("f", aReq.GetFloat("f", 0.5f));
	aResp.Set("b", aReq.GetBool("b", false));
	aResp.Set("has", aReq.HasKey("a"));
	aResp.SetRaw("raw", "[1,2]");
}

static void HFail(void*, const cHeadlessRequest&, cHeadlessResponse &aResp) { aResp.SetError("custom fail \"q\""); }

static void HPing(void*, const cHeadlessRequest&, cHeadlessResponse &aResp) { aResp.Set("pong2", true); }

int hplMain(const tString&)
{
	const char *pScript = getenv("H22_SCRIPT");
	if(!pScript) return 2;
	SDL_Init(SDL_INIT_EVENTS);
	const char *pXdg = getenv("H22_XDG");
	if(pXdg) gXdg = pXdg;
	if(!gXdg.empty() && gXdg.back() != '/') gXdg += "/";
	if(!gXdg.empty()) setenv("XDG_CACHE_HOME", gXdg.c_str(), 1);

	cFakeLow *pLow = new cFakeLow();
	static unsigned char vGfx[sizeof(cGraphics)], vRes[sizeof(cResources)], vInput[sizeof(cInput)], vLowIn[sizeof(cLowLevelInputSDL)], vEng[sizeof(cEngine)];
	cGraphics *pGfx = (cGraphics*)vGfx;
	pGfx->mpLowLevelGraphics = pLow;
	cResources *pRes = (cResources*)vRes;
	cBitmapLoaderHandler *pHandler = new cBitmapLoaderHandler(NULL, pGfx);
	pHandler->AddLoader(new cFakeLoader());
	pRes->mpBitmapLoaderHandler = pHandler;
	cLowLevelInputSDL *pLowIn = (cLowLevelInputSDL*)vLowIn;
	new (&pLowIn->mlstInjected) std::list<SDL_Event>();
	cInput *pInput = (cInput*)vInput;
	pInput->mpLowLevelInput = pLowIn;
	cEngine *pEng = (cEngine*)vEng;
	pEng->mpMutex = cPlatform::CreateMutEx();
	pEng->mpUpdater = new cUpdater(NULL);
	pEng->mpUpdater->AddContainer("Default");
	pEng->mpUpdater->SetContainer("Default");
	cRecUpd *pRec = new cRecUpd();
	pEng->mpUpdater->AddGlobalUpdate(pRec);
	pEng->mpGraphics = pGfx;
	pEng->mpResources = pRes;
	pEng->mpInput = pInput;
	pEng->mbGameIsDone = false;
	pEng->mbDevHudActive = false;

	char sSock[128];
	const char *pRun = getenv("XDG_RUNTIME_DIR");
	snprintf(sSock, sizeof(sSock), "%s/ohpl-h22-%d.sock", pRun ? pRun : "/tmp", (int)getpid());
	cHeadlessControlServer *pServer = new cHeadlessControlServer(pEng, sSock);
	pEng->mpHeadlessControl = pServer;
	if(!pServer->IsListening()) { fprintf(stderr, "not listening\n"); return 1; }

	int lFd = socket(AF_UNIX, SOCK_STREAM, 0);
	sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sSock, sizeof(addr.sun_path) - 1);
	if(connect(lFd, (sockaddr*)&addr, sizeof(addr)) != 0) { perror("connect"); return 1; }

	std::string sRecv;
	bool bLoading = false;
	bool bExitSeen = false;
	bool bDevHud = false;
	auto Drain = [&]() {
		pollfd pfd = { lFd, POLLIN, 0 };
		while(poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
		{
			char b[4096];
			ssize_t n = recv(lFd, b, sizeof(b), 0);
			if(n <= 0) break;
			sRecv.append(b, n);
		}
		for(size_t nl; (nl = sRecv.find('\n')) != std::string::npos;)
		{
			Out("< " + sRecv.substr(0, nl));
			sRecv.erase(0, nl + 1);
		}
	};
	auto Effects = [&]() {
		DrainEvents();
		while(!pLowIn->mlstInjected.empty())
		{
			Out("inj " + Hex(pLowIn->mlstInjected.front().text.text, strlen(pLowIn->mlstInjected.front().text.text)));
			pLowIn->mlstInjected.pop_front();
		}
		if(pEng->mbGameIsDone && !bExitSeen) { bExitSeen = true; Out("state exit"); }
		if(pEng->mbDevHudActive != bDevHud) { bDevHud = pEng->mbDevHudActive; Out(std::string("state dev_hud ") + (bDevHud ? "1" : "0")); }
	};
	auto Pump = [&](int n) {
		for(int i = 0; i < n; ++i)
		{
			pServer->Update(bLoading);
			pServer->LogicStep();
			Effects();
			Drain();
		}
	};

	std::ifstream in(pScript);
	if(!in) { fprintf(stderr, "no script\n"); return 3; }
	std::string sLine;
	while(std::getline(in, sLine))
	{
		if(sLine.empty() || sLine[0] == '#') continue;
		if(sLine[0] == '<' || sLine.compare(0, 2, "ev") == 0 || sLine.compare(0, 3, "inj") == 0 || sLine.compare(0, 5, "state") == 0) continue;
		Out(sLine);
		std::istringstream ss(sLine);
		std::string cmd;
		ss >> cmd;
		std::string rest;
		std::getline(ss, rest);
		if(!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
		if(cmd == ">")
		{
			std::string raw = Unescape(rest);
			send(lFd, raw.data(), raw.size(), 0);
		}
		else if(cmd == ">>") { std::string raw = Unescape(rest); send(lFd, raw.data(), raw.size(), 0); }
		else if(cmd == "settle") usleep(60000);
		else if(cmd == "pump") Pump(atoi(rest.c_str()));
		else if(cmd == "register")
		{
			pServer->RegisterHandler("echo", HEcho, (void*)7);
			pServer->RegisterHandler("ping", HPing, NULL);
			pServer->RegisterHandler("echo", HEcho, (void*)9);
			pServer->RegisterHandler("fail", HFail, NULL);
			pServer->RegisterHandler("wait_frames", HEcho, (void*)3);
			pServer->RegisterHandler("dev_hud", HEcho, (void*)5);
		}
		else if(cmd == "loading") bLoading = atoi(rest.c_str()) != 0;
		else if(cmd == "frames") for(int i = atoi(rest.c_str()); i > 0; --i) cEngineDiagnostics::EndFrame();
		else if(cmd == "log") pServer->PushLogLine(eLogOutputType_Normal, Unescape(rest));
		else if(cmd == "set")
		{
			std::istringstream rs(rest);
			std::string k;
			int v;
			rs >> k >> v;
			if(k == "copy_ok") gK.copyOk = v;
			else if(k == "save_ok") gK.saveOk = v;
			else if(k == "resize_ok") gK.resizeOk = v;
			else if(k == "float_ok") gK.floatOk = v;
			else if(k == "gamma100") gK.gamma100 = v;
		}
		else if(cmd == "shader")
		{
			std::istringstream rs(rest);
			std::string name, stage, log;
			int ok;
			rs >> name >> stage >> ok;
			std::getline(rs, log);
			if(!log.empty()) log.erase(0, 1);
			cEngineDiagnostics::ReportShader(Unescape(name), stage.c_str(), ok != 0, Unescape(log));
		}
		Drain();
	}
	Pump(2);
	delete pServer;
	return 0;
}
