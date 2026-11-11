#ifndef SOMA_LUX_PLAYER_H
#define SOMA_LUX_PLAYER_H

#include "SomaLuxGame.h"

#include <deque>
#include <map>

class cSomaLuxPlayer;
class cSomaLuxEntity;
struct cSomaID;

class cSomaLuxPlayerState : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
	cSomaLuxPlayer *mpPlayer = NULL;
};

class cSomaLuxMoveState : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
	cSomaLuxPlayer *mpPlayer = NULL;
	bool mbAutoUpdateSpeedSettings = true;

	float mfMaxForwardSpeed = 0;
	float mfMaxBackwardSpeed = 0;
	float mfMaxSidwaySpeed = 0;
	float mfForwardAcc = 0;
	float mfSidewayAcc = 0;
	float mfForwardDeacc = 0;
	float mfSidewayDeacc = 0;
	float mfForwardOppositeDirAccMul = 1;
	float mfSidewayOppositeDirAccMul = 1;
	float mfMaxForwardSpeedMul = 1;
	float mfMaxBackwardSpeedMul = 1;
	float mfMaxSidwaySpeedMul = 1;

	// cLuxMoveState::Update
	void OnUpdate(float afTimeStep) override;
};

class cSomaLuxPlayer : public cSomaLuxHandler
{
public:
	cSomaLuxPlayer();
	~cSomaLuxPlayer();

	static cSomaLuxPlayer *Get() { return mpInstance; }

	void SetCamera(cCamera *apCamera);
	cCamera *GetCamera() { return mpCamera; }
	iCharacterBody *GetCharacterBody() { return mpCharBody; }
	void SetCharacterBody(iCharacterBody *apBody);

	bool IsActive() { return mbActive; }
	void SetActive(bool abX) { mbActive = abX; }

	void AddState(const tString &asName, int alId, const tString &asFile, const tString &asClass);
	void AddMoveState(const tString &asName, int alId, const tString &asFile, const tString &asClass);
	void ChangeState(int alId);
	void ChangeMoveState(int alId);
	cSomaLuxPlayerState *GetState() { return mpState; }
	cSomaLuxMoveState *GetMoveState() { return mpMoveState; }

	// cLuxMapHandler::SetCurrentMap: after CreateWorldEntities/OnMapEnter
	void PlaceAtStart(const cVector3f &avFeetPos, float afYaw, bool abCrouching = false);

	void OnMessage(const char *apDecl) override;
	void OnUpdate(float afTimeStep) override;
	void OnPostUpdate(float afTimeStep) override;
	void OnVariableUpdate(float afTimeStep) override;
	void OnGui(float afTimeStep) override;
	void OnMapMessage(const char *apDecl, void *apMap) override;
	void OnAction(int alAction, bool abPressed) override;
	void OnAnalogInput(int alAnalogId, const cVector3f &avAmount) override;

	float GetDefaultFOV() { return mfDefaultFOV; }

	static void RegisterNatives(asIScriptEngine *apEngine);

	struct cMoveVector : OhplGs3MoveVec {};
	struct cFadeValue : OhplGs3Fade {};

	cVector3f mvBaseCameraPosAdd = 0;
	std::map<int, cMoveVector> mmapCameraPosAdd;
	std::map<int, cFadeValue> mmapCameraRoll;
	std::map<int, cFadeValue> mmapFOVMul;
	cFadeValue &FOVMul(int alId)
	{
		auto it = mmapFOVMul.find(alId);
		if (it == mmapFOVMul.end())
		{
			it = mmapFOVMul.emplace(alId, cFadeValue()).first;
			it->second.mfValue = it->second.mfGoal = 1;
		}
		return it->second;
	}
	float GetFOVMul()
	{
		std::vector<float> v;
		for (auto &it : mmapFOVMul)
			v.push_back(it.second.mfValue);
		return ohpl_gs3_fov_mul(mFOVMul.mfValue, v.data(), v.size());
	}
	cFadeValue mFOVMul, mAspectMul, mFOV;

	void SetHealth(float afX);
	bool GiveDamage(float afAmount, int alType, float afMinHealth, const cSomaID &aSource, int alDamageID);
	std::vector<int> mvDamageIDs;
	bool IsDead() const { return ohpl_gs3_is_dead(mfHealth, mbCanDie); }
	float mfHealth = 1;
	bool mbCanDie = true;
	float mfMaxHealth = 1;

	bool mbCameraRotateActive = false;
	cVector3f mvCameraRotateTarget = 0;
	bool mbCameraRotateLocal = false;
	float mfCameraRotateSpeed = 0, mfCameraRotateAcc = 0, mfCameraRotateSpeedMul = 0, mfCameraRotateMaxSpeed = 0;

	bool mbAutomoveActive = false;
	cVector3f mvAutomoveTarget = 0;
	float mfAutomoveSpeedMul = 0;

