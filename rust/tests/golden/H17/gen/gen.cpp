// Drives the real cInput / cAction (HPL2/core/sources/input) with scripted event streams and prints
// action states. Run with OHPL_IMPL=cpp or rust; both must print the golden trace.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "input/Input.h"
#include "input/Action.h"
#include "input/Keyboard.h"
#include "input/Mouse.h"
#include "input/Gamepad.h"
#include "input/LowLevelInput.h"
#include "input/RustInput.h"

using namespace hpl;

namespace hpl {
	void Log(const char*, ...) {}
	void Warning(const char*, ...) {}
	void Error(const char*, ...) {}
}

static std::vector<OhplInputEvent> gEvents;
static unsigned long gNow;
static const int kButtons = 21, kAxes = 6;

class FakeKeyboard : public iKeyboard
{
public:
	FakeKeyboard() : iKeyboard("fake kb") { mpRust = ohpl_input_kb_new(); }
	~FakeKeyboard() { ohpl_input_kb_free(mpRust); }
	void Update() { ohpl_input_kb_update(mpRust, gEvents.data(), gEvents.size(), 0); }
	bool KeyIsDown(eKey k) { return ohpl_input_kb_key_is_down(mpRust, k); }
	cKeyPress GetKey() { cKeyPress k(eKey_None, 0, 0); ohpl_input_kb_pop_pressed(mpRust, &k); return k; }
	bool KeyIsPressed() { return ohpl_input_kb_key_is_pressed(mpRust); }
	cKeyPress GetReleasedKey() { cKeyPress k(eKey_None, 0, 0); ohpl_input_kb_pop_released(mpRust, &k); return k; }
	bool KeyIsReleased() { return ohpl_input_kb_key_is_released(mpRust); }
	const OhplKeyboard* RustHandle() { return mpRust; }
private:
	OhplKeyboard *mpRust;
};

class FakeMouse : public iMouse
{
public:
	FakeMouse() : iMouse("fake mouse") { mpRust = ohpl_input_mouse_new(); }
	~FakeMouse() { ohpl_input_mouse_free(mpRust); }
	void Update()
	{
		ohpl_input_mouse_take_first_time(mpRust);
		OhplVec2l rel = {0, 0};
		ohpl_input_mouse_update(mpRust, gEvents.data(), gEvents.size(), false, rel);
	}
	bool ButtonIsDown(eMouseButton b) { return ohpl_input_mouse_button_is_down(mpRust, b); }
	cVector2l GetAbsPosition() { OhplVec2l v = ohpl_input_mouse_abs(mpRust); return cVector2l(v.x, v.y); }
	cVector2l GetRelPosition() { OhplVec2l v = ohpl_input_mouse_take_rel(mpRust); return cVector2l(v.x, v.y); }
	const OhplMouse* RustHandle() { return mpRust; }
private:
	OhplMouse *mpRust;
};

class FakeGamepad : public iGamepad
{
public:
	FakeGamepad(int idx) : iGamepad("fake pad", idx) { mpRust = ohpl_input_pad_new(idx, 100 + idx, kButtons, kAxes, kButtons + kAxes); }
	~FakeGamepad() { ohpl_input_pad_free(mpRust); }
	tString GetGamepadName() { return "fake"; }
	int GetNumButtons() { return kButtons; }
	int GetNumAxes() { return kAxes; }
	int GetNumHats() { return 0; }
	int GetNumBalls() { return 0; }
	void ClearState() { ohpl_input_pad_clear_state(mpRust); }
	float GetTimeSinceLastActive() { return 0; }
	void Update() { ohpl_input_pad_update(mpRust, gEvents.data(), gEvents.size(), gNow); }
	cGamepadInputData GetInputUpdate() { return cGamepadInputData(); }
	bool HasInputUpdates() { return false; }
	bool ButtonIsDown(eGamepadButton b) { return ohpl_input_pad_button_is_down(mpRust, b); }
	cGamepadInputData GetButton() { return cGamepadInputData(); }
	bool ButtonIsPressed() { return false; }
	cGamepadInputData GetReleasedButton() { return cGamepadInputData(); }
	bool ButtonIsReleased() { return false; }
	float GetAxisValue(eGamepadAxis a) { return ohpl_input_pad_axis_value(mpRust, a); }
	float GetAxisDeadZoneRadiusValue() { return 0; }
	void SetAxisDeadZoneRadiusValue(float) {}
	cGamepadInputData GetUpdatedAxis() { return cGamepadInputData(); }
	bool AxesUpdated() { return false; }
	eGamepadHatState GetHatCurrentState(eGamepadHat) { return eGamepadHatState_Centered; }
	bool HatIsInState(eGamepadHat h, eGamepadHatState s) { return (GetHatCurrentState(h) & s) != 0; }
	cGamepadInputData GetHatState() { return cGamepadInputData(); }
	bool HatsChanged() { return false; }
	cVector2l GetBallAbsPos(eGamepadBall) { return cVector2l(0, 0); }
	cVector2l GetBallRelPos(eGamepadBall) { return cVector2l(0, 0); }
	void SetRumble(float, int) {}
	const OhplGamepad* RustHandle() { return mpRust; }
private:
	OhplGamepad *mpRust;
};

class FakeLowLevelInput : public iLowLevelInput
{
public:
	int mlPads;
	FakeLowLevelInput(int pads) : mlPads(pads) {}
	void LockInput(bool) {}
	void RelativeMouse(bool) {}
	void BeginInputUpdate() {}
	void EndInputUpdate() {}
	bool isQuitMessagePosted() { return false; }
	void resetQuitMessagePosted() {}
	void InitGamepadSupport() {}
	void DropGamepadSupport() {}
	int GetPluggedGamepadNum() { return mlPads; }
	iMouse* CreateMouse() { return new FakeMouse(); }
	iKeyboard* CreateKeyboard() { return new FakeKeyboard(); }
	iGamepad* CreateGamepad(int i) { return new FakeGamepad(i); }
};