	std::map<int, float> mmapVisibilityRangeMul;
	std::map<int, float> mmapVisibilityMaxRange;
	float GetVisibilityRangeMul()
	{
		std::vector<float> v;
		for (auto &it : mmapVisibilityRangeMul)
			v.push_back(it.second);
		return ohpl_gs3_visibility_range_mul(v.data(), v.size());
	}
	float GetVisibilityMaxRange()
	{
		std::vector<float> v;
		for (auto &it : mmapVisibilityMaxRange)
			v.push_back(it.second);
		return ohpl_gs3_visibility_max_range(v.data(), v.size());
	}

	float mfTimeSincePhysicsInteraction = 0;
	float mfAverageMoveSpeed = 0;
	float mfSmoothingRotAmount = 0;
	cVector3f mvAverageMoveDirection = 0;
	cVector2f mvCameraTrackingAvg = 0;

private:
	void UpdateCamera(float afTimeStep);

	struct cCharBodyCallback : public iCharacterBodyCallback
	{
		cSomaLuxPlayer *mpPlayer = NULL;
		void OnGravityCollide(iCharacterBody *, iPhysicsBody *, cCollideData *) override {}
		void OnHitGround(iCharacterBody *apBody, const cVector3f &avVel) override;
	} mCharBodyCallback;

	static cSomaLuxPlayer *mpInstance;

	cCamera *mpCamera = NULL;
	iCharacterBody *mpCharBody = NULL;
	bool mbActive = true;
	float mfDefaultFOV = 70;

	std::map<int, cSomaLuxPlayerState *> mmapStates;
	std::map<int, cSomaLuxMoveState *> mmapMoveStates;
	cSomaLuxPlayerState *mpState = NULL;
	cSomaLuxMoveState *mpMoveState = NULL;
};

class cSomaLuxInputHandler : public cSomaLuxHandler
{
public:
	cSomaLuxInputHandler();

	static cSomaLuxInputHandler *Get() { return mpInstance; }

	// cLuxInputHandler::LoadKeyConfig: script CreateActions(), then LoadKeyConfig(cfg) binds them
	void LoadKeyConfig();
	// cLuxInputHandler::LoadScript
	void LoadScript();
	void LoadUserConfig();
	// cLuxInputHandler::Update: action edges and analog sums to every updateable, then mouse look
	void UpdateInput(float afTimeStep, bool abGameInput);

	static void RegisterNatives(asIScriptEngine *apEngine);

	struct cLuxAction
	{
		tString msName;
		int mlId;
		int mlAxis = -1;
		float mfMul = 0;
		int mlAnalogId = -1;
		bool mbGamepad = false;
	};
	std::vector<cLuxAction> mvActions;

	void CreateAction(const cLuxAction &aAction);
	void CreateActionInput(const tString &asInput, int alActionId);
	tString GetActionName(int alId, bool abGamepad = false);

	struct cGamepadPreset
	{
		tString msName;
		std::vector<int> mvActions;
		tStringVec mvBindings;
		std::vector<bool> mvAnalog;
	};
	struct cGamepadProfile
	{
		tString msName, msPrefix;
		tStringVec mvButtons, mvAxes;
		std::vector<unsigned> mvDPad;
		std::vector<cGamepadPreset> mvPresets;
	};
	std::vector<cGamepadProfile> mvGamepadProfiles;
	tString msGamepadProfile, msGamepadPreset;
	cGamepadProfile *GetGamepadProfile(const tString &asName);
	cGamepadPreset *GetGamepadPreset();
	void GetActionsAssociatedToGamepadControl(const tString &asProfile, const tString &asPreset, const tString &asControl, tString &asActions);
	bool FetchGamepadInputLayoutString(const tString &asInput, tString &asPrefix, tString &asLayout);

	// Per-frame action edges; HPL2's cAction::BecameTriggerd consumes the edge on the first call
	void LatchActions();
	void ClearGuiInput();
	bool IsDown(int alId) { return alId >= 0 && alId < kMaxActions && mvDown[alId]; }
	bool BecameDown(int alId) { return ActionEdge(alId) == 1; }
	bool BecameUp(int alId) { return ActionEdge(alId) == 2; }

	bool mbInvertMouse = false;
	bool mbSmoothMouse = true;
	float mfMouseSensitivity = 1;
	float mfGamepadSensitivity = 2;
	int mlMaxSmoothMousePos = 7;
	float mfPrevSmoothMousePosMul = 0.7f;
	std::deque<cVector2f> mlstSmoothMousePos;
	tString msLatestKey;

private:
	static const int kMaxActions = 256;
	int ActionEdge(int alId)
	{
		bool bInRange = alId >= 0 && alId < kMaxActions;
		return ohpl_gs3_action_edge(alId, kMaxActions, bInRange && mvDown[alId], bInRange && mvPrevDown[alId]);
	}
	bool mvDown[kMaxActions] = {};
	bool mvPrevDown[kMaxActions] = {};
	bool mvPrevKeys[eKey_LastEnum] = {};
	bool mvPrevButtons[eMouseButton_LastEnum] = {};
	static cSomaLuxInputHandler *mpInstance;
};

#endif // SOMA_LUX_PLAYER_H