int main(int argc, char **argv)
{
	if(argc < 2) return 2;
	std::ifstream in(argv[1]);
	std::string line;
	FakeLowLevelInput *pLow = NULL;
	cInput *pInput = NULL;
	std::vector<cAction*> vTable;
	int lCmd = 0;
	auto indexOf = [&](cAction *p) {
		if(p == NULL) return -1;
		for(size_t i = 0; i < vTable.size(); ++i) if(vTable[i] == p) return (int)i;
		return -2;
	};
	auto states = [&](cAction *p) {
		bool a = p->IsTriggerd(), b = p->WasTriggerd(), c = p->BecameTriggerd(), d = p->DoubleTriggerd(0.25f);
		return std::to_string(a) + std::to_string(b) + std::to_string(c) + std::to_string(d);
	};
	while(std::getline(in, line))
	{
		if(line.empty() || line[0] == '#') continue;
		std::istringstream ss(line);
		std::string cmd;
		ss >> cmd;
		++lCmd;
		auto act = [&](int k) -> cAction* { return (k >= 0 && k < (int)vTable.size()) ? vTable[k] : NULL; };
		if(cmd == "new")
		{
			int n; ss >> n;
			delete pInput; delete pLow;
			vTable.clear();
			pLow = new FakeLowLevelInput(n);
			pInput = new cInput(pLow);
		}
		else if(cmd == "refresh")
		{
			int n; ss >> n;
			pLow->mlPads = n;
			pInput->AppDeviceWasPlugged();
			printf("%d pads %d\n", lCmd, pInput->GetGamepadNum());
		}
		else if(cmd == "create")
		{
			std::string name; int id; ss >> name >> id;
			vTable.push_back(pInput->CreateAction(name, id));
		}
		else if(cmd == "bind")
		{
			int k; std::string t; ss >> k >> t;
			cAction *p = act(k);
			if(!p) continue;
			int a, b, c; float lo, hi;
			if(t == "key") { ss >> a; p->AddKey((eKey)a); }
			else if(t == "mouse") { ss >> a; p->AddMouseButton((eMouseButton)a); }
			else if(t == "padbtn") { ss >> a >> b; p->AddGamepadButton(a, (eGamepadButton)b); }
			else if(t == "padaxis") { ss >> a >> b >> c >> lo >> hi; p->AddGamepadAxis(a, (eGamepadAxis)b, (eGamepadAxisRange)c, lo, hi); }
			else if(t == "padhat") { ss >> a >> b >> c; p->AddGamepadHat(a, (eGamepadHat)b, (eGamepadHatState)c); }
		}
		else if(cmd == "clear") { int k; ss >> k; if(act(k)) act(k)->ClearSubActions(); }
		else if(cmd == "destroy")
		{
			int k; ss >> k;
			if(act(k)) { pInput->DestroyAction(act(k)); vTable[k] = NULL; }
		}
		else if(cmd == "getn")
		{
			std::string name; ss >> name;
			printf("%d getn %d\n", lCmd, indexOf(pInput->GetAction(name)));
		}
		else if(cmd == "getid")
		{
			int id; ss >> id;
			printf("%d getid %d\n", lCmd, indexOf(pInput->GetAction(id)));
		}
		else if(cmd == "ev")
		{
			OhplInputEvent e;
			memset(&e, 0, sizeof(e));
			int st, bt, av;
			ss >> e.kind >> e.sym >> e.key_mod >> st >> bt >> av >> e.x >> e.y >> e.xrel >> e.yrel >> e.wheel_y >> e.which;
			e.state = st; e.button = bt; e.axis_value = av;
			gEvents.push_back(e);
		}
		else if(cmd == "update")
		{
			float dt; ss >> dt;
			gNow += 16;
			pInput->Update(dt);
			gEvents.clear();
		}
		else if(cmd == "q")
		{
			int k; ss >> k;
			if(act(k)) printf("%d q %d %s\n", lCmd, k, states(act(k)).c_str());
		}
		else if(cmd == "qa")
		{
			for(size_t k = 0; k < vTable.size(); ++k)
				if(vTable[k]) printf("%d q %d %s\n", lCmd, (int)k, states(vTable[k]).c_str());
		}
		else if(cmd == "qn")
		{
			std::string name; ss >> name;
			bool a = pInput->IsTriggerd(name), b = pInput->WasTriggerd(name), c = pInput->BecameTriggerd(name), d = pInput->DoubleTriggerd(name, 0.25f);
			printf("%d qn %d%d%d%d\n", lCmd, a, b, c, d);
		}
		else if(cmd == "qi")
		{
			int id; ss >> id;
			bool a = pInput->IsTriggerd(id), b = pInput->WasTriggerd(id), c = pInput->BecameTriggerd(id), d = pInput->DoubleTriggerd(id, 0.25f);
			printf("%d qi %d%d%d%d\n", lCmd, a, b, c, d);
		}
		else if(cmd == "reset") pInput->ResetActionsToCurrentState();
		else if(cmd == "resetone") { int k; ss >> k; if(act(k)) act(k)->ResetToCurrentState(); }
		else if(cmd == "any") printf("%d any %d\n", lCmd, (int)pInput->CheckForInput());
		else if(cmd == "sub")
		{
			iSubAction *p = pInput->InputToSubAction();
			if(p) { printf("%d sub %s %s\n", lCmd, p->GetInputType().c_str(), p->GetInputName().c_str()); delete p; }
			else printf("%d sub none\n", lCmd);
		}
		else if(cmd == "names")
		{
			int k; ss >> k;
			cAction *p = act(k);
			if(!p) continue;
			printf("%d ~ %d n=%d", lCmd, k, p->GetSubActionNum());
			for(int i = 0; i < p->GetSubActionNum(); ++i)
			{
				iSubAction *s = p->GetSubAction(i);
				printf(" [%s %s %d %g]", s->GetInputType().c_str(), s->GetInputName().c_str(), (int)s->IsTriggerd(), s->GetValue());
			}
			printf("\n");
		}
	}
	delete pInput;
	delete pLow;
	return 0;
}
